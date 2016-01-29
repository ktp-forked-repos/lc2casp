// 
// Copyright (c) 2015, Benjamin Kaufmann
// 
// This file is part of Potassco. See http://potassco.sourceforge.net/ 
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
#ifdef _MSC_VER
#pragma warning (disable : 4996) // std::copy unsafe
#endif
#include <potassco/theory_data.h>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cstring>

#define FAIL_IF(exp, msg) \
	(void)( (!(exp)) || (throw std::logic_error(msg), 0))	


namespace Potassco {
template <class T>
static std::size_t nBytes(const IdSpan& ids) {
	return sizeof(T) + (ids.size * sizeof(Id_t));
}
struct FuncData {
	static FuncData* newFunc(int32_t base, const IdSpan& args);
	static void destroy(FuncData*);
	int32_t  base;
	uint32_t size;
	Id_t     args[0];
};
FuncData* FuncData::newFunc(int32_t base, const IdSpan& args) {
	std::size_t nb = nBytes<FuncData>(args);
	FuncData* f = new (::operator new(nb)) FuncData;
	f->base = base;
	f->size = args.size;
	std::memcpy(f->args, begin(args), f->size * sizeof(Id_t));
	return f;
}
void FuncData::destroy(FuncData* f) {
	if (f) { f->~FuncData(); ::operator delete(f); }
}
const uint64_t nulTerm  = static_cast<uint64_t>(-1);
const uint64_t typeMask = static_cast<uint64_t>(3);

TheoryTerm::TheoryTerm() : data_(nulTerm) {}
TheoryTerm::TheoryTerm(int num) {
	data_ = (static_cast<uint64_t>(num) << 2) | Theory_t::Number;
}
TheoryTerm::TheoryTerm(const char* sym) {
	data_ = (assertPtr(sym) | Theory_t::Symbol);
	assert(sym == symbol());
}
TheoryTerm::TheoryTerm(const FuncData* c) {
	data_ = (assertPtr(c) | Theory_t::Compound);
}
uint64_t TheoryTerm::assertPtr(const void* p) const { 
	uint64_t invalid_pointer_size[ (sizeof(uint64_t) >= sizeof(uintptr_t)) ] = {
		static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p))
	};
	FAIL_IF((invalid_pointer_size[0] & 3u) != 0u, "Invalid pointer alignment!");
	return invalid_pointer_size[0];
}
void TheoryTerm::assertType(Theory_t t) const { FAIL_IF(type() != t, "Invalid term cast!"); }
bool TheoryTerm::valid() const { return data_ != nulTerm; }
Theory_t TheoryTerm::type() const { FAIL_IF(!valid(), "Invalid term!"); return static_cast<Theory_t>(data_&typeMask); }
int TheoryTerm::number() const {
	assertType(Theory_t::Number);
	return static_cast<int>(data_ >> 2);
}
uintptr_t TheoryTerm::getPtr() const {
	return static_cast<uintptr_t>(data_ & ~typeMask);
}
const char* TheoryTerm::symbol() const {
	assertType(Theory_t::Symbol);
	return reinterpret_cast<const char*>(getPtr());
}
FuncData* TheoryTerm::func() const {
	return reinterpret_cast<FuncData*>(getPtr());
}
int TheoryTerm::compound() const {
	assertType(Theory_t::Compound);
	return func()->base;
}
bool TheoryTerm::isFunction() const { return type() == Theory_t::Compound && func()->base >= 0; }
bool TheoryTerm::isTuple()    const { return type() == Theory_t::Compound && func()->base < 0; }
Id_t TheoryTerm::function()   const { FAIL_IF(!isFunction(), "Invalid term cast: not a function!"); return static_cast<Id_t>(func()->base); }
TupleType TheoryTerm::tuple() const { FAIL_IF(!isTuple(), "Invalid term cast: not a tuple!"); return static_cast<TupleType>(func()->base); }
uint32_t TheoryTerm::size()   const { return type() == Theory_t::Compound ? func()->size : 0; }
TheoryTerm::iterator TheoryTerm::begin() const { return type() == Theory_t::Compound ? func()->args : 0; }
TheoryTerm::iterator TheoryTerm::end()   const { return type() == Theory_t::Compound ? func()->args + func()->size : 0; }

TheoryElement::TheoryElement(const IdSpan& terms, Id_t c) : nTerms_(terms.size), nCond_(c != 0) {
	std::memcpy(term_, terms.first, nTerms_ * sizeof(Id_t));
	if (nCond_ != 0) { term_[nTerms_] = c; }
}
TheoryElement* TheoryElement::newElement(const IdSpan& terms, Id_t c) {
	std::size_t nb = nBytes<TheoryElement>(terms);
	if (c != 0) { nb += sizeof(Id_t); }
	return new (::operator new(nb)) TheoryElement(terms, c);
}
void TheoryElement::destroy(TheoryElement* e) {
	if (e) {
		e->~TheoryElement();
		::operator delete(e);
	}
}
Id_t TheoryElement::condition() const {
	return nCond_ == 0 ? 0 : term_[nTerms_];
}
void TheoryElement::setCondition(Id_t c) {
	term_[nTerms_] = c;
}

TheoryAtom::TheoryAtom(Id_t a, Occurrence occ, Id_t term, const IdSpan& args, Id_t* op, Id_t* rhs)
	: atom_(a)
	, occ_(static_cast<uint32_t>(occ))
	, guard_(op != 0)
	, termId_(term)
	, nTerms_(args.size) {
	std::memcpy(term_, args.first, nTerms_ * sizeof(Id_t));
	if (op) { 
		term_[nTerms_] = *op; 
		term_[nTerms_ + 1] = *rhs; 
	}
}

TheoryAtom* TheoryAtom::newAtom(Id_t a, Occurrence occ, Id_t term, const IdSpan& args) {
	return new (::operator new(nBytes<TheoryAtom>(args))) TheoryAtom(a, occ, term, args, 0, 0);
}
TheoryAtom* TheoryAtom::newAtom(Id_t a, Occurrence occ, Id_t term, const IdSpan& args, Id_t op, Id_t rhs) {
	std::size_t nb = nBytes<TheoryAtom>(args) + (2*sizeof(Id_t));
	return new (::operator new(nb)) TheoryAtom(a, occ, term, args, &op, &rhs);
}
void TheoryAtom::destroy(TheoryAtom* a) {
	if (a) {
		a->~TheoryAtom();
		::operator delete(a);
	}
}
const Id_t* TheoryAtom::guard() const {
	return guard_ != 0 ? &term_[nTerms_] : 0;
}
const Id_t* TheoryAtom::rhs() const {
	return guard_ != 0 ? &term_[nTerms_ + 1] : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
// TheoryData
//////////////////////////////////////////////////////////////////////////////////////////////////////
struct TheoryData::DestroyT { 
	template <class T> void operator()(T* x) const { return T::destroy(x); }
	void operator()(TheoryTerm& t) const {
		if (t.valid()) {
			if (t.type() == Theory_t::Compound) {
				this->operator()(t.func());
			}
			else if (t.type() == Theory_t::Symbol) {
				delete[] const_cast<char*>(t.symbol());
			}
		}
	}
};
TheoryData::TheoryData()  {}
TheoryData::~TheoryData() {
	reset();
}
const TheoryTerm& TheoryData::addTerm(Id_t termId, int number) {
	return setTerm(termId) = TheoryTerm(number);
}
const TheoryTerm& TheoryData::addTerm(Id_t termId, const StringSpan& name) {
	char* buf = new char[name.size + 1];
	*std::copy(Potassco::begin(name), Potassco::end(name), buf) = 0;
	return setTerm(termId) = TheoryTerm(buf);
}
const TheoryTerm& TheoryData::addTerm(Id_t termId, const char* name) {
	return addTerm(termId, Potassco::toSpan(name, name ? std::strlen(name) : 0));
}
const TheoryTerm& TheoryData::addTerm(Id_t termId, Id_t funcId, const IdSpan& args) {
	return setTerm(termId) = TheoryTerm(FuncData::newFunc(static_cast<int32_t>(funcId), args));
}
const TheoryTerm& TheoryData::addTerm(Id_t termId, TupleType type, const IdSpan& args) {
	return setTerm(termId) = TheoryTerm(FuncData::newFunc(static_cast<int32_t>(type), args));
}
void TheoryData::removeTerm(Id_t termId) {
	if (hasTerm(termId)) {
		DestroyT()(terms_.data[termId]);
		terms_.data[termId] = Term();
	}
}
template <class T>
void TheoryData::grow(TheoryData::Vec<T>& vec, uint32_t ns, const T& def) {
	if (vec.cap < ns) {
		uint32_t new_cap = (vec.cap*3)>>1;
		FAIL_IF(new_cap < vec.cap, "vector: max capacity exceeded!");
		if (ns > new_cap) { new_cap = ns > 8u ? ns : 8u; }
		T* temp = (T*)::operator new(new_cap * sizeof(T));
		std::memcpy(temp, vec.data, vec.sz*sizeof(T));
		::operator delete(vec.data);
		vec.cap  = new_cap;
		vec.data = temp;
	}
	std::fill_n(vec.data + vec.sz, ns - vec.sz, def);
	vec.sz = ns;
}
const TheoryElement& TheoryData::addElement(Id_t id, const IdSpan& terms, Id_t cId) {
	if (id >= elems_.sz) {
		grow(elems_, id + 1, static_cast<TheoryElement*>(0));
	}
	FAIL_IF(elems_.data[id] != 0, "Redefinition of theory element!");
	return *(elems_.data[id] = TheoryElement::newElement(terms, cId));
}
const TheoryAtom& TheoryData::addAtom(Id_t atomOrZero, TheoryAtom::Occurrence occ, Id_t termId, const IdSpan& elems) {
	grow(atoms_, atoms_.sz + 1, TheoryAtom::newAtom(atomOrZero, occ, termId, elems));
	return *atoms_.data[atoms_.sz - 1];
}
const TheoryAtom& TheoryData::addAtom(Id_t atomOrZero, TheoryAtom::Occurrence occ, Id_t termId, const IdSpan& elems, Id_t op, Id_t rhs) {
	grow(atoms_, atoms_.sz + 1, TheoryAtom::newAtom(atomOrZero, occ, termId, elems, op, rhs));
	return *atoms_.data[atoms_.sz - 1];
}
TheoryTerm& TheoryData::setTerm(Id_t id) {
	if (id >= terms_.sz) {
		grow(terms_, id + 1, TheoryTerm());
	}
	FAIL_IF(terms_.data[id].valid(), "Redefinition of theory term!");
	return terms_.data[id];
}
void TheoryData::setCondition(Id_t elementId, Id_t newCond) {
	FAIL_IF(getElement(elementId).condition() != COND_DEFERRED, "Precondition violated!");
	elems_.data[elementId]->setCondition(newCond);
}

void TheoryData::reset() {
	DestroyT destroy;
	std::for_each(terms_.data, terms_.data + terms_.sz, destroy);
	std::for_each(elems_.data, elems_.data + elems_.sz, destroy);
	std::for_each(atoms_.data, atoms_.data + atoms_.sz, destroy);
	::operator delete(terms_.data);
	::operator delete(elems_.data);
	::operator delete(atoms_.data);
	terms_ = Vec<TheoryTerm>();
	elems_ = Vec<TheoryElement*>();
	atoms_ = Vec<TheoryAtom*>();
	frame_ = Up();
}
void TheoryData::update() {
	frame_.atom = numAtoms();
	frame_.term = terms_.sz;
	frame_.elem = elems_.sz;
}
uint32_t TheoryData::numAtoms() const {
	return atoms_.sz;
}
TheoryData::atom_iterator TheoryData::begin() const { 
	return atoms_.data;
}
TheoryData::atom_iterator TheoryData::currBegin() const { 
	return atoms_.data + frame_.atom; 
}
TheoryData::atom_iterator TheoryData::end() const { 
	return atoms_.data + atoms_.sz;
}
bool TheoryData::hasTerm(Id_t id) const { 
	return id < terms_.sz && terms_.data[id].valid(); 
}
bool TheoryData::isNewTerm(Id_t id) const {
	return hasTerm(id) && id >= frame_.term;
}
bool TheoryData::hasElement(Id_t id) const { 
	return id < elems_.sz && elems_.data[id] != 0; 
}
bool TheoryData::isNewElement(Id_t id) const { 
	return hasElement(id) && id >= frame_.elem;
}
const TheoryTerm& TheoryData::getTerm(Id_t id) const {
	FAIL_IF(!hasTerm(id), "Invalid term id!");
	return terms_.data[id];
}
const TheoryElement& TheoryData::getElement(Id_t id) const {
	FAIL_IF(!hasElement(id), "Invalid element id!");
	return *elems_.data[id];
}

}
