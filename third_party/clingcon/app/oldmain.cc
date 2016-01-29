#include "clasp/clasp_facade.h"
#include "clasp/solver.h"
#include "clasp/cli/clasp_output.h"
#include "clingcon/clingconorderpropagator.h"
#include "clingcon/solver.h"
#include "order/normalizer.h"
#include <memory>
#include <sstream>


class ClingconConfig : public Clasp::ClaspConfig {
public:
    ClingconConfig(Clasp::SharedContext& c, order::Config conf) : creator_(c), n_(creator_, conf), conf_(conf) {}

    virtual bool addPost(Clasp::Solver& s) const
    {
        ///solver takes ownership of propagator
        clingcon::ClingconOrderPropagator* cp = new clingcon::ClingconOrderPropagator(s, n_.getVariableCreator(), conf_,
                                                                            std::move(n_.getCopyOfConstraints()));
        s.addPost(cp);
        return ClaspConfig::addPost(s);
    }

    //clingcon::ClingconPropagator* prop_;///prop_ = new clingcon::ClingconPropagator(new MySolver(&s));

    //std::vector<std::unique_ptr<MySolver> > solvers_;
    //std::vector<std::unique_ptr<clingcon::ClingconPropagator> > props_;
    MySharedContext creator_;
    order::Normalizer n_;
    order::Config conf_;
};



int main( int , const char** )
{

    Clasp::ClaspFacade f;
    ClingconConfig conf(f.ctx, order::lazySolveConfig);
    conf.solve.numModels = 0;
    //conf.solve.project =  1;

    Clasp::Asp::LogicProgram& lp = f.startAsp(conf);
    //lp.start(f.ctx);

    std::vector<order::ReifiedLinearConstraint> linearConstraints;

 

    /// {a,b}.
    /// a :- b.
    /// :- a, b, not a+b+c <= 17.
    /// :- not b.
    auto a = lp.newAtom();
    lp.addOutput("a",Clasp::Literal(a,false));
    auto b = lp.newAtom();
    lp.addOutput("b",Clasp::Literal(b,false));
    {
    Clasp::Asp::Rule r(Clasp::Asp::Rule::Choice);
    
    r.addHead(a);
    r.addHead(b);
    lp.addRule(r);
    }

    {
    Clasp::Asp::Rule r(Clasp::Asp::Rule::Normal);
    r.addToBody(b, true);
    r.addHead(a);
    lp.addRule(r);
    }

    auto constraint1 = lp.newAtom();
    lp.addOutput("a+b+c<=17", Clasp::Literal(constraint1,false));
    {
    Clasp::Asp::Rule r(Clasp::Asp::Rule::Normal);
    //lp.freeze(constraint1,Clasp::value_free);
    r.addToBody(a, true);
    r.addToBody(b, true);
    r.addToBody(constraint1, false);
    r.addHead(lp.falseAtom());

    lp.addRule(r);
    }

    //r.clear();
    //r.setType(Clasp::Asp::BASICRULE);
    //r.addToBody(b, false);
    //r.addHead(False);
    //lp.addRule(r);

    //// ADDITIONAL STUFF
    {
    Clasp::Asp::Rule r(Clasp::Asp::Rule::Choice);
    r.addHead(constraint1);
    lp.addRule(r);
    lp.freeze(constraint1,Clasp::value_free);
    }




    if (!lp.end())
        return -1; /// UNSAT

    auto ia = conf.n_.createView(order::Domain(5,10));
    auto ib = conf.n_.createView(order::Domain(5,10));
    auto ic = conf.n_.createView(order::Domain(5,10));
    order::LinearConstraint l(order::LinearConstraint::Relation::LE);
    l.addRhs(17);
    l.add(ia);
    l.add(ib);
    l.add(ic);
    //// at least getting the literal from the variable has to take place after lp.end() has been called
    linearConstraints.emplace_back(order::ReifiedLinearConstraint(std::move(l),toOrderFormat(lp.getLiteral(constraint1))));
    ////
    f.ctx.unfreeze();
    for (auto &i : linearConstraints)
        conf.n_.addConstraint(std::move(i));

    conf.n_.prepare();
    //conf.creator_.createNewLiterals(conf.n_.estimateVariables());
    conf.n_.createClauses();


    ///name the order lits
    /// i just use free id's for this.
    /// in the next incremental step these id's need to be
    /// made false variables
    /*
    auto atomId = lp.numAtoms()+1;
    Clasp::SymbolTable& st = f.ctx.symbolTable();
    st.startInit(Clasp::SymbolTable::map_indirect);
    for (std::size_t i = 0; i < conf.n_.getVariableCreator().numVariables(); ++i)
    {
        std::string varname;
        switch(i)
        {
        case 0: varname="a"; break;
        case 1: varname="b"; break;
        case 2: varname="c"; break;
        }

        auto lr = conf.n_.getVariableCreator().getLiteralRestrictor(i);
        for (auto litresit = lr.begin(); litresit != lr.end(); ++litresit )
        {
            std::stringstream ss;
            ss << varname << "<=" << *litresit;
            st.addUnique(atomId++, ss.str().c_str()).lit = toClaspFormat(conf.n_.getVariableCreator().getLiteral(litresit));
        }
    }
    atomId--;
    st.endInit();*/
    f.ctx.startAddConstraints(1000);
    if (!conf.n_.createClauses())
        return false; /// UNSAT
    f.prepare();
    
    Clasp::Cli::TextOutput to(0,Clasp::Cli::TextOutput::format_asp);
    f.solve(&to); /// 442 solutions
    //f.solve();

    /// first thing in next incremental step is
    /*
     *r.clear();
    r.setType(Clasp::Asp::BASICRULE);
    r.addToBody(atomId, false);
    r.addHead(False);
    lp.addRule(r);
     *
     */

    return 0;
}
