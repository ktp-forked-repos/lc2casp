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

#include "gringo/output/backends.hh"
#include "gringo/output/literals.hh"
#include "gringo/output/theory.hh"
#include <cstring>

namespace Gringo { namespace Output {

namespace {

// {{{1 print helpers

template <class T>
struct PrintWrapper {
    PrintWrapper(T const &value) : value(value) { }
    T const &value;
};

template <class T>
PrintWrapper<T> p(T const &value) {
    return {value};
}

template <class T>
std::ostream &operator<<(std::ostream &out, PrintWrapper<std::vector<T>> const &p) {
    out << p.value.size();
    for (auto &x : p.value) { out << " " << x; }
    return out;
}

std::ostream &operator<<(std::ostream &out, PrintWrapper<IntermediateFormatBackend::LitWeightVec> const &p) {
    out << p.value.size();
    for (auto &x : p.value) { out << " " << x.lit << " " << x.weight; }
    return out;
}

// }}}1

} // namespace

// {{{1 definition of IntermediateFormatBackend

IntermediateFormatBackend::IntermediateFormatBackend(Potassco::TheoryData const &data, std::ostream &out)
: data_(data)
, out_(out) { }

void IntermediateFormatBackend::init(bool incremental) {
    out_ << "asp 1 0 0" << (incremental ? " incremental" : "") << "\n";
}

void IntermediateFormatBackend::beginStep() {
}

void IntermediateFormatBackend::printHead(bool choice, AtomVec const &atoms) {
    out_ << Potassco::Directive_t::Rule << " " << (choice ? Potassco::Head_t::Choice : Potassco::Head_t::Disjunctive) << " " << p(atoms);
}

void IntermediateFormatBackend::printNormalBody(LitVec const &body) {
    out_ << " " << Potassco::Body_t::Normal << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) {
    out_ << " " << Potassco::Body_t::Sum << " " << lower << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printMinimize(int priority, LitWeightVec const &body) {
    out_ << Potassco::Directive_t::Minimize << " " << priority << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printProject(AtomVec const &lits) {
    out_ << Potassco::Directive_t::Project << " " << p(lits) << "\n";
}

void IntermediateFormatBackend::printOutput(char const *value, LitVec const &body) {
    out_ << Potassco::Directive_t::Output << " " << strlen(value) << " " << value << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printEdge(unsigned u, unsigned v, LitVec const &body) {
    out_ << Potassco::Directive_t::Edge << " " << u << " " << v << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) {
    out_ << Potassco::Directive_t::Heuristic << " " << modifier << " " << atom << " " << value << " " << priority << " " << p(body) << "\n";
}

void IntermediateFormatBackend::printExternal(Potassco::Atom_t atom, Potassco::Value_t value) {
    out_ << Potassco::Directive_t::External << " " << atom << " " << value << "\n";
}

void IntermediateFormatBackend::printAssume(LitVec const &lits) {
    out_ << Potassco::Directive_t::Assume << " " << p(lits) << "\n";
}

void IntermediateFormatBackend::endStep() {
    out_ << "0\n";
}

void IntermediateFormatBackend::printTerm(Potassco::Id_t termId) {
    if (seenTerms_.size() <= termId) { seenTerms_.resize(termId + 1, false); }
    if (!seenTerms_[termId]) {
        seenTerms_[termId] = true;
        auto &term = data_.getTerm(termId);
        switch (term.type()) {
            case Potassco::Theory_t::Number: {
                out_ << Potassco::Directive_t::Theory << " " << Potassco::Theory_t::Number << " " << termId << " " << term.number() << "\n";
                break;
            }
            case Potassco::Theory_t::Symbol: {
                out_ << Potassco::Directive_t::Theory<< " " << Potassco::Theory_t::Symbol << " " << termId << " " << std::strlen(term.symbol()) << " " << term.symbol() << "\n";
                break;
            }
            case Potassco::Theory_t::Compound: {
                for (auto &termId : term) { printTerm(termId); }
                if (term.isFunction()) { printTerm(term.function()); }
                out_ << Potassco::Directive_t::Theory << " " << Potassco::Theory_t::Compound << " " << termId << " " << term.compound() << " " << term.size();
                for (auto &termId : term) { out_ << " " << termId; }
                out_ << "\n";
                break;
            }
        }
    }
}

void IntermediateFormatBackend::printTheoryAtom(Potassco::TheoryAtom const &atom, GetCond getCond) {
    printTerm(atom.term());
    for (auto &elemId : atom) {
        if (seenElems_.size() <= elemId) { seenElems_.resize(elemId + 1, false); }
        if (!seenElems_[elemId]) {
            seenElems_[elemId] = true;
            auto &elem = data_.getElement(elemId);
            for (auto &termId : elem) { printTerm(termId); }
            LitVec cond = getCond(elemId);
            out_ << Potassco::Directive_t::Theory << " " << Potassco::Theory_t::Element << " " << elemId << " " << elem.size();
            for (auto &termId : elem) { out_ << " " << termId; }
            out_ << " " << cond.size();
            for (auto &lit : cond) { out_ << " " << lit; }
            out_ << "\n";
        }
    }
    if (atom.guard()) {
        printTerm(*atom.rhs());
        printTerm(*atom.guard());
    }
    out_ << Potassco::Directive_t::Theory << " " << (atom.guard() ? Potassco::Theory_t::AtomWithGuard : Potassco::Theory_t::Atom) << " " << atom.atom() << " " << atom.occurrence() << " " << atom.term() << " " << atom.size();
    for (auto &elemId : atom) { out_ << " " << elemId; }
    if (atom.guard()) { out_ << " " << *atom.guard() << " " << *atom.rhs(); }
    out_ << "\n";
}

IntermediateFormatBackend::~IntermediateFormatBackend() noexcept = default;

// {{{1 definition of SmodelsFormatBackend

SmodelsFormatBackend::SmodelsFormatBackend(std::ostream &out)
: out_(out, true) { }

void SmodelsFormatBackend::init(bool incremental) {
    out_.initProgram(incremental);
}

void SmodelsFormatBackend::beginStep() {
    out_.beginStep();
}

void SmodelsFormatBackend::printHead(bool choice, AtomVec const &atoms) {
    type_ = choice ? Potassco::Head_t::Choice : Potassco::Head_t::Disjunctive;
    atoms_ = atoms;
}

void SmodelsFormatBackend::printNormalBody(LitVec const &body) {
    wlits_.clear();
    for (auto &lit : body) { wlits_.push_back({lit, 1}); }
    Potassco::Weight_t size = static_cast<Potassco::Weight_t>(wlits_.size());
    out_.rule(Potassco::HeadView{type_, Potassco::toSpan(atoms_)}, Potassco::BodyView{Potassco::Body_t::Normal, size, Potassco::toSpan(wlits_)});
}

void SmodelsFormatBackend::printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) {
    out_.rule(Potassco::HeadView{type_, Potassco::toSpan(atoms_)}, Potassco::BodyView{Potassco::Body_t::Sum, lower, Potassco::toSpan(body)});
}

void SmodelsFormatBackend::printMinimize(int priority, LitWeightVec const &body) {
    out_.minimize(priority, toSpan(body));
}

void SmodelsFormatBackend::printProject(AtomVec const &atoms) {
    out_.project(Potassco::toSpan(atoms));
}

void SmodelsFormatBackend::printOutput(char const *value, LitVec const &body) {
    out_.output(Potassco::StringSpan{value, strlen(value)}, Potassco::toSpan(body));
}

void SmodelsFormatBackend::printEdge(unsigned u, unsigned v, LitVec const &body) {
    out_.acycEdge(u, v, Potassco::toSpan(body));
}

void SmodelsFormatBackend::printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) {
    out_.heuristic(atom, modifier, value, priority, Potassco::toSpan(body));
}

void SmodelsFormatBackend::printExternal(Potassco::Atom_t atom, Potassco::Value_t value) {
    out_.external(atom, value);
}

void SmodelsFormatBackend::printAssume(LitVec const &lits) {
    out_.assume(Potassco::toSpan(lits));
}

void SmodelsFormatBackend::printTheoryAtom(Potassco::TheoryAtom const &, GetCond) {
    throw std::runtime_error("smodels format does not support theory atoms");
}

void SmodelsFormatBackend::endStep() {
    out_.endStep();
}

SmodelsFormatBackend::~SmodelsFormatBackend() noexcept = default;

// }}}1

} } // namespace Output Gringo
