//
// Copyright (c) 2006-2015 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Clasp is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Clasp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Clasp; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
#include <clasp/program_builder.h>
#include <clasp/shared_context.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/weight_constraint.h>
#include <clasp/parser.h>
#include <limits>
namespace Clasp {

/////////////////////////////////////////////////////////////////////////////////////////
// class ProgramBuilder
/////////////////////////////////////////////////////////////////////////////////////////
ProgramBuilder::ProgramBuilder() : ctx_(0), frozen_(true) {}
ProgramBuilder::~ProgramBuilder() {}
bool ProgramBuilder::ok() const { return ctx_ && ctx_->ok(); }
bool ProgramBuilder::startProgram(SharedContext& ctx) {
	ctx.report(message(Event::subsystem_load, "Reading"));
	ctx_    = &ctx;
	frozen_ = ctx.frozen();
	return ctx_->ok() && doStartProgram();
}
bool ProgramBuilder::updateProgram() {
	CLASP_ASSERT_CONTRACT_MSG(ctx_, "startProgram() not called!");
	bool up = frozen();
	bool ok = ctx_->ok() && ctx_->unfreeze() && doUpdateProgram();
	frozen_ = ctx_->frozen();
	if (up && !frozen()){ ctx_->report(message(Event::subsystem_load, "Reading")); }
	return ok;
}
bool ProgramBuilder::endProgram() {
	CLASP_ASSERT_CONTRACT_MSG(ctx_, "startProgram() not called!");
	bool ok = ctx_->ok();
	if (ok && !frozen_) {
		ctx_->report(message(Event::subsystem_prepare, "Preprocessing"));
		ok = doEndProgram();
		frozen_ = true;
	}
	return ok;
}
void ProgramBuilder::getAssumptions(LitVec& out) const {
	CLASP_ASSERT_CONTRACT(ctx_ && frozen());
	if (!isSentinel(ctx_->stepLiteral())) {
		out.push_back(ctx_->stepLiteral());
	}
	doGetAssumptions(out);
}
void ProgramBuilder::getWeakBounds(SumVec& out) const {
	CLASP_ASSERT_CONTRACT(ctx_ && frozen());
	doGetWeakBounds(out);
}
ProgramParser& ProgramBuilder::parser() {
	if (!parser_.get()) {
		parser_.reset(doCreateParser());
	}
	return *parser_;
}
bool ProgramBuilder::parseProgram(std::istream& input) {
	CLASP_ASSERT_CONTRACT(ctx_ && !frozen());
	ProgramParser& p = parser();
	CLASP_FAIL_IF(!p.accept(input), "unrecognized input format");
	return p.parse();
}
void ProgramBuilder::addMinLit(weight_t prio, WeightLiteral x) {
	ctx_->addMinimize(x, prio);
}
void ProgramBuilder::doGetWeakBounds(SumVec&) const  {}
/////////////////////////////////////////////////////////////////////////////////////////
// class SatBuilder
/////////////////////////////////////////////////////////////////////////////////////////
SatBuilder::SatBuilder(bool maxSat) : ProgramBuilder(), hardWeight_(0), vars_(0), pos_(0), maxSat_(maxSat) {}
bool SatBuilder::markAssigned() {
	if (pos_ == ctx()->master()->trail().size()) { return true; }
	bool ok = ctx()->ok() && ctx()->master()->propagate();
	for (const LitVec& trail = ctx()->master()->trail(); pos_ < trail.size(); ++pos_) {
		markLit(~trail[pos_]);
	}
	return ok;
}
void SatBuilder::prepareProblem(uint32 numVars, wsum_t cw, uint32 clauseHint) {
	CLASP_ASSERT_CONTRACT_MSG(ctx(), "startProgram() not called!");
	ctx()->resizeVars(numVars + 1);
	ctx()->output.add(Range32(1, numVars+1));
	ctx()->startAddConstraints(std::min(clauseHint, uint32(10000)));
	varState_.resize(numVars + 1);
	vars_       = ctx()->numVars();
	hardWeight_ = cw;
	markAssigned();
}
bool SatBuilder::addObjective(const WeightLitVec& min) {
	for (WeightLitVec::const_iterator it = min.begin(), end = min.end(); it != end; ++it) {
		addMinLit(0, *it);
		varState_[it->first.var()] |= (falseValue(it->first) << 2u);
	}
	return ctx()->ok();
}
void SatBuilder::addProject(Var v) {
	ctx()->output.addProject(posLit(v));
}
bool SatBuilder::addClause(LitVec& clause, wsum_t cw) {
	if (!ctx()->ok() || satisfied(clause)) { return ctx()->ok(); }
	CLASP_ASSERT_CONTRACT_MSG(cw >= 0 && (cw <= std::numeric_limits<weight_t>::max() || cw == hardWeight_), "Clause weight out of bounds!");
	if (cw == 0 && maxSat_){ cw = 1; }
	if (cw == hardWeight_) {
		return ClauseCreator::create(*ctx()->master(), clause, Constraint_t::Static).ok() && markAssigned();
	}
	else {
		// Store weight, relaxtion var, and (optionally) clause
		softClauses_.push_back(Literal::fromRep((uint32)cw));
		if      (clause.size() > 1){ softClauses_.push_back(posLit(++vars_)); softClauses_.insert(softClauses_.end(), clause.begin(), clause.end()); }
		else if (!clause.empty())  { softClauses_.push_back(~clause.back());  }
		else                       { softClauses_.push_back(lit_true()); }
		softClauses_.back().flag(); // mark end of clause
	}
	return true;
}
bool SatBuilder::satisfied(LitVec& cc) {
	bool sat = false;
	LitVec::iterator j = cc.begin();
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		Literal x = *it;
		uint32  m = 1+x.sign();
		uint32  n = uint32(varState_[it->var()] & 3u) + m;
		if      (n == m) { varState_[it->var()] |= m; x.unflag(); *j++ = x; }
		else if (n == 3u){ sat = true; break; }
	}
	cc.erase(j, cc.end());
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		if (!sat) { varState_[it->var()] |= (varState_[it->var()] & 3u) << 2; }
		varState_[it->var()] &= ~3u;
	}
	return sat;
}
bool SatBuilder::doStartProgram() {
	vars_ = ctx()->numVars();
	pos_  = 0;
	return markAssigned();
}
ProgramParser* SatBuilder::doCreateParser() {
	return new SatParser(*this);
}
bool SatBuilder::doEndProgram() {
	bool ok = ctx()->ok();
	if (!softClauses_.empty() && ok) {
		ctx()->setPreserveModels(true);
		ctx()->resizeVars(vars_+1);
		ctx()->startAddConstraints();
		LitVec cc;
		for (LitVec::const_iterator it = softClauses_.begin(), end = softClauses_.end(); it != end && ok; ++it) {
			weight_t w     = (weight_t)it->rep();
			Literal  relax = *++it;
			if (!relax.flagged()) {
				cc.assign(1, relax);
				do { cc.push_back(*++it); } while (!cc.back().flagged());
				cc.back().unflag();
				ok = ClauseCreator::create(*ctx()->master(), cc, Constraint_t::Static).ok();
			}
			addMinLit(0, WeightLiteral(relax.unflag(), w));
		}
		LitVec().swap(softClauses_);
	}
	if (ok) {
		const uint32 seen = 12;
		const bool   elim = !ctx()->preserveModels();
		for (Var v = 1; v != (Var)varState_.size() && ok; ++v) {
			uint32 m = varState_[v];
			if ( (m & seen) != seen ) {
				if      (m)   { ctx()->master()->setPref(v, ValueSet::pref_value, ValueRep(m >> 2)); }
				else if (elim){ ctx()->eliminate(v); }
			}
		}
	}
	return ok;
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PBBuilder
/////////////////////////////////////////////////////////////////////////////////////////
PBBuilder::PBBuilder() : nextVar_(0) {}
void PBBuilder::prepareProblem(uint32 numVars, uint32 numProd, uint32 numSoft, uint32 numCons) {
	CLASP_ASSERT_CONTRACT_MSG(ctx(), "startProgram() not called!");
	uint32 maxVar = numVars + numProd + numSoft;
	nextVar_      = numVars;
	maxVar_       = maxVar;
	ctx()->resizeVars(maxVar + 1);
	ctx()->output.add(Range32(1, numVars+1));
	ctx()->startAddConstraints(numCons);
}
uint32 PBBuilder::getNextVar() {
	CLASP_ASSERT_CONTRACT_MSG(ctx()->validVar(nextVar_+1), "Variables out of bounds");
	return ++nextVar_;
}
bool PBBuilder::addConstraint(WeightLitVec& lits, weight_t bound, bool eq, weight_t cw) {
	if (!ctx()->ok()) { return false; }
	Var eqVar = 0;
	if (cw > 0) { // soft constraint
		if (lits.size() != 1) {
			eqVar = getNextVar();
			addMinLit(0, WeightLiteral(negLit(eqVar), cw));
		}
		else {
			if (lits[0].second < 0)    { bound += (lits[0].second = -lits[0].second); lits[0].first = ~lits[0].first; }
			if (lits[0].second < bound){ lits[0].first = lit_false(); }
			addMinLit(0, WeightLiteral(~lits[0].first, cw));
			return true;
		}
	}
	return WeightConstraint::create(*ctx()->master(), posLit(eqVar), lits, bound, !eq ? 0 : WeightConstraint::create_eq_bound).ok();
}

bool PBBuilder::addObjective(const WeightLitVec& min) {
	for (WeightLitVec::const_iterator it = min.begin(), end = min.end(); it != end; ++it) {
		addMinLit(0, *it);
	}
	return ctx()->ok();
}
void PBBuilder::addProject(Var v) {
	ctx()->output.addProject(posLit(v));
}
bool PBBuilder::setSoftBound(wsum_t b) {
	if (b > 0) { soft_ = b-1; }
	return true;
}

void PBBuilder::doGetWeakBounds(SumVec& out) const {
	if (soft_ != std::numeric_limits<wsum_t>::max()) {
		if      (out.empty())   { out.push_back(soft_); }
		else if (out[0] > soft_){ out[0] = soft_; }
	}
}

Literal PBBuilder::addProduct(LitVec& lits) {
	if (!ctx()->ok()) { return lit_false(); }
	prod_.lits.reserve(lits.size() + 1);
	if (productSubsumed(lits, prod_)){
		return lits[0];
	}
	Literal& eq = products_[prod_];
	if (eq != lit_true()) {
		return eq;
	}
	eq = posLit(getNextVar());
	addProductConstraints(eq, lits);
	return eq;
}
bool PBBuilder::productSubsumed(LitVec& lits, PKey& prod) {
	Literal last       = lit_true();
	LitVec::iterator j = lits.begin();
	Solver& s          = *ctx()->master();
	uint32  abst       = 0;
	prod.lits.assign(1, lit_true()); // room for abst
	for (LitVec::const_iterator it = lits.begin(), end = lits.end(); it != end; ++it) {
		if (s.isFalse(*it) || ~*it == last) { // product is always false
			lits.assign(1, lit_false());
			return true;
		}
		else if (last.var() > it->var()) { // not sorted - redo with sorted product
			std::sort(lits.begin(), lits.end());
			return productSubsumed(lits, prod);
		}
		else if (!s.isTrue(*it) && last != *it) {
			prod.lits.push_back(*it);
			abst += hashLit(*it);
			last  = *it;
			*j++  = last;
		}
	}
	prod.lits[0].rep() = abst;
	lits.erase(j, lits.end());
	if (lits.empty()) { lits.assign(1, lit_true()); }
	return lits.size() < 2;
}
void PBBuilder::addProductConstraints(Literal eqLit, LitVec& lits) {
	Solver& s = *ctx()->master();
	assert(s.value(eqLit.var()) == value_free);
	bool ok   = ctx()->ok();
	for (LitVec::iterator it = lits.begin(), end = lits.end(); it != end && ok; ++it) {
		assert(s.value(it->var()) == value_free);
		ok    = ctx()->addBinary(~eqLit, *it);
		*it   = ~*it;
	}
	lits.push_back(eqLit);
	if (ok) { ClauseCreator::create(s, lits, ClauseCreator::clause_no_prepare, ClauseInfo()); }
}

bool PBBuilder::doStartProgram() {
	nextVar_ = ctx()->numVars();
	soft_    = std::numeric_limits<wsum_t>::max();
	return true;
}
bool PBBuilder::doEndProgram() {
	while (nextVar_ < maxVar_) {
		if (!ctx()->addUnary(negLit(++nextVar_))) { return false; }
	}
	return true;
}
ProgramParser* PBBuilder::doCreateParser() {
	return new SatParser(*this);
}

}
