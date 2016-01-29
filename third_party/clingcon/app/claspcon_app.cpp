#include "claspcon_app.h"
#include "clingcon/solver.h"
#include "order/normalizer.h"
#include "clingcon/clingconorderpropagator.h"
#include "clingcon/clingcondlpropagator.h"
#include "clingcon/theoryparser.h"
#include <sstream>
#include <atomic>

/////////////////////////////////////////////////////////////////////////////////////////
// ClaspApp
/////////////////////////////////////////////////////////////////////////////////////////
ClaspConApp::ClaspConApp() : conf_(order::lazySolveConfig) {}

Clasp::ProblemType ClaspConApp::getProblemType() {
    return Clasp::ClaspFacade::detectProblemType(getStream());
}


class TheoryOutput : public Clasp::OutputTable::Theory
{
public:
    //! Called once on new model m. Shall return 0 to indicate no output.
    virtual const char* first(const Clasp::Model& m)
    {
        curr_ = 0;
        currentSolverId_ = m.sId;
        return next();
    }

    //! Shall return 0 to indicate no output.
    virtual const char* next()
    {
        for (; curr_ < names_.size(); ++curr_)
            if (names_[curr_].first.size()>0)
            {
                const char* ret = props_[currentSolverId_]->printModel(curr_,names_[curr_].first);
                if (ret)
                {
                    ++curr_;
                    return ret;
                }
            }
        return 0;        
    }
    order::Variable curr_;
    unsigned int currentSolverId_;
    std::vector<std::pair<std::string,Clasp::Literal>> names_; /// order::Variable to name + condition
    clingcon::ClingconOrderPropagator* props_[64];
};

class Configurator : public Clasp::Cli::ClaspCliConfig::Configurator
{
public:
    
    Configurator(order::Config conf, order::Normalizer& n, TheoryOutput& to) : conf_(conf), n_(n), to_(to)
    {}
    
    virtual bool addPost(Clasp::Solver& s)
    {
        if (conf_.dlprop==2)
            if (!addDLProp(s,n_.constraints()))
                return false;

        std::vector<order::ReifiedLinearConstraint> constraints;
        if (conf_.dlprop==1)
        {
            constraints = n_.constraints();
        }
        
        ///solver takes ownership of propagator
        clingcon::ClingconOrderPropagator* cp = new clingcon::ClingconOrderPropagator(s, n_.getVariableCreator(), conf_,
                                                                                      std::move(n_.constraints()),n_.getEqualities(),
                                                                                      &(to_.names_));
        to_.props_[s.id()]=cp;
        if (!s.addPost(cp))
            return false;
        
        if (conf_.dlprop==1)
            if (!addDLProp(s, constraints))
                return false;
        return true;
    }
    
private:
    bool addDLProp(Clasp::Solver& s, const std::vector<order::ReifiedLinearConstraint>& constraints)
    {
        clingcon::ClingconDLPropagator* dlp = new clingcon::ClingconDLPropagator(s, conf_);
        for (const auto&i : constraints)
        {
            if (dlp->isValidConstraint(i))
                dlp->addValidConstraint(i);
        }
        if (!s.addPost(dlp))
            return false;
        return true;        
    }

    order::Config conf_;
    order::Normalizer& n_;
    TheoryOutput& to_;
};

void ClaspConApp::initOptions(ProgramOptions::OptionContext& root)
{
    using namespace ProgramOptions;
    ClaspAppBase::initOptions(root);
    OptionGroup cspconf("Constraint Processing Options");
    cspconf.addOptions()
            ("redundant-clause-check", storeTo(conf_.redundantClauseCheck = true), "Check translated clauses for redundancies (default: true)")
            ("domain-size", storeTo(conf_.domSize = 10000), "the maximum number of chunks a domain can have when multiplied (default: 10000)")
            ("break-symmetries", storeTo(conf_.break_symmetries = true), "break symmetries, can't do enumeration without (default: true)")
            ("split-size", storeTo(conf_.splitsize_maxClauseSize.first = 3), "constraints are maybe split into this size (minimum: 3) (default: 3)")
            ("max-clause-size", storeTo(conf_.splitsize_maxClauseSize.second = 1024), "constraints are only split if they produce more clauses than this (default: 1024)")
            ("pidgeon-optimization", storeTo(conf_.pidgeon = false), "do pidgeon-hole optimization for alldistinct constraints (default: false)")
            ("permutation-optimization", storeTo(conf_.permutation = false), "do permutation optimization for alldistinct constraints (default: false)")
            ("disjoint-to-distinct", storeTo(conf_.disjoint2distinct = false), "do translate disjoint to distinct constraint if possible (default: false)")
            ("distinct-to-card", storeTo(conf_.alldistinctCard = true), "do translate distinct constraint with cardinality constraints (default: true)")
            ("explicit-binary-order", storeTo(conf_.explicitBinaryOrderClausesIfPossible = true), "explicitly create binary order clauses if possible (default: true)")
            ("learn-clauses", storeTo(conf_.learnClauses = true), "learn clauses while propagating (default: true)")
            //("lazy-variable-creation", flag(conf_.lazyLiterals = true), "create order variables lazily (default: true)")
            ("difference-logic", storeTo(conf_.dlprop = 0), "0: no difference logic propagator, 1 early, 2 late  (default: 0)")
            ("create-on-conflict", storeTo(conf_.createOnConflict = true), "lazily create variables on conflict (default: true)")
            ("translate-constraints", storeTo(conf_.translateConstraints = 1000), "translate constraints with an estimated number of clauses less than this (default: 1000)")
            ("min-lits-per-var", storeTo(conf_.minLitsPerVar = 1000), "minimum number of precreated literals per variable (-1=all) (default: 1000)")
            ("equalityProcessing", storeTo(conf_.equalityProcessing = true), "find and replace equal variable views (default: true)")
            ;
    root.add(cspconf);
    
 /*  OptionGroup basic("Basic Options");
    basic.addOptions()
            ("mode", storeTo(mode_ = mode_clingo, values<Mode>()
                             ("clingo", mode_clingo)
                             ("clasp", mode_clasp)
                             ("gringo", mode_gringo)), 
             "Run in {clingo|clasp|gringo} mode\n")
            ;
    root.add(basic);*/
}


void ClaspConApp::validateOptions(const ProgramOptions::OptionContext& root, const ProgramOptions::ParsedOptions& parsed, const ProgramOptions::ParsedValues& values)
{
    Clasp::Cli::ClaspAppBase::validateOptions(root,parsed,values);
}

void ClaspConApp::run(Clasp::ClaspFacade& clasp) {
    clasp.start(claspConfig_, getStream());
    if (!clasp.incremental()) {
        claspConfig_.releaseOptions();
    }
    if (claspAppOpts_.compute && clasp.program()->type() == Clasp::Problem_t::Asp) {
        bool val = claspAppOpts_.compute < 0;
        Clasp::Var  var = static_cast<Clasp::Var>(!val ? claspAppOpts_.compute : -claspAppOpts_.compute);
        static_cast<Clasp::Asp::LogicProgram*>(clasp.program())->startRule().addToBody(var, val).endRule();
    }
    
    auto* lp = static_cast<Clasp::Asp::LogicProgram*>(clasp.program());
    auto& td = lp->theoryData();
    MySharedContext s(clasp.ctx);
    order::Normalizer n(s,conf_);
    clingcon::TheoryParser tp(n,td,lp,s.trueLit());
    TheoryOutput to; /// only valid for this method, but i do have pointer to it in Configurator, is this ok ?
    Configurator conf(conf_,n,to);
    claspConfig_.addConfigurator(&conf);
    while (clasp.read()) {
        if (handlePostGroundOptions(*clasp.program())) {
            
            if (lp->end() && clasp.ctx.master()->propagate())
            {
                bool conflict = false;
                for (auto i = td.currBegin(); i != td.end(); ++i)
                {
                    if (!tp.readConstraint(i))
                        throw std::runtime_error("Unknown theory atom detected, cowardly refusing to continue");
                }
                to.names_ = std::move(tp.postProcess());
                clasp.ctx.output.theory = &to;
                for (unsigned int level = 0; level < tp.minimize().size(); ++level)
                    for (auto i : tp.minimize()[level])
                        n.addMinimize(i.second,level);

                if (!n.prepare())
                    conflict = true;
                else
                {
                    n.checkDomains(); // may throw! if domain was not restricted
                    s.createNewLiterals(n.estimateVariables());
                    
                    if (!n.createClauses())
                        conflict = true;                  
                }
                
                if (conflict && !clasp.ctx.master()->hasConflict())
                    clasp.ctx.master()->force(Clasp::Literal(0,true));
                
            }/// else, program is conflicting

            clasp.prepare();
  

//            
//            
            
            // i can not update the program if it is not incremental
            // do i have to make my program incremental for this ?
            // while estimating the number of variables i have to create
            // a stack of new variables all the time, probably updating clasp over and over ...
            //clasp.update();
//            

            //Maybe call prepare before, then update, then normalizer stuff, then prepare again
            // theory atoms already seem to be frozen
//            clasp.prepare();
            if (handlePreSolveOptions(clasp)) { clasp.solve(); }
        }
    }
}

void ClaspConApp::printHelp(const ProgramOptions::OptionContext& root) {
	ClaspAppBase::printHelp(root);
	printf("\nclasp is part of Potassco: %s\n", "http://potassco.sourceforge.net/#clasp");
	printf("Get help/report bugs via : http://sourceforge.net/projects/potassco/support\n");
	fflush(stdout);
}
