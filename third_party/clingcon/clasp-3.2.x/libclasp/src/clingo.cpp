// 
// Copyright (c) 2015-2016, Benjamin Kaufmann
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
#include <clasp/clingo.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/util/atomic.h>
#include <algorithm>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// TheoryPropagator::Data
/////////////////////////////////////////////////////////////////////////////////////////
struct TheoryPropagator::Watches {
	Potassco::Lit_t add(Literal lit) {
		uint32 word = lit.id() / 32;
		uint32 bit  = lit.id() & 31;
		if (word >= map.size()) { map.resize(word + 1, 0); }
		if (!test_bit(map[word], bit)) {
			vec.push_back(lit);
			set_bit(map[word], bit);
		}
		return encodeLit(lit);
	}
	VarVec map;
	LitVec vec;
};
/////////////////////////////////////////////////////////////////////////////////////////
// TheoryPropagator::PP
/////////////////////////////////////////////////////////////////////////////////////////
class TheoryPropagator::PP
	: public Clasp::PostPropagator
	, public Potassco::AbstractSolver
	, public Potassco::AbstractAssignment {
public:
	typedef Clasp::PostPropagator::PropResult PPair;
	PP(TheoryPropagator& w) : wrapper_(&w), init_(0), delta_(0), inProp_(0) {
		undo_.push_back(Undo(0, 0));
	}
	// PostPropagator
	virtual uint32 priority() const;
	virtual bool   init(Solver& s);
	virtual bool   propagateFixpoint(Clasp::Solver& s, Clasp::PostPropagator* ctx);
	virtual PPair  propagate(Solver&, Literal, uint32&);
	virtual bool   isModel(Solver& s);
	virtual void   reason(Solver&, Literal, LitVec&);
	virtual void   reset();
	virtual void   undoLevel(Solver& s);
	// AbstractSolver
	virtual Potassco::Id_t id() const { return solver_->id(); }
	virtual const AbstractAssignment& assignment() const { return *this; }
	virtual bool addClause(const Potassco::LitSpan& clause);
	virtual bool propagate();
	// AbstractAssignment
	virtual bool     hasConflict()     const { return solver_->hasConflict(); }
	virtual uint32_t level()           const { return solver_->decisionLevel(); }
	virtual bool     hasLit(Lit_t lit) const { return solver_->validVar(decodeVar(lit)); }
	//! Returns the truth value that is currently assigned to lit or Value_t::Free if lit is unassigned.
	virtual Value_t  value(Lit_t lit) const {
		CLASP_FAIL_IF(!this->TheoryPropagator::PP::hasLit(lit), "invalid variable");
		switch (solver_->value(decodeVar(lit))) {
			default: return Value_t::Free;
			case value_true:  return lit >= 0 ? Value_t::True  : Value_t::False;
			case value_false: return lit >= 0 ? Value_t::False : Value_t::True;
		}
	}
	virtual uint32_t level(Lit_t lit) const {
		return value(lit) != Potassco::Value_t::Free ? solver_->level(decodeVar(lit)) : uint32_t(-1);
	}
private:
	struct Undo {
		Undo(uint32 dl, size_t d) : level(dl), delta(d) {}
		uint32 level;
		size_t delta; 
	};
	void pushTrail(Literal p) {
		trail_.push_back(encodeLit(p));
	}
	typedef LitVec::size_type size_t;
	typedef PodVector<Undo>::type UndoSt;
	typedef ClauseCreator::Status ClStatus;
	typedef PodVector<Potassco::Lit_t>::type TrailVec;
	static const uint32 ccFlags_s;
	TheoryPropagator* wrapper_;
	Solver*  solver_;
	TrailVec trail_;
	LitVec   clause_;
	UndoSt   undo_;   
	size_t   init_;
	size_t   delta_;
	ClStatus status_;
	bool     inProp_;
};

const uint32 TheoryPropagator::PP::ccFlags_s = Clasp::ClauseCreator::clause_not_sat | Clasp::ClauseCreator::clause_int_lbd;

uint32 TheoryPropagator::PP::priority() const {
	return static_cast<uint32>(priority_reserved_look - 1);
}
bool TheoryPropagator::PP::init(Solver& s) {
	const LitVec& watches = wrapper_->watches_->vec;
	CLASP_FAIL_IF(init_ > watches.size(), "invalid watch list!");
	for (size_t max = watches.size(); init_ != max; ++init_) {
		Literal p = watches[init_];
		if (s.value(p.var()) == value_free || s.level(p.var()) > s.rootLevel()) {
			s.addWatch(p, this, init_);
		}
		else if (s.isTrue(p)) {
			pushTrail(p);
		}
	}
	return true;
}

Constraint::PropResult TheoryPropagator::PP::propagate(Solver&, Literal p, uint32&) {
	pushTrail(p);
	return PropResult(true, true);
}
void TheoryPropagator::PP::reason(Solver&, Literal p, LitVec& r) {
	for (LitVec::const_iterator it = clause_.begin() + (p == clause_[0]), end = clause_.end(); it != end; ++it) {
		r.push_back(~*it);
	}
}
void TheoryPropagator::PP::reset() {
	trail_.resize(delta_);
}
bool TheoryPropagator::PP::propagateFixpoint(Clasp::Solver& s, Clasp::PostPropagator*) {
	typedef Potassco::AbstractPropagator::ChangeList ChangeList;
	while (delta_ != trail_.size()) {
		uint32 dl = s.decisionLevel();
		if (undo_.back().level != dl) {
			s.addUndoWatch(dl, this);
			undo_.push_back(Undo(dl, delta_));
		}
		ChangeList change = Potassco::toSpan(&trail_[0] + delta_, trail_.size() - delta_);
		delta_  = trail_.size();
		status_ = ClauseCreator::status_open;
		solver_ = &s;
		inProp_ = true;
		bool ok = wrapper_->prop_->propagate(*this, change);
		inProp_ = false;
		if (!ok && !s.hasConflict() && (status_ & ClauseCreator::status_asserting) != 0) {
			if (s.level(clause_[1].var()) < dl && dl != s.backtrackLevel()) {
				TheoryPropagator::PP::reset();
				for (PostPropagator* n = this->next; n; n = n->next) { n->reset(); }
				dl = s.undoUntil(s.level(clause_[1].var()));
			}
			if (!s.isFalse(clause_[0])) {
				ok = ClauseCreator::create(*solver_, clause_, ccFlags_s | ClauseCreator::clause_no_prepare, Constraint_t::Other);
			}
			else {
				return s.force(clause_[0], this);
			}
		}
		if (s.hasConflict() || !ok || (s.queueSize() && !s.propagateUntil(this))) {
			if (!s.hasConflict()) { s.setStopConflict(); }
			return false;
		}
		CLASP_FAIL_IF(dl < s.decisionLevel(), "invalid operation in propagation");
	}
	return true;
}
bool TheoryPropagator::PP::isModel(Solver& s) {
	solver_ = &s;
	inProp_ = false;
	return wrapper_->prop_->model(*this) && !s.hasConflict();
}
void TheoryPropagator::PP::undoLevel(Solver& s) {
	typedef Potassco::AbstractPropagator::ChangeList ChangeList;
	CLASP_FAIL_IF(s.decisionLevel() != undo_.back().level, "invalid undo!");
	delta_ = undo_.back().delta;
	undo_.pop_back();
	ChangeList change = Potassco::toSpan(&trail_[0] + delta_, trail_.size() - delta_);
	wrapper_->prop_->undo(*this, change);
	trail_.resize(delta_);
}
bool TheoryPropagator::PP::addClause(const Potassco::LitSpan& clause) {
	CLASP_FAIL_IF(solver_->hasConflict(), "invalid addClause()!");
	clause_.clear();
	for (const Potassco::Lit_t* it = Potassco::begin(clause); it != Potassco::end(clause); ++it) {
		clause_.push_back(decodeLit(*it));
	}
	ClauseRep rep = ClauseCreator::prepare(*solver_, clause_, Clasp::ClauseCreator::clause_force_simplify, Constraint_t::Other);
	if (clause_.size() < 2) { clause_.resize(2, lit_false()); }
	status_ = ClauseCreator::status(*solver_, rep);
	uint32 impLevel = (status_ & ClauseCreator::status_asserting) != 0 ? solver_->level(clause_[1].var()) : UINT32_MAX;
	if (!inProp_ || impLevel >= solver_->decisionLevel()) {
		status_ = ClauseCreator::status_open;
		return ClauseCreator::create(*solver_, rep, ccFlags_s);
	}
	return false;
}
bool TheoryPropagator::PP::propagate() {
	return !solver_->hasConflict() && solver_->propagateUntil(this);
}
/////////////////////////////////////////////////////////////////////////////////////////
// TheoryPropagator
/////////////////////////////////////////////////////////////////////////////////////////
TheoryPropagator::TheoryPropagator(Potassco::AbstractPropagator& cb) : prop_(&cb), watches_(new Watches()) {}
TheoryPropagator::~TheoryPropagator() { delete watches_; }
Potassco::Lit_t TheoryPropagator::addWatch(Literal lit) { 
	return watches_->add(lit);
}
bool TheoryPropagator::attach(Clasp::Solver& s) { 
	return s.addPost(new PP(*this)); 
}
Potassco::AbstractPropagator* TheoryPropagator::propagator() const {
	return prop_;
}
}
