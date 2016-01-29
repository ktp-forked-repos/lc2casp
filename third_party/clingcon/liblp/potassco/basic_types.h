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
#ifndef LIBLP_BASIC_TYPES_H_INCLUDED
#define LIBLP_BASIC_TYPES_H_INCLUDED

#include <cstddef>
#include <stdint.h>
#include <cassert>
namespace Potassco {

// Basic type definitions for reading/writing logic programs.

//! Ids are non-negative integers in the range [0..idMax].
typedef uint32_t Id_t;
const Id_t idMax = static_cast<Id_t>(-1);
//! Atom ids are positive integers in the range [1..atomMax].
typedef uint32_t Atom_t;
const Atom_t atomMin = static_cast<Atom_t>(1);
const Atom_t atomMax = static_cast<Atom_t>(((1u)<<31)-1);
//! Literals are signed atoms.
typedef int32_t Lit_t;
//! (Literal) weights are integers.
typedef int32_t Weight_t;
//! A literal with an associated weight.
struct WeightLit_t {
	Lit_t    lit;
	Weight_t weight;
};
// Basic interface functions
inline Atom_t   atom(Lit_t lit) { return static_cast<Atom_t>(lit >= 0 ? lit : -lit); }
inline Atom_t   atom(const WeightLit_t& w) { return atom(w.lit); }
inline Id_t     id(Lit_t lit) { return static_cast<Id_t>(lit); }
inline Lit_t    lit(Id_t a) { return static_cast<Lit_t>(a); }
inline Lit_t    lit(Lit_t lit) { return lit; }
inline Lit_t    lit(const WeightLit_t& w) { return w.lit; }
inline Lit_t    neg(Atom_t a) { return -lit(a); }
inline Weight_t weight(Atom_t) { return 1; }
inline Weight_t weight(Lit_t) { return 1; }
inline Weight_t weight(const WeightLit_t& w) { return w.weight; }
inline bool operator==(const WeightLit_t& lhs, const WeightLit_t& rhs) { return lit(lhs) == lit(rhs) && weight(lhs) == weight(rhs); }
inline bool operator!=(const WeightLit_t& lhs, const WeightLit_t& rhs) { return !(lhs == rhs); }
inline bool operator<(const WeightLit_t& lhs, const WeightLit_t& rhs) { return lhs.lit != rhs.lit ? lhs.lit < rhs.lit : lhs.weight < rhs.weight; }
inline bool operator==(Lit_t lhs, const WeightLit_t& rhs) { return lit(lhs) == lit(rhs) && weight(rhs) == 1; }
inline bool operator==(const WeightLit_t& lhs, Lit_t rhs) { return rhs == lhs; }
inline bool operator!=(Lit_t lhs, const WeightLit_t& rhs) { return !(lhs == rhs); }
inline bool operator!=(const WeightLit_t& lhs, Lit_t rhs) { return rhs != lhs; }

//! A span consists of a starting address and a length.
/*!
* A span does not own the data and it is in general not safe to store a span.
*/
template <class T>
struct Span {
	typedef const T*    iterator;
	typedef std::size_t size_t;
	const T* first;
	size_t   size;
};
template <class T> inline const T* begin(const Span<T>& s) { return s.first; }
template <class T> inline const T* end(const Span<T>& s) { return begin(s) + s.size; }
template <class T> inline std::size_t size(const Span<T>& s) { return s.size; }
template <class T> inline bool empty(const Span<T>& s) { return s.size == 0; }
template <class T> inline Span<T> toSpan(const T* x, std::size_t s) {
	Span<T> r = {x, s};
	return r;
}
template <class T> inline Span<T> toSpan() { return toSpan(static_cast<const T*>(0), 0); }
template <class C> inline Span<typename C::value_type> toSpan(const C& c) {
	return !c.empty() ? toSpan(&c[0], c.size()) : toSpan<typename C::value_type>();
}
typedef Span<Id_t>        IdSpan;
typedef Span<Atom_t>      AtomSpan;
typedef Span<Lit_t>       LitSpan;
typedef Span<WeightLit_t> WeightLitSpan;
typedef Span<char>        StringSpan;


#define POTASSCO_CONSTANTS(TypeName, ...) \
	enum E { __VA_ARGS__, __eEnd, eMin = 0, eMax = __eEnd - 1 };\
	TypeName(E x = eMin) : val_(x) {}\
	explicit TypeName(unsigned x) : val_(static_cast<E>(x)) {assert(x <= eMax);}\
	operator unsigned() const { return static_cast<unsigned>(val_); } \
	E val_

//! Supported rule head types.
struct Head_t {
	POTASSCO_CONSTANTS(Head_t, Disjunctive = 0, Choice = 1);
};
//! Supported rule body types.
struct Body_t {
	POTASSCO_CONSTANTS(Body_t, Normal = 0, Sum = 1, Count = 2);
	static const Weight_t BOUND_NONE = static_cast<Weight_t>(-1);
};

//! Type representing an external value.
struct Value_t {
	POTASSCO_CONSTANTS(Value_t,
		Free = 0, True = 1,
		False = 2, Release = 3
	);
};

//! Supported heuristic modifications.
struct Heuristic_t {
	POTASSCO_CONSTANTS(Heuristic_t,
		Level = 0, Sign = 1, Factor = 2,
		Init = 3, True = 4, False = 5
	);
	static const StringSpan pred; // zero-terminated pred name
};
inline const char* toString(Heuristic_t t) {
	switch (t) {
		case Heuristic_t::Level: return "level";
		case Heuristic_t::Sign:  return "sign";
		case Heuristic_t::Factor:return "factor";
		case Heuristic_t::Init:  return "init";
		case Heuristic_t::True:  return "true";
		case Heuristic_t::False: return "false";
		default: return "";
	}
}

//! Supported aspif directives.
struct Directive_t {
	POTASSCO_CONSTANTS(Directive_t,
		End       = 0,
		Rule      = 1, Minimize = 2,
		Project   = 3, Output   = 4,
		External  = 5, Assume   = 6,
		Heuristic = 7, Edge     = 8,
		Theory    = 9, Comment  = 10
	);
};

//! Supported aspif theory directives.
struct Theory_t {
	POTASSCO_CONSTANTS(Theory_t,
		Number  = 0, Symbol = 1, Compound = 2, Reserved = 3,
		Element = 4, 
		Atom    = 5, AtomWithGuard = 6
	);
};

#undef POTASSCO_CONSTANTS
#undef POTASSCO_ENUM

//! A type representing a view of rule head.
struct HeadView {
	typedef AtomSpan::iterator iterator;
	Head_t   type;  // type of head
	AtomSpan atoms; // contained atoms
};
inline Head_t        type(const HeadView& h)  { return h.type; }
inline std::size_t   size(const HeadView& h)  { return size(h.atoms); }
inline std::size_t   empty(const HeadView& h) { return empty(h.atoms); }
inline HeadView::iterator begin(const HeadView& h) { return begin(h.atoms); }
inline HeadView::iterator end(const HeadView& h)   { return end(h.atoms); }

//! A type representing a view of a rule body.
struct BodyView {
	typedef WeightLitSpan::iterator iterator;
	Body_t        type;  // type of body
	Weight_t      bound; // optional lower bound - only if type != Normal
	WeightLitSpan lits;  // body literals - weights only relevant if type = Sum
};
inline Body_t      type(const BodyView& b)  { return b.type; }
inline std::size_t size(const BodyView& b)  { return size(b.lits); }
inline std::size_t empty(const BodyView& b) { return empty(b.lits); }
inline bool hasWeights(const BodyView& b) { return b.type == Body_t::Sum; }
inline bool hasBound(const BodyView& b)   { return b.type != Body_t::Normal; }
inline WeightLitSpan::iterator begin(const BodyView& b) { return begin(b.lits); }
inline WeightLitSpan::iterator end(const BodyView& b)   { return end(b.lits); }

//! Basic callback interface.
class LpElement {
public:
	virtual ~LpElement();
	//! Called once to prepare for a new logic program.
	virtual void initProgram(bool incremental) = 0;
	//! Called once before rules and directives of the current program step are added.
	virtual void beginStep() = 0;
	//! Add the given rule to the program.
	virtual void rule(const HeadView& head, const BodyView& body) = 0;
	//! Add the given minimize statement to the program.
	virtual void minimize(Weight_t prio, const WeightLitSpan& lits) = 0;
	//! Mark the given list of atoms as projection atoms.
	virtual void project(const AtomSpan& atoms) = 0;
	//! Output str whenever condition is true in a stable model.
	virtual void output(const StringSpan& str, const LitSpan& condition) = 0;
	//! If v is not equal to Value_t::Release, mark a as external and assume value v. Otherwise, treat a as regular atom.
	virtual void external(Atom_t a, Value_t v) = 0;
	//! Assume the given literals to true during solving.
	virtual void assume(const LitSpan& lits) = 0;
	//! Apply the given heuristic modification to atom a whenever condition is true.
	virtual void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition) = 0;
	//! Assume an edge between s and t whenever condition is true.
	virtual void acycEdge(int s, int t, const LitSpan& condition) = 0;
	//! Called once after all rules and directives of the current program step were added.
	virtual void endStep() = 0;
};
typedef int(*ErrorHandler)(int line, const char* what);

}
#endif
