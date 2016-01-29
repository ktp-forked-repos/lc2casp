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

#ifndef _GRINGO_CONTROL_HH
#define _GRINGO_CONTROL_HH

#include <gringo/value.hh>
#include <gringo/types.hh>
#include <gringo/locatable.hh>
#include <potassco/clingo.h>

namespace Gringo {

enum class SolveResult { UNKNOWN=0, SAT=1, UNSAT=2 };

// {{{1 declaration of Context

struct Context {
    virtual bool callable(FWString name) const = 0;
    virtual ValVec call(Location const &loc, FWString name, ValVec const &args) = 0;
    virtual ~Context() noexcept = default;
};

// {{{1 declaration of Model

using Int64Vec = std::vector<int64_t>;

struct Model {
    using LitVec = std::vector<std::pair<bool,Gringo::Value>>;
    static const unsigned CSP   = 1;
    static const unsigned SHOWN = 2;
    static const unsigned ATOMS = 4;
    static const unsigned TERMS = 8;
    static const unsigned COMP  = 16;
    virtual bool contains(Value atom) const = 0;
    virtual ValVec const &atoms(int showset) const = 0;
    virtual Int64Vec optimization() const = 0;
    virtual void addClause(LitVec const &lits) const = 0;
    virtual ~Model() { }
};

// {{{1 declaration of Statistics

struct Statistics {
    enum Error { error_none = 0, error_unknown_quantity = 1, error_ambiguous_quantity = 2, error_not_available = 3 };
    struct Quantity {
        Quantity(double d) : rep(d) { assert(d >= 0.0); }
        Quantity(Error e) : rep(-double(int(e))) { assert(e != error_none); }
        bool     valid()  const { return error() == error_none; }
        Error    error()  const { return rep >= 0.0 ? error_none : static_cast<Error>(int(-rep)); }
        operator bool()   const { return valid(); }
        operator double() const { return valid() ? rep : std::numeric_limits<double>::quiet_NaN(); }
    private:
        double rep;
    };
    virtual Quantity    getStat(char const* key) const = 0;
    virtual char const *getKeys(char const* key) const = 0;
    virtual ~Statistics() { }
};

// {{{1 declaration of SolveFuture

struct SolveFuture {
    virtual SolveResult get() = 0;
    virtual void wait() = 0;
    virtual bool wait(double timeout) = 0;
    virtual void cancel() = 0;
    virtual ~SolveFuture() { }
};

// {{{1 declaration of SolveIter

struct SolveIter {
    virtual Model const *next() = 0;
    virtual void close() = 0;
    virtual SolveResult get() = 0;
    virtual ~SolveIter() { }
};

// {{{1 declaration of ConfigProxy

struct ConfigProxy {
    virtual bool hasSubKey(unsigned key, char const *name, unsigned* subKey = nullptr) = 0;
    virtual unsigned getSubKey(unsigned key, char const *name) = 0;
    virtual unsigned getArrKey(unsigned key, unsigned idx) = 0;
    virtual void getKeyInfo(unsigned key, int* nSubkeys = 0, int* arrLen = 0, const char** help = 0, int* nValues = 0) const = 0;
    virtual const char* getSubKeyName(unsigned key, unsigned idx) const = 0;
    virtual bool getKeyValue(unsigned key, std::string &value) = 0;
    virtual void setKeyValue(unsigned key, const char *val) = 0;
    virtual unsigned getRootKey() = 0;
};

// {{{1 declaration of DomainProxy

struct DomainProxy {
    struct Element;
    using ElementPtr = std::unique_ptr<Element>;
    struct Element {
        virtual Value atom() const = 0;
        virtual Potassco::Lit_t literal() const = 0;
        virtual bool fact() const = 0;
        virtual bool external() const = 0;
        virtual ElementPtr next() = 0;
        virtual bool valid() const = 0;
        virtual ~Element() { };
    };
    virtual ElementPtr iter(Signature const &sig) const = 0;
    virtual ElementPtr iter() const = 0;
    virtual ElementPtr lookup(Value const &atom) const = 0;
    virtual std::vector<FWSignature> signatures() const = 0;
    virtual size_t length() const = 0;
    virtual ~DomainProxy() { }
};

struct TheoryData {
    enum class TermType { Tuple, List, Set, Function, Number, Symbol };
    enum class AtomType { Head, Body, Directive };

    virtual TermType termType(Id_t) const = 0;
    virtual int termNum(Id_t value) const = 0;
    virtual char const *termName(Id_t value) const = 0;
    virtual Potassco::IdSpan termArgs(Id_t value) const = 0;
    virtual Potassco::IdSpan elemTuple(Id_t value) const = 0;
    // This shall map to ids of literals in aspif format.
    virtual Potassco::LitSpan elemCond(Id_t value) const = 0;
    virtual Lit_t elemCondLit(Id_t value) const = 0;
    virtual AtomType atomType(Id_t) const = 0;
    virtual Potassco::IdSpan atomElems(Id_t value) const = 0;
    virtual Potassco::Id_t atomTerm(Id_t value) const = 0;
    virtual bool atomHasGuard(Id_t value) const = 0;
    virtual Potassco::Lit_t atomLit(Id_t value) const = 0;
    virtual std::pair<char const *, Id_t> atomGuard(Id_t value) const = 0;
    virtual Potassco::Id_t numAtoms() const = 0;
    virtual std::string termStr(Id_t value) const = 0;
    virtual std::string elemStr(Id_t value) const = 0;
    virtual std::string atomStr(Id_t value) const = 0;
    virtual ~TheoryData() noexcept = default;
};

// {{{1 declaration of Control

struct TheoryPropagator : Potassco::AbstractPropagator {
    struct Init {
        virtual TheoryData const &theory() const = 0;
        virtual DomainProxy &getDomain() = 0;
        virtual Lit_t mapLit(Lit_t lit) = 0;
        virtual void addWatch(Lit_t lit) = 0;
        virtual ~Init() noexcept = default;
    };
    virtual ~TheoryPropagator() noexcept = default;
    virtual void init(Init &init) = 0;
};

// {{{1 declaration of Control

using FWStringVec = std::vector<FWString>;

struct Control {
    using ModelHandler = std::function<bool (Model const &)>;
    using FinishHandler = std::function<void (SolveResult, bool)>;
    using Assumptions = std::vector<std::pair<Value, bool>>;
    using GroundVec = std::vector<std::pair<std::string, FWValVec>>;
    using NewControlFunc = Control* (*)(int, char const **);
    using FreeControlFunc = void (*)(Control *);

    virtual ConfigProxy &getConf() = 0;
    virtual DomainProxy &getDomain() = 0;

    virtual void ground(GroundVec const &vec, Context *context) = 0;
    virtual SolveResult solve(ModelHandler h, Assumptions &&assumptions) = 0;
    virtual SolveFuture *solveAsync(ModelHandler mh, FinishHandler fh, Assumptions &&assumptions) = 0;
    virtual SolveIter *solveIter(Assumptions &&assumptions) = 0;
    virtual void add(std::string const &name, FWStringVec const &params, std::string const &part) = 0;
    virtual void load(std::string const &filename) = 0;
    virtual Value getConst(std::string const &name) = 0;
    virtual bool blocked() = 0;
    virtual void assignExternal(Value ext, Potassco::Value_t val) = 0;
    virtual Statistics *getStats() = 0;
    virtual void useEnumAssumption(bool enable) = 0;
    virtual bool useEnumAssumption() = 0;
    virtual void cleanupDomains() = 0;
    virtual TheoryData const &theory() const = 0;
    virtual void registerPropagator(TheoryPropagator &p) = 0;
    virtual ~Control() { }
};

// {{{1 declaration of Gringo

struct GringoModule {
    virtual Control *newControl(int argc, char const **argv) = 0;
    virtual void freeControl(Control *ctrl) = 0;
    virtual Value parseValue(std::string const &repr) = 0;
    virtual ~GringoModule() { }
};

// }}}1

} // namespace Gringo

#endif // _GRINGO_CONTROL_HH

