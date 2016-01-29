// 
// Copyright (c) 2015-2016, Benjamin Kaufmann
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
#ifndef LIBLP_CLINGO_H_INCLUDED
#define LIBLP_CLINGO_H_INCLUDED
#include <potassco/basic_types.h>
namespace Potassco {

//! Represents an assignment of a particular solver.
class AbstractAssignment {
public:
	typedef Potassco::Value_t Value_t;
	typedef Potassco::Lit_t   Lit_t;
	virtual ~AbstractAssignment();
	//! Returns whether the current assignment is conflicting.
	virtual bool     hasConflict()     const = 0;
	//! Returns the number of decision literals in the assignment.
	virtual uint32_t level()           const = 0;
	//! Returns whether lit is a valid literal in this assignment.
	virtual bool     hasLit(Lit_t lit) const = 0;
	//! Returns the truth value that is currently assigned to lit or Value_t::Free if lit is unassigned.
	virtual Value_t  value(Lit_t lit)  const = 0;
	//! Returns the decision level on which lit was assigned or uint32_t(-1) if lit is unassigned.
	virtual uint32_t level(Lit_t lit)  const = 0;
	//! Returns whether the given literal is irrevocably assigned on the top level.
	bool isFixed(Lit_t lit) const;
	//! Returns whether the given literal is true wrt the current assignment. 
	bool isTrue(Lit_t lit)  const;
	//! Returns whether the given literal is false wrt the current assignment. 
	bool isFalse(Lit_t lit) const;
};
	
//! Represents a solver.
class AbstractSolver {
public:
	virtual ~AbstractSolver();
	//! Returns the id of the solver that is associated with this object.
	virtual Id_t id() const = 0;
	//! Returns the current assignment of the solver.
	virtual const AbstractAssignment& assignment() const = 0;
	//! Adds the given clause to the solver if possible.
	/*!
	 * Returns false to indicate that adding the clause would require backtracking first.
	 */
	virtual bool addClause(const Potassco::LitSpan& clause) = 0;
	//! Propagates any newly implied literals.
	virtual bool propagate() = 0;
};

//! Base class for implementing propagators.
class AbstractPropagator {
public:
	typedef Potassco::LitSpan ChangeList;
	virtual ~AbstractPropagator();
	virtual bool propagate(AbstractSolver& solver,  const ChangeList& changes) = 0;
	virtual void undo(const AbstractSolver& solver, const ChangeList& undo);
	virtual bool model(AbstractSolver& solver);
};
}
#endif
