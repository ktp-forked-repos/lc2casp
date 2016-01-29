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

#ifndef _GRINGO_OUTPUT_BACKENDS_HH
#define _GRINGO_OUTPUT_BACKENDS_HH

#include <gringo/output/backend.hh>
#include <potassco/smodels.h>
#include <potassco/theory_data.h>

namespace Gringo { namespace Output {

class IntermediateFormatBackend : public Backend {
public:
    IntermediateFormatBackend(Potassco::TheoryData const &data, std::ostream &out);
    void init(bool incremental) override;
    void beginStep() override;
    void printTheoryAtom(Potassco::TheoryAtom const &atom, GetCond getCond) override;
    void printHead(bool choice, AtomVec const &atoms) override;
    void printNormalBody(LitVec const &body) override;
    void printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) override;
    void printMinimize(int priority, LitWeightVec const &body) override;
    void printProject(AtomVec const &atoms) override;
    void printOutput(char const *value, LitVec const &body) override;
    void printEdge(unsigned u, unsigned v, LitVec const &body) override;
    void printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) override;
    void printExternal(Potassco::Atom_t atom, Potassco::Value_t value) override;
    void printAssume(LitVec const &lits) override;
    void endStep() override;
    virtual ~IntermediateFormatBackend() noexcept;

private:
    void printTerm(Potassco::Id_t termId);

private:
    Potassco::TheoryData const &data_;
    std::ostream &out_;
    std::vector<bool> seenTerms_;
    std::vector<bool> seenElems_;
};

class SmodelsFormatBackend : public Backend {
public:
    SmodelsFormatBackend(std::ostream &out);
    void init(bool incremental) override;
    void printTheoryAtom(Potassco::TheoryAtom const &atom, GetCond getCond) override;
    void beginStep() override;
    void printHead(bool choice, AtomVec const &atoms) override;
    void printNormalBody(LitVec const &body) override;
    void printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) override;
    void printMinimize(int priority, LitWeightVec const &body) override;
    void printProject(AtomVec const &atoms) override;
    void printOutput(char const *value, LitVec const &body) override;
    void printEdge(unsigned u, unsigned v, LitVec const &body) override;
    void printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) override;
    void printExternal(Potassco::Atom_t atom, Potassco::Value_t value) override;
    void printAssume(LitVec const &lits) override;
    void endStep() override;
    virtual ~SmodelsFormatBackend() noexcept;

private:
    Potassco::SmodelsOutput out_;
    AtomVec atoms_;
    LitWeightVec wlits_;
    Potassco::Head_t type_;
};

} } // namespace Output Gringo

#endif // _GRINGO_OUTPUT_BACKENDS_HH

