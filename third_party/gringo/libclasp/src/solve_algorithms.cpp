// 
// Copyright (c) 2006-2015, Benjamin Kaufmann
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
#include <clasp/solve_algorithms.h>
#include <clasp/solver.h>
#include <clasp/enumerator.h>
#include <clasp/minimize_constraint.h>
#include <clasp/util/timer.h>
#include <clasp/util/atomic.h>
namespace Clasp { 
/////////////////////////////////////////////////////////////////////////////////////////
// Basic solve
/////////////////////////////////////////////////////////////////////////////////////////
struct BasicSolve::State {
	typedef BasicSolveEvent EventType;
	State(Solver& s, const SolveParams& p);
	ValueRep solve(Solver& s, const SolveParams& p, SolveLimits* lim);
	uint64           dbGrowNext;
	double           dbMax;
	double           dbHigh;
	ScheduleStrategy dbRed;
	uint32           nRestart;
	uint32           nGrow;
	uint32           dbRedInit;
	uint32           dbPinned;
	uint32           rsShuffle;
};

BasicSolve::BasicSolve(Solver& s, const SolveLimits& lim) : solver_(&s), params_(&s.searchConfig()), limits_(lim), state_(0) {}
BasicSolve::BasicSolve(Solver& s, const SolveParams& p, const SolveLimits& lim) 
	: solver_(&s)
	, params_(&p)
	, limits_(lim)
	, state_(0) {
}

BasicSolve::~BasicSolve(){ delete state_; }
void BasicSolve::reset(bool reinit) { 
	if (!state_ || reinit) {
		delete state_;
		state_ = 0;
	}
	else {
		state_->~State();
		new (state_) State(*solver_, *params_); 
	}
}
void BasicSolve::reset(Solver& s, const SolveParams& p, const SolveLimits& lim) { 
	solver_ = &s; 
	params_ = &p; 
	limits_ = lim;
	reset(true); 
}

ValueRep BasicSolve::solve() {
	if (limits_.reached())                       { return value_free;  }
	if (!state_ && !params_->randomize(*solver_)){ return value_false; }
	if (!state_)                                 { state_ = new State(*solver_, *params_); }
	return state_->solve(*solver_, *params_, hasLimit() ? &limits_ : 0);
}

bool BasicSolve::satisfiable(const LitVec& path, bool init) {
	if (!solver_->clearAssumptions() || !solver_->pushRoot(path)){ return false; }
	if (init && !params_->randomize(*solver_))                   { return false; }
	State temp(*solver_, *params_);
	return temp.solve(*solver_, *params_, 0) == value_true;
}

bool BasicSolve::assume(const LitVec& path) {
	return solver_->pushRoot(path);
}

BasicSolve::State::State(Solver& s, const SolveParams& p) {
	Range32 dbLim= p.reduce.sizeInit(*s.sharedContext());
	dbGrowNext   = p.reduce.growSched.current();
	dbMax        = dbLim.lo;
	dbHigh       = dbLim.hi;
	dbRed        = p.reduce.cflSched;
	nRestart     = 0;
	nGrow        = 0;
	dbRedInit    = p.reduce.cflInit(*s.sharedContext());
	dbPinned     = 0;
	rsShuffle    = p.restart.shuffle;
	if (dbLim.lo < s.numLearntConstraints()) { 
		dbMax      = std::min(dbHigh, double(s.numLearntConstraints() + p.reduce.initRange.lo));
	}
	if (dbRedInit && dbRed.type != ScheduleStrategy::Luby) {
		if (dbRedInit < dbRed.base) {
			dbRedInit  = std::min(dbRed.base, std::max(dbRedInit,(uint32)5000));
			dbRed.grow = dbRedInit != dbRed.base ? std::min(dbRed.grow, dbRedInit/2.0f) : dbRed.grow;
			dbRed.base = dbRedInit;
		}
		dbRedInit = 0;
	}
	if (p.restart.dynamic()) {
		s.stats.enableQueue(p.restart.sched.base);
		s.stats.queue->resetGlobal();
		s.stats.queue->dynamicRestarts((float)p.restart.sched.grow, true);
	}
	s.stats.lastRestart = s.stats.analyzed;
}

ValueRep BasicSolve::State::solve(Solver& s, const SolveParams& p, SolveLimits* lim) {
	assert(!lim || !lim->reached());
	if (s.hasConflict() && s.decisionLevel() == s.rootLevel()) {
		return value_false;
	}
	struct ConflictLimits {
		uint64 restart; // current restart limit
		uint64 reduce;  // current reduce limit
		uint64 grow;    // current limit for next growth operation
		uint64 global;  // current global limit
		uint64 min()      const { return std::min(std::min(restart, grow), std::min(reduce, global)); }
		void  update(uint64 x)  { restart -= x; reduce -= x; grow -= x; global -= x; }
	};
	WeightLitVec inDegree;
	SearchLimits sLimit;
	ScheduleStrategy rs     = p.restart.sched;
	ScheduleStrategy dbGrow = p.reduce.growSched;
	Solver::DBInfo  db      = {0,0,dbPinned};
	ValueRep result         = value_free;
	ConflictLimits cLimit   = {UINT64_MAX, dbRed.current() + dbRedInit, dbGrowNext, lim ? lim->conflicts : UINT64_MAX};
	uint64  limRestarts     = lim ? lim->restarts : UINT64_MAX;
	uint64& rsLimit         = p.restart.local() ? sLimit.local : cLimit.restart;
	if (!dbGrow.disabled())  { dbGrow.advanceTo(nGrow); }
	if (nRestart == UINT32_MAX && p.restart.update() == RestartParams::seq_disable) {
		cLimit.restart  = UINT64_MAX;
		sLimit          = SearchLimits();
	}
	else if (p.restart.dynamic()) {
		if (!nRestart) { s.stats.queue->resetQueue(); s.stats.queue->dynamicRestarts((float)p.restart.sched.grow, true); }
		sLimit.dynamic  = s.stats.queue; 
		rsLimit         = sLimit.dynamic->upForce - std::min(sLimit.dynamic->samples, sLimit.dynamic->upForce - 1);
	}
	else {
		rs.advanceTo(!rs.disabled() ? nRestart : 0);
		rsLimit         = rs.current();
	}
	sLimit.setMemLimit(p.reduce.memMax);
	for (EventType progress(s, EventType::event_restart, 0, 0); cLimit.global; ) {
		uint64 minLimit = cLimit.min(); assert(minLimit);
		sLimit.learnts  = (uint32)std::min(dbMax + (db.pinned*p.reduce.strategy.noGlue), dbHigh);
		sLimit.conflicts= minLimit;
		progress.cLimit = std::min(minLimit, sLimit.local);
		progress.lLimit = sLimit.learnts;
		if (progress.op) { s.sharedContext()->report(progress); progress.op = (uint32)EventType::event_none; }
		result          = s.search(sLimit, p.randProb);
		minLimit       -= sLimit.conflicts; // number of conflicts in this iteration
		if (result != value_free) {
			progress.op = static_cast<uint32>(EventType::event_exit);
			if (result == value_true && p.restart.update() != RestartParams::seq_continue) { 
				if      (p.restart.update() == RestartParams::seq_repeat) { nRestart = 0; }
				else if (p.restart.update() == RestartParams::seq_disable){ nRestart = UINT32_MAX; }
			}
			if (!dbGrow.disabled()) { dbGrowNext = std::max(cLimit.grow - minLimit, uint64(1)); }
			s.sharedContext()->report(progress);
			break;
		}
		cLimit.update(minLimit);
		minLimit = 0;
		if (rsLimit == 0 || sLimit.hasDynamicRestart()) {
			// restart reached - do restart
			++nRestart;
			if (p.restart.counterRestart && (nRestart % p.restart.counterRestart) == 0 ) {
				inDegree.clear();
				s.heuristic()->bump(s, inDegree, p.restart.counterBump / (double)s.inDegree(inDegree));
			}
			if (sLimit.dynamic) {
				minLimit = sLimit.dynamic->samples;
				rsLimit  = sLimit.dynamic->restart(rs.len ? rs.len : UINT32_MAX, (float)rs.grow);
			}
			s.restart();
			if (rsLimit == 0)              { rsLimit   = rs.next(); }
			if (!minLimit)                 { minLimit  = rs.current(); }
			if (p.reduce.strategy.fRestart){ db        = s.reduceLearnts(p.reduce.fRestart(), p.reduce.strategy); }
			if (nRestart == rsShuffle)     { rsShuffle+= p.restart.shuffleNext; s.shuffleOnNextSimplify();}
			if (--limRestarts == 0)        { break; }
			s.stats.lastRestart = s.stats.analyzed;
			progress.op         = (uint32)EventType::event_restart;
		}
		if (cLimit.reduce == 0 || s.learntLimit(sLimit)) {
			// reduction reached - remove learnt constraints
			db              = s.reduceLearnts(p.reduce.fReduce(), p.reduce.strategy);
			cLimit.reduce   = dbRedInit + (cLimit.reduce == 0 ? dbRed.next() : dbRed.current());
			progress.op     = std::max(progress.op, (uint32)EventType::event_deletion);
			if (s.learntLimit(sLimit) || db.pinned >= dbMax) { 
				ReduceStrategy t; t.algo = 2; t.score = 2; t.glue = 0;
				db.pinned /= 2;
				db.size    = s.reduceLearnts(0.5f, t).size;
				if (db.size >= sLimit.learnts) { dbMax = std::min(dbMax + std::max(100.0, s.numLearntConstraints()/10.0), dbHigh); }
			}
		}
		if (cLimit.grow == 0 || (dbGrow.defaulted() && progress.op == (uint32)EventType::event_restart)) {
			// grow sched reached - increase max db size
			if (cLimit.grow == 0)                             { cLimit.grow = dbGrow.next(); minLimit = cLimit.grow; ++nGrow; }
			if ((s.numLearntConstraints() + minLimit) > dbMax){ dbMax  *= p.reduce.fGrow; progress.op = std::max(progress.op, (uint32)EventType::event_grow); }
			if (dbMax > dbHigh)                               { dbMax   = dbHigh; cLimit.grow = UINT64_MAX; dbGrow = ScheduleStrategy::none(); }
		}
	}
	dbPinned            = db.pinned;
	s.stats.lastRestart = s.stats.analyzed - s.stats.lastRestart;
	if (lim) {
		if (lim->conflicts != UINT64_MAX) { lim->conflicts = cLimit.global; }
		if (lim->restarts  != UINT64_MAX) { lim->restarts  = limRestarts;   }
	}
	return result;
}
/////////////////////////////////////////////////////////////////////////////////////////
// SolveAlgorithm
/////////////////////////////////////////////////////////////////////////////////////////
SolveAlgorithm::SolveAlgorithm(const SolveLimits& lim) : limits_(lim), ctx_(0), enum_(0), onModel_(0), enumLimit_(UINT64_MAX), time_(0.0), last_(0)  {
}
SolveAlgorithm::~SolveAlgorithm() {}
void SolveAlgorithm::setEnumerator(Enumerator& e) { 
	enum_.reset(&e); 
	enum_.release();
}
const Model& SolveAlgorithm::model() const {
	return enum_->lastModel();
}
bool SolveAlgorithm::interrupt() {
	return doInterrupt();
}
bool SolveAlgorithm::attach(SharedContext& ctx, ModelHandler* onModel) {
	CLASP_FAIL_IF(ctx_, "SolveAlgorithm is already running!");
	if (!ctx.frozen()) { ctx.endInit(); }	
	ctx.report(message(Event::subsystem_solve, Event::verbosity_low, "Solving"));
	if (ctx.master()->hasConflict() || !limits_.conflicts || interrupted()) {
		last_  = !ctx.ok() ? value_false : value_free;
		return false;
	}
	ctx_     = &ctx;
	time_    = ThreadTime::getTime();
	onModel_ = onModel;
	last_    = value_free;
	if (!enum_.get()) { enum_ = EnumOptions::nullEnumerator(); }
	return true;
}
void SolveAlgorithm::detach() {
	if (ctx_) {
		ctx_->master()->stats.addCpuTime(ThreadTime::getTime() - time_);
		onModel_ = 0;
		ctx_     = 0;
		path_    = 0;
	}
}

bool SolveAlgorithm::solve(SharedContext& ctx, const LitVec& assume, ModelHandler* onModel) {
	struct Scoped {
		Scoped(SolveAlgorithm* s) : self(s) {}
		~Scoped() { self->detach(); }
		bool solve(const LitVec& assume) { 
			if (self->maxModels() != UINT64_MAX) {
				if (self->enumerator().optimize() && !self->enumerator().tentative()) { 
					self->ctx().report(warning(Event::subsystem_solve, "#models not 0: optimality of last model not guaranteed."));
				}
				if (self->enumerator().lastModel().consequences()) { 
					self->ctx().report(warning(Event::subsystem_solve, "#models not 0: last model may not cover consequences.")); 
				}
			}
			self->path_.reset(&assume);
			self->path_.release();
			return self->doSolve(self->ctx(), assume); 
		}
		SolveAlgorithm* self;
	};
	return attach(ctx, onModel) ? Scoped(this).solve(assume) : ctx.ok();
}

void SolveAlgorithm::start(SharedContext& ctx, const LitVec& assume, ModelHandler* onModel) {
	if (attach(ctx, onModel)) {
		doStart(ctx, *(path_ = new LitVec(assume)));
	}
}
bool SolveAlgorithm::next() {
	if (!ctx_) { return false; }
	if (last_ != value_true || !enum_->commitSymmetric(*ctx_->solver(model().sId))) {
		last_ = doNext(last_);
	}
	if (last_ == value_true) {
		Solver& s = *ctx_->solver(model().sId);
		if (onModel_) { onModel_->onModel(s, model()); }
		ctx_->report(s, model());
		return true;
	}
	stop();
 	return false;
}
bool SolveAlgorithm::more() {
	return last_ != value_false;
}
void SolveAlgorithm::stop() {
	if (ctx_) {
		doStop();
		detach();
	}
}
bool SolveAlgorithm::reportModel(Solver& s) const {
	for (const Model& m = enum_->lastModel();;) {
		bool r1 = !onModel_ || onModel_->onModel(s, m);
		bool r2 = s.sharedContext()->report(s, m);
		bool res= r1 && r2 && (enumLimit_ > m.num || enum_->tentative());
		if (!res || (res = !interrupted()) == false || !enum_->commitSymmetric(s)) { return res; }
	}
}
bool SolveAlgorithm::moreModels(const Solver& s) const {
	return s.decisionLevel() != 0 || !s.symmetric().empty() || (!s.sharedContext()->preserveModels() && s.sharedContext()->numEliminatedVars());
}
void SolveAlgorithm::doStart(SharedContext&, const LitVec&) {
	throw std::logic_error("Iterative model generation not supported by this algorithm!");
}
int SolveAlgorithm::doNext(int) {
	throw std::logic_error("Iterative model generation not supported by this algorithm!");
}
void SolveAlgorithm::doStop() {}
/////////////////////////////////////////////////////////////////////////////////////////
// SequentialSolve
/////////////////////////////////////////////////////////////////////////////////////////
namespace {
struct InterruptHandler : public MessageHandler {
	InterruptHandler(Solver* s, volatile int* t) : solver(s), term(t) {
		if (s && t) { s->addPost(this); }
	}
	~InterruptHandler()  { if (solver) { solver->removePost(this); solver = 0; } }
	bool handleMessages(){ return !*term || (solver->setStopConflict(), false); }
	bool propagateFixpoint(Solver&, PostPropagator*){ return InterruptHandler::handleMessages(); }
	Solver*       solver;
	volatile int* term;
};
}
SequentialSolve::SequentialSolve(const SolveLimits& limit)
	: SolveAlgorithm(limit)
	, solve_(0)
	, term_(-1) {
}
void SequentialSolve::resetSolve()       { if (term_ > 0) { term_ = 0; } }
bool SequentialSolve::doInterrupt()      { return term_ >= 0 && ++term_ != 0; }
void SequentialSolve::enableInterrupts() { if (term_ < 0) { term_ = 0; } }
bool SequentialSolve::interrupted() const{ return term_ > 0; }
void SequentialSolve::doStart(SharedContext& ctx, const LitVec& gp) {
	solve_.reset(new BasicSolve(*ctx.master(), ctx.configuration()->search(0), limits()));
	if (!enumerator().start(solve_->solver(), gp)) { SequentialSolve::doStop(); }
}
int SequentialSolve::doNext(int last) {
	if (term_ > 0 || !solve_.get()) { return solve_.get() ? value_free : value_false; }
	Solver& s = solve_->solver();
	for (InterruptHandler term(term_ == 0 ? &s : (Solver*)0, &term_);;) {
		if (last != value_free) { enumerator().update(s); }
		last = solve_->solve();
 		if (last != value_true) {
 			if      (last == value_free || term_ > 0) { return value_free; }
			else if (enumerator().commitUnsat(s))     { solve_->reset(); }
			else if (enumerator().commitComplete())   { break; }
 			else {
				enumerator().end(s);
				if (!enumerator().start(s, path())) { break; }
				last = value_free;
 			}
 		}
		else if (enumerator().commitModel(s)) { break; }
 	}
 	return last;
}
void SequentialSolve::doStop() {
	if (solve_.get()) {
		enumerator().end(solve_->solver());
		ctx().detach(solve_->solver());
		solve_ = 0;
	}
}
bool SequentialSolve::doSolve(SharedContext& ctx, const LitVec& gp) {
	BasicSolve solve(*ctx.master(), ctx.configuration()->search(0), limits());
	// Add assumptions - if this fails, the problem is unsat 
	// under the current assumptions but not necessarily unsat.
	Solver& s = solve.solver();
	bool more = !interrupted() && ctx.attach(s) && enumerator().start(s, gp);
	for (InterruptHandler term(term_ == 0 ? &s : (Solver*)0, &term_); more; solve.reset()) {
		ValueRep res;
		while ((res = solve.solve()) == value_true && (!enumerator().commitModel(s) || reportModel(s))) {
			enumerator().update(s);
		}
		if      (res != value_false)           { more = (res == value_free || moreModels(s)); break; }
		else if (interrupted())                { more = true; break; }
		else if (enumerator().commitUnsat(s))  { enumerator().update(s); }
		else if (enumerator().commitComplete()){ more = false; break; }
		else                                   { enumerator().end(s); more = enumerator().start(s, gp); }
	}
	enumerator().end(s);
	ctx.detach(s);
	return more;
}
}
