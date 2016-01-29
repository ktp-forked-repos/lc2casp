// {{{ GPL License

// This file is part of gringo - a grounder for logic programs.
// Copyright (C) 2013  Benjamin Kaufmann
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

#include "clingo/clingocontrol.hh"
#include "clasp/solver.h"
#include <program_opts/typed_value.h>
#include <program_opts/application.h>
#include <potassco/basic_types.h>

// {{{1 definition of ClaspAPIBackend

void ClaspAPIBackend::init(bool) {

}

void ClaspAPIBackend::endStep() {

}

void ClaspAPIBackend::beginStep() {
}

void ClaspAPIBackend::addBody(const LitVec& body) {
    for (auto x : body) {
        body_.add((Clasp::Var)std::abs(x), x > 0);
    }
}

void ClaspAPIBackend::addBody(const LitWeightVec& body) {
    for (auto x : body) {
        body_.add((Clasp::Var)std::abs(x.lit), x.lit > 0, x.weight);
    }
}

Potassco::Id_t ClaspAPIBackend::addCond(const LitVec& body) {
    return prg_.newCondition(Potassco::toSpan(body));
}

void ClaspAPIBackend::printHead(bool choice, AtomVec const &atoms) {
    head_.reset(choice ? Clasp::Asp::Head_t::Choice : Clasp::Asp::Head_t::Disjunctive);
    for (auto x : atoms) { head_.add(x); }
}

void ClaspAPIBackend::printNormalBody(LitVec const &body) {
    body_.reset(Potassco::Body_t::Normal);
    addBody(body);
    prg_.addRule(head_.toView(), body_.toView());
}

void ClaspAPIBackend::printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) {
    body_.reset(Potassco::Body_t::Sum);
    body_.bound = lower;
    addBody(body);
    prg_.addRule(head_.toView(), body_.toView());
}

void ClaspAPIBackend::printMinimize(int priority, LitWeightVec const &body) {
    body_.reset(Potassco::Body_t::Sum);
    prg_.addMinimize(priority, Potassco::toSpan(body));
}

void ClaspAPIBackend::printProject(AtomVec const &lits) {
    prg_.addProject(Potassco::toSpan(lits));
}

void ClaspAPIBackend::printOutput(char const *symbol, LitVec const &body) {
    prg_.addOutput(symbol, addCond(body));
}

void ClaspAPIBackend::printEdge(unsigned u, unsigned v, LitVec const &body) {
    prg_.addAcycEdge(u, v, addCond(body));
}

void ClaspAPIBackend::printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) {
    prg_.addDomHeuristic(atom, modifier, value, priority, addCond(body));
}

void ClaspAPIBackend::printAssume(LitVec const &lits) {
    prg_.addAssumption(Potassco::toSpan(lits));
}

void ClaspAPIBackend::printExternal(Potassco::Atom_t atomUid, Potassco::Value_t type) {
    switch (type) {
        case Potassco::Value_t::False:   { prg_.freeze(atomUid, Clasp::value_false); break; }
        case Potassco::Value_t::True:    { prg_.freeze(atomUid, Clasp::value_true); break; }
        case Potassco::Value_t::Free:    { prg_.freeze(atomUid, Clasp::value_free); break; }
        case Potassco::Value_t::Release: { prg_.unfreeze(atomUid); break; }
    }
}

void ClaspAPIBackend::printTheoryAtom(Potassco::TheoryAtom const &atom, GetCond getCond) {
    for (auto&& e : atom.elements()) {
        Potassco::TheoryElement const &elem = data_.getElement(e);
        if (elem.condition() == Potassco::TheoryData::COND_DEFERRED) {
            Potassco::LitVec cond;
            data_.setCondition(e, prg_.newCondition(Potassco::toSpan(getCond(e))));
        }
    }
}

ClaspAPIBackend::~ClaspAPIBackend() noexcept = default;

// {{{1 definition of ClingoControl

#define LOG if (verbose_) std::cerr
ClingoControl::ClingoControl(Gringo::Scripts &scripts, bool clingoMode, Clasp::ClaspFacade *clasp, Clasp::Cli::ClaspCliConfig &claspConfig, PostGroundFunc pgf, PreSolveFunc psf)
    : scripts(scripts)
    , clasp(clasp)
    , claspConfig_(claspConfig)
    , pgf_(pgf)
    , psf_(psf)
    , clingoMode_(clingoMode) { }

void ClingoControl::parse_() {
    if (!parser->empty()) {
        parser->parse();
        defs.init();
        parsed = true;
    }
    if (Gringo::message_printer()->hasError()) {
        throw std::runtime_error("parsing failed");
    }
}

Potassco::Lit_t ClingoTheoryInit::mapLit(Lit_t lit) {
    const auto& prg = static_cast<Clasp::Asp::LogicProgram&>(*static_cast<ClingoControl&>(c_).clasp->program());
    return Clasp::encodeLit(Clasp::Asp::solverLiteral(prg, lit));
}

void ClingoControl::parse(const StringSeq& files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* claspOut, bool addStdIn) {
    using namespace Gringo;
    if (opts.wNoOperationUndefined) { message_printer()->disable(W_OPERATION_UNDEFINED); }
    if (opts.wNoAtomUndef)          { message_printer()->disable(W_ATOM_UNDEFINED); }
    if (opts.wNoVariableUnbounded)  { message_printer()->disable(W_VARIABLE_UNBOUNDED); }
    if (opts.wNoFileIncluded)       { message_printer()->disable(W_FILE_INCLUDED); }
    if (opts.wNoGlobalVariable)     { message_printer()->disable(W_GLOBAL_VARIABLE); }
    verbose_ = opts.verbose;
    Output::OutputPredicates outPreds;
    for (auto &x : opts.foobar) {
        outPreds.emplace_back(Location("<cmd>",1,1,"<cmd>", 1,1), x, false);
    }
    if (claspOut) {
        auto create = [&](Output::TheoryData &data) -> Output::UBackend { return gringo_make_unique<ClaspAPIBackend>(const_cast<Potassco::TheoryData&>(data.data()), *claspOut); };
        out = gringo_make_unique<Output::OutputBase>(create, claspOut->theoryData(), std::move(outPreds), opts.outputDebug);
    }
    else {
        data_ = gringo_make_unique<Potassco::TheoryData>();
        out = gringo_make_unique<Output::OutputBase>(*data_, std::move(outPreds), std::cout, opts.outputFormat, opts.outputDebug);
    }
    out->keepFacts = opts.keepFacts;
    pb = gringo_make_unique<Input::NongroundProgramBuilder>(scripts, prg, *out, defs, opts.rewriteMinimize);
    parser = gringo_make_unique<Input::NonGroundParser>(*pb);
    for (auto &x : opts.defines) {
        LOG << "define: " << x << std::endl;
        parser->parseDefine(x);
    }
    for (auto x : files) {
        LOG << "file: " << x << std::endl;
        parser->pushFile(std::move(x));
    }
    if (files.empty() && addStdIn) {
        LOG << "reading from stdin" << std::endl;
        parser->pushFile("-");
    }
    parse_();
}

bool ClingoControl::update() {
    if (clingoMode_) {
        clasp->update(configUpdate_);
        configUpdate_ = false;
        return clasp->ok();
    }
    return true;
}

void ClingoControl::ground(Gringo::Control::GroundVec const &parts, Gringo::Context *context) {
    if (!update()) { return; }
    if (parsed) {
        LOG << "************** parsed program **************" << std::endl << prg;
        prg.rewrite(defs);
        LOG << "************* rewritten program ************" << std::endl << prg;
        prg.check();
        if (Gringo::message_printer()->hasError()) {
            throw std::runtime_error("grounding stopped because of errors");
        }
        parsed = false;
    }
    if (!grounded) {
        out->beginStep();
        grounded = true;
    }
    if (!parts.empty()) {
        Gringo::Ground::Parameters params;
        for (auto &x : parts) { params.add(x.first, x.second); }
        auto gPrg = prg.toGround(out->data);
        LOG << "*********** intermediate program ***********" << std::endl << gPrg << std::endl;
        LOG << "************* grounded program *************" << std::endl;
        auto exit = Gringo::onExit([this]{ scripts.context = nullptr; });
        scripts.context = context;
        gPrg.ground(params, scripts, *out, false);
    }
}

void ClingoControl::main() {
    if (scripts.callable("main")) {
        out->init(true);
        clasp->enableProgramUpdates();
        scripts.main(*this);
    }
    else {
        out->init(false);
        claspConfig_.releaseOptions();
        Gringo::Control::GroundVec parts;
        parts.emplace_back("base", Gringo::FWValVec{});
        ground(parts, nullptr);
        solve(nullptr, {});
    }
}
bool ClingoControl::onModel(Clasp::Model const &m) {
    return !modelHandler || modelHandler(ClingoModel(static_cast<Clasp::Asp::LogicProgram&>(*clasp->program()), *out, clasp->ctx, &m));
}
void ClingoControl::onFinish(Clasp::ClaspFacade::Result ret) {
    if (finishHandler) {
        finishHandler(convert(ret), ret.interrupted());
        finishHandler = nullptr;
    }
    modelHandler = nullptr;
}
Gringo::Value ClingoControl::getConst(std::string const &name) {
    auto ret = defs.defs().find(name);
    if (ret != defs.defs().end()) {
        bool undefined = false;
        Gringo::Value val = std::get<2>(ret->second)->eval(undefined);
        if (!undefined) { return val; }
    }
    return Gringo::Value();
}
void ClingoControl::add(std::string const &name, Gringo::FWStringVec const &params, std::string const &part) {
    Gringo::Location loc("<block>", 1, 1, "<block>", 1, 1);
    Gringo::Input::IdVec idVec;
    for (auto &x : params) { idVec.emplace_back(loc, x); }
    parser->pushBlock(name, std::move(idVec), part);
    parse_();
}
void ClingoControl::load(std::string const &filename) {
    parser->pushFile(std::string(filename));
    parse_();
}
bool ClingoControl::hasSubKey(unsigned key, char const *name, unsigned* subKey) {
    *subKey = claspConfig_.getKey(key, name);
    return *subKey != Clasp::Cli::ClaspCliConfig::KEY_INVALID;
}
unsigned ClingoControl::getSubKey(unsigned key, char const *name) {
    unsigned ret = claspConfig_.getKey(key, name);
    if (ret == Clasp::Cli::ClaspCliConfig::KEY_INVALID) {
        throw std::runtime_error("invalid key");
    }
    return ret;
}
unsigned ClingoControl::getArrKey(unsigned key, unsigned idx) {
    unsigned ret = claspConfig_.getArrKey(key, idx);
    if (ret == Clasp::Cli::ClaspCliConfig::KEY_INVALID) {
        throw std::runtime_error("invalid key");
    }
    return ret;
}
void ClingoControl::getKeyInfo(unsigned key, int* nSubkeys, int* arrLen, const char** help, int* nValues) const {
    if (claspConfig_.getKeyInfo(key, nSubkeys, arrLen, help, nValues) < 0) {
        throw std::runtime_error("could not get key info");
    }
}
const char* ClingoControl::getSubKeyName(unsigned key, unsigned idx) const {
    char const *ret = claspConfig_.getSubkey(key, idx);
    if (!ret) {
        throw std::runtime_error("could not get subkey");
    }
    return ret;
}
bool ClingoControl::getKeyValue(unsigned key, std::string &value) {
    int ret = claspConfig_.getValue(key, value);
    if (ret < -1) {
        throw std::runtime_error("could not get option value");
    }
    return ret >= 0;
}
void ClingoControl::setKeyValue(unsigned key, const char *val) {
    configUpdate_ = true;
    if (claspConfig_.setValue(key, val) <= 0) {
        throw std::runtime_error("could not set option value");
    }
}
unsigned ClingoControl::getRootKey() {
    return Clasp::Cli::ClaspCliConfig::KEY_ROOT;
}
Gringo::ConfigProxy &ClingoControl::getConf() {
    return *this;
}
Gringo::SolveIter *ClingoControl::solveIter(Assumptions &&ass) {
    if (!clingoMode_) { throw std::runtime_error("solveIter is not supported in gringo gringo mode"); }
#if WITH_THREADS
    prepare_(nullptr, nullptr);
    auto a = toClaspAssumptions(std::move(ass));
    solveIter_ = Gringo::gringo_make_unique<ClingoSolveIter>(clasp->startSolveAsync(a), static_cast<Clasp::Asp::LogicProgram&>(*clasp->program()), *out, clasp->ctx);
    return solveIter_.get();
#else
    (void)ass;
    throw std::runtime_error("solveIter requires clingo to be build with thread support");
#endif
}
Gringo::SolveFuture *ClingoControl::solveAsync(ModelHandler mh, FinishHandler fh, Assumptions &&ass) {
    if (!clingoMode_) { throw std::runtime_error("solveAsync is not supported in gringo gringo mode"); }
#if WITH_THREADS
    prepare_(mh, fh);
    auto a = toClaspAssumptions(std::move(ass));
    solveFuture_ = Gringo::gringo_make_unique<ClingoSolveFuture>(clasp->solveAsync(nullptr, a));
    return solveFuture_.get();
#else
    (void)mh;
    (void)fh;
    (void)ass;
    throw std::runtime_error("solveAsync requires clingo to be build with thread support");
#endif
}
bool ClingoControl::blocked() {
    return clasp->solving();
}
void ClingoControl::prepare_(Gringo::Control::ModelHandler mh, Gringo::Control::FinishHandler fh) {
    grounded = false;
    if (update()) { out->endStep(); }
    if (clingoMode_) {
#if WITH_THREADS
        solveIter_    = nullptr;
        solveFuture_  = nullptr;
#endif
        finishHandler = fh;
        modelHandler  = mh;
        Clasp::ProgramBuilder *prg = clasp->program();
        if (pgf_) { pgf_(*prg); }
        if (!propagators_.empty()) {
            clasp->program()->endProgram();
            for (auto&& pp : propagators_) {
                ClingoTheoryInit i(*this, *pp);
                static_cast<Gringo::TheoryPropagator*>(pp->propagator())->init(i);
            }
        }
        clasp->prepare(enableEnumAssupmption_ ? Clasp::ClaspFacade::enum_volatile : Clasp::ClaspFacade::enum_static);
        if (psf_) { psf_(*clasp);}
    }
    out->reset();
}

Clasp::LitVec ClingoControl::toClaspAssumptions(Gringo::Control::Assumptions &&ass) const {
    Clasp::LitVec outAss;
    if (!clingoMode_ || !clasp->program()) { return outAss; }
    const Clasp::Asp::LogicProgram* prg = static_cast<const Clasp::Asp::LogicProgram*>(clasp->program());
    for (auto &x : ass) {
        auto atm = out->find(x.first);
        if (atm.second && atm.first->hasUid()) {
            Clasp::Literal lit = prg->getLiteral(atm.first->uid());
            outAss.push_back(x.second ? lit : ~lit);
        }
        else if (x.second) {
            outAss.push_back(Clasp::lit_false());
            break;
        }
    }
    return outAss;
}

Gringo::SolveResult ClingoControl::solve(ModelHandler h, Assumptions &&ass) {
    prepare_(h, nullptr);
    return clingoMode_ ? convert(clasp->solve(nullptr, toClaspAssumptions(std::move(ass)))) : Gringo::SolveResult::UNKNOWN;
}
void ClingoControl::registerPropagator(Gringo::TheoryPropagator &p) {
    propagators_.emplace_back(Gringo::gringo_make_unique<Clasp::TheoryPropagator>(p));
    claspConfig_.addTheoryPropagator(propagators_.back().get(), Clasp::Ownership_t::Retain);
}
void ClingoControl::cleanupDomains() {
    prepare_(nullptr, nullptr);
    if (clingoMode_) {
        Clasp::Asp::LogicProgram &prg = static_cast<Clasp::Asp::LogicProgram&>(*clasp->program());
        Clasp::Solver &solver = *clasp->ctx.master();
        auto assignment = [&prg, &solver](unsigned uid) {
            Clasp::Literal lit = prg.getLiteral(uid);
            Potassco::Value_t               truth = Potassco::Value_t::Free;
            if (solver.isTrue(lit))       { truth = Potassco::Value_t::True; }
            else if (solver.isFalse(lit)) { truth = Potassco::Value_t::False; }
            return std::make_pair(prg.isExternal(uid), truth);
        };
        auto stats = out->simplify(assignment);
        LOG << stats.first << " atom" << (stats.first == 1 ? "" : "s") << " became facts" << std::endl;
        LOG << stats.second << " atom" << (stats.second == 1 ? "" : "s") << " deleted" << std::endl;
    }
}

std::string ClingoControl::str() {
    return "[object:IncrementalControl]";
}

void ClingoControl::assignExternal(Gringo::Value ext, Potassco::Value_t val) {
    if (update()) {
        auto atm = out->find(ext);
        if (atm.second && atm.first->hasUid()) {
            Gringo::Id_t offset = atm.first - atm.second->begin();
            Gringo::Output::External external(Gringo::Output::LiteralId{Gringo::NAF::POS, Gringo::Output::AtomType::Predicate, offset, atm.second->domainOffset()}, val);
            out->output(external);
        }
    }
}
ClingoStatistics *ClingoControl::getStats() {
    clingoStats.clasp = clasp;
    return &clingoStats;
}
void ClingoControl::useEnumAssumption(bool enable) {
    enableEnumAssupmption_ = enable;
}
bool ClingoControl::useEnumAssumption() {
    return enableEnumAssupmption_;
}

Gringo::DomainProxy &ClingoControl::getDomain() {
    if (clingoMode_) { return *this; }
    else {
        throw std::runtime_error("domain introspection only supported in clingo mode");
    }
}

namespace {

static bool skipDomain(Gringo::FWSignature sig) {
    return (strncmp((*(*sig).name()).c_str(), "#", 1) == 0);
}

struct ClingoDomainElement : Gringo::DomainProxy::Element {
    using ElemIt = Gringo::Output::PredicateDomain::Iterator;
    using DomIt = Gringo::Output::PredDomMap::Iterator;
    ClingoDomainElement(Gringo::Output::OutputBase &out, Clasp::Asp::LogicProgram &prg, DomIt const &domIt, ElemIt const &elemIt, bool advanceDom = true)
    : out(out)
    , prg(prg)
    , domIt(domIt)
    , elemIt(elemIt)
    , advanceDom(advanceDom) {
        assert(domIt != out.predDoms().end() && elemIt != (*domIt)->end());
    }
    Gringo::Value atom() const {
        return *elemIt;
    }
    bool fact() const {
        return elemIt->fact();
    }
    bool external() const {
        return
            elemIt->hasUid() &&
            elemIt->isExternal() &&
            prg.isExternal(elemIt->uid());
    }

    Potassco::Lit_t literal() const {
        return elemIt->hasUid() ? elemIt->uid() : 0;
    }

    static std::unique_ptr<ClingoDomainElement> init(Gringo::Output::OutputBase &out, Clasp::Asp::LogicProgram &prg, bool advanceDom, Gringo::Output::PredDomMap::Iterator domIt) {
        for (; domIt != out.predDoms().end(); ++domIt) {
            if (!skipDomain(**domIt)) {
                auto elemIt = (*domIt)->begin();
                if (elemIt != (*domIt)->end()) {
                    return Gringo::gringo_make_unique<ClingoDomainElement>(out, prg, domIt, elemIt, advanceDom);
                }
            }
            if (!advanceDom) { return nullptr; }
        }
        return nullptr;
    }

    static std::unique_ptr<ClingoDomainElement> advance(Gringo::Output::OutputBase &out, Clasp::Asp::LogicProgram &prg, bool advanceDom, Gringo::Output::PredDomMap::Iterator domIt, Gringo::Output::PredicateDomain::Iterator elemIt) {
        auto domIe  = out.predDoms().end();
        auto elemIe = (*domIt)->end();
        ++elemIt;
        while (elemIt == elemIe) {
            if (!advanceDom) { return nullptr; }
            ++domIt;
            if (domIt == domIe) { return nullptr; }
            if (!skipDomain(**domIt)) {
                elemIt = (*domIt)->begin();
                elemIe = (*domIt)->end();
            }
        }
        return Gringo::gringo_make_unique<ClingoDomainElement>(out, prg, domIt, elemIt, advanceDom);
    }

    Gringo::DomainProxy::ElementPtr next() {
        return advance(out, prg, advanceDom, domIt, elemIt);
    }
    bool valid() const {
        return domIt != out.predDoms().end();
    }
    Gringo::Output::OutputBase &out;
    Clasp::Asp::LogicProgram &prg;
    DomIt domIt;
    ElemIt elemIt;
    bool advanceDom;
};

} // namespace

std::vector<Gringo::FWSignature> ClingoControl::signatures() const {
    std::vector<Gringo::FWSignature> ret;
    for (auto &dom : out->predDoms()) {
        if (!skipDomain(*dom)) { ret.emplace_back(*dom); }
    }
    return ret;
}

Gringo::DomainProxy::ElementPtr ClingoControl::iter(Gringo::Signature const &sig) const {
    return ClingoDomainElement::init(*out, static_cast<Clasp::Asp::LogicProgram&>(*clasp->program()), false, out->predDoms().find(sig));
}

Gringo::DomainProxy::ElementPtr ClingoControl::iter() const {
    return ClingoDomainElement::init(*out, static_cast<Clasp::Asp::LogicProgram&>(*clasp->program()), true, out->predDoms().begin());
}

Gringo::DomainProxy::ElementPtr ClingoControl::lookup(Gringo::Value const &atom) const {
    if (atom.hasSig()) {
        auto it = out->predDoms().find(atom.sig());
        if (it != out->predDoms().end()) {
            auto jt = (*it)->find(atom);
            if (jt != (*it)->end()) {
                return Gringo::gringo_make_unique<ClingoDomainElement>(*out, static_cast<Clasp::Asp::LogicProgram&>(*clasp->program()), it, jt);
            }
        }
    }
    return nullptr;
}

size_t ClingoControl::length() const {
    size_t ret = 0;
    for (auto &dom : out->predDoms()) {
        if (!skipDomain(*dom)) {
            ret += dom->size();
        }
    }
    return ret;
}

// {{{1 definition of ClingoStatistics

Gringo::Statistics::Quantity ClingoStatistics::getStat(char const* key) const {
    if (!clasp) { return std::numeric_limits<double>::quiet_NaN(); }
    auto ret = clasp->getStat(key);
    switch (ret.error()) {
        case Clasp::ExpectedQuantity::error_ambiguous_quantity: { return Gringo::Statistics::error_ambiguous_quantity; }
        case Clasp::ExpectedQuantity::error_not_available:      { return Gringo::Statistics::error_not_available; }
        case Clasp::ExpectedQuantity::error_unknown_quantity:   { return Gringo::Statistics::error_unknown_quantity; }
        case Clasp::ExpectedQuantity::error_none:               { return (double)ret; }
    }
    return std::numeric_limits<double>::quiet_NaN();
}
char const *ClingoStatistics::getKeys(char const* key) const {
    if (!clasp) { return ""; }
    return clasp->getKeys(key);
}

// {{{1 definition of ClingoSolveIter

#if WITH_THREADS
ClingoSolveIter::ClingoSolveIter(Clasp::ClaspFacade::AsyncResult const &future, Clasp::Asp::LogicProgram const &lp, Gringo::Output::OutputBase const &out, Clasp::SharedContext const &ctx)
    : future(future)
    , model(lp, out, ctx) { }
Gringo::Model const *ClingoSolveIter::next() {
    if (model.model)  { future.next(); }
    if (future.end()) { return nullptr; }
    model.reset(future.model());
    return &model;
}
void ClingoSolveIter::close() {
    if (!future.end()) { future.cancel(); }
}
Gringo::SolveResult ClingoSolveIter::get() {
    return convert(future.get());
}
ClingoSolveIter::~ClingoSolveIter() = default;
#endif

// {{{1 definition of ClingoSolveFuture

Gringo::SolveResult convert(Clasp::ClaspFacade::Result res) {
    switch (res) {
        case Clasp::ClaspFacade::Result::SAT:     { return Gringo::SolveResult::SAT; }
        case Clasp::ClaspFacade::Result::UNSAT:   { return Gringo::SolveResult::UNSAT; }
        case Clasp::ClaspFacade::Result::UNKNOWN: { return Gringo::SolveResult::UNKNOWN; }
    }
    return Gringo::SolveResult::UNKNOWN;
}

#if WITH_THREADS
ClingoSolveFuture::ClingoSolveFuture(Clasp::ClaspFacade::AsyncResult const &res)
    : future(res) { }
Gringo::SolveResult ClingoSolveFuture::get() {
    if (!done) {
        bool stop = future.interrupted() == SIGINT;
        ret       = convert(future.get());
        done      = true;
        if (stop) { throw std::runtime_error("solving stopped by signal"); }
    }
    return ret;
}
void ClingoSolveFuture::wait() { get(); }
bool ClingoSolveFuture::wait(double timeout) {
    if (!done) {
        if (timeout == 0 ? !future.ready() : !future.waitFor(timeout)) { return false; }
        wait();
    }
    return true;
}
void ClingoSolveFuture::cancel() { future.cancel(); }
ClingoSolveFuture::~ClingoSolveFuture() { }
#endif

// {{{1 definition of ClingoLib

ClingoLib::ClingoLib(Gringo::Scripts &scripts, int argc, char const **argv)
        : ClingoControl(scripts, true, &clasp_, claspConfig_, nullptr, nullptr) {
    using namespace ProgramOptions;
    OptionContext allOpts("<pyclingo>");
    initOptions(allOpts);
    ParsedValues values = parseCommandLine(argc, const_cast<char**>(argv), allOpts, false, parsePositional);
    ParsedOptions parsed;
    parsed.assign(values);
    allOpts.assignDefaults(parsed);
    claspConfig_.finalize(parsed, Clasp::Problem_t::Asp, true);
    clasp_.ctx.setEventHandler(this);
    Clasp::Asp::LogicProgram* lp = &clasp_.startAsp(claspConfig_, true);
    parse({}, grOpts_, lp, false);
    out->init(true);
}


static bool parseConst(const std::string& str, std::vector<std::string>& out) {
    out.push_back(str);
    return true;
}

void ClingoLib::initOptions(ProgramOptions::OptionContext& root) {
    using namespace ProgramOptions;
    grOpts_.defines.clear();
    grOpts_.verbose = false;
    OptionGroup gringo("Gringo Options");
    gringo.addOptions()
        ("verbose,V"                , flag(grOpts_.verbose = false), "Enable verbose output")
        ("const,c"                  , storeTo(grOpts_.defines, parseConst)->composing()->arg("<id>=<term>"), "Replace term occurences of <id> with <term>")
        ("output-debug", storeTo(grOpts_.outputDebug = Gringo::Output::OutputDebug::NONE, values<Gringo::Output::OutputDebug>()
          ("none", Gringo::Output::OutputDebug::NONE)
          ("text", Gringo::Output::OutputDebug::TEXT)
          ("translate", Gringo::Output::OutputDebug::TRANSLATE)
          ("all", Gringo::Output::OutputDebug::ALL)), "Print debug information during output:\n"
         "      none     : no additional info\n"
         "      text     : print rules as plain text (prefix %%)\n"
         "      translate: print translated rules as plain text (prefix %%%%)\n"
         "      all      : combines text and translate")
        ("warn,W"                   , storeTo(grOpts_, parseWarning)->arg("<warn>")->composing(), "Enable/disable warnings:\n"
         "      [no-]atom-undefined:        a :- b.\n"
         "      [no-]file-included:         #include \"a.lp\". #include \"a.lp\".\n"
         "      [no-]operation-undefined:   p(1/0).\n"
         "      [no-]variable-unbounded:    $x > 10.\n"
         "      [no-]global-variable:       :- #count { X } = 1, X = 1.")
        ("rewrite-minimize"         , flag(grOpts_.rewriteMinimize = false), "Rewrite minimize constraints into rules")
        ("keep-facts"               , flag(grOpts_.keepFacts = false), "Do not remove facts from normal rules")
        ;
    root.add(gringo);
    claspConfig_.addOptions(root);
}

bool ClingoLib::onModel(Clasp::Solver const&, Clasp::Model const& m) {
    return ClingoControl::onModel(m);
}
void ClingoLib::onEvent(Clasp::Event const& ev) {
#if WITH_THREADS
    Clasp::ClaspFacade::StepReady const *r = Clasp::event_cast<Clasp::ClaspFacade::StepReady>(ev);
    if (r && finishHandler) { onFinish(r->summary->result); }
#endif
    const Clasp::LogEvent* log = Clasp::event_cast<Clasp::LogEvent>(ev);
    if (log && log->isWarning()) {
        fflush(stdout);
        fprintf(stderr, "*** %-5s: (%s): %s\n", "Warn", "pyclingo", log->msg);
        fflush(stderr);
    }
}
bool ClingoLib::parsePositional(const std::string& t, std::string& out) {
    int num;
    if (bk_lib::string_cast(t, num)) {
        out = "number";
        return true;
    }
    return false;
}
ClingoLib::~ClingoLib() {
    // TODO: can be removed after bennies next update...
#if WITH_THREADS
    solveIter_   = nullptr;
    solveFuture_ = nullptr;
#endif
    clasp_.shutdown();
}

// {{{1 definition of DefaultGringoModule

DefaultGringoModule::DefaultGringoModule()
: scripts(*this) { }
Gringo::Control *DefaultGringoModule::newControl(int argc, char const **argv) {
    return new ClingoLib(scripts, argc, argv);
}

void DefaultGringoModule::freeControl(Gringo::Control *ctl) {
    if (ctl) { delete ctl; }
}
Gringo::Value DefaultGringoModule::parseValue(std::string const &str) { return parser.parse(str); }

// }}}1
