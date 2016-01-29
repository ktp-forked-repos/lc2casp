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

#include <gringo/input/theory.hh>
#include <gringo/input/literals.hh>
#include <gringo/ground/statements.hh>
#include <gringo/logger.hh>

namespace Gringo { namespace Input {

// {{{1 definition of TheoryElement

TheoryElement::TheoryElement(Output::UTheoryTermVec &&tuple, ULitVec &&cond)
: tuple_(std::move(tuple))
, cond_(std::move(cond))
{ }

TheoryElement::TheoryElement(TheoryElement &&) = default;

TheoryElement &TheoryElement::operator=(TheoryElement &&) = default;

TheoryElement::~TheoryElement() noexcept = default;

bool TheoryElement::operator==(TheoryElement const &other) const {
    return is_value_equal_to(tuple_, other.tuple_) && is_value_equal_to(cond_, other.cond_);
}

void TheoryElement::print(std::ostream &out) const {
    if (tuple_.empty() && cond_.empty()) {
        out << " : ";
    }
    else {
        print_comma(out, tuple_, ",", [](std::ostream &out, Output::UTheoryTerm const &term) { term->print(out); });
        if (!cond_.empty()) {
            out << ": ";
            print_comma(out, cond_, ",", [](std::ostream &out, ULit const &lit) { lit->print(out); });
        }
    }
}

size_t TheoryElement::hash() const {
    return get_value_hash(tuple_, cond_);
}

void TheoryElement::unpool(TheoryElementVec &elems, bool beforeRewrite) {
    Term::unpool(cond_.begin(), cond_.end(), [beforeRewrite](ULit &lit) {
        return lit->unpool(beforeRewrite);
    }, [&](ULitVec &&cond) {
        elems.emplace_back(get_clone(tuple_), std::move(cond));
    });
}

TheoryElement TheoryElement::clone() const {
    return {get_clone(tuple_), get_clone(cond_)};
}

bool TheoryElement::hasPool(bool beforeRewrite) const {
    for (auto &lit : cond_) {
        if (lit->hasPool(beforeRewrite)) { return true; }
    }
    return false;
}

void TheoryElement::replace(Defines &x) {
    for (auto &term : tuple_) {
        term->replace(x);
    }
    for (auto &lit : cond_) {
        lit->replace(x);
    }
}

void TheoryElement::collect(VarTermBoundVec &vars) const {
    for (auto &term : tuple_) { term->collect(vars); }
    for (auto &lit : cond_) { lit->collect(vars, false); }
}

void TheoryElement::assignLevels(AssignLevel &lvl) {
    AssignLevel &local(lvl.subLevel());
    VarTermBoundVec vars;
    for (auto &term : tuple_) { term->collect(vars); }
    for (auto &lit : cond_) { lit->collect(vars, true); }
    local.add(vars);
}

void TheoryElement::check(Location const &loc, Printable const &p, ChkLvlVec &levels) const {
    levels.emplace_back(loc, p);
    for (auto &lit : cond_) {
        levels.back().current = &levels.back().dep.insertEnt();
        VarTermBoundVec vars;
        lit->collect(vars, true);
        addVars(levels, vars);
    }
    VarTermBoundVec vars;
    levels.back().current = &levels.back().dep.insertEnt();
    for (auto &term : tuple_) {
        term->collect(vars);
    }
    addVars(levels, vars);
    levels.back().check();
    levels.pop_back();
}

bool TheoryElement::simplify(Projections &project, SimplifyState &state) {
    for (auto &lit : cond_) {
        if (!lit->simplify(project, state, true, true)) {
            return false;
        }
    }
    for (auto &dot : state.dots) { cond_.emplace_back(RangeLiteral::make(dot)); }
    for (auto &script : state.scripts) { cond_.emplace_back(ScriptLiteral::make(script)); }
    return true;
}

void TheoryElement::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    Literal::AssignVec assign;
    arith.emplace_back();
    for (auto &lit : cond_) { lit->rewriteArithmetics(arith, assign, auxGen); }
    for (auto &y : arith.back()) { cond_.emplace_back(RelationLiteral::make(y)); }
    for (auto &y : assign) { cond_.emplace_back(RelationLiteral::make(y)); }
    arith.pop_back();
}

void TheoryElement::initTheory(Output::TheoryParser &p) {
    for (auto &term : tuple_) {
        Term::replace(term, term->initTheory(p));
    }
}

std::unique_ptr<Ground::TheoryAccumulate> TheoryElement::toGround(ToGroundArg &x, Ground::TheoryComplete &completeRef, Ground::ULitVec &&lits) const {
    for (auto &z : cond_) { lits.emplace_back(z->toGround(x.domains, false)); }
    return gringo_make_unique<Ground::TheoryAccumulate>(completeRef, get_clone(tuple_), std::move(lits));
}

// {{{1 definition of TheoryAtom

TheoryAtom::TheoryAtom(UTerm &&name, TheoryElementVec &&elems)
: name_(std::move(name))
, elems_(std::move(elems))
, op_("")
, guard_(nullptr)
{ }

TheoryAtom::TheoryAtom(TheoryAtom &&) = default;

TheoryAtom &TheoryAtom::operator=(TheoryAtom &&) = default;

TheoryAtom::TheoryAtom(UTerm &&name, TheoryElementVec &&elems, FWString op, Output::UTheoryTerm &&guard, TheoryAtomType type)
: name_(std::move(name))
, elems_(std::move(elems))
, op_(op)
, guard_(std::move(guard))
, type_(type)
{ }

TheoryAtom TheoryAtom::clone() const {
    return { get_clone(name_), get_clone(elems_), op_, hasGuard() ? get_clone(guard_) : nullptr, type_ };
}

bool TheoryAtom::hasGuard() const {
    return static_cast<bool>(guard_);
}

bool TheoryAtom::operator==(TheoryAtom const &other) const {
    bool ret = is_value_equal_to(name_, other.name_) && is_value_equal_to(elems_, other.elems_) && hasGuard() == other.hasGuard();
    if (ret && hasGuard()) {
        ret = op_ == other.op_ && is_value_equal_to(guard_, other.guard_);
    }
    return ret;
}

void TheoryAtom::print(std::ostream &out) const {
    out << "&";
    name_->print(out);
    out << "{";
    print_comma(out, elems_, ";");
    out << "}";
    if (hasGuard()) {
        out << op_;
        guard_->print(out);
    }
}

size_t TheoryAtom::hash() const {
    size_t hash = get_value_hash(name_, elems_);
    if (hasGuard()) {
        hash = get_value_hash(hash, op_, guard_);
    }
    return hash;
}

template <class T>
void TheoryAtom::unpool(T f, bool beforeRewrite) {
    TheoryElementVec elems;
    for (auto &elem : elems_) {
        elem.unpool(elems, beforeRewrite);
    }
    UTermVec names;
    name_->unpool(names);
    for (auto &name : names) {
        f(TheoryAtom(std::move(name), get_clone(elems), op_, guard_ ? get_clone(guard_) : nullptr));
    }
}

bool TheoryAtom::hasPool(bool beforeRewrite) const {
    if (beforeRewrite && name_->hasPool()) { return true; }
    for (auto &elem : elems_) {
        if (elem.hasPool(beforeRewrite)) { return true; }
    }
    return false;
}

void TheoryAtom::replace(Defines &x) {
    Term::replace(name_, name_->replace(x, true));
    for (auto &elem : elems_) {
        elem.replace(x);
    }
    if (hasGuard()) { guard_->replace(x); }
}

void TheoryAtom::collect(VarTermBoundVec &vars) const {
    name_->collect(vars, false);
    if (hasGuard()) { guard_->collect(vars); }
    for (auto &elem : elems_) { elem.collect(vars); }
}

void TheoryAtom::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    name_->collect(vars, false);
    if (hasGuard()) { guard_->collect(vars); }
    lvl.add(vars);
    for (auto &elem : elems_) {
        elem.assignLevels(lvl);
    }
}

void TheoryAtom::check(Location const &loc, Printable const &p, ChkLvlVec &levels) const {
    levels.back().current = &levels.back().dep.insertEnt();

    VarTermBoundVec vars;
    name_->collect(vars, false);
    if (hasGuard()) { guard_->collect(vars); }
    addVars(levels, vars);

    for (auto &elem : elems_) {
        elem.check(loc, p, levels);
    }
}

bool TheoryAtom::simplify(Projections &project, SimplifyState &state) {
    if (name_->simplify(state, false, false).update(name_).undefined()) {
        return false;
    }
    for (auto &elem : elems_) {
        SimplifyState elemState(state);
        if (!elem.simplify(project, elemState)) {
            return false;
        }
    }
    return true;
}

void TheoryAtom::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &elem : elems_) {
        elem.rewriteArithmetics(arith, auxGen);
    }
}

void TheoryAtom::initTheory(Location const &loc, TheoryDefs &defs, bool inBody, bool hasBody) {
    FWSignature sig = name_->getSig();
    for (auto &def : defs) {
        if (auto atomDef = def.getAtomDef(sig)) {
            type_ = atomDef->type();
            if (inBody) {
                if (type_ == TheoryAtomType::Head) {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: theory body atom used in head:" << "\n"
                        << "  " << sig << "\n";
                    return;
                }
                else if (type_ == TheoryAtomType::Directive) {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: theory directive used in body:" << "\n"
                        << "  " << sig << "\n";
                    return;
                }
            }
            else {
                if (type_ == TheoryAtomType::Body) {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: theory head atom used in body:" << "\n"
                        << "  " << sig << "\n";
                    return;
                }
                if (type_ == TheoryAtomType::Directive && hasBody) {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: theory directive used with body:" << "\n"
                        << "  " << sig << "\n";
                    return;
                }
            }
            if (inBody) {
                type_ = TheoryAtomType::Body;
            }
            else if (type_ != TheoryAtomType::Directive) {
                type_ = TheoryAtomType::Head;
            }
            if (auto termDef = def.getTermDef(atomDef->elemDef())) {
                Output::TheoryParser p(loc, *termDef);
                for (auto &elem : elems_) {
                    elem.initTheory(p);
                }
            }
            else {
                GRINGO_REPORT(E_ERROR)
                    << loc << ": error: missing definition for term:" << "\n"
                    << "  " << atomDef->elemDef() << "\n";
            }
            if (hasGuard()) {
                if (!atomDef->hasGuard()) {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: unexpected guard:" << "\n"
                        << "  " << sig << "\n";
                }
                else if (auto termDef = def.getTermDef(atomDef->guardDef())) {
                    if (std::find(atomDef->ops().begin(), atomDef->ops().end(), op_) != atomDef->ops().end()) {
                        Output::TheoryParser p(loc, *termDef);
                        Term::replace(guard_, guard_->initTheory(p));
                    }
                    else {
                        std::stringstream ss;
                        print_comma(ss, atomDef->ops(), ",");
                        GRINGO_REPORT(E_ERROR)
                            << loc << ": error: unexpected operator:" << "\n"
                            << "  " << op_ << "\n"
                            << loc << ": note: expected one of:\n"
                            << "  " << ss.str() << "\n";
                    }
                }
                else {
                    GRINGO_REPORT(E_ERROR)
                        << loc << ": error: missing definition for term:" << "\n"
                        << "  " << atomDef->guardDef() << "\n";
                }
            }
            return;
        }
    }
    GRINGO_REPORT(E_ERROR)
        << loc << ": error: no definition found for theory atom:" << "\n"
        << "  " << sig << "\n";
}

CreateHead TheoryAtom::toGroundHead(std::shared_ptr<Ground::TheoryLiteral*> sharedLit) const {
    return CreateHead([&,sharedLit](Ground::ULitVec &&lits) {
        assert(*sharedLit);
        return gringo_make_unique<Ground::TheoryRule>(**sharedLit, std::move(lits));
    });
}

CreateBody TheoryAtom::toGroundBody(ToGroundArg &x, Ground::UStmVec &stms, NAF naf, UTerm &&id, std::shared_ptr<Ground::TheoryLiteral*> sharedLit) const {
    if (hasGuard()) {
        stms.emplace_back(gringo_make_unique<Ground::TheoryComplete>(x.domains, std::move(id), type_, get_clone(name_), op_, get_clone(guard_)));
    }
    else {
        stms.emplace_back(gringo_make_unique<Ground::TheoryComplete>(x.domains, std::move(id), type_, get_clone(name_)));
    }
    auto &completeRef = static_cast<Ground::TheoryComplete&>(*stms.back());
    CreateStmVec split;
    split.emplace_back([&completeRef, this](Ground::ULitVec &&lits) -> Ground::UStm {
        auto ret = gringo_make_unique<Ground::TheoryAccumulate>(completeRef, std::move(lits));
        completeRef.addAccuDom(*ret);
        return std::move(ret);
    });
    for (auto &y : elems_) {
        split.emplace_back([this,&completeRef,&y,&x](Ground::ULitVec &&lits) -> Ground::UStm {
            auto ret = y.toGround(x, completeRef, std::move(lits));
            completeRef.addAccuDom(*ret);
            return std::move(ret);
        });
    }
    bool aux1 = type_ != TheoryAtomType::Body;
    return CreateBody([this, &completeRef, naf, aux1, sharedLit](Ground::ULitVec &lits, bool primary, bool aux2) {
        if (primary) {
            auto ret = gringo_make_unique<Ground::TheoryLiteral>(completeRef, naf, aux1 || aux2);
            if (sharedLit) { *sharedLit = ret.get(); }
            lits.emplace_back(std::move(ret));
        }
    }, std::move(split));
}

TheoryAtom::~TheoryAtom() noexcept = default;

// {{{1 definition of HeadTheoryLiteral

HeadTheoryLiteral::HeadTheoryLiteral(TheoryAtom &&atom)
: atom_(std::move(atom))
{ }

HeadTheoryLiteral::~HeadTheoryLiteral() noexcept = default;

CreateHead HeadTheoryLiteral::toGround(ToGroundArg &, Ground::UStmVec &, Ground::RuleType) const {
    return atom_.toGroundHead(sharedLit_);
}

UHeadAggr HeadTheoryLiteral::rewriteAggregates(UBodyAggrVec &lits) {
    auto lit = make_locatable<BodyTheoryLiteral>(loc(), NAF::POS, atom_.clone());
    // TODO: this is an ugly hack to associate the head literal with the body literal
    sharedLit_ = lit->sharedLit();
    lits.emplace_back(std::move(lit));
    return nullptr;
}

void HeadTheoryLiteral::collect(VarTermBoundVec &vars) const {
    atom_.collect(vars);
}

void HeadTheoryLiteral::unpool(UHeadAggrVec &x, bool beforeRewrite) {
    atom_.unpool([&](TheoryAtom &&atom) { x.emplace_back(make_locatable<HeadTheoryLiteral>(loc(), std::move(atom))); }, beforeRewrite);
}

bool HeadTheoryLiteral::simplify(Projections &project, SimplifyState &state) {
    return atom_.simplify(project, state);
}

void HeadTheoryLiteral::assignLevels(AssignLevel &lvl) {
    atom_.assignLevels(lvl);
}

void HeadTheoryLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    atom_.rewriteArithmetics(arith, auxGen);
}

bool HeadTheoryLiteral::hasPool(bool beforeRewrite) const {
    return atom_.hasPool(beforeRewrite);
}

void HeadTheoryLiteral::check(ChkLvlVec &lvl) const {
    atom_.check(loc(), *this, lvl);
}

void HeadTheoryLiteral::replace(Defines &x) {
    atom_.replace(x);
}

size_t HeadTheoryLiteral::hash() const {
    return atom_.hash();
}

void HeadTheoryLiteral::print(std::ostream &out) const {
    if (sharedLit_) { out << "#false"; }
    else            { atom_.print(out); }
}

HeadTheoryLiteral *HeadTheoryLiteral::clone() const {
    assert(!sharedLit_);
    return make_locatable<HeadTheoryLiteral>(loc(), get_clone(atom_)).release();
}

bool HeadTheoryLiteral::operator==(HeadAggregate const &other) const {
    auto t = dynamic_cast<HeadTheoryLiteral const*>(&other);
    return t && atom_ == t->atom_;
}

void HeadTheoryLiteral::initTheory(TheoryDefs &defs, bool hasBody) {
    atom_.initTheory(loc(), defs, false, hasBody);
}

// {{{1 definition of BodyTheoryLiteral

BodyTheoryLiteral::BodyTheoryLiteral(NAF naf, TheoryAtom &&atom)
: atom_(std::move(atom))
, naf_(naf)
{ }

BodyTheoryLiteral::~BodyTheoryLiteral() noexcept = default;

void BodyTheoryLiteral::unpool(UBodyAggrVec &x, bool beforeRewrite) {
    atom_.unpool([&](TheoryAtom &&atom) { x.emplace_back(make_locatable<BodyTheoryLiteral>(loc(), naf_, std::move(atom))); }, beforeRewrite);
}

bool BodyTheoryLiteral::simplify(Projections &project, SimplifyState &state, bool) {
    return atom_.simplify(project, state);
}

void BodyTheoryLiteral::assignLevels(AssignLevel &lvl) {
    atom_.assignLevels(lvl);
}

void BodyTheoryLiteral::check(ChkLvlVec &lvl) const {
    atom_.check(loc(), *this, lvl);
}

void BodyTheoryLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::AssignVec &, AuxGen &auxGen) {
    atom_.rewriteArithmetics(arith, auxGen);
}

bool BodyTheoryLiteral::rewriteAggregates(UBodyAggrVec &) {
    return true;
}

void BodyTheoryLiteral::removeAssignment() {
}

bool BodyTheoryLiteral::isAssignment() const {
    return false;
}

void BodyTheoryLiteral::collect(VarTermBoundVec &vars) const {
    atom_.collect(vars);
}

bool BodyTheoryLiteral::hasPool(bool beforeRewrite) const {
    return atom_.hasPool(beforeRewrite);
}

void BodyTheoryLiteral::replace(Defines &x) {
    atom_.replace(x);
}

CreateBody BodyTheoryLiteral::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    return atom_.toGroundBody(x, stms, naf_, x.newId(*this), sharedLit_);
}

size_t BodyTheoryLiteral::hash() const {
    return get_value_hash(typeid(BodyTheoryLiteral).hash_code(), naf_, atom_);
}

void BodyTheoryLiteral::print(std::ostream &out) const {
    if (sharedLit_) { out << "not " << atom_; }
    else            { out << naf_ << atom_; }
}

BodyTheoryLiteral *BodyTheoryLiteral::clone() const {
    assert(!sharedLit_);
    return make_locatable<BodyTheoryLiteral>(loc(), naf_, get_clone(atom_)).release();
}

bool BodyTheoryLiteral::operator==(BodyAggregate const &other) const {
    auto t = dynamic_cast<BodyTheoryLiteral const*>(&other);
    return t && naf_ == t->naf_ && atom_ == t->atom_;
}

void BodyTheoryLiteral::initTheory(TheoryDefs &defs) {
    atom_.initTheory(loc(), defs, true, true);
}

// }}}1

} } // namespace Gringo Input

