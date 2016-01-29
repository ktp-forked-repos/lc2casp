    // {{{ GPL License

    // This file is part of gringo - a grounder for logic programs.
    // Copyright (C) 2013  Roland Kaminski

    // This program is free software: you can redistribute it and/or modify
    // it under the terms of the GNU General Public License as published by
    // the Free Software Foundation, either version 3 of the License, or
    // (at your option) any later version.

    // This program is distributed in the hope that it will be useful,
    // but WITHOUT ANY WARRANTY; without even the implied warranty of
    // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    // GNU General Public License for more details.

    // You should have received a copy of the GNU General Public License
    // along with this program.  If not, see <http://www.gnu.org/licenses/>.

    // }}}

#include "gringo/input/programbuilder.hh"
#include "gringo/input/literals.hh"
#include "gringo/input/aggregates.hh"
#include "gringo/input/program.hh"
#include "gringo/input/theory.hh"
#include "gringo/output/output.hh"
#include "gringo/scripts.hh"

namespace Gringo { namespace Input {

// {{{1 definition of NongroundProgramBuilder

NongroundProgramBuilder::NongroundProgramBuilder(Scripts &scripts, Program &prg, Output::OutputBase &out, Defines &defs, bool rewriteMinimize)
: scripts_(scripts)
, prg_(prg)
, out(out)
, defs_(defs)
, rewriteMinimize_(rewriteMinimize) { }

// {{{2 terms

TermUid NongroundProgramBuilder::term(Location const &loc, Value val) {
    return terms_.insert(make_locatable<ValTerm>(loc, val));
}

TermUid NongroundProgramBuilder::term(Location const &loc, FWString name) {
    if (*name == "_") { return terms_.insert(make_locatable<VarTerm>(loc, name, nullptr)); }
    else {
        auto &ret(vals_[name]);
        if (!ret) { ret = std::make_shared<Value>(); }
        return terms_.insert(make_locatable<VarTerm>(loc, name, ret));
    }
}

TermUid NongroundProgramBuilder::term(Location const &loc, UnOp op, TermUid a) {
    return term(loc, op, termvec(termvec(), a));
}

TermUid NongroundProgramBuilder::term(Location const &loc, UnOp op, TermVecUid a) {
    UTermVec vec(termvecs_.erase(a));
    if (vec.size() == 1) { return terms_.insert(make_locatable<UnOpTerm>(loc, op, std::move(vec.front()))); }
    else {
        UTermVec pool;
        for (auto &terms : vec) { pool.emplace_back(make_locatable<UnOpTerm>(loc, op, std::move(terms))); }
        return terms_.insert(make_locatable<PoolTerm>(loc, std::move(pool)));
    }
}

TermUid NongroundProgramBuilder::term(Location const &loc, BinOp op, TermUid a, TermUid b) {
    return terms_.insert(make_locatable<BinOpTerm>(loc, op, terms_.erase(a), terms_.erase(b)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, TermUid a, TermUid b) {
    return terms_.insert(make_locatable<DotsTerm>(loc, terms_.erase(a), terms_.erase(b)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, FWString name, TermVecVecUid a, bool lua) {
    assert(*name != "");
    auto create = [&lua, &name, &loc](UTermVec &&vec) -> UTerm {
        // lua terms
        if (lua) { return make_locatable<LuaTerm>(loc, name, std::move(vec)); }
        // constant symbols
        else if (vec.empty()) { return make_locatable<ValTerm>(loc, Value::createId(name)); }
        // function terms
        else { return make_locatable<FunctionTerm>(loc, name, std::move(vec)); }
    };
    TermVecVecs::ValueType vec(termvecvecs_.erase(a));
    // no pooling
    if (vec.size() == 1) { return terms_.insert(create(std::move(vec.front()))); }
    // pooling
    else {
        UTermVec pool;
        for (auto &terms : vec) { pool.emplace_back(create(std::move(terms))); }
        return terms_.insert(make_locatable<PoolTerm>(loc, std::move(pool)));
    }
}

TermUid NongroundProgramBuilder::term(Location const &loc, TermVecUid args, bool forceTuple) {
    UTermVec a(termvecs_.erase(args));
    UTerm ret(!forceTuple && a.size() == 1 ? std::move(a.front()) : make_locatable<FunctionTerm>(loc, "", std::move(a)));
    return terms_.insert(std::move(ret));
}

TermUid NongroundProgramBuilder::pool(Location const &loc, TermVecUid args) {
    return terms_.insert(make_locatable<PoolTerm>(loc, termvecs_.erase(args)));
}

// {{{2 id vectors

IdVecUid NongroundProgramBuilder::idvec() {
    return idvecs_.emplace();
}
IdVecUid NongroundProgramBuilder::idvec(IdVecUid uid, Location const &loc, FWString id) {
    idvecs_[uid].emplace_back(loc, id);
    return uid;
}

// {{{2 csp

CSPMulTermUid NongroundProgramBuilder::cspmulterm(Location const &, TermUid coe, TermUid var) {
    return cspmulterms_.emplace(terms_.erase(var), terms_.erase(coe));
}
CSPMulTermUid NongroundProgramBuilder::cspmulterm(Location const &, TermUid coe) {
    return cspmulterms_.emplace(nullptr, terms_.erase(coe));
}
CSPAddTermUid NongroundProgramBuilder::cspaddterm(Location const &loc, CSPAddTermUid a, CSPMulTermUid b, bool add) {
    if (add) {
        cspaddterms_[a].append(cspmulterms_.erase(b));
    }
    else {
        CSPMulTerm mul(cspmulterms_.erase(b));
        mul.coe = make_locatable<UnOpTerm>(loc, UnOp::NEG, std::move(mul.coe));
        cspaddterms_[a].append(std::move(mul));
    }

    return a;
}
CSPAddTermUid NongroundProgramBuilder::cspaddterm(Location const &, CSPMulTermUid a) {
    return cspaddterms_.emplace(cspmulterms_.erase(a));
}
LitUid NongroundProgramBuilder::csplit(CSPLitUid a) {
    return lits_.emplace(csplits_.erase(a));
}
CSPLitUid NongroundProgramBuilder::csplit(Location const &loc, CSPLitUid a, Relation rel, CSPAddTermUid b) {
    csplits_[a]->append(rel, cspaddterms_.erase(b));
    csplits_[a]->loc(csplits_[a]->loc() + loc);
    return a;
}
CSPLitUid NongroundProgramBuilder::csplit(Location const &loc, CSPAddTermUid a, Relation rel, CSPAddTermUid b) {
    return csplits_.insert(make_locatable<CSPLiteral>(loc, rel, cspaddterms_.erase(a), cspaddterms_.erase(b)));
}

// {{{2 termvecs

TermVecUid NongroundProgramBuilder::termvec() {
    return termvecs_.emplace();
}

TermVecUid NongroundProgramBuilder::termvec(TermVecUid uid, TermUid term) {
    termvecs_[uid].emplace_back(terms_.erase(term));
    return uid;
}

// {{{2 termvecvecs

TermVecVecUid NongroundProgramBuilder::termvecvec() {
    return termvecvecs_.emplace();
}

TermVecVecUid NongroundProgramBuilder::termvecvec(TermVecVecUid uid, TermVecUid termvecUid) {
    termvecvecs_[uid].emplace_back(termvecs_.erase(termvecUid));
    return uid;
}

// {{{2 literals

LitUid NongroundProgramBuilder::boollit(Location const &loc, bool type) {
    return rellit(loc, type ? Relation::EQ : Relation::NEQ, term(loc, Value::createNum(0)), term(loc, Value::createNum(0)));
}

LitUid NongroundProgramBuilder::predlit(Location const &loc, NAF naf, bool neg, FWString name, TermVecVecUid tvvUid) {
    return lits_.insert(make_locatable<PredicateLiteral>(loc, naf, terms_.erase(predRep(loc, neg, name, tvvUid))));
}

LitUid NongroundProgramBuilder::rellit(Location const &loc, Relation rel, TermUid termUidLeft, TermUid termUidRight) {
    return lits_.insert(make_locatable<RelationLiteral>(loc, rel, terms_.erase(termUidLeft), terms_.erase(termUidRight)));
}

// {{{2 literal vectors

LitVecUid NongroundProgramBuilder::litvec() {
    return litvecs_.emplace();
}

LitVecUid NongroundProgramBuilder::litvec(LitVecUid uid, LitUid literalUid) {
    litvecs_[uid].emplace_back(lits_.erase(literalUid));
    return uid;
}

// {{{2 body aggregate element vectors

BdAggrElemVecUid NongroundProgramBuilder::bodyaggrelemvec() {
    return bodyaggrelemvecs_.emplace();
}

BdAggrElemVecUid NongroundProgramBuilder::bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) {
    bodyaggrelemvecs_[uid].emplace_back(termvecs_.erase(termvec), litvecs_.erase(litvec));
    return uid;
}

CondLitVecUid NongroundProgramBuilder::condlitvec() {
    return condlitvecs_.emplace();
}

CondLitVecUid NongroundProgramBuilder::condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) {
    condlitvecs_[uid].emplace_back(lits_.erase(lit), litvecs_.erase(litvec));
    return uid;
}

// {{{2 head aggregate element vectors

HdAggrElemVecUid NongroundProgramBuilder::headaggrelemvec() {
    return headaggrelemvecs_.emplace();
}

HdAggrElemVecUid NongroundProgramBuilder::headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) {
    headaggrelemvecs_[uid].emplace_back(termvecs_.erase(termvec), lits_.erase(lit), litvecs_.erase(litvec));
    return uid;
}

// {{{2 bounds

BoundVecUid NongroundProgramBuilder::boundvec() {
    return bounds_.emplace();
}

BoundVecUid NongroundProgramBuilder::boundvec(BoundVecUid uid, Relation rel, TermUid term) {
    bounds_[uid].emplace_back(rel, terms_.erase(term));
    return uid;
}

// {{{2 rule bodies

BdLitVecUid NongroundProgramBuilder::body() {
    return bodies_.emplace();
}

BdLitVecUid NongroundProgramBuilder::bodylit(BdLitVecUid body, LitUid bodylit) {
    bodies_[body].push_back(gringo_make_unique<SimpleBodyLiteral>(lits_.erase(bodylit)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) {
    bodies_[body].push_back(make_locatable<TupleBodyAggregate>(loc, naf, fun, bounds_.erase(bounds), bodyaggrelemvecs_.erase(bodyaggrelemvec)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvecuid) {
    bodies_[body].push_back(make_locatable<LitBodyAggregate>(loc, naf, fun, bounds_.erase(bounds), condlitvecs_.erase(condlitvecuid)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) {
    bodies_[body].push_back(make_locatable<Conjunction>(loc, lits_.erase(head), litvecs_.erase(litvec)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::disjoint(BdLitVecUid body, Location const &loc, NAF naf, CSPElemVecUid elem) {
    bodies_[body].push_back(make_locatable<DisjointAggregate>(loc, naf, cspelems_.erase(elem)));
    return body;
}


// {{{2 rule heads

HdLitUid NongroundProgramBuilder::headlit(LitUid lit) {
    return heads_.insert(gringo_make_unique<SimpleHeadLiteral>(lits_.erase(lit)));
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) {
    return heads_.insert(make_locatable<TupleHeadAggregate>(loc, fun, bounds_.erase(bounds), headaggrelemvecs_.erase(headaggrelemvec)));
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvec) {
    return heads_.insert(make_locatable<LitHeadAggregate>(loc, fun, bounds_.erase(bounds), condlitvecs_.erase(condlitvec)));
}

HdLitUid NongroundProgramBuilder::disjunction(Location const &loc, CondLitVecUid condlitvec) {
    return heads_.insert(make_locatable<Disjunction>(loc, condlitvecs_.erase(condlitvec)));
}

// {{{2 csp constraint elements

CSPElemVecUid NongroundProgramBuilder::cspelemvec() {
    return cspelems_.emplace();
}

CSPElemVecUid NongroundProgramBuilder::cspelemvec(CSPElemVecUid uid, Location const &loc, TermVecUid termvec, CSPAddTermUid addterm, LitVecUid litvec) {
    cspelems_[uid].emplace_back(loc, termvecs_.erase(termvec), cspaddterms_.erase(addterm), litvecs_.erase(litvec));
    return uid;
}

// {{{2 statements

void NongroundProgramBuilder::rule(Location const &loc, HdLitUid head) {
    rule(loc, head, body());
}

void NongroundProgramBuilder::rule(Location const &loc, HdLitUid head, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, heads_.erase(head), bodies_.erase(body), StatementType::RULE));
}

void NongroundProgramBuilder::define(Location const &loc, FWString name, TermUid value, bool defaultDef) {
    defs_.add(loc, name, terms_.erase(value), defaultDef);
}

void NongroundProgramBuilder::optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) {
    if (rewriteMinimize_) {
        auto argsUid = termvec(termvec(termvec(), priority), weight);
        termvec(argsUid, term(loc, cond, true));
        auto predUid = predlit(loc, NAF::POS, false, "_criteria", termvecvec(termvecvec(), argsUid));
        rule(loc, headlit(predUid), body);
        out.outPredsForce.emplace_back(loc, Signature("_criteria", 3), false);
    }
    else {
        prg_.add(make_locatable<Statement>(loc, make_locatable<MinimizeHeadLiteral>(loc, terms_.erase(weight), terms_.erase(priority), termvecs_.erase(cond)), bodies_.erase(body), StatementType::WEAKCONSTRAINT));
    }
}

void NongroundProgramBuilder::showsig(Location const &loc, FWSignature sig, bool csp) {
    out.outPreds.emplace_back(loc, sig, csp);
}

void NongroundProgramBuilder::show(Location const &loc, TermUid t, BdLitVecUid body, bool csp) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<ShowHeadLiteral>(loc, terms_.erase(t), csp), bodies_.erase(body), StatementType::RULE));
}

void NongroundProgramBuilder::lua(Location const &loc, FWString code) {
    scripts_.luaExec(loc, *code);
}

void NongroundProgramBuilder::python(Location const &loc, FWString code) {
    scripts_.pyExec(loc, *code);
}

void NongroundProgramBuilder::block(Location const &loc, FWString name, IdVecUid args) {
    prg_.begin(loc, name, idvecs_.erase(args));
}

void NongroundProgramBuilder::external(Location const &loc, LitUid head, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, heads_.erase(headlit(head)), bodies_.erase(body), StatementType::EXTERNAL));
}

void NongroundProgramBuilder::edge(Location const &loc, TermVecVecUid edgesUid, BdLitVecUid body) {
    auto edges = termvecvecs_.erase(edgesUid);
    for (auto it = edges.begin(), end = edges.end(), last = end-1; it != end; ++it) {
        prg_.add(make_locatable<Statement>(
            loc,
            make_locatable<EdgeHeadAtom>(
                loc,
                std::move(it->front()),
                std::move(it->back())
            ),
            it == last ? bodies_.erase(body) : get_clone(bodies_[body]),
            StatementType::RULE
        ));
    }
}

TermUid NongroundProgramBuilder::predRep(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid) {
    if (neg) {
        for (auto &x : termvecvecs_[tvvUid]) { prg_.addClassicalNegation(Signature(name, x.size())); }
    }
    TermUid t = term(loc, name, tvvUid, false);
    if (neg) { t = term(loc, UnOp::NEG, t); }
    return t;

}

void NongroundProgramBuilder::heuristic(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<HeuristicHeadAtom>(loc, terms_.erase(predRep(loc, neg, name, tvvUid)), terms_.erase(a), terms_.erase(b), terms_.erase(mod)), bodies_.erase(body), StatementType::RULE));
}

void NongroundProgramBuilder::project(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<ProjectHeadAtom>(loc, terms_.erase(predRep(loc, neg, name, tvvUid))), bodies_.erase(body), StatementType::RULE));
}

void NongroundProgramBuilder::project(Location const &loc, FWSignature sig) {
    Signature s = *sig;
    auto tv = termvec();
    for (unsigned i = 0; i < s.length(); ++i) {
        std::ostringstream ss;
        ss << "X" << i;
        tv = termvec(tv, term(loc, ss.str()));
    }
    auto tvv = termvecvec(termvecvec(), tv);
    project(loc, s.sign(), s.name(), tvv, body());
}

// {{{2 theory

TheoryTermUid NongroundProgramBuilder::theorytermset(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Brace, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theoryoptermlist(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Bracket, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermtuple(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Paren, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermopterm(Location const &loc, TheoryOptermUid opterm) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
}

TheoryTermUid NongroundProgramBuilder::theorytermfun(Location const &loc, FWString name, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::FunctionTheoryTerm>(name, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermvalue(Location const &loc, Value val) {
    return theoryTerms_.emplace(gringo_make_unique<Output::TermTheoryTerm>(make_locatable<ValTerm>(loc, val)));
}

TheoryTermUid NongroundProgramBuilder::theorytermvar(Location const &loc, FWString var) {
    auto &ret(vals_[var]);
    if (!ret) { ret = std::make_shared<Value>(); }
    return theoryTerms_.emplace(gringo_make_unique<Output::TermTheoryTerm>(make_locatable<VarTerm>(loc, var, ret)));
}

TheoryOptermUid NongroundProgramBuilder::theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) {
    auto ret = theoryOpterms_.emplace();
    theoryOpterms_[ret].append(theoryOpVecs_.erase(ops), theoryTerms_.erase(term));
    return ret;
}

TheoryOptermUid NongroundProgramBuilder::theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) {
    theoryOpterms_[opterm].append(theoryOpVecs_.erase(ops), theoryTerms_.erase(term));
    return opterm;
}

TheoryOpVecUid NongroundProgramBuilder::theoryops() {
    return theoryOpVecs_.emplace();
}

TheoryOpVecUid NongroundProgramBuilder::theoryops(TheoryOpVecUid ops, FWString op) {
    theoryOpVecs_[ops].emplace_back(op);
    return ops;
}

TheoryOptermVecUid NongroundProgramBuilder::theoryopterms() {
    return theoryOptermVecs_.emplace();
}
TheoryOptermVecUid NongroundProgramBuilder::theoryopterms(TheoryOptermVecUid opterms, TheoryOptermUid opterm) {
    theoryOptermVecs_[opterms].emplace_back(gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
    return opterms;
}
TheoryOptermVecUid NongroundProgramBuilder::theoryopterms(TheoryOptermUid opterm, TheoryOptermVecUid opterms) {
    theoryOptermVecs_[opterms].insert(theoryOptermVecs_[opterms].begin(), gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
    return opterms;
}

TheoryElemVecUid NongroundProgramBuilder::theoryelems() {
    return theoryElems_.emplace();
}
TheoryElemVecUid NongroundProgramBuilder::theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) {
    theoryElems_[elems].emplace_back(theoryOptermVecs_.erase(opterms), litvecs_.erase(cond));
    return elems;
}

TheoryAtomUid NongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems) {
    return theoryAtoms_.emplace(terms_.erase(term), theoryElems_.erase(elems));
}
TheoryAtomUid NongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems, FWString op, TheoryOptermUid opterm) {
    return theoryAtoms_.emplace(terms_.erase(term), theoryElems_.erase(elems), op, gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) {
    bodies_[body].emplace_back(make_locatable<BodyTheoryLiteral>(loc, naf, theoryAtoms_.erase(atom)));
    return body;
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, TheoryAtomUid atom) {
    return heads_.emplace(make_locatable<HeadTheoryLiteral>(loc, theoryAtoms_.erase(atom)));
}

// {{{2 theory definitions

TheoryOpDefUid NongroundProgramBuilder::theoryopdef(Location const &loc, FWString op, unsigned priority, TheoryOperatorType type) {
    return theoryOpDefs_.emplace(loc, op, priority, type);
}

TheoryOpDefVecUid NongroundProgramBuilder::theoryopdefs() {
    return theoryOpDefVecs_.emplace();
}

TheoryOpDefVecUid NongroundProgramBuilder::theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) {
    theoryOpDefVecs_[defs].emplace_back(theoryOpDefs_.erase(def));
    return defs;
}

TheoryTermDefUid NongroundProgramBuilder::theorytermdef(Location const &loc, FWString name, TheoryOpDefVecUid defs) {
    TheoryTermDef def(loc, name);
    for (auto &opDef : theoryOpDefVecs_.erase(defs)) {
        def.addOpDef(std::move(opDef));
    }
    return theoryTermDefs_.emplace(std::move(def));
}

TheoryAtomDefUid NongroundProgramBuilder::theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type) {
    return theoryAtomDefs_.emplace(loc, name, arity, termDef, type);
}

TheoryAtomDefUid NongroundProgramBuilder::theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type, TheoryOpVecUid ops, FWString guardDef) {
    return theoryAtomDefs_.emplace(loc, name, arity, termDef, type, theoryOpVecs_.erase(ops), guardDef);
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs() {
    return theoryDefVecs_.emplace();
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) {
    theoryDefVecs_[defs].first.emplace_back(theoryTermDefs_.erase(def));
    return defs;
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) {
    theoryDefVecs_[defs].second.emplace_back(theoryAtomDefs_.erase(def));
    return defs;
}

void NongroundProgramBuilder::theorydef(Location const &loc, FWString name, TheoryDefVecUid defs) {
    TheoryDef def(loc, name);
    auto defsVec = theoryDefVecs_.erase(defs);
    for (auto &termDef : defsVec.first) {
        def.addTermDef(std::move(termDef));
    }
    for (auto &atomDef : defsVec.second) {
        def.addAtomDef(std::move(atomDef));
    }
    prg_.add(std::move(def));
}

// }}}2

NongroundProgramBuilder::~NongroundProgramBuilder() { }

// }}}1

} } // namespace Input Gringo

