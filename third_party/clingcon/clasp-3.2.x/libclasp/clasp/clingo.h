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
#ifndef CLASP_CLINGO_H_INCLUDED
#define CLASP_CLINGO_H_INCLUDED
#include <potassco/clingo.h>
#include <clasp/claspfwd.h>
#include <clasp/literal.h>
namespace Clasp {
class TheoryPropagator {
public:
	TheoryPropagator(Potassco::AbstractPropagator& cb);
	~TheoryPropagator();
	//! Registers a watch for lit and returns encodeLit(lit).
	Potassco::Lit_t addWatch(Literal lit);
	
	//! Registers the propagator with the given solver.
	bool attach(Clasp::Solver& s);

	Potassco::AbstractPropagator* propagator() const;
private:
	TheoryPropagator(const TheoryPropagator&);
	TheoryPropagator& operator=(const TheoryPropagator&);
	class PP;
	struct Watches;
	Potassco::AbstractPropagator* prop_;
	Watches* watches_;
};

}
#endif
