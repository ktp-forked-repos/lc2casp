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

#ifdef WITH_PYTHON

#include <Python.h>

#include "gringo/python.hh"
#include "gringo/version.hh"
#include "gringo/value.hh"
#include "gringo/locatable.hh"
#include "gringo/logger.hh"
#include "gringo/control.hh"
#include <iostream>
#include <sstream>

#if PY_MAJOR_VERSION >= 3
#define PyString_FromString PyUnicode_FromString
#if PY_MINOR_VERSION >= 3
#define PyString_AsString PyUnicode_AsUTF8
#else
#define PyString_AsString _PyUnicode_AsString
#endif
#define PyString_FromStringAndSize PyUnicode_FromStringAndSize
#define PyString_FromFormat PyUnicode_FromFormat
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AsLong PyLong_AsLong
#define PyInt_Check PyLong_Check
#define PyString_Check PyUnicode_Check
#define OBBASE(x) (&(x)->ob_base)
#else
#define OBBASE(x) x
#endif

#ifndef PyVarObject_HEAD_INIT
    #define PyVarObject_HEAD_INIT(type, size) \
        PyObject_HEAD_INIT(type) size,
#endif

#ifdef COUNT_ALLOCS
// tp_allocs, tp_frees, tp_maxalloc, tp_prev, tp_next,
#define GRINGO_STRUCT_EXTRA 0, 0, 0, nullptr, nullptr,
#else
#define GRINGO_STRUCT_EXTRA
#endif

namespace Gringo {

using Id_t = uint32_t;

namespace {

// {{{ workaround for gcc warnings

#if defined(__GNUC__) && !defined(__clang__)
void incRef(PyObject *op) {
    Py_INCREF(op);
}

template <class T>
void incRef(T *object) {
    incRef(reinterpret_cast<PyObject*>(object));
}

#undef Py_INCREF
#define Py_INCREF(op) incRef(op)
#endif

// }}}

// {{{ auxiliary functions and objects

PyObject *pyBool(bool ret) {
    if (ret) { Py_RETURN_TRUE; }
    else     { Py_RETURN_FALSE; }
}

struct Object {
    Object() : obj(nullptr)                               { }
    Object(PyObject *obj, bool inc = false) : obj(obj)    { if (inc) { Py_XINCREF(obj); } }
    Object(Object const &other) : Object(other.obj, true) { }
    Object(Object &&other) : Object(other.obj, false)     { other.obj = nullptr; }
    bool none() const                                     { return obj == Py_None; }
    bool valid() const                                    { return obj; }
    PyObject *get() const                                 { return obj; }
    PyObject *release()                                   { PyObject *ret = obj; obj = nullptr; return ret; }
    PyObject *operator->() const                          { return get(); }
    operator bool() const                                 { return valid(); }
    operator PyObject*() const                            { return get(); }
    Object &operator=(Object const &other)                { Py_XDECREF(obj); obj = other.obj; Py_XINCREF(obj); return *this; }
    ~Object()                                             { Py_XDECREF(obj); }
    PyObject *obj;
};

struct PyUnblock {
    PyUnblock() : state(PyEval_SaveThread()) { }
    ~PyUnblock() { PyEval_RestoreThread(state); }
    PyThreadState *state;
};
template <class T>
auto doUnblocked(T f) -> decltype(f()) {
    PyUnblock b; (void)b;
    return f();
}

struct PyBlock {
    PyBlock() : state(PyGILState_Ensure()) { }
    ~PyBlock() { PyGILState_Release(state); }
    PyGILState_STATE state;
};

Object pyExec(char const *str, char const *filename, PyObject *globals, PyObject *locals = Py_None) {
    if (locals == Py_None) { locals = globals; }
    Object x = Py_CompileString(str, filename, Py_file_input);
    if (!x) { return nullptr; }
#if PY_MAJOR_VERSION >= 3
    return PyEval_EvalCode(x.get(), globals, locals);
#else
    return PyEval_EvalCode((PyCodeObject*)x.get(), globals, locals);
#endif
}

bool pyToVal(Object obj, Value &val);
bool pyToVal(PyObject *obj, Value &val) { return pyToVal({obj, true}, val); }
bool pyToVals(Object obj, ValVec &vals);
bool pyToVals(PyObject *obj, ValVec &vals) { return pyToVals({obj, true}, vals); }

PyObject *valToPy(Value v);
template <class T>
PyObject *valsToPy(T const & vals);

#define PY_TRY try {
#define PY_CATCH(ret) \
} \
catch (std::bad_alloc const &e) { PyErr_SetString(PyExc_MemoryError, e.what()); } \
catch (std::exception const &e) { PyErr_SetString(PyExc_RuntimeError, e.what()); } \
catch (...)                     { PyErr_SetString(PyExc_RuntimeError, "unknown error"); } \
return (ret)

#define CHECK_CMP(a, b, op) \
    if ((a)->ob_type != (b)->ob_type) { \
        if ((a)->ob_type != (b)->ob_type) { \
            return pyBool(false); \
        } \
        else if ((op) == Py_NE && (a)->ob_type != (b)->ob_type) { \
            return pyBool(true); \
        } \
    } \
    if (!checkCmp((a), (b), (op))) { return nullptr; }
template <class T>
bool checkCmp(T *self, PyObject *b, int op) {
    if (b->ob_type == self->ob_type) { return true; }
    else {
        const char *ops = "<";
        switch (op) {
            case Py_LT: { ops = "<";  break; }
            case Py_LE: { ops = "<="; break; }
            case Py_EQ: { ops = "=="; break; }
            case Py_NE: { ops = "!="; break; }
            case Py_GT: { ops = ">";  break; }
            case Py_GE: { ops = ">="; break; }
        }
        PyErr_Format(PyExc_TypeError, "unorderable types: %s() %s %s()", self->ob_type->tp_name, ops, b->ob_type->tp_name);
        return false;
    }
}

template <class T>
PyObject *doCmp(T const &a, T const &b, int op) {
    switch (op) {
        case Py_LT: { return pyBool(a <  b); }
        case Py_LE: { return pyBool(a <= b); }
        case Py_EQ: { return pyBool(a == b); }
        case Py_NE: { return pyBool(a != b); }
        case Py_GT: { return pyBool(a >  b); }
        case Py_GE: { return pyBool(a >= b); }
    }
    return pyBool(false);
}

std::string errorToString() {
    Object type, value, traceback;
    PyErr_Fetch(&type.obj, &value.obj, &traceback.obj);
    if (!type)        { PyErr_Clear(); return "  error during error handling"; }
    PyErr_NormalizeException(&type.obj, &value.obj, &traceback.obj);
    Object tbModule  = PyImport_ImportModule("traceback");
    if (!tbModule)    { PyErr_Clear(); return "  error during error handling"; }
    PyObject *tbDict = PyModule_GetDict(tbModule);
    if (!tbDict)      { PyErr_Clear(); return "  error during error handling"; }
    PyObject *tbFE   = PyDict_GetItemString(tbDict, "format_exception");
    if (!tbFE)        { PyErr_Clear(); return "  error during error handling"; }
    Object ret       = PyObject_CallFunctionObjArgs(tbFE, type.get(), value ? value.get() : Py_None, traceback ? traceback.get() : Py_None, nullptr);
    if (!ret)         { PyErr_Clear(); return "  error during error handling"; }
    Object it        = PyObject_GetIter(ret);
    if (!it)          { PyErr_Clear(); return "  error during error handling"; }
    std::ostringstream oss;
    while (Object line = PyIter_Next(it)) {
        char const *msg = PyString_AsString(line);
        if (!msg) { break; }
        oss << "  " << msg;
    }
    if (PyErr_Occurred()) { PyErr_Clear(); return "  error during error handling"; }
    PyErr_Clear();
    return oss.str();
}
void handleError(Location const &loc, char const *msg) {
    std::string s = errorToString();
    GRINGO_REPORT(E_ERROR)
        << loc << ": error: " << msg << ":\n"
        << s
        ;
    throw std::runtime_error("grounding stopped because of errors");
}

void handleError(char const *loc, char const *msg) {
    Location l(loc, 1, 1, loc, 1, 1);
    handleError(l, msg);
}

template <class T>
struct ObjectBase {
    PyObject_HEAD
    static PyTypeObject type;

    static constexpr reprfunc tp_repr = nullptr;
    static reprfunc tp_str;
    static constexpr hashfunc tp_hash = nullptr;
    static constexpr destructor tp_dealloc = nullptr;
    static constexpr richcmpfunc tp_richcompare = nullptr;
    static constexpr getiterfunc tp_iter = nullptr;
    static constexpr iternextfunc tp_iternext = nullptr;
    static constexpr initproc tp_init = nullptr;
    static constexpr newfunc tp_new = nullptr;
    static constexpr getattrofunc tp_getattro = nullptr;
    static constexpr setattrofunc tp_setattro = nullptr;
    static constexpr PySequenceMethods *tp_as_sequence = nullptr;
    static constexpr PyMappingMethods *tp_as_mapping = nullptr;
    static constexpr PyGetSetDef *tp_getset = nullptr;
    static PyMethodDef tp_methods[];

    static bool initType(PyObject *module) {
        if (PyType_Ready(&type) < 0) { return false; }
        Py_INCREF(&type);
        if (PyModule_AddObject(module, T::tp_type, (PyObject*)&type) < 0) { return false; }
        return true;
    }

    static T *new_() {
        return new_(&type, nullptr, nullptr);
    }

    static T *new_(PyTypeObject *type, PyObject *, PyObject *) {
        T *self;
        self = reinterpret_cast<T*>(type->tp_alloc(type, 0));
        if (!self) { return nullptr; }
        return self;
    }
};

template <class T>
reprfunc ObjectBase<T>::tp_str = reinterpret_cast<reprfunc>(T::tp_repr);

template <class T>
PyMethodDef ObjectBase<T>::tp_methods[] = {{nullptr, nullptr, 0, nullptr}};

template <class T>
PyTypeObject ObjectBase<T>::type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    T::tp_name,                                       // tp_name
    sizeof(T),                                        // tp_basicsize
    0,                                                // tp_itemsize
    reinterpret_cast<destructor>(T::tp_dealloc),      // tp_dealloc
    nullptr,                                          // tp_print
    nullptr,                                          // tp_getattr
    nullptr,                                          // tp_setattr
    nullptr,                                          // tp_compare
    reinterpret_cast<reprfunc>(T::tp_repr),           // tp_repr
    nullptr,                                          // tp_as_number
    T::tp_as_sequence,                                // tp_as_sequence
    T::tp_as_mapping,                                 // tp_as_mapping
    reinterpret_cast<hashfunc>(T::tp_hash),           // tp_hash
    nullptr,                                          // tp_call
    reinterpret_cast<reprfunc>(T::tp_str),            // tp_str
    reinterpret_cast<getattrofunc>(T::tp_getattro),   // tp_getattro
    reinterpret_cast<setattrofunc>(T::tp_setattro),   // tp_setattro
    nullptr,                                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         // tp_flags
    T::tp_doc,                                        // tp_doc
    nullptr,                                          // tp_traverse
    nullptr,                                          // tp_clear
    reinterpret_cast<richcmpfunc>(T::tp_richcompare), // tp_richcompare
    0,                                                // tp_weaklistoffset
    reinterpret_cast<getiterfunc>(T::tp_iter),        // tp_iter
    reinterpret_cast<iternextfunc>(T::tp_iternext),   // tp_iternext
    T::tp_methods,                                    // tp_methods
    nullptr,                                          // tp_members
    T::tp_getset,                                     // tp_getset
    nullptr,                                          // tp_base
    nullptr,                                          // tp_dict
    nullptr,                                          // tp_descr_get
    nullptr,                                          // tp_descr_set
    0,                                                // tp_dictoffset
    reinterpret_cast<initproc>(T::tp_init),           // tp_init
    nullptr,                                          // tp_alloc
    reinterpret_cast<newfunc>(T::tp_new),             // tp_new
    nullptr,                                          // tp_free
    nullptr,                                          // tp_is_gc
    nullptr,                                          // tp_bases
    nullptr,                                          // tp_mro
    nullptr,                                          // tp_cache
    nullptr,                                          // tp_subclasses
    nullptr,                                          // tp_weaklist
    nullptr,                                          // tp_del
    0,                                                // tp_version_tag
    GRINGO_STRUCT_EXTRA
};

template <class T>
struct EnumType : ObjectBase<T> {
    unsigned offset;

    static bool initType(PyObject *module) {
        return  ObjectBase<T>::initType(module) && addAttr() >= 0;
    }
    static PyObject *new_(unsigned offset) {
        EnumType *self;
        self = reinterpret_cast<EnumType*>(ObjectBase<T>::type.tp_alloc(&ObjectBase<T>::type, 0));
        if (!self) { return nullptr; }
        self->offset = offset;
        return reinterpret_cast<PyObject*>(self);
    }

    static PyObject *tp_repr(EnumType *self) {
        return PyString_FromString(T::strings[self->offset]);
    }

    template <class U>
    static PyObject *getAttr(U ret) {
        for (unsigned i = 0; i < sizeof(T::values) / sizeof(*T::values); ++i) {
            if (T::values[i] == ret) {
                PyObject *res = PyDict_GetItemString(ObjectBase<T>::type.tp_dict, T::strings[i]);
                Py_XINCREF(res);
                return res;
            }
        }
        return PyErr_Format(PyExc_RuntimeError, "should not happen");
    }

    static int addAttr() {
        for (unsigned i = 0; i < sizeof(T::values) / sizeof(*T::values); ++i) {
            Object elem(new_(i));
            if (!elem) { return -1; }
            if (PyDict_SetItemString(ObjectBase<T>::type.tp_dict, T::strings[i], elem) < 0) { return -1; }
        }
        return 0;
    }

    static long tp_hash(EnumType *self) {
        return static_cast<long>(self->offset);
    }

    static PyObject *tp_richcompare(EnumType *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(self->offset, reinterpret_cast<EnumType*>(b)->offset, op);
    }
};

// }}}

// {{{ wrap TheoryTerm

struct TheoryTermType : EnumType<TheoryTermType> {
    static constexpr char const *tp_type = "TheoryTermType";
    static constexpr char const *tp_name = "gringo.TheoryTermType";
    static constexpr char const *tp_doc =
R"(Captures the result of a solve call.

TheoryTermType objects cannot be constructed from python. Instead the
following preconstructed objects are available:

TheoryTermType.Function -- a function theor term
TheoryTermType.Number   -- a numeric theory term
TheoryTermType.Symbol   -- a symbolic theory term
TheoryTermType.List     -- a list theory term
TheoryTermType.Tuple    -- a tuple theory term
TheoryTermType.Set      -- a set theory term)";

    static constexpr Gringo::TheoryData::TermType values[] = {
        Gringo::TheoryData::TermType::Function,
        Gringo::TheoryData::TermType::Number,
        Gringo::TheoryData::TermType::Symbol,
        Gringo::TheoryData::TermType::List,
        Gringo::TheoryData::TermType::Tuple,
        Gringo::TheoryData::TermType::Set
    };
    static constexpr const char * strings[] = {
        "Function",
        "Number",
        "Symbol",
        "List",
        "Tuple",
        "Set"
    };
};

constexpr TheoryData::TermType TheoryTermType::values[];
constexpr const char * TheoryTermType::strings[];

struct TheoryTerm : ObjectBase<TheoryTerm> {
    Gringo::TheoryData const *data;
    Id_t value;
    static PyMethodDef tp_methods[];
    static constexpr char const *tp_type = "TheoryTerm";
    static constexpr char const *tp_name = "gringo.TheoryTerm";
    static constexpr char const *tp_doc =
R"(TheoryTerm objects represent theory terms.

This are read-only objects, which can be obtained from theory atoms and
elements.)";

    static PyObject *new_(Gringo::TheoryData const *data, Id_t value) {
        TheoryTerm *self = reinterpret_cast<TheoryTerm*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->value = value;
        self->data = data;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject *name(TheoryTerm *self) {
        PY_TRY
            return PyString_FromString(self->data->termName(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *number(TheoryTerm *self) {
        PY_TRY
            return PyInt_FromLong(self->data->termNum(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *args(TheoryTerm *self) {
        PY_TRY
            Potassco::IdSpan span = self->data->termArgs(self->value);
            Object list = PyList_New(span.size);
            if (!list) { return nullptr; }
            for (size_t i = 0; i < span.size; ++i) {
                Object arg = new_(self->data, *(span.first + i));
                if (!arg) { return nullptr; }
                if (PyList_SetItem(list, i, arg.release()) < 0) { return nullptr; }
            }
            return list.release();
        PY_CATCH(nullptr);
    }
    static PyObject *tp_repr(TheoryTerm *self) {
        PY_TRY
            return PyString_FromString(self->data->termStr(self->value).c_str());
        PY_CATCH(nullptr);
    }
    static PyObject *termType(TheoryTerm *self) {
        PY_TRY
            return TheoryTermType::getAttr(self->data->termType(self->value));
        PY_CATCH(nullptr);
    }
    static long tp_hash(TheoryTerm *self) {
        return self->value;
    }
    static PyObject *tp_richcompare(TheoryTerm *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(self->value, reinterpret_cast<TheoryTerm*>(b)->value, op);
    }
};

PyMethodDef TheoryTerm::tp_methods[] = {
    {"type", (PyCFunction)TheoryTerm::termType, METH_NOARGS, "type(self) -> gringo.TheoryTermType object\n\nReturn the type of the theory term."},
    {"name", (PyCFunction)TheoryTerm::name, METH_NOARGS, "name(self) -> string object\n\nReturn the name of the TheoryTerm\n(for symbols and functions)."},
    {"args", (PyCFunction)TheoryTerm::args, METH_NOARGS, "args(self) -> list object\n\nReturn the arguments of the TheoryTerm\n(for functions, tuples, list, and sets)."},
    {"number", (PyCFunction)TheoryTerm::number, METH_NOARGS, "number(self) -> integer\n\nReturn the numeric representation of the TheoryTerm\n(for numbers)."},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap TheoryElement

struct TheoryElement : ObjectBase<TheoryElement> {
    Gringo::TheoryData const *data;
    Id_t value;
    static constexpr char const *tp_type = "TheoryElement";
    static constexpr char const *tp_name = "gringo.TheoryElement";
    static constexpr char const *tp_doc =
R"(TheoryElement objects represent theory elements which consist of a tuple of
terms and a set of literals.)";
    static PyMethodDef tp_methods[];
    static PyObject *new_(Gringo::TheoryData const *data, Id_t value) {
        TheoryElement *self;
        self = reinterpret_cast<TheoryElement*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->value = value;
        self->data = data;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject *terms(TheoryElement *self) {
        PY_TRY
            Potassco::IdSpan span = self->data->elemTuple(self->value);
            Object list = PyList_New(span.size);
            if (!list) { return nullptr; }
            for (size_t i = 0; i < span.size; ++i) {
                Object arg = TheoryTerm::new_(self->data, *(span.first + i));
                if (!arg) { return nullptr; }
                if (PyList_SetItem(list, i, arg.release()) < 0) { return nullptr; }
            }
            return list.release();
        PY_CATCH(nullptr);
    }
    static PyObject *condition(TheoryElement *self) {
        PY_TRY
            Potassco::LitSpan span = self->data->elemCond(self->value);
            Object list = PyList_New(span.size);
            if (!list) { return nullptr; }
            for (size_t i = 0; i < span.size; ++i) {
                Object arg = PyInt_FromLong(*(span.first + i));
                if (!arg) { return nullptr; }
                if (PyList_SetItem(list, i, arg.release()) < 0) { return nullptr; }
            }
            return list.release();
        PY_CATCH(nullptr);
    }
    static PyObject *condition_literal(TheoryElement *self) {
        PY_TRY
            return PyInt_FromLong(self->data->elemCondLit(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *tp_repr(TheoryElement *self) {
        PY_TRY
            return PyString_FromString(self->data->elemStr(self->value).c_str());
        PY_CATCH(nullptr);
    }
    static long tp_hash(TheoryElement *self) {
        return self->value;
    }
    static PyObject *tp_richcompare(TheoryElement *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(self->value, reinterpret_cast<TheoryElement*>(b)->value, op);
    }
};

PyMethodDef TheoryElement::tp_methods[] = {
    {"terms", (PyCFunction)terms, METH_NOARGS, "terms(self) -> list object\n\nReturn the tuple of the element."},
    {"condition", (PyCFunction)condition, METH_NOARGS, "condition(self) -> list object\n\nReturn the conditon of the element."},
    {"condition_literal", (PyCFunction)condition_literal, METH_NOARGS, "condition_literal(self) -> int\n\nReturn the single program literal associated with the condition."},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap TheoryAtom

struct TheoryAtomType : EnumType<TheoryAtomType> {
    static constexpr Gringo::TheoryData::AtomType values[] = {
        Gringo::TheoryData::AtomType::Head,
        Gringo::TheoryData::AtomType::Body,
        Gringo::TheoryData::AtomType::Directive
    };
    static constexpr const char * strings[] = {
        "Head",
        "Body",
        "Directive"
    };
    static constexpr char const *tp_type = "TheoryAtomType";
    static constexpr char const *tp_name = "gringo.TheoryAtomType";
    static constexpr char const *tp_doc =
R"(Captures the result of a solve call.

TheoryTermType objects cannot be constructed from python. Instead the
following preconstructed objects are available:

TheoryTermType.Head      -- head occurrence
TheoryTermType.Body      -- body occurrence
TheoryTermType.Directive -- directive without atom)";
};

constexpr Gringo::TheoryData::AtomType TheoryAtomType::values[];
constexpr const char * TheoryAtomType::strings[];

struct TheoryAtom : ObjectBase<TheoryAtom> {
    Gringo::TheoryData const *data;
    Id_t value;
    static PyMethodDef tp_methods[];
    static constexpr char const *tp_type = "TheoryAtom";
    static constexpr char const *tp_name = "gringo.TheoryAtom";
    static constexpr char const *tp_doc =
R"(TheoryAtom objects represent theory atoms.)";
    static PyObject *new_(Gringo::TheoryData const *data, Id_t value) {
        TheoryAtom *self;
        self = reinterpret_cast<TheoryAtom*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->value = value;
        self->data = data;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject *elements(TheoryAtom *self) {
        PY_TRY
            Potassco::IdSpan span = self->data->atomElems(self->value);
            Object list = PyList_New(span.size);
            if (!list) { return nullptr; }
            for (size_t i = 0; i < span.size; ++i) {
                Object arg = TheoryElement::new_(self->data, *(span.first + i));
                if (!arg) { return nullptr; }
                if (PyList_SetItem(list, i, arg.release()) < 0) { return nullptr; }
            }
            return list.release();
        PY_CATCH(nullptr);
    }
    static PyObject *term(TheoryTerm *self) {
        PY_TRY
            return TheoryTerm::new_(self->data, self->data->atomTerm(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *literal(TheoryTerm *self) {
        PY_TRY
            return PyInt_FromLong(self->data->atomLit(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *guard(TheoryTerm *self) {
        PY_TRY
            if (!self->data->atomHasGuard(self->value)) { Py_RETURN_NONE; }
            std::pair<char const *, Id_t> guard = self->data->atomGuard(self->value);
            Object tuple = PyTuple_New(2);
            if (!tuple) { return nullptr; }
            Object op = PyString_FromString(guard.first);
            if (!op) { return nullptr; }
            if (PyTuple_SetItem(tuple, 0, op.release()) < 0) { return nullptr; }
            Object term = TheoryTerm::new_(self->data, guard.second);
            if (!term) { return nullptr; }
            if (PyTuple_SetItem(tuple, 0, term.release()) < 0) { return nullptr; }
            return tuple.release();
        PY_CATCH(nullptr);
    }
    static PyObject *atomType(TheoryTerm *self) {
        PY_TRY
            return TheoryAtomType::getAttr(self->data->atomType(self->value));
        PY_CATCH(nullptr);
    }
    static PyObject *tp_repr(TheoryAtom *self) {
        PY_TRY
            return PyString_FromString(self->data->atomStr(self->value).c_str());
        PY_CATCH(nullptr);
    }
    static long tp_hash(TheoryAtom *self) {
        return self->value;
    }
    static PyObject *tp_richcompare(TheoryAtom *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(self->value, reinterpret_cast<TheoryAtom*>(b)->value, op);
    }
};

PyMethodDef TheoryAtom::tp_methods[] = {
    {"elements", (PyCFunction)elements, METH_NOARGS, "elements(self) -> list object\n\nReturn the theory elements of the theory atom."},
    {"term", (PyCFunction)term, METH_NOARGS, "term(self) -> TheoryTerm object\n\nReturn the term of the theory atom."},
    {"guard", (PyCFunction)guard, METH_NOARGS, "guard(self) -> (str object, TheoryTerm object)\n\nReturn the guard of the theory atom or None if the atom has no guard."},
    {"literal", (PyCFunction)literal, METH_NOARGS, "literal(self) -> (str object, TheoryTerm object)\n\nReturn the aspif literal associated with the theory atom."},
    {"type", (PyCFunction)atomType, METH_NOARGS, "type(self) -> TheoryAtomType\n\nReturn the type of the atom."},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap TheoryIter

struct TheoryIter : ObjectBase<TheoryIter> {
    Gringo::TheoryData const *data;
    Id_t offset;
    static PyMethodDef tp_methods[];

    static constexpr char const *tp_type = "TheoryIter";
    static constexpr char const *tp_name = "gringo.TheoryIter";
    static constexpr char const *tp_doc =
R"(Object to iterate over all theory atoms.)";
    static PyObject *new_(Gringo::TheoryData const *data, Id_t offset) {
        TheoryIter *self;
        self = reinterpret_cast<TheoryIter*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->data = data;
        self->offset = offset;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject* tp_iter(PyObject *self) {
        Py_INCREF(self);
        return self;
    }
    static PyObject* get(TheoryIter *self) {
        return TheoryAtom::new_(self->data, self->offset);
    }
    static PyObject* tp_iternext(TheoryIter *self) {
        PY_TRY
            if (self->offset < self->data->numAtoms()) {
                Object next = get(self);
                ++self->offset;
                return next.release();
            } else {
                PyErr_SetNone(PyExc_StopIteration);
                return nullptr;
            }
        PY_CATCH(nullptr);
    }
    static PyObject *enter(TheoryIter *self) {
        Py_INCREF(self);
        return (PyObject*)self;
    }
    static PyObject *exit(TheoryIter *, PyObject *) {
        return pyBool(false);
    }
};

PyMethodDef TheoryIter::tp_methods[] = {
    {"__enter__", (PyCFunction)enter, METH_NOARGS,
R"(__enter__(self) -> TheoryIter

Returns self.)"},
    {"get", (PyCFunction)get, METH_NOARGS,
R"(get(self) -> TheoryAtom)"},
    {"__exit__", (PyCFunction)exit, METH_VARARGS,
R"(__exit__(self, type, value, traceback) -> Boolean

Follows python __exit__ conventions. Does not suppress exceptions.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap Fun

struct Fun : ObjectBase<Fun> {
    Value val;
    static PyMethodDef tp_methods[];
    static constexpr char const *tp_type = "Fun";
    static constexpr char const *tp_name = "gringo.Fun";
    static constexpr char const *tp_doc =
R"(Fun(name, args) -> Fun object

Represents a gringo function term.

This also includes symbolic terms, which have to be created by either omitting
the arguments or passing an empty sequence.

Arguments:
name -- string representing the name of the function symbol
        (must follow gringo's identifier syntax)
args -- optional sequence of terms representing the arguments of the function
        symbol (Default: [])

Fun objects are ordered like in gringo and their string representation
corresponds to their gringo representation.)";

    static PyObject *tp_new(PyTypeObject *type, PyObject *, PyObject *) {
        Fun *self;
        self = reinterpret_cast<Fun*>(type->tp_alloc(type, 0));
        if (!self) { return nullptr; }
        self->val = Value();
        return reinterpret_cast<PyObject*>(self);
    }

    static int tp_init(Fun *self, PyObject *args, PyObject *kwds) {
        PY_TRY
            static char const *kwlist[] = {"name", "args", nullptr};
            char const *name;
            PyObject *params = nullptr;
            if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O", const_cast<char**>(kwlist), &name, &params)) { return -1; }
            if (strcmp(name, "") == 0) {
                PyErr_SetString(PyExc_RuntimeError, "The name of a Fun object must not be empty");
                return -1;
            }
            if (params) {
                ValVec vals;
                if (!pyToVals(params, vals)) { return -1; }
                self->val = vals.empty() ? Value::createId(name) : Value::createFun(name, vals);
            }
            else {
                self->val = Value::createId(name);
            }
            return 0;
        PY_CATCH(-1);
    }

    static PyObject *name(Fun *self) {
        // Note: should not throw
        return PyString_FromString((*FWString(self->val.name())).c_str());
    }

    static PyObject *args(Fun *self) {
        if (self->val.type() == Value::FUNC) {
            // Note: should not throw
            return valsToPy(self->val.args());
        }
        else {
            ValVec vals;
            return valsToPy(vals);
        }
    }

    static PyObject *tp_repr(Fun *self) {
        PY_TRY
            std::ostringstream oss;
            oss << self->val;
            return PyString_FromString(oss.str().c_str());
        PY_CATCH(nullptr);
    }

    static long tp_hash(Fun *self) {
        return self->val.hash();
    }
    static PyObject *tp_richcompare(Fun *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        // Note: should not throw
        return doCmp(self->val, reinterpret_cast<Fun*>(b)->val, op);
    }
};

PyMethodDef Fun::tp_methods[] = {
    {"name", (PyCFunction)Fun::name, METH_NOARGS, "name(self) -> string object\n\nReturn the name of the Fun object."},
    {"args", (PyCFunction)Fun::args, METH_NOARGS, "args(self) -> list object\n\nReturn the arguments of the Fun object."},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap SupType

struct SupType : ObjectBase<SupType> {
    static PyObject *sup;
    static constexpr char const *tp_type = "SupType";
    static constexpr char const *tp_name = "gringo.SupType";
    static constexpr char const *tp_doc =
R"(SupType object

Represents a gringo #sup term.

SupType objects cannot be created and there is just one instance: Sup. They are
ordered like in gringo and their string representation corresponds to their
gringo representation.)";

    static bool initType(PyObject *module) {
        if (!ObjectBase<SupType>::initType(module)) { return false; }
        sup = type.tp_alloc(&type, 0);
        if (!sup) { return false; }
        if (PyModule_AddObject(module, "Sup", sup) < 0) { return false; }
        return true;
    }

    static PyObject *tp_repr(SupType *, PyObject *, PyObject *) {
        return PyString_FromString("#sup");
    }

    static PyObject *tp_richcompare(SupType *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(0, 0, op);
    }

    static long tp_hash(SupType *) {
        return Value::createSup().hash();
    }

};

PyObject *SupType::sup = nullptr;

// }}}
// {{{ wrap InfType

struct InfType : ObjectBase<InfType> {
    static PyObject *inf;
    static constexpr char const *tp_type = "InfType";
    static constexpr char const *tp_name = "gringo.InfType";
    static constexpr char const *tp_doc =
R"(InfType object

Represents a gringo #inf term.

InfType objects cannot be created and there is just one instance: Inf. They are
ordered like in gringo and their string representation corresponds to their
gringo representation.)";

    static bool initType(PyObject *module) {
        if (!ObjectBase<InfType>::initType(module)) { return false; }
        inf = type.tp_alloc(&type, 0);
        if (!inf) { return false; }
        if (PyModule_AddObject(module, "Inf", inf) < 0) { return false; }
        return true;
    }

    static PyObject *tp_repr(InfType *, PyObject *, PyObject *) {
        return PyString_FromString("#inf");
    }

    static PyObject *tp_richcompare(InfType *self, PyObject *b, int op) {
        CHECK_CMP(OBBASE(self), b, op)
        return doCmp(0, 0, op);
    }

    static long tp_hash(InfType *) {
        return Value::createInf().hash();
    }

};

PyObject *InfType::inf = nullptr;

// }}}
// {{{ wrap SolveResult

struct SolveResult : EnumType<SolveResult> {
    static constexpr Gringo::SolveResult values[] = {
        Gringo::SolveResult::SAT,
        Gringo::SolveResult::UNSAT,
        Gringo::SolveResult::UNKNOWN,
    };
    static constexpr const char * strings[] = {
        "SAT",
        "UNSAT",
        "UNKNOWN"
    };
    static constexpr char const * tp_type = "SolveResult";
    static constexpr char const * tp_name = "gringo.SolveResult";
    static constexpr char const * tp_doc =
R"(Captures the result of a solve call.

SolveResult objects cannot be constructed from python. Instead the
preconstructed objects SolveResult.SAT, SolveResult.UNSAT, and
SolveResult.UNKNOWN have to be used.

SolveResult.SAT     -- solve call during which at least one model has been found.
SolveResult.UNSAT   -- solve call during which no model has been found.
SolveResult.UNKNOWN -- an canceled solve call - e.g. by SolveFuture.cancel,
                       or a signal)";
};

constexpr Gringo::SolveResult SolveResult::values[];
constexpr const char * SolveResult::strings[];

// }}}
// {{{ wrap Statistics

PyObject *getStatistics(Statistics const *stats, char const *prefix) {
    PY_TRY
        Statistics::Quantity ret = stats->getStat(prefix);
        switch (ret.error()) {
            case Statistics::error_none: {
                double val = ret;
                return val == (int)val ? PyLong_FromDouble(val) : PyFloat_FromDouble(val);
            }
            case Statistics::error_not_available: {
                return PyErr_Format(PyExc_RuntimeError, "error_not_available: %s", prefix);
            }
            case Statistics::error_unknown_quantity: {
                return PyErr_Format(PyExc_RuntimeError, "error_unknown_quantity: %s", prefix);
            }
            case Statistics::error_ambiguous_quantity: {
                char const *keys = stats->getKeys(prefix);
                if (!keys) { return PyErr_Format(PyExc_RuntimeError, "error zero keys string: %s", prefix); }
                if (strcmp(keys, "__len") == 0) {
                    std::string lenPrefix;
                    lenPrefix += prefix;
                    lenPrefix += "__len";
                    int len = (int)(double)stats->getStat(lenPrefix.c_str());
                    Object list = PyList_New(len);
                    if (!list) { return nullptr; }
                    for (int i = 0; i < len; ++i) {
                        Object objPrefix = PyString_FromFormat("%s%d.", prefix, i);
                        if (!objPrefix) { return nullptr; }
                        char const *subPrefix = PyString_AsString(objPrefix);
                        if (!subPrefix) { return nullptr; }
                        Object subStats = getStatistics(stats, subPrefix);
                        if (!subStats) { return nullptr; }
                        if (PyList_SetItem(list, i, subStats.release()) < 0) { return nullptr; }
                    }
                    return list.release();
                }
                else {
                    Object dict = PyDict_New();
                    if (!dict) { return nullptr; }
                    for (char const *it = keys; *it; it+= strlen(it) + 1) {
                        int len = strlen(it);
                        Object key = PyString_FromStringAndSize(it, len - (it[len-1] == '.'));
                        if (!key) { return nullptr; }
                        Object objPrefix = PyString_FromFormat("%s%s", prefix, it);
                        if (!objPrefix) { return nullptr; }
                        char const *subPrefix = PyString_AsString(objPrefix);
                        if (!subPrefix) { return nullptr; }
                        Object subStats = getStatistics(stats, subPrefix);
                        if (!subStats) { return nullptr; }
                        if (PyDict_SetItem(dict, key, subStats) < 0) { return nullptr; }
                    }
                    return dict.release();
                }
            }
        }
        return PyErr_Format(PyExc_RuntimeError, "error unhandled prefix: %s", prefix);
    PY_CATCH(nullptr);
}

// }}}
// {{{ wrap SolveControl

struct SolveControl : ObjectBase<SolveControl> {
    Gringo::Model const *model;
    static PyMethodDef tp_methods[];
    static constexpr char const *tp_type = "SolveControl";
    static constexpr char const *tp_name = "gringo.SolveControl";
    static constexpr char const *tp_doc =
R"(Object that allows for controlling a running search.

Note that SolveControl objects cannot be constructed from python.  Instead
they are available as properties of Model objects.)";

    static PyObject *new_(Gringo::Model const &model) {
        SolveControl *self;
        self = reinterpret_cast<SolveControl*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->model = &model;
        return reinterpret_cast<PyObject*>(self);
    }

    static PyObject *getClause(SolveControl *self, PyObject *pyLits, bool invert) {
        PY_TRY
            Object it = PyObject_GetIter(pyLits);
            if (!it) { return nullptr; }
            Gringo::Model::LitVec lits;
            while (Object pyPair = PyIter_Next(it)) {
                Object pyPairIt = PyObject_GetIter(pyPair);
                if (!pyPairIt) { return nullptr; }
                Object pyAtom = PyIter_Next(pyPairIt);
                if (!pyAtom) { return PyErr_Occurred() ? nullptr : PyErr_Format(PyExc_RuntimeError, "tuple of atom and boolean expected"); }
                Object pyBool = PyIter_Next(pyPairIt);
                if (!pyBool) { return PyErr_Occurred() ? nullptr : PyErr_Format(PyExc_RuntimeError, "tuple of atom and boolean expected"); }
                Value atom;
                if (!pyToVal(pyAtom, atom)) { return nullptr; }
                int truth = PyObject_IsTrue(pyBool);
                if (truth == -1) { return nullptr; }
                lits.emplace_back(bool(truth) ^ invert, atom);
            }
            if (PyErr_Occurred()) { return nullptr; }
            self->model->addClause(lits);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }

    static PyObject *add_clause(SolveControl *self, PyObject *pyLits) {
        return getClause(self, pyLits, false);
    }

    static PyObject *add_nogood(SolveControl *self, PyObject *pyLits) {
        return getClause(self, pyLits, true);
    }
};

PyMethodDef SolveControl::tp_methods[] = {
    // add_clause
    {"add_clause", (PyCFunction)add_clause, METH_O,
R"(add_clause(self, lits) -> None

Adds a clause to the solver during the search.

Arguments:
lits -- A list of literals represented as pairs of atoms and Booleans
        representing the clause.

Note that this function can only be called in the model callback (or while
iterating when using a SolveIter).)"},
    // add_nogood
    {"add_nogood", (PyCFunction)add_nogood, METH_O,
R"(add_nogood(self, lits) -> None

Equivalent to add_clause with the literals inverted.

Arguments:
lits -- A list of pairs of Booleans and atoms representing the nogood.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap Model

struct Model : ObjectBase<Model> {
    Gringo::Model const *model;
    static PyMethodDef tp_methods[];
    static PyGetSetDef tp_getset[];

    static constexpr char const *tp_type = "Model";
    static constexpr char const *tp_name = "gringo.Model";
    static constexpr char const *tp_doc =
R"(Provides access to a model during a solve call.

Note that model objects cannot be constructed from python.  Instead they are
passed as argument to a model callback (see Control.solve and
Control.solve_async).  Furthermore, the lifetime of a model object is limited
to the scope of the callback. They must not be stored for later use in other
places like - e.g., the main function.)";

    static bool initType(PyObject *module) {
        return ObjectBase<Model>::initType(module) && addAttr() >= 0;
    }

    static PyObject *new_(Gringo::Model const &model) {
        Model *self;
        self = reinterpret_cast<Model*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->model = &model;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject *contains(Model *self, PyObject *arg) {
        PY_TRY
            Value val;
            if(!pyToVal(arg, val)) { return nullptr; }
            return pyBool(self->model->contains(val));
        PY_CATCH(nullptr);
    }
    static PyObject *atoms(Model *self, PyObject *args) {
        PY_TRY
            int atomset = Gringo::Model::SHOWN;
            if (!PyArg_ParseTuple(args, "|i", &atomset)) { return nullptr; }
            ValVec const &vals = self->model->atoms(atomset);
            Object list = PyList_New(vals.size());
            if (!list) { return nullptr; }
            int i = 0;
            for (auto x : vals) {
                Object val(valToPy(x));
                if (!val) { return nullptr; }
                if (PyList_SetItem(list, i, val.release()) < 0) { return nullptr; }
                ++i;
            }
            return list.release();
        PY_CATCH(nullptr);
    }
    static PyObject *optimization(Model *self) {
        Int64Vec values(self->model->optimization());
        Object list = PyList_New(values.size());
        if (!list) { return nullptr; }
        int i = 0;
        for (auto x : values) {
            Object val = PyInt_FromLong(x);
            if (!val) { return nullptr; }
            if (PyList_SetItem(list, i, val.release()) < 0) { return nullptr; }
            ++i;
        }
        return list.release();
    }
    static PyObject *tp_repr(Model *self, PyObject *) {
        PY_TRY
            auto printAtom = [](std::ostream &out, Value val) {
                if (val.type() == Value::FUNC && *val.sig() == Signature("$", 2)) { out << val.args().front() << "=" << val.args().back(); }
                else { out << val; }
            };
            std::ostringstream oss;
            print_comma(oss, self->model->atoms(Gringo::Model::SHOWN), " ", printAtom);
            return PyString_FromString(oss.str().c_str());
        PY_CATCH(nullptr);
    }
    static int addAttr() {
        Object csp(PyInt_FromLong(Gringo::Model::CSP));
        if (!csp) { return -1; }
        if (PyDict_SetItemString(type.tp_dict, "CSP", csp) < 0) { return -1; }
        Object atoms(PyInt_FromLong(Gringo::Model::ATOMS));
        if (!atoms) { return -1; }
        if (PyDict_SetItemString(type.tp_dict, "ATOMS", atoms) < 0) { return -1; }
        Object terms(PyInt_FromLong(Gringo::Model::TERMS));
        if (!terms) { return -1; }
        if (PyDict_SetItemString(type.tp_dict, "TERMS", terms) < 0) { return -1; }
        Object shown(PyInt_FromLong(Gringo::Model::SHOWN));
        if (!shown) { return -1; }
        if (PyDict_SetItemString(type.tp_dict, "SHOWN", shown) < 0) { return -1; }
        Object comp(PyInt_FromLong(Gringo::Model::COMP));
        if (!comp) { return -1; }
        if (PyDict_SetItemString(type.tp_dict, "COMP", comp) < 0) { return -1; }
        return 0;
    }
    static PyObject *getContext(Model *self, void *) {
        return SolveControl::new_(*self->model);
    }
};

PyGetSetDef Model::tp_getset[] = {
    {(char*)"context", (getter)getContext, nullptr, (char*)"SolveControl object that allows for controlling the running search.", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyMethodDef Model::tp_methods[] = {
    {"atoms", (PyCFunction)atoms, METH_VARARGS,
R"(atoms(self, atomset=SHOWN) -> list of terms

Return the list of atoms, terms, or CSP assignments in the model.

Argument atomset is a bitset to select what kind of atoms or terms are returned:
Model.ATOMS -- selects all atoms in the model (independent of #show statements)
Model.TERMS -- selects all terms displayed with #show statements in the model
Model.SHOWN -- selects all atoms and terms as outputted by clingo's default
               output format
Model.CSP   -- selects all csp assignments (independent of #show statements)
Model.COMP  -- return the complement of the answer set w.r.t. to the Herbrand
               base accumulated so far (does not affect csp assignments)

The string representation of a model object is similar to the output of models
by clingo using the default output.

Note that atoms are represented using Fun objects, and that CSP assignments are
represented using function symbols with name "$" where the first argument is
the name of the CSP variable and the second its value.)"},
    {"contains", (PyCFunction)contains, METH_O,
R"(contains(self, a) -> Boolean

Returns true if atom a is contained in the model.

Atom a must be represented using a Fun term.)"},
    {"optimization", (PyCFunction)optimization, METH_NOARGS,
R"(optimization(self) -> [int]

Returns the list of optimization values of the model. This corresponds to
clasp's optimization output.)"},
    {nullptr, nullptr, 0, nullptr}

};

// }}}
// {{{ wrap SolveFuture

struct SolveFuture : ObjectBase<SolveFuture> {
    Gringo::SolveFuture *future;
    PyObject *mh;
    PyObject *fh;

    static PyMethodDef tp_methods[];
    static constexpr char const *tp_type = "SolveFuture";
    static constexpr char const *tp_name = "gringo.SolveFuture";
    static constexpr char const *tp_doc =
R"(Handle for asynchronous solve calls.

SolveFuture objects cannot be created from python. Instead they are returned by
Control.solve_async, which performs a search in the background.  A SolveFuture
object can be used to wait for such a background search or cancel it.

Functions in this object release the GIL. They are not thread-safe though.

See Control.solve_async for an example.)";

    static PyObject *new_(Gringo::SolveFuture &future, PyObject *mh, PyObject *fh) {
        SolveFuture *self;
        self = reinterpret_cast<SolveFuture*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->future = &future;
        self->mh = mh;
        self->fh = fh;
        Py_XINCREF(self->mh);
        Py_XINCREF(self->fh);
        return reinterpret_cast<PyObject*>(self);
    }

    static void tp_dealloc(SolveFuture *self) {
        Py_XDECREF(self->mh);
        Py_XDECREF(self->fh);
        type.tp_free(self);
    }

    static PyObject *get(SolveFuture *self, PyObject *) {
        PY_TRY
            return SolveResult::getAttr(doUnblocked([self]() { return self->future->get(); }));
        PY_CATCH(nullptr);
    }

    static PyObject *wait(SolveFuture *self, PyObject *args) {
        PY_TRY
            PyObject *timeout = nullptr;
            if (!PyArg_ParseTuple(args, "|O", &timeout)) { return nullptr; }
            if (!timeout) {
                doUnblocked([self](){ self->future->wait(); });
                Py_RETURN_NONE;
            }
            else {
                double time = PyFloat_AsDouble(timeout);
                if (PyErr_Occurred()) { return nullptr; }
                return pyBool(doUnblocked([self, time](){ return self->future->wait(time); }));
            }
        PY_CATCH(nullptr);
    }

    static PyObject *cancel(SolveFuture *self, PyObject *) {
        PY_TRY
            doUnblocked([self](){ self->future->cancel(); });
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
};

PyMethodDef SolveFuture::tp_methods[] = {
    {"get", (PyCFunction)get, METH_NOARGS,
R"(get(self) -> SolveResult object

Get the result of an solve_async call. If the search is not completed yet, the
function blocks until the result is ready.)"},
    {"wait", (PyCFunction)wait,  METH_VARARGS,
R"(wait(self, timeout) -> None or Boolean

Wait for solve_async call to finish. If a timeout is given, the function waits
at most timeout seconds and returns a Boolean indicating whether the search has
finished. Otherwise, the function blocks until the search is finished and
returns nothing.

Arguments:
timeout -- optional timeout in seconds
           (permits floating point values))"},
    {"cancel", (PyCFunction)cancel, METH_NOARGS,
R"(cancel(self) -> None

Interrupts the running search.

Note that unlike other functions of this class, this function can safely be
called from other threads.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap SolveIter

struct SolveIter : ObjectBase<SolveIter> {
    Gringo::SolveIter *solve_iter;
    static PyMethodDef tp_methods[];

    static constexpr char const *tp_type = "SolveIter";
    static constexpr char const *tp_name = "gringo.SolveIter";
    static constexpr char const *tp_doc =
R"(Object to conveniently iterate over all models.

During solving the GIL is released. The functions in this object are not
thread-safe though.)";

    static PyObject *new_(Gringo::SolveIter &iter) {
        SolveIter *self;
        self = reinterpret_cast<SolveIter*>(type.tp_alloc(&type, 0));
        if (!self) { return nullptr; }
        self->solve_iter = &iter;
        return reinterpret_cast<PyObject*>(self);
    }
    static PyObject* tp_iter(PyObject *self) {
        Py_INCREF(self);
        return self;
    }
    static PyObject* get(SolveIter *self) {
        PY_TRY
            return SolveResult::getAttr(doUnblocked([self]() { return self->solve_iter->get(); }));
        PY_CATCH(nullptr);
    }
    static PyObject* tp_iternext(SolveIter *self) {
        PY_TRY
            if (Gringo::Model const *m = doUnblocked([self]() { return self->solve_iter->next(); })) {
                return Model::new_(*m);
            } else {
                PyErr_SetNone(PyExc_StopIteration);
                return nullptr;
            }
        PY_CATCH(nullptr);
    }
    static PyObject *enter(SolveIter *self) {
        Py_INCREF(self);
        return (PyObject*)self;
    }
    static PyObject *exit(SolveIter *self, PyObject *) {
        PY_TRY
            doUnblocked([self]() { return self->solve_iter->close(); });
            return pyBool(false);
        PY_CATCH(nullptr);
    }
};

PyMethodDef SolveIter::tp_methods[] = {
    {"__enter__",      (PyCFunction)enter,      METH_NOARGS,
R"(__exit__(self) -> SolveIter

Returns self.)"},
    {"get",            (PyCFunction)get,        METH_NOARGS,
R"(get(self) -> SolveResult

Returns the result of the search. Note that this might start a search for the
next model and then returns a result accordingly. The function might be called
after iteration to check if the search has been canceled.)"},
    {"__exit__",       (PyCFunction)exit,       METH_VARARGS,
R"(__exit__(self, type, value, traceback) -> Boolean

Follows python __exit__ conventions. Does not suppress exceptions.

Stops the current search. It is necessary to call this method after each search.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap ConfigProxy

struct ConfigProxy : ObjectBase<ConfigProxy> {
    unsigned key;
    int nSubkeys;
    int arrLen;
    int nValues;
    char const* help;
    Gringo::ConfigProxy *proxy;
    static PyMethodDef tp_methods[];
    static PySequenceMethods tp_as_sequence[];

    static constexpr char const *tp_type = "ConfigProxy";
    static constexpr char const *tp_name = "gringo.ConfigProxy";
    static constexpr char const *tp_doc =
R"(Proxy object that allows for changing the configuration of the
underlying solver.

Options are organized hierarchically. To change and inspect an option use:

  proxy.group.subgroup.option = "value"
  value = proxy.group.subgroup.option

There are also arrays of option groups that can be accessed using integer
indices:

  proxy.group.subgroup[0].option = "value1"
  proxy.group.subgroup[1].option = "value2"

To list the subgroups of an option group, use the keys() method. Array option
groups, like solver, have have a non-negative length and can be iterated.
Furthermore, there are meta options having key "configuration". Assigning a
meta option sets a number of related options.  To get further information about
an option or option group <opt>, use property __desc_<opt> to retrieve a
description.

Example:

#script (python)
import gringo

def main(prg):
    prg.conf.solve.models = 0
    prg.ground([("base", [])])
    prg.solve()

#end.

{a; c}.

Expected Answer Sets:

{ {}, {a}, {c}, {a,c} })";

    static PyObject *new_(unsigned key, Gringo::ConfigProxy &proxy) {
        PY_TRY
            Object ret(type.tp_alloc(&type, 0));
            if (!ret) { return nullptr; }
            ConfigProxy *self = reinterpret_cast<ConfigProxy*>(ret.get());
            self->proxy = &proxy;
            self->key   = key;
            self->proxy->getKeyInfo(self->key, &self->nSubkeys, &self->arrLen, &self->help, &self->nValues);
            return ret.release();
        PY_CATCH(nullptr);
    }

    static PyObject *keys(ConfigProxy *self) {
        PY_TRY
            if (self->nSubkeys < 0) { Py_RETURN_NONE; }
            else {
                Object list = PyList_New(self->nSubkeys);
                if (!list) { return nullptr; }
                for (int i = 0; i < self->nSubkeys; ++i) {
                    char const *key = self->proxy->getSubKeyName(self->key, i);
                    Object pyString = PyString_FromString(key);
                    if (!pyString) { return nullptr; }
                    if (PyList_SetItem(list, i, pyString.release()) < 0) { return nullptr; }
                }
                return list.release();
            }
        PY_CATCH(nullptr);
    }

    static PyObject *tp_getattro(ConfigProxy *self, PyObject *name) {
        PY_TRY
            char const *current = PyString_AsString(name);
            if (!current) { return nullptr; }
            bool desc = strncmp("__desc_", current, 7) == 0;
            if (desc) { current += 7; }
            unsigned key;
            if (self->proxy->hasSubKey(self->key, current, &key)) {
                Object subKey(new_(key, *self->proxy));
                if (!subKey) { return nullptr; }
                ConfigProxy *sub = reinterpret_cast<ConfigProxy*>(subKey.get());
                if (desc) { return PyString_FromString(sub->help); }
                else if (sub->nValues < 0) { return subKey.release(); }
                else {
                    std::string value;
                    if (!sub->proxy->getKeyValue(sub->key, value)) { Py_RETURN_NONE; }
                    return PyString_FromString(value.c_str());
                }
            }
            return PyObject_GenericGetAttr(reinterpret_cast<PyObject*>(self), name);
        PY_CATCH(nullptr);
    }

    static int tp_setattro(ConfigProxy *self, PyObject *name, PyObject *pyValue) {
        PY_TRY
            char const *current = PyString_AsString(name);
            if (!current) { return -1; }
            unsigned key;
            if (self->proxy->hasSubKey(self->key, current, &key)) {
                Object pyStr(PyObject_Str(pyValue));
                if (!pyStr) { return -1; }
                char const *value = PyString_AsString(pyStr);
                if (!value) { return -1; }
                self->proxy->setKeyValue(key, value);
                return 0;
            }
            return PyObject_GenericSetAttr(reinterpret_cast<PyObject*>(self), name, pyValue);
        PY_CATCH(-1);
    }

    static Py_ssize_t length(ConfigProxy *self) {
        return self->arrLen;
    }

    static PyObject* item(ConfigProxy *self, Py_ssize_t index) {
        PY_TRY
            if (index < 0 || index >= self->arrLen) { return nullptr; }
            return new_(self->proxy->getArrKey(self->key, index), *self->proxy);
        PY_CATCH(nullptr);
    }
};

PyMethodDef ConfigProxy::tp_methods[] = {
    // ground
    {"keys", (PyCFunction)keys, METH_NOARGS,
R"(keys(self) -> [string]

Returns the list of sub-option groups or options of this proxy. Returns None if
the proxy is not an option group.
)"},
    {nullptr, nullptr, 0, nullptr}
};

PySequenceMethods ConfigProxy::tp_as_sequence[] = {{
    (lenfunc)length,
    nullptr,
    nullptr,
    (ssizeargfunc)item,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
}};

// }}}
// {{{ wrap DomainElement

struct DomainElement : public ObjectBase<DomainElement> {
    Gringo::DomainProxy::Element *elem;

    static constexpr char const *tp_type = "DomainElement";
    static constexpr char const *tp_name = "gringo.DomainElement";
    static constexpr char const *tp_doc = "Captures a domain element and provides properties to inspect its state.";
    static PyGetSetDef tp_getset[];

    static PyObject *new_(Gringo::DomainProxy::ElementPtr &&elem) {
        Object ret(type.tp_alloc(&type, 0));
        if (!ret) { return nullptr; }
        DomainElement *self = reinterpret_cast<DomainElement*>(ret.get());
        self->elem = elem.release();
        return ret.release();
    }
    static PyObject *atom(DomainElement *self, void *) {
        PY_TRY
            return valToPy(self->elem->atom());
        PY_CATCH(nullptr);
    }
    static PyObject *literal(DomainElement *self, void *) {
        PY_TRY
            return PyInt_FromLong(self->elem->literal());
        PY_CATCH(nullptr);
    }
    static PyObject *is_fact(DomainElement *self, void *) {
        PY_TRY
            return pyBool(self->elem->fact());
        PY_CATCH(nullptr);
    }
    static PyObject *is_external(DomainElement *self, void *) {
        PY_TRY
            return pyBool(self->elem->external());
        PY_CATCH(nullptr);
    }
    static void tp_dealloc(DomainElement *self) {
        delete self->elem;
        self->elem = nullptr;
        type.tp_free(self);
    }
};

PyGetSetDef DomainElement::tp_getset[] = {
    {(char *)"atom", (getter)atom, nullptr, (char *)R"(atom -> Fun

Returns the representation of the domain element in form of a term (Fun
object).)", nullptr},
    {(char *)"literal", (getter)literal, nullptr, (char *)R"(literal -> int

Returns the program literal associated with the atom.)", nullptr},
    {(char *)"is_fact", (getter)is_fact, nullptr, (char *)R"(is_fact -> bool

Returns wheather the domain element is a is_fact.)", nullptr},
    {(char *)"is_external", (getter)is_external, nullptr, (char *)R"(is_external -> bool

Returns wheather the domain element is external.)", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

// }}}
// {{{ wrap DomainIter

struct DomainIter : ObjectBase<DomainIter> {
    DomainElement *elem;

    static constexpr char const *tp_type = "DomainIter";
    static constexpr char const *tp_name = "gringo.DomainIter";
    static constexpr char const *tp_doc = "Class to iterate over domains.";

    static PyObject *new_(Gringo::DomainProxy::ElementPtr &&elem) {
        Object ret(type.tp_alloc(&type, 0));
        if (!ret) { return nullptr; }
        DomainIter *self = reinterpret_cast<DomainIter*>(ret.get());
        if (elem) {
            self->elem = reinterpret_cast<DomainElement*>(DomainElement::new_(std::move(elem)));
            if (!self->elem) { return nullptr; }
        }
        else { self->elem = nullptr; }
        return ret.release();
    }
    static PyObject* tp_iter(DomainIter *self) {
        Py_XINCREF(self);
        return reinterpret_cast<PyObject*>(self);
    }
    static void tp_dealloc(DomainIter *self) {
        if (self->elem) {
            Py_XDECREF(self->elem);
            self->elem = nullptr;
        }
        type.tp_free(self);
    }
    static PyObject* tp_iternext(DomainIter *self) {
        PY_TRY
            if (self->elem) {
                Gringo::DomainProxy::ElementPtr elem = self->elem->elem->next();
                Object oldElem = reinterpret_cast<PyObject*>(self->elem);
                if (elem) {
                    self->elem = reinterpret_cast<DomainElement*>(DomainElement::new_(std::move(elem)));
                    if (!self->elem) { return nullptr; }
                }
                else { self->elem = nullptr; }
                return oldElem.release();
            }
            PyErr_SetNone(PyExc_StopIteration);
            return nullptr;
        PY_CATCH(nullptr);
    }
};

// }}}
// {{{ wrap DomainProxy

struct DomainProxy : ObjectBase<DomainProxy> {
    Gringo::DomainProxy *proxy;
    static PyMethodDef tp_methods[];
    static PyMappingMethods tp_as_mapping[];

    static constexpr char const *tp_type = "Domain";
    static constexpr char const *tp_name = "gringo.Domain";
    static constexpr char const *tp_doc =
R"(This class provides read-only acces to the domains of the grounder.

Example:

p(1).
{ p(3) }.
#external p(1..3).

q(X) :- p(X).

#script (python)

import gringo

def main(prg):
    prg.ground([("base", [])])
    print "universe:", len(prg.domains)
    for x in prg.domains:
        print x.atom, x.is_fact, x.is_external
    print "p(2) is in domain:", prg.domains[gringo.Fun("p", [3])] is not None
    print "p(4) is in domain:", prg.domains[gringo.Fun("p", [6])] is not None
    print "domain of p/1:"
    for x in prg.domains.by_signature(("p", 1)):
        print x.atom, x.is_fact, x.is_external
    print "signatures:", prg.domains.signatures()

#end.

Expected Output:

universe: 6
p(1) True False
p(3) False False
p(2) False True
q(1) True False
q(3) False False
q(2) False False
p(2) is in domain: True
p(4) is in domain: False
domain of p/1:
p(1) True False
p(3) False False
p(2) False True
signatures: [('p', 1), ('q', 1)])";

    static PyObject *new_(Gringo::DomainProxy &proxy) {
        Object ret(type.tp_alloc(&type, 0));
        if (!ret) { return nullptr; }
        DomainProxy *self = reinterpret_cast<DomainProxy*>(ret.get());
        self->proxy = &proxy;
        return ret.release();
    }

    static Py_ssize_t length(DomainProxy *self) {
        return self->proxy->length();
    }

    static PyObject* tp_iter(DomainProxy *self) {
        PY_TRY
            return DomainIter::new_(self->proxy->iter());
        PY_CATCH(nullptr);
    }

    static PyObject* subscript(DomainProxy *self, PyObject *key) {
        PY_TRY
            Gringo::Value atom;
            if (!pyToVal(key, atom)) { return nullptr; }
            Gringo::DomainProxy::ElementPtr elem = self->proxy->lookup(atom);
            if (!elem) { Py_RETURN_NONE; }
            return DomainElement::new_(std::move(elem));
        PY_CATCH(nullptr);
    }

    static PyObject* by_signature(DomainProxy *self, PyObject *pyargs) {
        PY_TRY
            char const *name;
            int arity;
            if (!PyArg_ParseTuple(pyargs, "si", &name, &arity)) { return nullptr; }
            Gringo::DomainProxy::ElementPtr elem = self->proxy->iter(Signature(name, arity));
            return DomainIter::new_(std::move(elem));
        PY_CATCH(nullptr);
    }

    static PyObject* signatures(DomainProxy *self) {
        PY_TRY
            std::vector<FWSignature> ret = self->proxy->signatures();
            Object pyRet = PyList_New(ret.size());
            if (!pyRet) { return nullptr; }
            int i = 0;
            for (auto &sig : ret) {
                Object pySig = Py_BuildValue("(si)", (*(*sig).name()).c_str(), (int)(*sig).length());
                if (!pySig) { return nullptr; }
                if (PyList_SetItem(pyRet, i, pySig.release()) < 0) { return nullptr; }
                ++i;
            }
            return pyRet.release();
        PY_CATCH(nullptr);
    }
};

PyMethodDef DomainProxy::tp_methods[] = {
    {"by_signature", (PyCFunction)by_signature, METH_VARARGS,
R"(by_signature(self, name, arity) -> DomainIter

Returns an iterator over the domain elements with the given signature.

Arguments:
name  -- the name of the signature
arity -- the arity of the signature
)"},
    {"signatures", (PyCFunction)signatures, METH_NOARGS,
R"(signatures(self) -> list((str, int))

Returns the list of predicate signatures occurring in the program.
)"},
    {nullptr, nullptr, 0, nullptr}
};

PyMappingMethods DomainProxy::tp_as_mapping[] = {{
    (lenfunc)length,
    (binaryfunc)subscript,
    nullptr,
}};

// }}}
// {{{ wrap TheoryPropagatorInit

struct TheoryPropagatorInit : ObjectBase<TheoryPropagatorInit> {
    TheoryPropagator::Init *init;
    static constexpr char const *tp_type = "TheoryPropagatorInit";
    static constexpr char const *tp_name = "gringo.TheoryPropagatorInit";
    static constexpr char const *tp_doc = "Object that is used to initialize a theory propagator before each solving step.";
    static PyMethodDef tp_methods[];

    using ObjectBase<TheoryPropagatorInit>::new_;
    static PyObject *construct(TheoryPropagator::Init &init) {
        TheoryPropagatorInit *self = new_();
        self->init = &init;
        return reinterpret_cast<PyObject*>(self);
    }

    static PyObject *theoryIter(TheoryPropagatorInit *self) {
        return TheoryIter::new_(&self->init->theory(), 0);
    }

    static PyObject *domains(TheoryPropagatorInit *self) {
        return DomainProxy::new_(self->init->getDomain());
    }

    static PyObject *mapLit(TheoryPropagatorInit *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            Lit_t r = 0;
            r = self->init->mapLit(l);
            return PyInt_FromLong(r);
        PY_CATCH(nullptr);
    }

    static PyObject *addWatch(TheoryPropagatorInit *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            self->init->addWatch(l);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
};

PyMethodDef TheoryPropagatorInit::tp_methods[] = {
    {"theory_iter", (PyCFunction)theoryIter, METH_NOARGS, "theory_iter(self) -> TheoryIter object\n\nReturns an iterator over all theory atoms."},
    {"domains", (PyCFunction)domains, METH_NOARGS, "domains(self) -> Domain object\n\nReturns a Domain object."},
    {"add_watch", (PyCFunction)addWatch, METH_O, "add_watch(self, lit) -> None\n\nAdd a watch for the literal in the given phase."},
    {"map_literal", (PyCFunction)mapLit, METH_O, "map_literal(self, lit) -> int\n\nMaps a program literal to a solver literal."},
    {nullptr, nullptr, 0, nullptr}
};


// }}}
// {{{ wrap TheoryPropagatorSolverControl

struct TheoryPropagatorSolverControl : ObjectBase<TheoryPropagatorSolverControl> {
    Potassco::AbstractSolver* ctl;
    static constexpr char const *tp_type = "TheoryPropagatorSolverControl";
    static constexpr char const *tp_name = "gringo.TheoryPropagatorSolverControl";
    static constexpr char const *tp_doc = "This object can be used to add clauses and propagate literals.";
    static PyMethodDef tp_methods[];

    using ObjectBase<TheoryPropagatorSolverControl>::new_;
    static PyObject *construct(Potassco::AbstractSolver &ctl) {
        TheoryPropagatorSolverControl *self = new_();
        self->ctl = &ctl;
        return reinterpret_cast<PyObject*>(self);
    }

    static PyObject *id(TheoryPropagatorSolverControl *self) {
        return PyInt_FromLong(self->ctl->id());
    }

    static PyObject *addClause(TheoryPropagatorSolverControl *self, PyObject *clause) {
        PY_TRY
            Object it = PyObject_GetIter(clause);
            if (!it) { return nullptr; }
            std::vector<Lit_t> lits;
            while (Object lit = PyIter_Next(it)) {
                lits.emplace_back(PyLong_AsLong(lit));
                if (PyErr_Occurred()) { return nullptr; }
            }
            if (PyErr_Occurred()) { return nullptr; }
            return pyBool(doUnblocked([self, &lits](){ return self->ctl->addClause(Potassco::toSpan(lits)); }));
        PY_CATCH(nullptr);
    }

    static PyObject *propagate(TheoryPropagatorSolverControl *self) {
        PY_TRY
            return pyBool(doUnblocked([self](){ return self->ctl->propagate(); }));
        PY_CATCH(nullptr);
    }
};

PyMethodDef TheoryPropagatorSolverControl::tp_methods[] = {
    {"id", (PyCFunction)id, METH_NOARGS, "id(self) -> int\n\nThe id of the current solver."},
    {"add_clause", (PyCFunction)addClause, METH_O, R"(add_clause(self, clause) -> bool

Adds the given clause to the solver if possible and returns false to indicate
that adding the clause would require backtracking first.

Arguments:
clause -- sequence of literals)"},
    {"propagate", (PyCFunction)propagate, METH_NOARGS, R"(propagate(self) -> bool

Propagates implied literals.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap TheoryPropagatorAssignment

struct TheoryPropagatorAssignment : ObjectBase<TheoryPropagatorAssignment> {
    Potassco::AbstractAssignment const *assign;
    static constexpr char const *tp_type = "TheoryPropagatorAssignment";
    static constexpr char const *tp_name = "gringo.TheoryPropagatorAssignment";
    static constexpr char const *tp_doc = "Object to inspect the (parital) assignment of an associated solver.";
    static PyMethodDef tp_methods[];

    static PyObject *construct(Potassco::AbstractAssignment const &assign) {
        TheoryPropagatorAssignment *self = new_();
        self->assign = &assign;
        return reinterpret_cast<PyObject*>(self);
    }

    static PyObject *hasConflict(TheoryPropagatorAssignment *self) {
        PY_TRY
            return pyBool(self->assign->hasConflict());
        PY_CATCH(nullptr);
    }

    static PyObject *decisionLevel(TheoryPropagatorAssignment *self) {
        PY_TRY
            return PyInt_FromLong(self->assign->level());
        PY_CATCH(nullptr);
    }

    static PyObject *hasLit(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            return pyBool(self->assign->hasLit(l));
        PY_CATCH(nullptr);
    }

    static PyObject *level(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            return PyInt_FromLong(self->assign->level(l));
        PY_CATCH(nullptr);
    }

    static PyObject *isFixed(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            return pyBool(self->assign->isFixed(l));
        PY_CATCH(nullptr);
    }

    static PyObject *truthValue(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            Potassco::Value_t val = self->assign->value(l);
            if (val == Potassco::Value_t::False){ return pyBool(false); }
            if (val == Potassco::Value_t::True) { return pyBool(true); }
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }

    static PyObject *isTrue(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            return pyBool(self->assign->isTrue(l));
        PY_CATCH(nullptr);
    }

    static PyObject *isFalse(TheoryPropagatorAssignment *self, PyObject *lit) {
        PY_TRY
            Lit_t l = PyLong_AsLong(lit);
            if (PyErr_Occurred()) { return nullptr; }
            return pyBool(self->assign->isFalse(l));
        PY_CATCH(nullptr);
    }
};

PyMethodDef TheoryPropagatorAssignment::tp_methods[] = {
    {"has_conflict", (PyCFunction)hasConflict, METH_NOARGS, R"(has_conflict(self) -> bool

Returns true if the current assignment is conflicting.)"},
    {"decision_level", (PyCFunction)decisionLevel, METH_NOARGS, R"(decision_level(self) -> int

Returns the current decision level.)"},
    {"has_lit", (PyCFunction)hasLit, METH_O, R"(has_lit(self, lit) -> bool

Returns true if the literal is valid in this solver.)"},
    {"truth_value", (PyCFunction)truthValue, METH_O, R"(truth_value(self, lit) -> bool or None

Returns the truth value of the given literal or None if it has none.)"},
    {"level", (PyCFunction)level, METH_O, R"(level(self, lit) -> int

Returns the decision value of the given literal.)"},
    {"is_fixed", (PyCFunction)isFixed, METH_O, R"(is_fixed(self, lit) -> bool

Returns true if the literal is assigned on the top level.)"},
    {"is_true", (PyCFunction)isTrue, METH_O, R"(is_true(self, lit) -> bool

Returns true if the literal is true.)"},
    {"is_false", (PyCFunction)isFalse, METH_O, R"(is_false(self, lit) -> bool

Returns true if the literal is false.)"},
    {nullptr, nullptr, 0, nullptr}
};

// }}}
// {{{ wrap TheoryPropagator

class PythonTheoryPropagator : public Gringo::TheoryPropagator {
public:
    PythonTheoryPropagator(PyObject *tp) : tp_(tp, true) {}
    void init(Init &init) override {
        PyBlock block;
        Object i = TheoryPropagatorInit::construct(init);
        if (!i) { handleError("TheoryPropagator::init", "error during initialization"); }
        Object n = PyString_FromString("init");
        if (!n) { handleError("TheoryPropagator::init", "error during initialization"); }
        Object ret = PyObject_CallMethodObjArgs(tp_, n.get(), i.get(), nullptr);
        if (!ret) { handleError("TheoryPropagator::init", "error during initialization"); }
    }
    bool propagate(Potassco::AbstractSolver &solver, Potassco::LitSpan const &changes) override {
        PyBlock block;
        Object c = TheoryPropagatorSolverControl::construct(solver);
        if (!c) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        Object a = TheoryPropagatorAssignment::construct(solver.assignment());
        if (!a) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        Object l = toList(changes);
        if (!l) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        Object n = PyString_FromString("propagate");
        if (!n) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        Object ret = PyObject_CallMethodObjArgs(tp_, n, c.get(), a.get(), l.get(), nullptr);
        if (!ret) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        bool r = PyInt_AsLong(ret);
        if (PyErr_Occurred()) { handleError("TheoryPropagator::propagate", "error during propagation"); }
        return r != 0;
    }
    void undo(Potassco::AbstractSolver const &solver, Potassco::LitSpan const &undo) override {
        PyBlock block;
        Object i = PyInt_FromLong(solver.id());
        if (!i) { handleError("TheoryPropagator::undo", "error during undo"); }
        Object l = toList(undo);
        if (!l) { handleError("TheoryPropagator::undo", "error during undo"); }
        Object n = PyString_FromString("undo");
        if (!n) { handleError("TheoryPropagator::undo", "error during undo"); }
        Object ret = PyObject_CallMethodObjArgs(tp_, n, i.get(), l.get(), nullptr);
        if (!ret) { handleError("TheoryPropagator::undo", "error during undo"); }
    }
    bool model(Potassco::AbstractSolver &solver) override {
        PyBlock block;
        Object c = TheoryPropagatorSolverControl::construct(solver);
        if (!c) { handleError("TheoryPropagator::model", "error during model"); }
        Object a = TheoryPropagatorAssignment::construct(solver.assignment());
        if (!a) { handleError("TheoryPropagator::model", "error during model"); }
        Object n = PyString_FromString("model");
        if (!n) { handleError("TheoryPropagator::model", "error during model"); }
        Object ret = PyObject_CallMethodObjArgs(tp_, n, c.get(), a.get(), nullptr);
        if (!ret) { handleError("TheoryPropagator::model", "error during model"); }
        bool r = PyInt_AsLong(ret);
        if (PyErr_Occurred()) { handleError("TheoryPropagator::model", "error during model"); }
        return r != 0;
    }
    ~PythonTheoryPropagator() noexcept = default;
private:
    PyObject *toList(Potassco::LitSpan const &lits) {
        Object list = PyList_New(lits.size);
        if (!list) { return nullptr; }
        int i = 0;
        for (auto &lit : lits) {
            Object l = PyInt_FromLong(lit);
            if (PyList_SetItem(list, i, l.release()) < 0) { return nullptr; }
            ++i;
        }
        return list.release();
    }
private:
    Object tp_;
};

// }}}
// {{{ wrap Control

bool pycall(PyObject *fun, ValVec const &args, ValVec &vals) {
    Object params = PyTuple_New(args.size());
    if (!params) { return false; }
    int i = 0;
    for (auto &val : args) {
        Object pyVal = valToPy(val);
        if (!pyVal) { return false; }
        if (PyTuple_SetItem(params, i, pyVal.release()) < 0) { return false; }
        ++i;
    }
    Object ret = PyObject_Call(fun, params, Py_None);
    if (!ret) { return false; }
    if (PyList_Check(ret)) {
        if (!pyToVals(ret, vals)) { return false; }
    }
    else {
        Value val;
        if (!pyToVal(ret, val)) { return false; }
        vals.emplace_back(val);
    }
    return true;
}

class PyContext : public Context {
public:
    PyContext()
    : ctx(nullptr) { }
    bool callable(FWString name) const override {
        return ctx && PyObject_HasAttrString(ctx, (*name).c_str());
    }
    ValVec call(Location const &loc, FWString name, ValVec const &args) override {
        assert(ctx);
        Object fun = PyObject_GetAttrString(ctx, (*name).c_str());
        ValVec ret;
        if (!fun || !pycall(fun, args, ret)) {
            GRINGO_REPORT(W_OPERATION_UNDEFINED)
                << loc << ": info: operation undefined:\n"
                << errorToString()
                ;
            return {};
        }
        return ret;
    }
    operator bool() const { return ctx; }
    virtual ~PyContext() noexcept = default;

    PyObject *ctx;
};

struct ControlWrap : ObjectBase<ControlWrap> {
    using Propagators = std::vector<std::unique_ptr<PythonTheoryPropagator>>;
    Gringo::Control *ctl;
    Gringo::Control *freeCtl;
    PyObject        *stats;
    Propagators      propagators_;

    static PyGetSetDef tp_getset[];
    static PyMethodDef tp_methods[];

    static constexpr char const *tp_type = "Control";
    static constexpr char const *tp_name = "gringo.Control";
    static constexpr char const *tp_doc =
    R"(Control(args) -> Control object

Control object for the grounding/solving process.

Arguments:
args -- optional arguments to the grounder and solver (default: []).

Note that only gringo options (without --text) and clasp's search options are
supported. Furthermore, a Control object is blocked while a search call is
active; you must not call any member function during search.)";

    static bool checkBlocked(ControlWrap *self, char const *function) {
        if (self->ctl->blocked()) {
            PyErr_Format(PyExc_RuntimeError, "Control.%s must not be called during solve call", function);
            return false;
        }
        return true;
    }
    static PyObject *new_(Gringo::Control &ctl) {
        PyObject *self = tp_new(&type, nullptr, nullptr);
        if (!self) { return nullptr; }
        reinterpret_cast<ControlWrap*>(self)->ctl = &ctl;
        return self;
    }
    static Gringo::GringoModule *module;
    static PyObject *tp_new(PyTypeObject *type, PyObject *, PyObject *) {
        ControlWrap *self;
        self = reinterpret_cast<ControlWrap*>(type->tp_alloc(type, 0));
        if (!self) { return nullptr; }
        self->ctl     = nullptr;
        self->freeCtl = nullptr;
        self->stats   = nullptr;
        new (&self->propagators_) Propagators();
        return reinterpret_cast<PyObject*>(self);
    }
    static void tp_dealloc(ControlWrap *self) {
        module->freeControl(self->freeCtl);
        self->ctl = self->freeCtl = nullptr;
        Py_XDECREF(self->stats);
        self->propagators_.~Propagators();
        type.tp_free(self);
    }
    static int tp_init(ControlWrap *self, PyObject *pyargs, PyObject *pykwds) {
        PY_TRY
            static char const *kwlist[] = {"args", nullptr};
            PyObject *params = nullptr;
            if (!PyArg_ParseTupleAndKeywords(pyargs, pykwds, "|O", const_cast<char**>(kwlist), &params)) { return -1; }
            std::vector<char const *> args;
            args.emplace_back("clingo");
            if (params) {
                Object it = PyObject_GetIter(params);
                if (!it) { return -1; }
                while (Object pyVal = PyIter_Next(it)) {
                    char const *x = PyString_AsString(pyVal);
                    if (!x) { return -1; }
                    args.emplace_back(x);
                }
                if (PyErr_Occurred()) { return -1; }
            }
            args.emplace_back(nullptr);
            self->ctl = self->freeCtl = module->newControl(args.size(), args.data());
            return 0;
        PY_CATCH(-1);
    }
    static PyObject *add(ControlWrap *self, PyObject *args) {
        PY_TRY
            if (!checkBlocked(self, "add")) { return nullptr; }
            char     *name;
            PyObject *pyParams;
            char     *part;
            if (!PyArg_ParseTuple(args, "sOs", &name, &pyParams, &part)) { return nullptr; }
            FWStringVec params;
            Object it = PyObject_GetIter(pyParams);
            if (!it) { return nullptr; }
            while (Object pyVal = PyIter_Next(it)) {
                char const *val = PyString_AsString(pyVal);
                if (!val) { return nullptr; }
                params.emplace_back(val);
            }
            if (PyErr_Occurred()) { return nullptr; }
            self->ctl->add(name, params, part);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *load(ControlWrap *self, PyObject *args) {
        PY_TRY
            if (!checkBlocked(self, "load")) { return nullptr; }
            char *filename;
            if (!PyArg_ParseTuple(args, "s", &filename)) { return nullptr; }
            if (!filename) { return nullptr; }
            self->ctl->load(filename);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *ground(ControlWrap *self, PyObject *args, PyObject *kwds) {
        PY_TRY
            if (!checkBlocked(self, "ground")) { return nullptr; }
            Gringo::Control::GroundVec parts;
            static char const *kwlist[] = {"parts", "context", nullptr};
            PyObject *pyParts;
            PyContext context;
            if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O", const_cast<char**>(kwlist), &pyParts, &context.ctx)) { return nullptr; }
            Object it = PyObject_GetIter(pyParts);
            if (!it) { return nullptr; }
            while (Object pyVal = PyIter_Next(it)) {
                Object jt = PyObject_GetIter(pyVal);
                if (!jt) { return nullptr; }
                Object pyName = PyIter_Next(jt);
                if (!pyName) { return PyErr_Occurred() ? nullptr : PyErr_Format(PyExc_RuntimeError, "tuple of name and arguments expected"); }
                Object pyArgs = PyIter_Next(jt);
                if (!pyArgs) { return PyErr_Occurred() ? nullptr : PyErr_Format(PyExc_RuntimeError, "tuple of name and arguments expected"); }
                if (PyIter_Next(jt)) { return PyErr_Format(PyExc_RuntimeError, "tuple of name and arguments expected"); }
                char *name = PyString_AsString(pyName);
                if (!name) { return nullptr; }
                ValVec args;
                if (!pyToVals(pyArgs, args)) { return nullptr; }
                parts.emplace_back(name, args);
            }
            if (PyErr_Occurred()) { return nullptr; }
            // anchor
            self->ctl->ground(parts, context ? &context : nullptr);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *getConst(ControlWrap *self, PyObject *args) {
        PY_TRY
            if (!checkBlocked(self, "get_const")) { return nullptr; }
            char *name;
            if (!PyArg_ParseTuple(args, "s", &name)) { return nullptr; }
            Value val;
            val = self->ctl->getConst(name);
            if (val.type() == Value::SPECIAL) { Py_RETURN_NONE; }
            else { return valToPy(val); }
        PY_CATCH(nullptr);
    }
    static bool on_model(Gringo::Model const &m, PyObject *mh) {
        Object model(Model::new_(m));
        if (!model) {
            Location loc("<on_model>", 1, 1, "<on_model>", 1, 1);
            handleError(loc, "error in model callback");
            throw std::runtime_error("error in model callback");
        }
        Object ret = PyObject_CallFunction(mh, const_cast<char*>("O"), model.get());
        if (!ret) {
            Location loc("<on_model>", 1, 1, "<on_model>", 1, 1);
            handleError(loc, "error in model callback");
            throw std::runtime_error("error in model callback");
        }
        if (ret == Py_None)       { return true; }
        else if (ret == Py_True)  { return true; }
        else if (ret == Py_False) { return false; }
        else {
            PyErr_Format(PyExc_RuntimeError, "unexpected %s() object as result of on_model", ret->ob_type->tp_name);
            Location loc("<on_model>", 1, 1, "<on_model>", 1, 1);
            handleError(loc, "error in model callback");
            throw std::runtime_error("error in model callback");
        }

    }
    static void on_finish(Gringo::SolveResult ret, bool canceled, PyObject *fh) {
        Object pyRet = SolveResult::getAttr(ret);
        if (!pyRet) {
            Location loc("<on_finish>", 1, 1, "<on_finish>", 1, 1);
            handleError(loc, "error in finish callback");
            throw std::runtime_error("error in finish callback");
        }
        Object pyInterrupted = PyBool_FromLong(canceled);
        if (!pyInterrupted) {
            Location loc("<on_finish>", 1, 1, "<on_finish>", 1, 1);
            handleError(loc, "error in finish callback");
            throw std::runtime_error("error in finish callback");
        }
        Object fhRet = PyObject_CallFunction(fh, const_cast<char*>("OO"), pyRet.get(), pyInterrupted.get());
        if (!fhRet) {
            Location loc("<on_finish>", 1, 1, "<on_finish>", 1, 1);
            handleError(loc, "error in finish callback");
            throw std::runtime_error("error in finish callback");
        }
    }
    static bool getAssumptions(PyObject *pyAss, Gringo::Control::Assumptions &ass) {
        PY_TRY
            if (pyAss && pyAss != Py_None) {
                Object it = PyObject_GetIter(pyAss);
                if (!it) { return false; }
                while (Object pyPair = PyIter_Next(it)) {
                    Object pyPairIt = PyObject_GetIter(pyPair);
                    if (!pyPairIt) { return false; }
                    Object pyAtom = PyIter_Next(pyPairIt);
                    if (!pyAtom) {
                        if (!PyErr_Occurred()) { PyErr_Format(PyExc_RuntimeError, "tuple expected"); }
                        return false;
                    }
                    Object pyBool = PyIter_Next(pyPairIt);
                    if (!pyBool) {
                        if (!PyErr_Occurred()) { PyErr_Format(PyExc_RuntimeError, "tuple expected"); }
                        return false;
                    }
                    Value atom;
                    if (!pyToVal(pyAtom, atom)) { return false; }
                    int truth = PyObject_IsTrue(pyBool);
                    if (truth == -1) { return false; }
                    ass.emplace_back(atom, truth);
                }
                if (PyErr_Occurred()) { return false; }
            }
            return true;
        PY_CATCH(false);
    }
    static PyObject *solve_async(ControlWrap *self, PyObject *args, PyObject *kwds) {
        PY_TRY
            if (!checkBlocked(self, "solve_async")) { return nullptr; }
            Py_XDECREF(self->stats);
            self->stats = nullptr;
            static char const *kwlist[] = {"assumptions", "on_model", "on_finish", nullptr};
            PyObject *pyAss = nullptr, *mh = Py_None, *fh = Py_None;
            if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOO", const_cast<char **>(kwlist), &pyAss, &mh, &fh)) { return nullptr; }
            Gringo::Control::Assumptions ass;
            if (!getAssumptions(pyAss, ass)) { return nullptr; }
            Gringo::SolveFuture *future = self->ctl->solveAsync(
                mh == Py_None ? Control::ModelHandler(nullptr) : [mh](Gringo::Model const &m) -> bool { PyBlock b; (void)b; return on_model(m, mh); },
                fh == Py_None ? Control::FinishHandler(nullptr) : [fh](Gringo::SolveResult ret, bool canceled) -> void { PyBlock b; (void)b; on_finish(ret, canceled, fh); },
                std::move(ass)
            );
            return SolveFuture::new_(*future, mh, fh);
        PY_CATCH(nullptr);
    }
    static PyObject *solve_iter(ControlWrap *self, PyObject *args, PyObject *kwds) {
        PY_TRY
            if (!checkBlocked(self, "solve_iter")) { return nullptr; }
            Py_XDECREF(self->stats);
            self->stats = nullptr;
            PyObject *pyAss = nullptr;
            static char const *kwlist[] = {"assumptions", nullptr};
            if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", const_cast<char **>(kwlist), &pyAss)) { return nullptr; }
            Gringo::Control::Assumptions ass;
            if (!getAssumptions(pyAss, ass)) { return nullptr; }
            return SolveIter::new_(*self->ctl->solveIter(std::move(ass)));
        PY_CATCH(nullptr);
    }
    static PyObject *solve(ControlWrap *self, PyObject *args, PyObject *kwds) {
        PY_TRY
            if (!checkBlocked(self, "solve")) { return nullptr; }
            Py_XDECREF(self->stats);
            self->stats = nullptr;
            static char const *kwlist[] = {"assumptions", "on_model", nullptr};
            PyObject *mh = Py_None;
            PyObject *pyAss = nullptr;
            if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OO", const_cast<char **>(kwlist), &pyAss, &mh)) { return nullptr; }
            Gringo::Control::Assumptions ass;
            if (!getAssumptions(pyAss, ass)) { return nullptr; }
            Gringo::SolveResult ret = doUnblocked([self, mh, &ass]() {
                return self->ctl->solve(
                    mh == Py_None ? Control::ModelHandler(nullptr) : [mh](Gringo::Model const &m) { PyBlock block; return on_model(m, Object(mh, true)); },
                    std::move(ass)); });
            return SolveResult::getAttr(ret);
        PY_CATCH(nullptr);
    }
    static PyObject *cleanup_domains(ControlWrap *self) {
        PY_TRY
            if (!checkBlocked(self, "cleanup_domains")) { return nullptr; }
            self->ctl->cleanupDomains();
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *assign_external(ControlWrap *self, PyObject *args) {
        PY_TRY
            if (!checkBlocked(self, "assign_external")) { return nullptr; }
            PyObject *pyExt, *pyVal;
            if (!PyArg_ParseTuple(args, "OO", &pyExt, &pyVal)) { return nullptr; }
            Potassco::Value_t val;
            if (pyVal == Py_True)       { val = Potassco::Value_t::True; }
            else if (pyVal == Py_False) { val = Potassco::Value_t::False; }
            else if (pyVal == Py_None)  { val = Potassco::Value_t::Free; }
            else {
                PyErr_Format(PyExc_RuntimeError, "unexpected %s() object as second argumet", pyVal->ob_type->tp_name);
                return nullptr;
            }
            Value ext;
            if (!pyToVal(pyExt, ext)) { return nullptr; }
            self->ctl->assignExternal(ext, val);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *release_external(ControlWrap *self, PyObject *args) {
        PY_TRY
            if (!checkBlocked(self, "release_external")) { return nullptr; }
            PyObject *pyExt;
            if (!PyArg_ParseTuple(args, "O", &pyExt)) { return nullptr; }
            Value ext;
            if (!pyToVal(pyExt, ext)) { return nullptr; }
            self->ctl->assignExternal(ext, Potassco::Value_t::Release);
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
    static PyObject *getStats(ControlWrap *self, void *) {
        PY_TRY
            if (!checkBlocked(self, "stats")) { return nullptr; }
            if (!self->stats) {
                Statistics *stats = self->ctl->getStats();
                self->stats = getStatistics(stats, "");
            }
            Py_XINCREF(self->stats);
            return self->stats;
        PY_CATCH(nullptr);
    }
    static int set_use_enum_assumption(ControlWrap *self, PyObject *pyEnable, void *) {
        PY_TRY
        if (!checkBlocked(self, "use_enum_assumption")) { return -1; }
        int enable = PyObject_IsTrue(pyEnable);
        if (enable < 0) { return -1; }
        self->ctl->useEnumAssumption(enable);
        return 0;
        PY_CATCH(-1);
    }
    static PyObject *get_use_enum_assumption(ControlWrap *self, void *) {
        PY_TRY
            return PyBool_FromLong(self->ctl->useEnumAssumption());
        PY_CATCH(nullptr);
    }
    static PyObject *conf(ControlWrap *self, void *) {
        PY_TRY
            Gringo::ConfigProxy &proxy = self->ctl->getConf();
            return ConfigProxy::new_(proxy.getRootKey(), proxy);
        PY_CATCH(nullptr);
    }
    static PyObject *get_domains(ControlWrap *self, void *) {
        return DomainProxy::new_(self->ctl->getDomain());
    }
    static PyObject *theoryIter(ControlWrap *self) {
        if (!checkBlocked(self, "theory_atoms")) { return nullptr; }
        return TheoryIter::new_(&self->ctl->theory(), 0);
    }
    static PyObject *registerPropagator(ControlWrap *self, PyObject *tp) {
        PY_TRY
            self->propagators_.emplace_back(gringo_make_unique<PythonTheoryPropagator>(tp));
            self->ctl->registerPropagator(*self->propagators_.back());
            Py_RETURN_NONE;
        PY_CATCH(nullptr);
    }
};

Gringo::GringoModule *ControlWrap::module  = nullptr;

PyMethodDef ControlWrap::tp_methods[] = {
    // ground
    {"ground", (PyCFunction)ground, METH_KEYWORDS | METH_VARARGS,
R"(ground(self, parts, context) -> None

Grounds the given list programs specified by tuples of names and arguments.

Keyword Arguments:
parts   -- list of tuples of program names and program arguments to ground
context -- an context object whose methods are called during grounding using
           the @-syntax (if ommitted methods from the main module are used)

Note that parts of a logic program without an explicit #program specification
are by default put into a program called base without arguments.

Example:

#script (python)
import gringo

def main(prg):
    parts = []
    parts.append(("p", [1]))
    parts.append(("p", [2]))
    prg.ground(parts)
    prg.solve()

#end.

#program p(t).
q(t).

Expected Answer Set:
q(1) q(2))"},
    // get_const
    {"get_const", (PyCFunction)getConst, METH_VARARGS,
R"(get_const(self, name) -> Fun, integer, string, or tuple object

Returns the term for a constant definition of form: #const name = term.)"},
    // add
    {"add", (PyCFunction)add, METH_VARARGS,
R"(add(self, name, params, program) -> None

Extend the logic program with the given non-ground logic program in string form.

Arguments:
name    -- name of program block to add
params  -- parameters of program block
program -- non-ground program as string

Example:

#script (python)
import gringo

def main(prg):
    prg.add("p", ["t"], "q(t).")
    prg.ground([("p", [2])])
    prg.solve()

#end.

Expected Answer Set:
q(2))"},
    // load
    {"load", (PyCFunction)load, METH_VARARGS,
R"(load(self, path) -> None

Extend the logic program with a (non-ground) logic program in a file.

Arguments:
path -- path to program)"},
    // solve_async
    {"solve_async", (PyCFunction)solve_async, METH_KEYWORDS | METH_VARARGS,
R"(solve_async(self, assumptions, on_model, on_finish) -> SolveFuture

Start a search process in the background and return a SolveFuture object.

Keyword Arguments:
assumptions -- a list of (atom, boolean) tuples that serve as assumptions for
               the solve call, e.g. - solving under assumptions [(Fun("a"),
               True)] only admits answer sets that contain atom a
on_model    -- optional callback for intercepting models
               a Model object is passed to the callback
on_finish   -- optional callback once search has finished
               a SolveResult and a Boolean indicating whether the solve call
               has been canceled is passed to the callback

Note that this function is only available in clingo with thread support
enabled. Both the on_model and the on_finish callbacks are called from another
thread.  To ensure that the methods can be called, make sure to not use any
functions that block the GIL indefinitely. Furthermore, you might want to start
clingo using the --outf=3 option to disable all output from clingo.

Example:

#script (python)
import gringo

def on_model(model):
    print model

def on_finish(res, canceled):
    print res, canceled

def main(prg):
    prg.ground([("base", [])])
    f = prg.solve_async(None, on_model, on_finish)
    f.wait()

#end.

q.

Expected Output:
q
SAT False)"},
    // solve_iter
    {"solve_iter", (PyCFunction)solve_iter, METH_KEYWORDS | METH_VARARGS,
R"(solve_iter(self, assumptions) -> SolveIter

Returns a SolveIter object, which can be used to iterate over models.

Keyword Arguments:
assumptions -- a list of (atom, boolean) tuples that serve as assumptions for
               the solve call, e.g. - solving under assumptions [(Fun("a"),
               True)] only admits answer sets that contain atom a

Example:

#script (python)
import gringo

def main(prg):
    prg.add("p", "{a;b;c}.")
    prg.ground([("p", [])])
    with prg.solve_iter() as it:
        for m in it: print m

#end.)"},
    // solve
    {"solve", (PyCFunction)solve, METH_KEYWORDS | METH_VARARGS,
R"(solve(self, assumptions, on_model) -> SolveResult

Starts a search process and returns a SolveResult.

Keyword Arguments:
assumptions -- a list of (atom, boolean) tuples that serve as assumptions for
               the solve call, e.g. - solving under assumptions [(Fun("a"),
               True)] only admits answer sets that contain atom a
on_model    -- optional callback for intercepting models
               a Model object is passed to the callback

Note that in gringo or in clingo with lparse or text output enabled this
functions just grounds and returns SolveResult.UNKNOWN. Furthermore, you might
want to start clingo using the --outf=3 option to disable all output from
clingo.

This function releases the GIL but it is not thread-safe.

Take a look at Control.solve_async for an example on how to use the model
callback.)"},
    // cleanup_domains
    {"cleanup_domains", (PyCFunction)cleanup_domains, METH_NOARGS,
R"(cleanup_domains(self) -> None

This functions cleans up the domains used for grounding.  This is done by first
simplifying the current program representation (falsifying released external
atoms).  Afterwards, the top-level implications are used to either remove atoms
from domains or mark them as facts.

Note that any atoms falsified are completely removed from the logic program.
Hence, a definition for such an atom in a successive step introduces a fresh atom.)"},
    // assign_external
    {"assign_external", (PyCFunction)assign_external, METH_VARARGS,
R"(assign_external(self, external, truth) -> None

Assign a truth value to an external atom (represented as a term). It is
possible to assign a Boolean or None.  A Boolean fixes the external to the
respective truth value; and None leaves its truth value open.

The truth value of an external atom can be changed before each solve call. An
atom is treated as external if it has been declared using an #external
directive, and has not been forgotten by calling Control.release_external or
defined in a logic program with some rule. If the given atom is not external,
then the function has no effect.

Arguments:
external -- term representing the external atom
truth    -- Boolean or None indicating the truth value

See Control.release_external for an example.)"},
    // release_external
    {"release_external", (PyCFunction)release_external, METH_VARARGS,
R"(release_external(self, term) -> None

Release an external represented by the given term.

This function causes the corresponding atom to become permanently false if
there is no definition for the atom in the program. In all other cases this
function has no effect.

Example:

#script (python)
from gringo import Fun

def main(prg):
    prg.ground([("base", [])])
    prg.assign_external(Fun("b"), True)
    prg.solve()
    prg.release_external(Fun("b"))
    prg.solve()

#end.

a.
#external b.

Expected Answer Sets:
a b
a)"},
    // theory_atoms
    {"theory_atoms", (PyCFunction)theoryIter, METH_NOARGS,
R"(theory_atoms() -> TheoryIter

Returns a TheoryIter object, which can be used to iterate over the theory atoms.)"},
    {"register_propagator", (PyCFunction)registerPropagator, METH_O,
R"(register_propagator() -> None

Registers the given theory propagator with the underlying solvers.)"},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef ControlWrap::tp_getset[] = {
    {(char*)"conf", (getter)conf, nullptr, (char*)"ConfigProxy to change the configuration.", nullptr},
    {(char*)"domains", (getter)get_domains, nullptr, (char*)"Domain object to inspect the domains.", nullptr},
    {(char*)"use_enum_assumption", (getter)get_use_enum_assumption, (setter)set_use_enum_assumption,
(char*)R"(Boolean determining how learnt information from enumeration modes is treated.

If the enumeration assumption is enabled, then all information learnt from
clasp's various enumeration modes is removed after a solve call. This includes
enumeration of cautious or brave consequences, enumeration of answer sets with
or without projection, or finding optimal models; as well as clauses/nogoods
added with Model.add_clause()/Model.add_nogood().

Note that initially the enumeration assumption is enabled.)", nullptr},
    {(char*)"stats", (getter)getStats, nullptr,
(char*)R"(A dictionary containing solve statistics of the last solve call.

Contains the statistics of the last Control.solve, Control.solve_async, or
Control.solve_iter call. The statistics correspond to the --stats output of
clingo.  The detail of the statistics depends on what level is requested on the
command line. Furthermore, you might want to start clingo using the --outf=3
option to disable all output from clingo.

Note that this (read-only) property is only available in clingo.

Example:
import json
json.dumps(prg.stats, sort_keys=True, indent=4, separators=(',', ': ')))", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

// }}}
// {{{ wrap module functions

static PyObject *cmpVal(PyObject *, PyObject *args) {
    PyObject *a, *b;
    if (!PyArg_ParseTuple(args, "OO", &a, &b)) { return nullptr; }
    Value va, vb;
    if (!pyToVal(a, va)) { return nullptr; }
    if (!pyToVal(b, vb)) { return nullptr; }
    int ret;
    if (va == vb)     { ret =  0; }
    else if (va < vb) { ret = -1; }
    else              { ret =  1; }
    return PyInt_FromLong(ret);
}

static PyObject *parseTerm(PyObject *, PyObject *objString) {
    PY_TRY
        char const *current = PyString_AsString(objString);
        Value value = ControlWrap::module->parseValue(current);
        if (value.type() == Value::SPECIAL) { Py_RETURN_NONE; }
        else { return valToPy(value); }
    PY_CATCH(nullptr);
}

// }}}
// {{{ gringo module

static PyMethodDef gringoMethods[] = {
    {"cmp",  (PyCFunction)cmpVal, METH_VARARGS,
R"(cmp(a, b) -> { -1, 0, 1 }

Compare terms a and b using gringo's inbuilt comparison function.

Returns:
    -1 if a < b,
     0 if a = b, and
     1 otherwise.)"},
    {"parse_term", (PyCFunction)parseTerm, METH_O,
R"(parse_term(s) -> term

Parses the given string s using gringo's term parser for ground terms. The
function also evaluates arithmetic functions.

Example:

parse_term('p(1+2)') == Fun("p", [3])
)"},
    {nullptr, nullptr, 0, nullptr}
};
static char const *strGrMod =
"The gringo-" GRINGO_VERSION R"( module.

This module provides functions and classes to work with ground terms and to
control the instantiation process.  In clingo builts, additional functions to
control and inspect the solving process are available.

Functions defined in a python script block are callable during the
instantiation process using @-syntax. The default grounding/solving process can
be customized if a main function is provided.

Note that gringo terms are wrapped in python classes provided in this module.
For string terms, numbers, and tuples the respective inbuilt python classes are
used.  Functions called during the grounding process from the logic program
must either return a term or a sequence of terms.  If a sequence is returned,
the corresponding @-term is successively substituted by the values in the
sequence.

Static Objects:

__version__ -- version of the gringo module ()" GRINGO_VERSION  R"()
Inf         -- represents #inf term
Sup         -- represents #sup term

Functions:

cmp()        -- compare terms as gringo does
parse_term() -- parse ground terms

Classes:

Control        -- control object for the grounding/solving process
ConfigProxy    -- proxy to change configuration
Domain         -- inspection of domains
DomainElement  -- captures information about domain element
DomainIter     -- to iterate over domains
Fun            -- capture function terms - e.g., f, f(x), f(g(x)), etc.
InfType        -- capture #inf terms
Model          -- provides access to a model during solve call
SolveControl   -- object to control running search
SolveFuture    -- handle for asynchronous solve calls
SolveIter      -- handle to iterate models
SolveResult    -- result of a solve call
SupType        -- capture #sup terms
TheoryAtom     -- captures theory atoms
TheoryAtomType -- the occurrence type of a theory atom
TheoryElement  -- captures theory elements
TheoryTerm     -- captures theory terms
TheoryTermType -- the type of a theory term

Example:

#script (python)
import gringo
def id(x):
    return x

def seq(x, y):
    return [x, y]

def main(prg):
    prg.ground([("base", [])])
    prg.solve()

#end.

p(@id(10)).
q(@seq(1,2)).
)";

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef gringoModule = {
    PyModuleDef_HEAD_INIT,
    "gringo",
    strGrMod,
    -1,
    gringoMethods,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif

PyObject *initgringo_() {
    if (!PyEval_ThreadsInitialized()) { PyEval_InitThreads(); }
#if PY_MAJOR_VERSION >= 3
    Object m = PyModule_Create(&gringoModule);
#else
    Object m = Py_InitModule3("gringo", gringoMethods, strGrMod);
#endif
    if (!m ||
        !SolveResult::initType(m)                   || !TheoryTermType::initType(m)       || !TheoryAtomType::initType(m)             ||
        !TheoryElement::initType(m)                 || !TheoryAtom::initType(m)           || !TheoryIter::initType(m)                 ||
        !SupType::initType(m)                       || !InfType::initType(m)              || !Fun::initType(m)                        ||
        !Model::initType(m)                         || !SolveIter::initType(m)            || !SolveFuture::initType(m)                ||
        !ControlWrap::initType(m)                   || !ConfigProxy::initType(m)          || !SolveControl::initType(m)               ||
        !DomainElement::initType(m)                 || !DomainIter::initType(m)           || !DomainProxy::initType(m)                ||
        !TheoryTerm::initType(m)                    || !TheoryPropagatorInit::initType(m) || !TheoryPropagatorAssignment::initType(m) ||
        !TheoryPropagatorSolverControl::initType(m) ||
        PyModule_AddStringConstant(m, "__version__", GRINGO_VERSION) < 0 ||
        false) { return nullptr; }
    return m.release();
}

// }}}

// {{{ auxiliary functions and objects

PyObject *valToPy(Value v) {
    switch (v.type()) {
        case Value::FUNC: {
            if (*v.name() == "") {
                FWValVec args = v.args();
                Object tuple = PyTuple_New(args.size());
                if (!tuple) { return nullptr; }
                int i = 0;
                for (auto &val : args) {
                    Object pyVal = valToPy(val);
                    if (!pyVal) { return nullptr; }
                    if (PyTuple_SetItem(tuple, i, pyVal.release()) < 0) { return nullptr; }
                    ++i;
                }
                return tuple.release();
            }
        }
        case Value::ID: {
            Object fun(Fun::tp_new(&Fun::type, nullptr, nullptr));
            if (!fun) { return nullptr; }
            reinterpret_cast<Fun*>(fun.get())->val = v;
            return fun.release();
        }
        case Value::SUP: {
            return Object(SupType::sup, true).release();
        }
        case Value::INF: {
            return Object(InfType::inf, true).release();
        }
        case Value::NUM: {
            return PyInt_FromLong(v.num());
        }
        case Value::STRING: {
            return PyString_FromString((*v.string()).c_str());
        }
        default: {
            PyErr_SetString(PyExc_RuntimeError, "cannot happen");
            return nullptr;
        }
    }

}

bool pyToVal(Object obj, Value &val) {
    PY_TRY
        if (obj->ob_type == &SupType::type) {
            val = Value::createSup();
        }
        else if (obj->ob_type == &InfType::type) {
            val = Value::createInf();
        }
        else if (obj->ob_type == &Fun::type) {
            val = reinterpret_cast<Fun*>(obj.get())->val;
        }
        else if (PyTuple_Check(obj)) {
            ValVec vals;
            if (!pyToVals(obj, vals)) { return false; }
            if (vals.size() < 2) {
                PyErr_SetString(PyExc_RuntimeError, "cannot convert to value: tuples need at least two arguments");
            }
            val = Value::createTuple(vals);
        }
        else if (PyInt_Check(obj)) {
            val = Value::createNum(int(PyInt_AsLong(obj)));
        }
        else if (PyString_Check(obj)) {
            char const *value = PyString_AsString(obj);
            if (!value) { return false; }
            val = Value::createStr(value);
        }
        else {
            PyErr_Format(PyExc_RuntimeError, "cannot convert to value: unexpected %s() object", obj->ob_type->tp_name);
            return false;
        }
        return true;
    PY_CATCH(false);
}

template <class T>
PyObject *valsToPy(T const &vals) {
    Object list = PyList_New(vals.size());
    if (!list) { return nullptr; }
    int i = 0;
    for (auto &val : vals) {
        Object pyVal = valToPy(val);
        if (!pyVal) { return nullptr; }
        if (PyList_SetItem(list, i, pyVal.release()) < 0) { return nullptr; }
        ++i;
    }
    return list.release();
}

bool pyToVals(Object obj, ValVec &vals) {
    PY_TRY
        Object it = PyObject_GetIter(obj);
        if (!it) { return false; }
        while (Object pyVal = PyIter_Next(it)) {
            Value val;
            if (!pyToVal(pyVal, val)) { return false; }
            vals.emplace_back(val);
        }
        if (PyErr_Occurred()) { return false; }
        return true;
    PY_CATCH(false);
}

// }}}

} // namespace

// {{{ definition of PythonImpl

struct PythonInit {
    PythonInit() : selfInit(!Py_IsInitialized()) {
        if (selfInit) {
#if PY_MAJOR_VERSION >= 3
            PyImport_AppendInittab("gringo", &initgringo_);
#endif
            Py_Initialize();
        }
    }
    ~PythonInit() {
        if (selfInit) { Py_Finalize(); }
    }
    bool selfInit;
};

struct PythonImpl {
    PythonImpl() {
        if (init.selfInit) {
#if PY_MAJOR_VERSION >= 3
            static wchar_t const *argv[] = {L"clingo", 0};
            PySys_SetArgvEx(1, const_cast<wchar_t**>(argv), 0);
#else
            static char const *argv[] = {"clingo", 0};
            PySys_SetArgvEx(1, const_cast<char**>(argv), 0);
#endif
        }
#if PY_MAJOR_VERSION < 3
        PyObject *sysModules = PyImport_GetModuleDict();
        if (!sysModules) { throw std::runtime_error("could not initialize python interpreter"); }
        Object gringoStr = PyString_FromString("gringo");
        if (!gringoStr) { throw std::runtime_error("could not initialize python interpreter"); }
        int ret = PyDict_Contains(sysModules, gringoStr);
        if (ret == -1) { throw std::runtime_error("could not initialize python interpreter"); }
        if (ret == 0 && !initgringo_()) { throw std::runtime_error("could not initialize gringo module"); }
#endif
        Object gringoModule = PyImport_ImportModule("gringo");
        if (!gringoModule) { throw std::runtime_error("could not initialize python interpreter"); }
        gringoModule.release(); // Note: stored in a few other places
        Object mainModule = PyImport_ImportModule("__main__");
        if (!mainModule) { throw std::runtime_error("could not initialize python interpreter"); }
        main = PyModule_GetDict(mainModule);
        if (!main) { throw std::runtime_error("could not initialize python interpreter"); }
    }
    bool exec(Location const &loc, FWString code) {
        std::ostringstream oss;
        oss << "<" << loc << ">";
        if (!pyExec((*code).c_str(), oss.str().c_str(), main)) { return false; }
        return true;
    }
    bool callable(FWString name) {
        Object fun = PyMapping_GetItemString(main, const_cast<char*>((*name).c_str()));
        PyErr_Clear();
        bool ret = fun && PyCallable_Check(fun);
        return ret;
    }
    bool call(FWString name, ValVec const &args, ValVec &vals) {
        Object fun = PyMapping_GetItemString(main, const_cast<char*>((*name).c_str()));
        return fun && pycall(fun, args, vals);
    }
    bool call(Gringo::Control &ctl) {
        Object fun = PyMapping_GetItemString(main, const_cast<char*>("main"));
        if (!fun) { return false; }
        Object params = PyTuple_New(1);
        if (!params) { return false; }
        Object param(ControlWrap::new_(ctl));
        if (!param) { return false; }
        if (PyTuple_SetItem(params, 0, param.release()) < 0) { return false; }
        Object ret = PyObject_Call(fun, params, Py_None);
        if (!ret) { return false; }
        return true;
    }
    PythonInit init;
    PyObject  *main;
};

// }}}
// {{{ definition of Python

std::unique_ptr<PythonImpl> Python::impl = nullptr;

Python::Python(GringoModule &module) {
    ControlWrap::module = &module;
}
bool Python::exec(Location const &loc, FWString code) {
    if (!impl) { impl = gringo_make_unique<PythonImpl>(); }
    if (!impl->exec(loc, code)) {
        handleError(loc, "parsing failed");
        return false;
    }
    return true;
}
bool Python::callable(FWString name) {
    if (Py_IsInitialized() && !impl) { impl = gringo_make_unique<PythonImpl>(); }
    return impl && impl->callable(name);
}
ValVec Python::call(Location const &loc, FWString name, ValVec const &args) {
    assert(impl);
    ValVec vals;
    if (!impl->call(name, args, vals)) {
        GRINGO_REPORT(W_OPERATION_UNDEFINED)
            << loc << ": info: operation undefined:\n"
            << errorToString()
            ;
        return {};
    }
    return vals;
}
void Python::main(Gringo::Control &ctl) {
    assert(impl);
    if (!impl->call(ctl)) {
        Location loc("<internal>", 1, 1, "<internal>", 1, 1);
        handleError(loc, "error while calling main function");
        return;
    }
}
Python::~Python() = default;

void *Python::initlib(Gringo::GringoModule &module) {
    PY_TRY
        ControlWrap::module = &module;
        PyObject *ret = initgringo_();
        return ret;
    PY_CATCH(nullptr);
}

// }}}

} // namespace Gringo

#else // WITH_PYTHON

#include "gringo/python.hh"
#include "gringo/value.hh"
#include "gringo/locatable.hh"
#include "gringo/logger.hh"

namespace Gringo {

// {{{ definition of Python

struct PythonImpl { };

std::unique_ptr<PythonImpl> Python::impl = nullptr;

Python::Python(GringoModule &) { }
bool Python::exec(Location const &loc, FWString ) {
    GRINGO_REPORT(E_ERROR)
        << loc << ": error: gringo has been build without python support\n"
        ;
    throw std::runtime_error("grounding stopped because of errors");
}
bool Python::callable(FWString) {
    return false;
}
ValVec Python::call(Location const &, FWString , ValVec const &) {
    return {};
}
void Python::main(Control &) { }
Python::~Python() = default;
void *Python::initlib(Gringo::GringoModule &) {
    throw std::runtime_error("gringo lib has been build without python support");
}

// }}}

} // namespace Gringo

#endif // WITH_PYTHON
