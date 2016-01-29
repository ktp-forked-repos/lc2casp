// 
// Copyright (c) 2015, Benjamin Kaufmann
// 
// This file is part of Potassco. See http://potassco.sourceforge.net/
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
#ifndef LIBLP_ASPIF_H_INCLUDED
#define LIBLP_ASPIF_H_INCLUDED
#include <potassco/match_basic_types.h>
namespace Potassco {
class TheoryData;
/*!
 * Parses the given program in asp intermediate format and calls ctx on each parsed element.
 * The error handler h is called on error. If h is 0, ParseError exceptions are used to signal errors.
 */
int readAspif(std::istream& prg, LpElement& out, ErrorHandler h = 0, TheoryData* theory = 0);

class AspifInput : public ProgramReader {
public:
	AspifInput(LpElement& out, TheoryData* theory);
	virtual ~AspifInput();
protected:
	virtual bool doAttach(bool& inc);
	virtual bool doParse();
	virtual void matchTheory(unsigned t);
	virtual unsigned newTheoryCondition(const LitSpan& lits);
private:
	StringSpan matchString();
	AtomSpan   matchTermList();
	struct ParseData;
	LpElement&  out_;
	ParseData*  data_;
	TheoryData* theory_;
};

//! Writes a program in potassco's asp intermediate format to the given output stream.
class AspifOutput : public LpElement {
public:
	AspifOutput(std::ostream& os);
	virtual void initProgram(bool incremental);
	virtual void beginStep();
	virtual void rule(const HeadView& head, const BodyView& body);
	virtual void minimize(Weight_t prio, const WeightLitSpan& lits);
	virtual void output(const StringSpan& str, const LitSpan& cond);
	virtual void external(Atom_t a, Value_t v);
	virtual void assume(const LitSpan& lits);
	virtual void project(const AtomSpan& atoms);
	virtual void acycEdge(int s, int t, const LitSpan& condition);
	virtual void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition);
	virtual void endStep();

	virtual void theory(const TheoryData& data);
	virtual void theoryCondition(unsigned cId);
protected:
	AspifOutput& startDir(Directive_t r);
	AspifOutput& add(int x);
	AspifOutput& add(const WeightLitSpan& lits);
	AspifOutput& add(const LitSpan& lits);
	AspifOutput& add(const AtomSpan& atoms);
	AspifOutput& endDir();
private:
	struct IdSet;
	AspifOutput(const AspifOutput&);
	AspifOutput& operator=(const AspifOutput&);
	void term(const TheoryData& data, unsigned tId);
	std::ostream& os_;
	IdSet*        ids_;
};
}
#endif
