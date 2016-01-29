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

#include "gringo/input/nongroundparser.hh"
#include "gringo/input/programbuilder.hh"
#include "input/nongroundgrammar/grammar.hh"
#include "gringo/value.hh"
#include "tests/tests.hh"

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>
#include <climits>
#include <sstream>
#include <unistd.h>

namespace Gringo { namespace Input { namespace Test {

// {{{ declaration of TestNongroundProgramBuilder

class TestNongroundProgramBuilder : public INongroundProgramBuilder {
public:

    // {{{ terms
    virtual TermUid term(Location const &loc, Value val) override;
    virtual TermUid term(Location const &loc, FWString name) override;
    virtual TermUid term(Location const &loc, UnOp op, TermUid a) override;
    virtual TermUid term(Location const &loc, UnOp op, TermVecUid a) override;
    virtual TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) override;
    virtual TermUid term(Location const &loc, TermUid a, TermUid b) override;
    virtual TermUid term(Location const &loc, FWString name, TermVecVecUid b, bool lua) override;
    virtual TermUid term(Location const &loc, TermVecUid args, bool forceTuple) override;
    virtual TermUid pool(Location const &loc, TermVecUid args) override;
    // }}}
    // {{{ csp
    virtual CSPMulTermUid cspmulterm(Location const &loc, TermUid coe, TermUid var) override;
    virtual CSPMulTermUid cspmulterm(Location const &loc, TermUid coe) override;
    virtual CSPAddTermUid cspaddterm(Location const &loc, CSPAddTermUid a, CSPMulTermUid b, bool add) override;
    virtual CSPAddTermUid cspaddterm(Location const &loc, CSPMulTermUid a) override;
    virtual LitUid csplit(CSPLitUid a) override;
    virtual CSPLitUid csplit(Location const &loc, CSPLitUid a, Relation rel, CSPAddTermUid b) override;
    virtual CSPLitUid csplit(Location const &loc, CSPAddTermUid a, Relation rel, CSPAddTermUid b) override;
    // }}}
    // {{{ id vectors
    virtual IdVecUid idvec() override;
    virtual IdVecUid idvec(IdVecUid uid, Location const &loc, FWString id) override;
    // }}}
    // {{{ term vectors
    virtual TermVecUid termvec() override;
    virtual TermVecUid termvec(TermVecUid uid, TermUid term) override;
    // }}}
    // {{{ term vector vectors
    virtual TermVecVecUid termvecvec() override;
    virtual TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) override;
    // }}}
    // {{{ literals
    virtual LitUid boollit(Location const &loc, bool type) override;
    virtual LitUid predlit(Location const &loc, NAF naf, bool neg, FWString name, TermVecVecUid argvecvecUid) override;
    virtual LitUid rellit(Location const &loc, Relation rel, TermUid termUidLeft, TermUid termUidRight) override;
    // }}}
    // {{{ literal vectors
    virtual LitVecUid litvec() override;
    virtual LitVecUid litvec(LitVecUid uid, LitUid literalUid) override;
    // }}}
    // {{{ count aggregate elements (body/head)
    virtual CondLitVecUid condlitvec() override;
    virtual CondLitVecUid condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) override;
    // }}}
    // {{{ body aggregate elements
    virtual BdAggrElemVecUid bodyaggrelemvec() override;
    virtual BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) override;
    // }}}
    // {{{ head aggregate elements
    virtual HdAggrElemVecUid headaggrelemvec() override;
    virtual HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) override;
    // }}}
    // {{{ bounds
    virtual BoundVecUid boundvec() override;
    virtual BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) override;
    // }}}
    // {{{ heads
    virtual HdLitUid headlit(LitUid lit) override;
    virtual HdLitUid headaggr(Location const &loc, TheoryAtomUid atom) override;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) override;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid headaggrelemvec) override;
    virtual HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) override;
    // }}}
    // {{{ bodies
    virtual BdLitVecUid body() override;
    virtual BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) override;
    virtual BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) override;
    virtual BdLitVecUid disjoint(BdLitVecUid body, Location const &loc, NAF naf, CSPElemVecUid elem) override;
    // }}}
    // {{{ csp constraint elements
    virtual CSPElemVecUid cspelemvec() override;
    virtual CSPElemVecUid cspelemvec(CSPElemVecUid uid, Location const &loc, TermVecUid termvec, CSPAddTermUid addterm, LitVecUid litvec) override;
    // }}}
    // {{{ statements
    virtual void rule(Location const &loc, HdLitUid head) override;
    virtual void rule(Location const &loc, HdLitUid head, BdLitVecUid body) override;
    virtual void define(Location const &loc, FWString name, TermUid value, bool defaultDef) override;
    virtual void optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) override;
    virtual void showsig(Location const &loc, FWSignature sig, bool csp) override;
    virtual void show(Location const &loc, TermUid t, BdLitVecUid body, bool csp) override;
    virtual void python(Location const &loc, FWString code) override;
    virtual void lua(Location const &loc, FWString code) override;
    virtual void block(Location const &loc, FWString name, IdVecUid args) override;
    virtual void external(Location const &loc, LitUid head, BdLitVecUid body) override;
    virtual void edge(Location const &loc, TermVecVecUid edges, BdLitVecUid body) override;
    virtual void heuristic(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) override;
    virtual void project(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body) override;
    virtual void project(Location const &loc, FWSignature sig) override;
    // }}}
    // {{{ theory atoms
    virtual TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) override;
    virtual TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermfun(Location const &loc, FWString name, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermvalue(Location const &loc, Value val) override;
    virtual TheoryTermUid theorytermvar(Location const &loc, FWString var) override;

    virtual TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) override;
    virtual TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) override;

    virtual TheoryOpVecUid theoryops() override;
    virtual TheoryOpVecUid theoryops(TheoryOpVecUid ops, FWString op) override;

    virtual TheoryOptermVecUid theoryopterms() override;
    virtual TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, TheoryOptermUid opterm) override;
    virtual TheoryOptermVecUid theoryopterms(TheoryOptermUid opterm, TheoryOptermVecUid opterms) override;

    virtual TheoryElemVecUid theoryelems() override;
    virtual TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) override;

    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) override;
    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, FWString op, TheoryOptermUid opterm) override;
    // }}}
    // {{{ theory definitions

    virtual TheoryOpDefUid theoryopdef(Location const &loc, FWString op, unsigned priority, TheoryOperatorType type) override;
    virtual TheoryOpDefVecUid theoryopdefs() override;
    virtual TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) override;

    virtual TheoryTermDefUid theorytermdef(Location const &loc, FWString name, TheoryOpDefVecUid defs) override;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type) override;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type, TheoryOpVecUid ops, FWString guardDef) override;

    virtual TheoryDefVecUid theorydefs() override;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) override;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) override;

    virtual void theorydef(Location const &loc, FWString name, TheoryDefVecUid defs) override;

    // }}}

    std::string toString();

    virtual ~TestNongroundProgramBuilder();

private:
    // {{{ typedefs
    using StringVec = std::vector<std::string>;
    using StringVecVec = std::vector<StringVec>;
    using StringPair = std::pair<std::string, std::string>;
    using StringPairVec = std::vector<StringPair>;
    using TermUidVec = std::vector<TermUid>;
    using TermVecUidVec = std::vector<TermVecUid>;
    using LitUidVec = std::vector<LitUid>;

    using IdVecs = Indexed<StringVec, IdVecUid>;
    using Terms = Indexed<std::string, TermUid>;
    using TermVecs = Indexed<StringVec, TermVecUid>;
    using TermVecVecs = Indexed<StringVecVec, TermVecVecUid>;
    using Lits = Indexed<std::string, LitUid>;
    using LitVecs = Indexed<StringVec, LitVecUid>;
    using BodyAggrElemVecs = Indexed<StringVec, BdAggrElemVecUid>;
    using CondLitVecs = Indexed<StringVec, CondLitVecUid>;
    using HeadAggrElemVecs = Indexed<StringVec, HdAggrElemVecUid>;
    using Bodies = Indexed<StringVec, BdLitVecUid>;
    using CSPElems = Indexed<StringVec, CSPElemVecUid>;
    using Heads = Indexed<std::string, HdLitUid>;
    using CSPAddTerms = Indexed<std::string, CSPAddTermUid>;
    using CSPMulTerms = Indexed<std::string, CSPMulTermUid>;
    using CSPLits = Indexed<std::string, CSPLitUid>;
    using Bounds = Indexed<StringPairVec, BoundVecUid>;
    using Statements = std::vector<std::string>;

    using TheoryOps = Indexed<StringVec, TheoryOpVecUid>;
    using TheoryTerms = Indexed<std::string, TheoryTermUid>;
    using TheoryOpterms = Indexed<std::string, TheoryOptermUid>;
    using TheoryOptermVecs = Indexed<StringVec, TheoryOptermVecUid>;
    using TheoryElemVecs = Indexed<StringVec, TheoryElemVecUid>;
    using TheoryAtoms = Indexed<std::string, TheoryAtomUid>;

    using TheoryOpDefs = Indexed<std::string, TheoryOpDefUid>;
    using TheoryOpDefVecs = Indexed<StringVec, TheoryOpDefVecUid>;
    using TheoryTermDefs = Indexed<std::string, TheoryTermDefUid>;
    using TheoryAtomDefs = Indexed<std::string, TheoryAtomDefUid>;
    using TheoryDefVecs = Indexed<StringVec, TheoryDefVecUid>;

    // }}}
    // {{{ auxiliary functions
    std::string str();
    void print(StringVec const &vec, char const *sep);
    void print(StringVecVec const &vec);
    void print(AggregateFunction fun, BoundVecUid boundvecuid, StringVec const &elems);
    void print(NAF naf);
    // }}}
    // {{{ member variables
    IdVecs idvecs_;
    Terms terms_;
    TermVecs termvecs_;
    TermVecVecs termvecvecs_;
    Lits lits_;
    LitVecs litvecs_;
    BodyAggrElemVecs bodyaggrelemvecs_;
    CondLitVecs condlitvecs_;
    HeadAggrElemVecs headaggrelemvecs_;
    Bodies bodies_;
    Heads heads_;
    CSPAddTerms cspaddterms_;
    CSPMulTerms cspmulterms_;
    CSPLits csplits_;
    CSPElems cspelems_;
    Bounds bounds_;
    Statements statements_;
    std::stringstream current_;

    TheoryOps theoryOps_;
    TheoryTerms theoryTerms_;
    TheoryOpterms theoryOpterms_;
    TheoryOptermVecs theoryOptermVecs_;
    TheoryElemVecs theoryElemVecs_;
    TheoryAtoms theoryAtoms_;

    TheoryOpDefs theoryOpDefs_;
    TheoryOpDefVecs theoryOpDefVecs_;
    TheoryTermDefs theoryTermDefs_;
    TheoryAtomDefs theoryAtomDefs_;
    TheoryDefVecs theoryDefVecs_;
    // }}}
};

// }}}
// {{{ declaration of TestNongroundGrammar
class TestNongroundGrammar : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(TestNongroundGrammar);
        CPPUNIT_TEST(test_term);
        CPPUNIT_TEST(test_atomargs);
        CPPUNIT_TEST(test_literal);
        CPPUNIT_TEST(test_bdaggr);
        CPPUNIT_TEST(test_hdaggr);
        CPPUNIT_TEST(test_conjunction);
        CPPUNIT_TEST(test_disjunction);
        CPPUNIT_TEST(test_rule);
        CPPUNIT_TEST(test_define);
        CPPUNIT_TEST(test_optimize);
        CPPUNIT_TEST(test_show);
        CPPUNIT_TEST(test_include);
        CPPUNIT_TEST(test_csp);
        CPPUNIT_TEST(test_edge);
        CPPUNIT_TEST(test_project);
        CPPUNIT_TEST(test_heuristic);
        CPPUNIT_TEST(test_theory);
        CPPUNIT_TEST(test_theoryDefinition);
    CPPUNIT_TEST_SUITE_END();

public:
    virtual void setUp();
    virtual void tearDown();

    void test_term();
    void test_atomargs();
    void test_literal();
    void test_bdaggr();
    void test_hdaggr();
    void test_conjunction();
    void test_disjunction();
    void test_rule();
    void test_define();
    void test_optimize();
    void test_show();
    void test_include();
    void test_csp();
    void test_edge();
    void test_project();
    void test_heuristic();
    void test_theory();
    void test_theoryDefinition();

    virtual ~TestNongroundGrammar();

    std::string parse(std::string const &str);
};

// }}}

// {{{ definition of TestNongroundProgramBuilder

// {{{ id vectors

IdVecUid TestNongroundProgramBuilder::idvec() {
    return idvecs_.emplace();
}

IdVecUid TestNongroundProgramBuilder::idvec(IdVecUid uid, Location const &, FWString id) {
    idvecs_[uid].emplace_back(*id);
    return uid;
}

// }}}
// {{{ terms

TermUid TestNongroundProgramBuilder::term(Location const &, Value val) {
    current_ << val;
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, FWString name) {
    current_ << *name;
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, UnOp op, TermUid a) {
    if(op == UnOp::ABS) { current_ << "|"; }
    else { current_ << "(" << op; }
    current_ << terms_.erase(a) << (op == UnOp::ABS ? "|" : ")");
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, UnOp op, TermVecUid a) {
    if(op == UnOp::ABS) { current_ << "|"; }
    else { current_ << op << "("; }
    print(termvecs_.erase(a), ";");
    current_ << (op == UnOp::ABS ? "|" : ")");
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, BinOp op, TermUid a, TermUid b) {
    current_ << "(" << terms_.erase(a) << op << terms_.erase(b) << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, TermUid a, TermUid b) {
    current_ << "(" << terms_.erase(a) << ".." << terms_.erase(b) << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, FWString name, TermVecVecUid a, bool lua) {
    assert(*name != "");
    assert(!termvecvecs_[a].empty());
    bool nempty = lua || termvecvecs_[a].size() > 1 || !termvecvecs_[a].front().empty();
    if (lua) { current_ << "@"; }
    current_ << *name;
    if (nempty) { current_ << "("; }
    print(termvecvecs_.erase(a));
    if (nempty) { current_ << ")"; }
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, TermVecUid args, bool forceTuple) {
    current_ << "(";
    print(termvecs_.erase(args), ",");
    if (forceTuple) { current_ << ","; }
    current_ << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::pool(Location const &, TermVecUid args) {
    current_ << "(";
    print(termvecs_.erase(args), ";");
    current_ << ")";
    return terms_.emplace(str());
}

// }}}
// {{{ csp
CSPMulTermUid TestNongroundProgramBuilder::cspmulterm(Location const &, TermUid coe, TermUid var) {
    current_ << terms_.erase(coe) << "$*$" << terms_.erase(var);
    return cspmulterms_.emplace(str());
}
CSPMulTermUid TestNongroundProgramBuilder::cspmulterm(Location const &, TermUid coe) {
    current_ << terms_.erase(coe);
    return cspmulterms_.emplace(str());
}
CSPAddTermUid TestNongroundProgramBuilder::cspaddterm(Location const &, CSPAddTermUid a, CSPMulTermUid b, bool add) {
    cspaddterms_[a] += (add ? "$+" : "$-") + cspmulterms_.erase(b);
    return a;
}
CSPAddTermUid TestNongroundProgramBuilder::cspaddterm(Location const &, CSPMulTermUid a) {
    current_ << cspmulterms_.erase(a);
    return cspaddterms_.emplace(str());
}
LitUid TestNongroundProgramBuilder::csplit(CSPLitUid a) {
    return lits_.emplace(csplits_.erase(a));
}
CSPLitUid TestNongroundProgramBuilder::csplit(Location const &, CSPLitUid a, Relation rel, CSPAddTermUid b) {
    csplits_[a] += "$" + IO::to_string(rel) + cspaddterms_.erase(b);
    return a;
}
CSPLitUid TestNongroundProgramBuilder::csplit(Location const &, CSPAddTermUid a, Relation rel, CSPAddTermUid b) {
    current_ << cspaddterms_.erase(a) << "$" << rel << cspaddterms_.erase(b);
    return csplits_.emplace(str());
}
// }}}
// {{{ term vectors

TermVecUid TestNongroundProgramBuilder::termvec() {
    return termvecs_.emplace();
}

TermVecUid TestNongroundProgramBuilder::termvec(TermVecUid uid, TermUid term) {
    termvecs_[uid].emplace_back(terms_.erase(term));
    return uid;
}

// }}}
// {{{ term vector vectors

TermVecVecUid TestNongroundProgramBuilder::termvecvec() {
    return termvecvecs_.emplace();
}

TermVecVecUid TestNongroundProgramBuilder::termvecvec(TermVecVecUid uid, TermVecUid termvecUid) {
    termvecvecs_[uid].push_back(termvecs_.erase(termvecUid));
    return uid;
}

// }}}
// {{{ literals

LitUid TestNongroundProgramBuilder::boollit(Location const &, bool type) {
    return lits_.emplace(type ? "#true" : "#false");
}

LitUid TestNongroundProgramBuilder::predlit(Location const &, NAF naf, bool neg, FWString name, TermVecVecUid tvvUid) {
    print(naf);
    if (neg) { current_ << "-"; }
    current_ << *name;
    bool nempty = termvecvecs_[tvvUid].size() > 1 || !termvecvecs_[tvvUid].front().empty();
    if (nempty) { current_ << "("; }
    print(termvecvecs_.erase(tvvUid));
    if (nempty) { current_ << ")"; }
    return lits_.emplace(str());
}

LitUid TestNongroundProgramBuilder::rellit(Location const &, Relation rel, TermUid termUidLeft, TermUid termUidRight) {
    current_ << terms_.erase(termUidLeft) << rel << terms_.erase(termUidRight);
    return lits_.emplace(str());
}

// }}}
// {{{ literal vectors

LitVecUid TestNongroundProgramBuilder::litvec() {
    return litvecs_.emplace();
}

LitVecUid TestNongroundProgramBuilder::litvec(LitVecUid uid, LitUid literalUid) {
    litvecs_[uid].emplace_back(lits_.erase(literalUid));
    return uid;
}

// }}}
// {{{ count aggregate elements (body/head)

CondLitVecUid TestNongroundProgramBuilder::condlitvec() {
    return condlitvecs_.emplace();
}

CondLitVecUid TestNongroundProgramBuilder::condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) {
    current_ << lits_.erase(lit);
    current_ << ":";
    print(litvecs_.erase(litvec), ",");
    condlitvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ body aggregate elements

BdAggrElemVecUid TestNongroundProgramBuilder::bodyaggrelemvec() {
    return bodyaggrelemvecs_.emplace();
}

BdAggrElemVecUid TestNongroundProgramBuilder::bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) {
    print(termvecs_.erase(termvec), ",");
    current_ << ":";
    print(litvecs_.erase(litvec), ",");
    bodyaggrelemvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ head aggregate elements

HdAggrElemVecUid TestNongroundProgramBuilder::headaggrelemvec() {
    return headaggrelemvecs_.emplace();
}

HdAggrElemVecUid TestNongroundProgramBuilder::headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) {
    print(termvecs_.erase(termvec), ",");
    current_ << ":" << lits_.erase(lit) << ":";
    print(litvecs_.erase(litvec), ",");
    headaggrelemvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ bounds

BoundVecUid TestNongroundProgramBuilder::boundvec() {
    return bounds_.emplace();
}

BoundVecUid TestNongroundProgramBuilder::boundvec(BoundVecUid uid, Relation rel, TermUid term) {
    std::stringstream ss;
    current_ << terms_.erase(term);
    ss << rel << current_.str();
    current_ << inv(rel);
    bounds_[uid].emplace_back(str(), ss.str());
    return uid;
}

// }}}
// {{{ heads

HdLitUid TestNongroundProgramBuilder::headlit(LitUid lit) {
    return heads_.emplace(lits_.erase(lit));
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, TheoryAtomUid atom) {
    return heads_.emplace(theoryAtoms_.erase(atom));
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, AggregateFunction fun, BoundVecUid boundvecuid, HdAggrElemVecUid headaggrelemvec) {
    print(fun, boundvecuid, headaggrelemvecs_.erase(headaggrelemvec));
    return heads_.emplace(str());
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, AggregateFunction fun, BoundVecUid boundvecuid, CondLitVecUid headaggrelemvec) {
    print(fun, boundvecuid, condlitvecs_.erase(headaggrelemvec));
    return heads_.emplace(str());
}

HdLitUid TestNongroundProgramBuilder::disjunction(Location const &, CondLitVecUid condlitvec) {
    print(condlitvecs_.erase(condlitvec), ";");
    return heads_.emplace(str());
}

// }}}
// {{{ bodies

BdLitVecUid TestNongroundProgramBuilder::body() {
    return bodies_.emplace();
}

BdLitVecUid TestNongroundProgramBuilder::bodylit(BdLitVecUid uid, LitUid lit) {
    bodies_[uid].emplace_back(lits_.erase(lit));
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &, NAF naf, TheoryAtomUid atom) {
    print(naf);
    current_ << theoryAtoms_.erase(atom);
    bodies_[body].emplace_back(str());
    return body;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid uid, Location const &, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) {
    print(naf);
    print(fun, bounds, bodyaggrelemvecs_.erase(bodyaggrelemvec));
    bodies_[uid].emplace_back(str());
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid uid, Location const &, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) {
    print(naf);
    print(fun, bounds, condlitvecs_.erase(bodyaggrelemvec));
    bodies_[uid].emplace_back(str());
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::conjunction(BdLitVecUid uid, Location const &, LitUid head, LitVecUid litvec) {
    current_ << lits_.erase(head) << ":";
    print(litvecs_.erase(litvec), ",");
    bodies_[uid].emplace_back(str());
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::disjoint(BdLitVecUid body, Location const &, NAF naf, CSPElemVecUid elem) {
    current_ << naf << "#disjoint{";
    print(cspelems_.erase(elem), ",");
    current_ << "}";
    bodies_[body].emplace_back(str());
    return body;
}

// }}}
// {{{ csp constraint elements

CSPElemVecUid TestNongroundProgramBuilder::cspelemvec() {
    return cspelems_.emplace();
}

CSPElemVecUid TestNongroundProgramBuilder::cspelemvec(CSPElemVecUid uid, Location const &, TermVecUid termvec, CSPAddTermUid addterm, LitVecUid litvec) {
    print(termvecs_.erase(termvec), ",");
    current_ << ":" << cspaddterms_.erase(addterm) << ":";
    print(litvecs_.erase(litvec), ",");
    cspelems_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ statements

void TestNongroundProgramBuilder::rule(Location const &, HdLitUid head) {
    current_ << heads_.erase(head) << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::rule(Location const &, HdLitUid head, BdLitVecUid bodyuid) {
    current_ << heads_.erase(head);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":-";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::define(Location const &, FWString name, TermUid value, bool) {
    current_ << "#const " << *name << "=" << terms_.erase(value) << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::optimize(Location const &, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) {
    current_ << ":~";
    StringVec bd(bodies_.erase(body));
    print(bd, ";");
    current_ << ".[" << terms_.erase(weight) << "@" << terms_.erase(priority);
    StringVec cd(termvecs_.erase(cond));
    if (!cd.empty()) {
        current_ << ",";
        print(cd, ",");
    }
    current_ << "]";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::showsig(Location const &, FWSignature sig, bool csp) {
    current_ << "#showsig " << (csp ? "$" : "") << *sig << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::show(Location const &, TermUid t, BdLitVecUid bodyuid, bool csp) {
    current_ << "#show " << (csp ? "$" : "") << terms_.erase(t);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::python(Location const &, FWString code) {
    current_ << *code << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::lua(Location const &, FWString code) {
    current_ << *code << ".";
    statements_.emplace_back(str());
}
void TestNongroundProgramBuilder::block(Location const &, FWString name, IdVecUid args) {
    current_ << "#program " << *name << "(";
    print(idvecs_.erase(args), ",");
    current_ << ").";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::external(Location const &, LitUid head, BdLitVecUid bodyuid) {
    current_ << "#external " << lits_.erase(head);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::edge(Location const &, TermVecVecUid edges, BdLitVecUid bodyuid) {
    current_ << "#edge(";
    print(termvecvecs_.erase(edges));
    current_ << ")";
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::heuristic(Location const &, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid bodyuid, TermUid a, TermUid b, TermUid mod) {
    current_ << "#heuristic " << (neg ? "-" : "") << *name;
    bool nempty = termvecvecs_[tvvUid].size() > 1 || !termvecvecs_[tvvUid].front().empty();
    if (nempty) { current_ << "("; }
    print(termvecvecs_.erase(tvvUid));
    if (nempty) { current_ << ")"; }
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".[" << terms_.erase(a) << "@" << terms_.erase(b) << "," << terms_.erase(mod) << "]";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::project(Location const &, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid bodyuid) {
    current_ << "#project " << (neg ? "-" : "") << *name;
    bool nempty = termvecvecs_[tvvUid].size() > 1 || !termvecvecs_[tvvUid].front().empty();
    if (nempty) { current_ << "("; }
    print(termvecvecs_.erase(tvvUid));
    if (nempty) { current_ << ")"; }
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());

}

void TestNongroundProgramBuilder::project(Location const &, FWSignature sig) {
    current_ << "#project " << *sig << ".";
    statements_.emplace_back(str());
}

// }}}
// {{{ theory atoms

TheoryTermUid TestNongroundProgramBuilder::theorytermset(Location const &, TheoryOptermVecUid args) {
    current_ << "{";
    print(theoryOptermVecs_.erase(args), " ; ");
    current_ << "}";
    return theoryTerms_.emplace(str());
}
TheoryTermUid TestNongroundProgramBuilder::theoryoptermlist(Location const &, TheoryOptermVecUid args) {
    current_ << "[";
    print(theoryOptermVecs_.erase(args), " ; ");
    current_ << "]";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermopterm(Location const &, TheoryOptermUid opterm) {
    current_ << "(" << theoryOpterms_.erase(opterm) << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermtuple(Location const &, TheoryOptermVecUid args) {
    current_ << "(";
    auto size = theoryOptermVecs_[args].size();
    print(theoryOptermVecs_.erase(args), ",");
    if (size == 1) { current_ << ","; }
    current_ << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermfun(Location const &, FWString name, TheoryOptermVecUid args) {
    current_ << *name << "(";
    print(theoryOptermVecs_.erase(args), ",");
    current_ << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermvalue(Location const &, Value val) {
    current_ << val;
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermvar(Location const &, FWString var) {
    current_ << var;
    return theoryTerms_.emplace(str());
}

TheoryOptermUid TestNongroundProgramBuilder::theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) {
    auto to = theoryOps_.erase(ops);
    print(to, " ");
    if (!to.empty()) { current_ << " "; }
    current_ << theoryTerms_.erase(term);
    return theoryOpterms_.emplace(str());

}

TheoryOptermUid TestNongroundProgramBuilder::theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) {
    auto to = theoryOps_.erase(ops);
    if (!to.empty()) { current_ << " "; }
    print(to, " ");
    current_ << " " << theoryTerms_.erase(term);
    theoryOpterms_[opterm] += str();
    return opterm;
}

TheoryOpVecUid TestNongroundProgramBuilder::theoryops() {
    return theoryOps_.emplace();
}

TheoryOpVecUid TestNongroundProgramBuilder::theoryops(TheoryOpVecUid ops, FWString op) {
    theoryOps_[ops].emplace_back(op);
    return ops;
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms() {
    return theoryOptermVecs_.emplace();
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms(TheoryOptermVecUid opterms, TheoryOptermUid opterm) {
    theoryOptermVecs_[opterms].emplace_back(theoryOpterms_.erase(opterm));
    return opterms;
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms(TheoryOptermUid opterm, TheoryOptermVecUid opterms) {
    theoryOptermVecs_[opterms].emplace(theoryOptermVecs_[opterms].begin(), theoryOpterms_.erase(opterm));
    return opterms;
}

TheoryElemVecUid TestNongroundProgramBuilder::theoryelems() {
    return theoryElemVecs_.emplace();
}

TheoryElemVecUid TestNongroundProgramBuilder::theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) {
    print(theoryOptermVecs_.erase(opterms), ",");
    current_ << ":";
    print(litvecs_.erase(cond), ",");
    theoryElemVecs_[elems].emplace_back(str());
    return elems;
}

TheoryAtomUid TestNongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems) {
    current_ << "&" << terms_.erase(term) << "{";
    print(theoryElemVecs_.erase(elems), " ; ");
    current_ << "}";
    return theoryAtoms_.emplace(str());
}

TheoryAtomUid TestNongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems, FWString op, TheoryOptermUid opterm) {
    current_ << "&" << terms_.erase(term) << "{";
    print(theoryElemVecs_.erase(elems), " ; ");
    current_ << "} " << *op << " " << theoryOpterms_.erase(opterm);
    return theoryAtoms_.emplace(str());
}

// }}}
// {{{ theory definitions

TheoryOpDefUid TestNongroundProgramBuilder::theoryopdef(Location const &, FWString op, unsigned priority, TheoryOperatorType type) {
    current_ << op << " :" << priority << "," << type;
    return theoryOpDefs_.emplace(str());
}

TheoryOpDefVecUid TestNongroundProgramBuilder::theoryopdefs() {
    return theoryOpDefVecs_.emplace();
}

TheoryOpDefVecUid TestNongroundProgramBuilder::theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) {
    theoryOpDefVecs_[defs].emplace_back(theoryOpDefs_.erase(def));
    return defs;
}

TheoryTermDefUid TestNongroundProgramBuilder::theorytermdef(Location const &, FWString name, TheoryOpDefVecUid defs) {
    current_ << name << "{";
    print(theoryOpDefVecs_.erase(defs), ",");
    current_ << "}";
    return theoryTermDefs_.emplace(str());
}

TheoryAtomDefUid TestNongroundProgramBuilder::theoryatomdef(Location const &, FWString name, unsigned arity, FWString termDef, TheoryAtomType type) {
    current_ << "&" << name << "/" << arity << ":" << termDef << "," << type;
    return theoryAtomDefs_.emplace(str());
}

TheoryAtomDefUid TestNongroundProgramBuilder::theoryatomdef(Location const &, FWString name, unsigned arity, FWString termDef, TheoryAtomType type, TheoryOpVecUid ops, FWString guardDef) {
    current_ << "&" << name << "/" << arity << ":" << termDef << ",{";
    print(theoryOps_.erase(ops), ",");
    current_ << "}," << guardDef << "," << type;
    return theoryAtomDefs_.emplace(str());
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs() {
    return theoryDefVecs_.emplace();
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) {
    theoryDefVecs_[defs].emplace_back(theoryTermDefs_.erase(def));
    return defs;
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) {
    theoryDefVecs_[defs].emplace_back(theoryAtomDefs_.erase(def));
    return defs;
}

void TestNongroundProgramBuilder::theorydef(Location const &, FWString name, TheoryDefVecUid defs) {
    current_ << "#theory " << name << "{";
    print(theoryDefVecs_.erase(defs), ";");
    current_ << "}.";
    statements_.emplace_back(str());
}

// }}}
// {{{ auxiliary functions

void TestNongroundProgramBuilder::print(AggregateFunction fun, BoundVecUid boundvecuid, StringVec const &elems) {
    StringPairVec bounds(bounds_.erase(boundvecuid));
    auto it = bounds.begin(), end = bounds.end();
    if (it != end) { current_ << it->first; ++it; }
    current_ << fun << "{";
    print(elems, ";");
    current_ << "}";
    for (; it != end; ++it) { current_ << it->second; }
}

void TestNongroundProgramBuilder::print(NAF naf) {
    switch (naf) {
        case NAF::NOT:    { current_ << "#not "; break; }
        case NAF::NOTNOT: { current_ << "#not #not "; break; }
        case NAF::POS:    { break; }
    }
}

std::string TestNongroundProgramBuilder::str() {
    std::string str(current_.str());
    current_.str("");
    return str;
}

void TestNongroundProgramBuilder::print(StringVec const &vec, char const *sep) {
    auto it = vec.begin(), end = vec.end();
    if (it != end) {
        current_ << *it++;
        for (; it != end; ++it) { current_ << sep << *it; }
    }
}

void TestNongroundProgramBuilder::print(StringVecVec const &vec) {
    auto it = vec.begin(), end = vec.end();
    if (it != end) {
        print(*it++, ",");
        for (; it != end; ++it) {
            current_ << ";";
            print(*it, ",");
        }
    }
}

// }}}

std::string TestNongroundProgramBuilder::toString() {
    auto it = statements_.begin(), end = statements_.end();
    if (it != end) {
        current_ << *it;
        for (++it; it != end; ++it) { current_ << '\n' << *it; }
    }
    statements_.clear();
    return str();
}

TestNongroundProgramBuilder::~TestNongroundProgramBuilder() { }

// }}}
// {{{ definition of TestNongroundGrammar

void TestNongroundGrammar::setUp() { }

void TestNongroundGrammar::tearDown() { }

std::string TestNongroundGrammar::parse(std::string const &str) {
    TestNongroundProgramBuilder pb;
    NonGroundParser ngp(pb);
    ngp.pushStream("-", std::unique_ptr<std::istream>(new std::stringstream(str)));
    ngp.parse();
    return pb.toString();
}

void TestNongroundGrammar::test_term() {
    // testing constants
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(x)."), parse("p(x)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1)."), parse("p(1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(\"1\")."), parse("p(\"1\")."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(#inf)."), parse("p(#inf)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(#sup)."), parse("p(#sup)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(X)."), parse("p(X)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_)."), parse("p(_)."));
    // absolute
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(|1|)."), parse("p(|1|)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(|1;2;3|)."), parse("p(|1;2;3|)."));
    // lua function calls
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(@f())."), parse("p(@f())."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(@f(1))."), parse("p(@f(1))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(@f(1,2))."), parse("p(@f(1,2))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(@f(1,2,3))."), parse("p(@f(1,2,3))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(@f(;;;1,2;3))."), parse("p(@f(;;;1,2;3))."));
    // function symbols
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(f)."), parse("p(f())."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(f(1))."), parse("p(f(1))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(f(1,2))."), parse("p(f(1,2))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(f(1,2,3))."), parse("p(f(1,2,3))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(f(;;;1,2;3))."), parse("p(f(;;;1,2;3))."));
    // tuples / parenthesis
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((()))."), parse("p(())."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(((1)))."), parse("p((1))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(((1,2)))."), parse("p((1,2))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(((1,2,3)))."), parse("p((1,2,3))."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((();();();(1,2);(3,)))."), parse("p((;;;1,2;3,))."));
    // unary operations
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((-1))."), parse("p(-1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((~1))."), parse("p(~1)."));
    // binary operations
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1**2))."), parse("p(1**2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1\\2))."), parse("p(1\\2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1/2))."), parse("p(1/2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1*2))."), parse("p(1*2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1-2))."), parse("p(1-2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1+2))."), parse("p(1+2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1&2))."), parse("p(1&2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1?2))."), parse("p(1?2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1^2))."), parse("p(1^2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((1..2))."), parse("p(1..2)."));
    // precedence
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(((1+2)+((3*4)*(5**(6**7)))))."), parse("p(1+2+3*4*5**6**7)."));
    // nesting
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np((f(1,(();();();(1,(x..Y));(3)),3)+p(f(1,#sup,3))))."), parse("p(f(1,(;;;1,x..Y;3),3)+p(f(1,#sup,3)))."));
}

void TestNongroundGrammar::test_atomargs() {
    // identifier
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np."), parse("p."));
    // identifier LPAREN argvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np."), parse("p()."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1)."), parse("p(1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(;;;1,2;3,4)."), parse("p(;;;1,2;3,4)."));
    // identifier LPAREN MUL RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_)."), parse("p(_)."));
    // identifier LPAREN MUL cpredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_,1)."), parse("p(_,1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_,1,2)."), parse("p(_,1,2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_,1,2;2,3)."), parse("p(_,1,2;2,3)."));
    // identifier LPAREN MUL spredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_;1)."), parse("p(_;1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_;1,2)."), parse("p(_;1,2)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(_;1,2;2,3)."), parse("p(_;1,2;2,3)."));
    // identifier LPAREN argvec COMMAMUL MUL RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,_)."), parse("p(1,_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2,_)."), parse("p(1,2,_)."));
    // identifier LPAREN argvec COMMAMUL MUL cpredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2,_,3,_;_)."), parse("p(1,2,_,3,_;_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2,_,3,4,_;_)."), parse("p(1,2,_,3,4,_;_)."));
    // identifier LPAREN argvec COMMAMUL MUL spredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2,_;3,_;_)."), parse("p(1,2,_;3,_;_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2,_;3,4,_;_)."), parse("p(1,2,_;3,4,_;_)."));
    // identifier LPAREN argvec SEMMUL MUL RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1;_)."), parse("p(1;_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2;_)."), parse("p(1,2;_)."));
    // identifier LPAREN argvec SEMMUL MUL cpredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2;_,3,_;_)."), parse("p(1,2;_,3,_;_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2;_,3,4,_;_)."), parse("p(1,2;_,3,4,_;_)."));
    // identifier LPAREN argvec SEMMUL MUL spredargvec RPAREN
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2;_;3,_;_)."), parse("p(1,2;_;3,_;_)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np(1,2;_;3,4,_;_)."), parse("p(1,2;_;3,4,_;_)."));
}

void TestNongroundGrammar::test_literal() {
    // TRUE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#true."), parse("#true."));
    // FALSE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false."), parse("#false."));
    // atom
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\np."), parse("p."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n-p."), parse("-p."));
    // NOT atom
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#not p."), parse("not p."));
    // NOT NOT atom
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#not #not p."), parse("not not p."));
    // term cmp term
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n(1+2)!=3."), parse("1+2!=3."));
}

void TestNongroundGrammar::test_bdaggr() {
    // aggregatefunction
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{}."), parse(":-{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#sum{}."), parse(":-#sum{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#sum+{}."), parse(":-#sum+{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#min{}."), parse(":-#min{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#max{}."), parse(":-#max{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{}."), parse(":-#count{}."));
    // LBRACE altbodyaggrelemvec RBRACE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{p:;p:;p:p;p:p,q}."), parse(":-{p;p:;p:p;p:p,q}."));
    // aggregatefunction LBRACE bodyaggrelemvec RBRACE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{:;:p;:p,q}."), parse(":-#count{:;:p;:p,q}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{p:;p,q:}."), parse(":-#count{p;p,q}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{p:;p,q:}."), parse(":-#count{p:;p,q:}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{p:q,r,s}."), parse(":-#count{p:q,r,s}."));
    // test lower
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<=#count{}."), parse(":-1{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<#count{}."), parse(":-1<{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<=#count{}."), parse(":-1<={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1>#count{}."), parse(":-1>{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1>=#count{}."), parse(":-1>={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1=#count{}."), parse(":-1=={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1=#count{}."), parse(":-1={}."));
    // test upper
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1>=#count{}."), parse(":-{}1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<#count{}."), parse(":-{}>1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<=#count{}."), parse(":-{}>=1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1>#count{}."), parse(":-{}<1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1>=#count{}."), parse(":-{}<=1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1=#count{}."), parse(":-{}==1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1=#count{}."), parse(":-{}=1."));
    // test both
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<=#count{}<=2."), parse(":-1{}2."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-1<#count{}<2."), parse(":-1<{}<2."));
}

void TestNongroundGrammar::test_hdaggr() {
    // aggregatefunction
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{}."), parse("{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#sum{}."), parse("#sum{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#sum+{}."), parse("#sum+{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#min{}."), parse("#min{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#max{}."), parse("#max{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{}."), parse("#count{}."));
    // LBRACE altheadaggrelemvec RBRACE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{p:;p:;p:p;p:p,q}."), parse("{p;p:;p:p;p:p,q}."));
    // aggregatefunction LBRACE headaggrelemvec RBRACE
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{:p:;:q:r;:s:t,u}."), parse("#count{:p;:q:r;:s:t,u}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{p:q:;r,s:t:}."), parse("#count{p:q;r,s:t}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{p:q:;r,s:t:}."), parse("#count{p:q:;r,s:t:}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{p:q:x,y,z;r,s:t:q,w,e}."), parse("#count{p:q:x,y,z;r,s:t:q,w,e}."));
    // test lower
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<=#count{}."), parse("1{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<#count{}."), parse("1<{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<=#count{}."), parse("1<={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1>#count{}."), parse("1>{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1>=#count{}."), parse("1>={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1=#count{}."), parse("1=={}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1=#count{}."), parse("1={}."));
    // test upper
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1>=#count{}."), parse("{}1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<#count{}."), parse("{}>1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<=#count{}."), parse("{}>=1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1>#count{}."), parse("{}<1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1>=#count{}."), parse("{}<=1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1=#count{}."), parse("{}==1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1=#count{}."), parse("{}=1."));
    // test both
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<=#count{}<=2."), parse("1{}2."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1<#count{}<2."), parse("1<{}<2."));
}

void TestNongroundGrammar::test_conjunction() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a:."), parse(":-a:."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a:b."), parse(":-a:b."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a:b,c."), parse(":-a:b,c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a:b,c;x."), parse(":-a:b,c;x."));
}

void TestNongroundGrammar::test_disjunction() {
    // Note: first disjunction element moves to the end (parsing related)
    // literal COLON litvec
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na."), parse("a."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na:b."), parse("a:b."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na:b,c."), parse("a:b,c."));
    // literal COMMA disjunctionsep literal optcondition
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;a:."), parse("a,b,c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;d:;a:."), parse("a,b;c;d."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:d,e;a:."), parse("a,b,c:d,e."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:d,e;c:;a:."), parse("a,b:d,e;c."));
    // literal SEM disjunctionsep literal optcondition
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;a:."), parse("a;b,c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;d:;a:."), parse("a;b;c;d."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:d,e;a:."), parse("a;b,c:d,e."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:d,e;c:;a:."), parse("a;b:d,e;c."));
    // literal COLON litvec SEM disjunctionsep literal optcondition
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nc:;a:."), parse("a;c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nc:;a:x."), parse("a:x;c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nc:;a:x,y."), parse("a:x,y;c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;a:x,y."), parse("a:x,y;b,c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:;d:;a:x,y."), parse("a:x,y;b;c;d."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;c:d,e;a:x,y."), parse("a:x,y;b,c:d,e."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:d,e;c:;a:x,y."), parse("a:x,y;b:d,e;c."));
}

void TestNongroundGrammar::test_rule() {
    // body literal
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a;x."), parse(":-a,x."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a;x."), parse(":-a;x."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a;x."), parse(":-a,x."));
    // body aggregate
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{};#count{}."), parse(":-{},{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#count{};#count{}."), parse(":-{};{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #count{};#not #count{}."), parse(":-not{},not{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #count{};#not #count{}."), parse(":-not{};not{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #not #count{};#not #not #count{}."), parse(":-not not{},not not{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #not #count{};#not #not #count{}."), parse(":-not not{};not not{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #not nott<=#count{};#not #not nott<=#count{}."), parse(":-not not nott{},not not nott{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-#not #not nott<=#count{};#not #not nott<=#count{}."), parse(":-not not nott{};not not nott{}."));
    // conjunction
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a:;b:."), parse(":-a:;b:."));
    // head literal
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na."), parse("a."));
    // head aggregate
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#count{}."), parse("{}."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nnott<=#count{}."), parse("nott{}."));
    // disjunction
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\nb:;a:."), parse("a,b."));
    // rules
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na."), parse("a."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na:-b."), parse("a:-b."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na:-b;c."), parse("a:-b,c."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false."), parse(":-."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-b."), parse(":-b."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-b;c."), parse(":-b,c."));
}

void TestNongroundGrammar::test_define() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#const a=10."), parse("#const a=10."));
}

void TestNongroundGrammar::test_optimize() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n:~p(X,Y).[X@Y,a]"), parse(":~ p(X,Y). [X@Y,a]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n:~p(X,Y);r;s.[X@0]"), parse(":~ p(X,Y),r,s. [X]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n:~p(X,Y);s.[X@Y,a]\n:~q(Y).[Y@f]\n:~.[1@0]"), parse("#minimize { X@Y,a : p(X,Y),s; Y@f : q(Y); 1 }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base()."), parse("#minimize { }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n:~p(X,Y);r.[(-X)@Y,a]\n:~q(Y).[(-Y)@f]\n:~.[(-2)@0]"), parse("#maximize { X@Y,a : p(X,Y), r; Y@f : q(Y); 2 }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base()."), parse("#maximize { }."));
}

void TestNongroundGrammar::test_show() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#showsig p/1."), parse("#show p/1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#showsig p/1."), parse("#show p  /  1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#showsig -p/1."), parse("#show -p/1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#show ((-p)/(-1))."), parse("#show -p/-1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#show X:p(X);1<=#count{q(X):p(X)}."), parse("#show X:p(X), 1 { q(X):p(X) }."));
}

void TestNongroundGrammar::test_include() {
    std::ofstream("/tmp/test_include.lp") << "b.\n";
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\na.\nb.\n#program base().\nc.\nd."), parse("a.\n#include \"/tmp/test_include.lp\".\nc.\nd.\n"));
}

void TestNongroundGrammar::test_csp() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1$<=1$*$x$<=10."), parse("1 $<= $x $<= 10."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n1$*$a$<=X$*$x$+8$*$X$<=10:-p(X)."), parse("1 $* $a $<= $x $* X $+ 8 $* $X $<= 10 :- p(X)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-a;b;not #disjoint{a:1$*$a:a,a,b:1$*$b:b}."), parse("#disjoint{a:$a:a,a; b:$b:b} :- a, b."));
}

void TestNongroundGrammar::test_edge() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#edge(a,b)."), parse("#edge (a, b)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#edge(a,b;e,f)."), parse("#edge (a, b;e,f):."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#edge(a,b;e,f):p;q."), parse("#edge (a, b;e,f):p,q."));
}

void TestNongroundGrammar::test_project() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#project a/1."), parse("#project a/1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#project -a/1."), parse("#project -a/1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#project a."), parse("#project a."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#project a."), parse("#project a:."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#project a:b;c."), parse("#project a:b,c."));
}

void TestNongroundGrammar::test_heuristic() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q.[1@2,level]"), parse("#heuristic p : q. [1@2,level]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q.[1@2,sign]"), parse("#heuristic p : q. [1@2,sign]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q.[1@2,true]"), parse("#heuristic p : q. [1@2,true]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q.[1@2,false]"), parse("#heuristic p : q. [1@2,false]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q.[1@2,factor]"), parse("#heuristic p : q. [1@2,factor]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p:q;r.[1@2,init]"), parse("#heuristic p : q,r. [1@2,init]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p.[1@0,init]"), parse("#heuristic p:. [1,init]"));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#heuristic p.[1@0,init]"), parse("#heuristic p. [1,init]"));
}

void TestNongroundGrammar::test_theory() {
    // NOTE: things would be less error prone if : and ; would not be valid theory connectives
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n&x{}."), parse("&x { }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-&x{}."), parse(":-&x { }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n&x{} < 42."), parse("&x { } < 42."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-&x{} < 42."), parse(":-&x { } < 42."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-&x{} < 42 + 17 ^ (- 1)."), parse(":-&x { } < 42+17^(-1)."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-&x{} < 42 + 17 ^ - 1."), parse(":-&x { } < 42+17^ -1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#false:-&x{} < 42 + 17 ^- 1."), parse(":-&x { } < 42+17^-1."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n&x{u,v: ; u: ; u: ; : ; :p ; :p,q}."), parse("&x { u,v; u ; u: ; : ; :p; :p,q }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n&x{u: ; u: ; : ; :p ; :p,q}."), parse("&x { u ; u: ; : ; :p; :p,q }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n&x{i(u + v ^ (a +- 3 * b),7 + (1,) *** (2) - () + [a ; b] ? {1 ; 2 ; 3}):}."), parse("&x { i(u+v^(a+- 3 * b),7+(1,)***(2)-()+[a,b]?{1,2,3}) }."));
}

void TestNongroundGrammar::test_theoryDefinition() {
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{}."), parse("#theory t { }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{}}."), parse("#theory t { x { } }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{};y{}}."), parse("#theory t { x { }; y{} }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{++ :42,unary}}."), parse("#theory t { x { ++ : 42, unary } }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{++ :42,unary,** :21,binary,left,-* :1,binary,right}}."), parse("#theory t { x { ++ : 42, unary; ** : 21, binary, left; -* : 1, binary, right } }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{};&a/0:x,any}."), parse("#theory t { x{}; &a/0: x, any }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{};&a/0:x,any;&b/0:x,head;&c/0:x,body;&d/0:x,directive}."), parse("#theory t { x{}; &a/0: x, any; &b/0: x, head; &c/0: x, body; &d/0: x, directive }."));
    CPPUNIT_ASSERT_EQUAL(std::string("#program base().\n#theory t{x{};&a/0:x,{+,-,++,**},x,any}."), parse("#theory t { x{}; &a/0: x, {+,-, ++, **}, x, any }."));
}

TestNongroundGrammar::~TestNongroundGrammar() { }

// }}}

CPPUNIT_TEST_SUITE_REGISTRATION(TestNongroundGrammar);

} } } // namespace Test Input Gringo

