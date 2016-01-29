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

#ifndef _GRINGO_CLINGOCONTROL_HH
#define _GRINGO_CLINGOCONTROL_HH

#include <gringo/output/output.hh>
#include <gringo/input/program.hh>
#include <gringo/input/programbuilder.hh>
#include <gringo/input/nongroundparser.hh>
#include <gringo/input/groundtermparser.hh>
#include <gringo/control.hh>
#include <gringo/logger.hh>
#include <gringo/scripts.hh>
#include <clasp/logic_program.h>
#include <clasp/clasp_facade.h>
#include <clasp/clingo.h>
#include <clasp/cli/clasp_options.h>
#include <program_opts/application.h>
#include <program_opts/string_convert.h>

// {{{1 declaration of ClaspAPIBackend

class ClaspAPIBackend : public Gringo::Output::Backend {
public:
    ClaspAPIBackend(Potassco::TheoryData &data, Clasp::Asp::LogicProgram& out) : data_(data), prg_(out) { }
    void init(bool incremental) override;
    void beginStep() override;
    void printTheoryAtom(Potassco::TheoryAtom const &atom, GetCond getCond) override;
    void printHead(bool choice, AtomVec const &atoms) override;
    void printNormalBody(LitVec const &body) override;
    void printWeightBody(Potassco::Weight_t lower, LitWeightVec const &body) override;
    void printProject(AtomVec const &lits) override;
    void printOutput(char const *symbol, LitVec const &body) override;
    void printEdge(unsigned u, unsigned v, LitVec const &body) override;
    void printHeuristic(Potassco::Heuristic_t modifier, Potassco::Atom_t atom, int value, unsigned priority, LitVec const &body) override;
    void printExternal(Potassco::Atom_t atom, Potassco::Value_t value) override;
    void printAssume(LitVec const &lits) override;
    void printMinimize(int priority, LitWeightVec const &body) override;
    void endStep() override;
    ~ClaspAPIBackend() noexcept;

private:
    void addBody(const LitVec& body);
    void addBody(const LitWeightVec& body);
    void updateConditions(Potassco::IdSpan const& elems, GetCond getCond);
    Potassco::Id_t addCond(const LitVec& body);
    ClaspAPIBackend(const ClaspAPIBackend&);
    ClaspAPIBackend& operator=(const ClaspAPIBackend&);
    Potassco::TheoryData &data_;
    Clasp::Asp::LogicProgram& prg_;
    Clasp::Asp::HeadData head_;
    Clasp::Asp::BodyData body_;
    std::stringstream str_;
};

// {{{1 declaration of ClingoOptions

struct ClingoOptions {
    using Foobar = std::vector<Gringo::FWSignature>;
    ProgramOptions::StringSeq    defines;
    Gringo::Output::OutputDebug  outputDebug  = Gringo::Output::OutputDebug::NONE;
    Gringo::Output::OutputFormat outputFormat = Gringo::Output::OutputFormat::INTERMEDIATE;
    bool verbose               = false;
    bool wNoOperationUndefined = false;
    bool wNoAtomUndef          = false;
    bool wNoFileIncluded       = false;
    bool wNoVariableUnbounded  = false;
    bool wNoGlobalVariable     = false;
    bool rewriteMinimize       = false;
    bool keepFacts             = false;
    Foobar foobar;
};

inline bool parseWarning(const std::string& str, ClingoOptions& out) {
    if (str == "no-atom-undefined")        { out.wNoAtomUndef          = true;  return true; }
    if (str ==    "atom-undefined")        { out.wNoAtomUndef          = false; return true; }
    if (str == "no-file-included")         { out.wNoFileIncluded       = true;  return true; }
    if (str ==    "file-included")         { out.wNoFileIncluded       = false; return true; }
    if (str == "no-operation-undefined")   { out.wNoOperationUndefined = true;  return true; }
    if (str ==    "operation-undefined")   { out.wNoOperationUndefined = false; return true; }
    if (str == "no-variable-unbounded")    { out.wNoVariableUnbounded  = true;  return true; }
    if (str ==    "variable-unbounded")    { out.wNoVariableUnbounded  = false; return true; }
    if (str == "no-global-variable")       { out.wNoGlobalVariable     = true;  return true; }
    if (str ==    "global-variable")       { out.wNoGlobalVariable     = false; return true; }
    return false;
}

inline std::vector<std::string> split(std::string const &source, char const *delimiter = " ", bool keepEmpty = false) {
    std::vector<std::string> results;
    size_t prev = 0;
    size_t next = 0;
    while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
        if (keepEmpty || (next - prev != 0)) { results.push_back(source.substr(prev, next - prev)); }
        prev = next + 1;
    }
    if (prev < source.size()) { results.push_back(source.substr(prev)); }
    return results;
}

inline bool parseFoobar(const std::string& str, ClingoOptions::Foobar& foobar) {
    for (auto &x : split(str, ",")) {
        auto y = split(x, "/");
        if (y.size() != 2) { return false; }
        unsigned a;
        if (!bk_lib::string_cast<unsigned>(y[1], a)) { return false; }
        foobar.emplace_back(y[0], a);
    }
    return true;
}

// {{{1 declaration of ClingoStatistics

struct ClingoStatistics : Gringo::Statistics {
    virtual Quantity    getStat(char const* key) const;
    virtual char const *getKeys(char const* key) const;
    virtual ~ClingoStatistics() { }

    Clasp::ClaspFacade *clasp = nullptr;
};

// {{{1 declaration of ClingoModel

struct ClingoModel : Gringo::Model {
    ClingoModel(Clasp::Asp::LogicProgram const &lp, Gringo::Output::OutputBase const &out, Clasp::SharedContext const &ctx, Clasp::Model const *model = nullptr)
        : lp(lp)
        , out(out)
        , ctx(ctx)
        , model(model) { }
    void reset(Clasp::Model const &m) { model = &m; }
    virtual bool contains(Gringo::Value atom) const {
        auto atm = out.find(atom);
        return atm.second && atm.first->hasUid() && model->isTrue(lp.getLiteral(atm.first->uid()));
    }
    virtual Gringo::ValVec const &atoms(int atomset) const {
        atms = out.atoms(atomset, [this, atomset](unsigned uid) -> bool { return bool(atomset & COMP) ^ model->isTrue(lp.getLiteral(uid)); });
        return atms;
    }
    virtual Gringo::Int64Vec optimization() const {
        return model->costs ? Gringo::Int64Vec(model->costs->begin(), model->costs->end()) : Gringo::Int64Vec();
    }
    virtual void addClause(LitVec const &lits) const {
        Clasp::LitVec claspLits;
        for (auto &x : lits) {
            auto atom = out.find(x.second);
            if (atom.second && atom.first->hasUid()) {
                Clasp::Literal lit = lp.getLiteral(atom.first->uid());
                claspLits.push_back(x.first ? lit : ~lit);
            }
            else if (!x.first) { return; }
        }
        claspLits.push_back(~ctx.stepLiteral());
        model->ctx->commitClause(claspLits);
    }
    virtual ~ClingoModel() { }
    Clasp::Asp::LogicProgram const   &lp;
    Gringo::Output::OutputBase const &out;
    Clasp::SharedContext const       &ctx;
    Clasp::Model const               *model;
    mutable Gringo::ValVec            atms;
};

// {{{1 declaration of ClingoSolveIter

#if WITH_THREADS
struct ClingoSolveIter : Gringo::SolveIter {
    ClingoSolveIter(Clasp::ClaspFacade::AsyncResult const &future, Clasp::Asp::LogicProgram const &lp, Gringo::Output::OutputBase const &out, Clasp::SharedContext const &ctx);

    virtual Gringo::Model const *next();
    virtual void close();
    virtual Gringo::SolveResult get();

    virtual ~ClingoSolveIter();

    Clasp::ClaspFacade::AsyncResult future;
    ClingoModel                     model;
};
#endif

// {{{1 declaration of ClingoSolveFuture

Gringo::SolveResult convert(Clasp::ClaspFacade::Result res);
#if WITH_THREADS
struct ClingoSolveFuture : Gringo::SolveFuture {
    ClingoSolveFuture(Clasp::ClaspFacade::AsyncResult const &res);
    // async
    virtual Gringo::SolveResult get();
    virtual void wait();
    virtual bool wait(double timeout);
    virtual void cancel();

    virtual ~ClingoSolveFuture();
    void reset(Clasp::ClaspFacade::AsyncResult res);

    Clasp::ClaspFacade::AsyncResult future;
    Gringo::SolveResult             ret = Gringo::SolveResult::UNKNOWN;
    bool                            done = false;
};
#endif

// {{{1 declaration of ClingoControl
class ClingoTheoryInit : public Gringo::TheoryPropagator::Init {
public:
    using Lit_t = Potassco::Lit_t;
    ClingoTheoryInit(Gringo::Control &c, Clasp::TheoryPropagator &p) : c_(c), p_(p) { }
    Gringo::TheoryData const &theory() const override { return c_.theory(); }
    Gringo::DomainProxy &getDomain() override { return c_.getDomain(); }
    Lit_t mapLit(Lit_t lit) override;
    void addWatch(Lit_t lit) override { p_.addWatch(Clasp::decodeLit(lit)); }
    virtual ~ClingoTheoryInit() noexcept = default;
private:
    Gringo::Control &c_;
    Clasp::TheoryPropagator &p_;
};

class ClingoControl : public Gringo::Control, private Gringo::ConfigProxy, private Gringo::DomainProxy {
public:
    using StringVec        = std::vector<std::string>;
    using ExternalVec      = std::vector<std::pair<Gringo::Value, Potassco::Value_t>>;
    using StringSeq        = ProgramOptions::StringSeq;
    using PostGroundFunc   = std::function<bool (Clasp::ProgramBuilder &)>;
    using PreSolveFunc     = std::function<bool (Clasp::ClaspFacade &)>;
    enum class ConfigUpdate { KEEP, REPLACE };

    ClingoControl(Gringo::Scripts &scripts, bool clingoMode, Clasp::ClaspFacade *clasp, Clasp::Cli::ClaspCliConfig &claspConfig, PostGroundFunc pgf, PreSolveFunc psf);
    void prepare_(Gringo::Control::ModelHandler mh, Gringo::Control::FinishHandler fh);
    void commitExternals();
    void parse_();
    void parse(const StringSeq& files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* out, bool addStdIn = true);
    void main();
    bool onModel(Clasp::Model const &m);
    void onFinish(Clasp::ClaspFacade::Result ret);
    bool update();

    Clasp::LitVec toClaspAssumptions(Gringo::Control::Assumptions &&ass) const;

    // {{{2 DomainProxy interface

    virtual ElementPtr iter(Gringo::Signature const &sig) const;
    virtual ElementPtr iter() const;
    virtual ElementPtr lookup(Gringo::Value const &atom) const;
    virtual size_t length() const;
    virtual std::vector<Gringo::FWSignature> signatures() const;

    // {{{2 ConfigProxy interface

    virtual bool hasSubKey(unsigned key, char const *name, unsigned* subKey = nullptr);
    virtual unsigned getSubKey(unsigned key, char const *name);
    virtual unsigned getArrKey(unsigned key, unsigned idx);
    virtual void getKeyInfo(unsigned key, int* nSubkeys = 0, int* arrLen = 0, const char** help = 0, int* nValues = 0) const;
    virtual const char* getSubKeyName(unsigned key, unsigned idx) const;
    virtual bool getKeyValue(unsigned key, std::string &value);
    virtual void setKeyValue(unsigned key, const char *val);
    virtual unsigned getRootKey();

    // {{{2 Control interface

    virtual Gringo::DomainProxy &getDomain();
    virtual void ground(Gringo::Control::GroundVec const &vec, Gringo::Context *ctx);
    virtual void add(std::string const &name, Gringo::FWStringVec const &params, std::string const &part);
    virtual void load(std::string const &filename);
    virtual Gringo::SolveResult solve(ModelHandler h, Assumptions &&ass);
    virtual bool blocked();
    virtual std::string str();
    virtual void assignExternal(Gringo::Value ext, Potassco::Value_t);
    virtual Gringo::Value getConst(std::string const &name);
    virtual ClingoStatistics *getStats();
    virtual Gringo::ConfigProxy &getConf();
    virtual void useEnumAssumption(bool enable);
    virtual bool useEnumAssumption();
    virtual void cleanupDomains();
    virtual Gringo::SolveIter *solveIter(Assumptions &&ass);
    virtual Gringo::SolveFuture *solveAsync(ModelHandler mh, FinishHandler fh, Assumptions &&ass);
    virtual Gringo::TheoryData const &theory() const { return out->data.theoryInterface(); }
    virtual void registerPropagator(Gringo::TheoryPropagator &p);

    // }}}2

    std::unique_ptr<Gringo::Output::OutputBase>             out;
    Gringo::Scripts                                        &scripts;
    Gringo::Input::Program                                  prg;
    Gringo::Defines                                         defs;
    std::unique_ptr<Gringo::Input::NongroundProgramBuilder> pb;
    std::unique_ptr<Gringo::Input::NonGroundParser>         parser;
    ModelHandler                                            modelHandler;
    FinishHandler                                           finishHandler;
    ClingoStatistics                                        clingoStats;
    Clasp::ClaspFacade                                     *clasp = nullptr;
    Clasp::Cli::ClaspCliConfig                             &claspConfig_;
    PostGroundFunc                                          pgf_;
    PreSolveFunc                                            psf_;
    std::unique_ptr<Potassco::TheoryData>                   data_;
    std::vector<std::unique_ptr<Clasp::TheoryPropagator>>   propagators_;
#if WITH_THREADS
    std::unique_ptr<ClingoSolveFuture> solveFuture_;
    std::unique_ptr<ClingoSolveIter>   solveIter_;
#endif
    bool enableEnumAssupmption_ = true;
    bool clingoMode_;
    bool verbose_               = false;
    bool parsed                 = false;
    bool grounded               = false;
    bool incremental            = false;
    bool configUpdate_          = false;
};

// {{{1 declaration of ClingoLib

class ClingoLib : public Clasp::EventHandler, public ClingoControl {
    using StringVec    = std::vector<std::string>;
public:
    ClingoLib(Gringo::Scripts &scripts, int argc, char const **argv);
    virtual ~ClingoLib();
protected:
    void initOptions(ProgramOptions::OptionContext& root);
    static bool parsePositional(const std::string& s, std::string& out);
    // -------------------------------------------------------------------------------------------
    // Event handler
    virtual void onEvent(const Clasp::Event& ev);
    virtual bool onModel(const Clasp::Solver& s, const Clasp::Model& m);
private:
    ClingoLib(const ClingoLib&);
    ClingoLib& operator=(const ClingoLib&);
    ClingoOptions                       grOpts_;
    Clasp::Cli::ClaspCliConfig          claspConfig_;
    Clasp::ClaspFacade                  clasp_;
};

// {{{1 declaration of DefaultGringoModule

struct DefaultGringoModule : Gringo::GringoModule {
    DefaultGringoModule();
    Gringo::Control *newControl(int argc, char const **argv);
    void freeControl(Gringo::Control *ctl);
    virtual Gringo::Value parseValue(std::string const &str);
    Gringo::Input::GroundTermParser parser;
    Gringo::Scripts scripts;
};

// }}}1

#endif // _GRINGO_CLINGOCONTROL_HH
