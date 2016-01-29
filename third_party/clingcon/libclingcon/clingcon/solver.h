#pragma once
#include "order/solver.h"
#include "clasp/solver.h"
#include "clasp/clause.h"
#include "clasp/weight_constraint.h"
#include <iostream>


//Clasp::Literal toClaspFormat(order::Literal l);
//order::Literal toOrderFormat(Clasp::Literal l);
//inline Clasp::Literal toClaspFormat(order::Literal l) { return Clasp::Literal::fromRep(l.asUint()); }
inline Clasp::Literal& toClaspFormat(order::Literal& l) { return *reinterpret_cast<Clasp::Literal*>(&l); }
inline const Clasp::Literal& toClaspFormat(const order::Literal& l) { return *reinterpret_cast<const Clasp::Literal*>(&l); }
//inline order::Literal toOrderFormat(Clasp::Literal l) { return order::Literal::fromRep(l.asUint()); }
inline order::Literal& toOrderFormat(Clasp::Literal& l) { return *reinterpret_cast<order::Literal*>(&l); }
inline const order::Literal& toOrderFormat(const Clasp::Literal& l) { return *reinterpret_cast<const order::Literal*>(&l); }
///order::Literal vecToOrderFormat(const Clasp::LitVec& l); // linker error on these ? why
/// This does not work in debug mode, as Clasp uses a PODVector
//inline Clasp::LitVec& vecToClaspFormat(order::LitVec& l) { return *reinterpret_cast<Clasp::LitVec*>(&l);}
//inline const Clasp::LitVec& vecToClaspFormat(const order::LitVec& l) { return *reinterpret_cast<const Clasp::LitVec*>(&l);}

//inline order::LitVec& vecToOrderFormat(Clasp::LitVec& l) { return *reinterpret_cast<order::LitVec*>(&l);}
//inline const order::LitVec& vecToOrderFormat(const Clasp::LitVec& l) { return *reinterpret_cast<const order::LitVec*>(&l);}


class MySharedContext : public order::CreatingSolver
{
public:
    using Literal = order::Literal;
    using LitVec = order::LitVec;
    MySharedContext(Clasp::SharedContext& c) : c_(c), s_(*c.master()), currentVar_(0), maxVar_(0), growth(8096) {}
    /// all variables are free, except var 0
    ///Alle variablen die ich erzeugt habe mittels programbuilder können einen wert haben (lp.end() was called)
    ///Alle neuen Variablen haben keinen, sind "free" aber der Solver kennt sie noch nicht und schmeisst assertion!!!
    bool isTrue(const Literal& l) const { assert(s_.validVar(toClaspFormat(l).var())); return s_.level(toClaspFormat(l).var())==0 && s_.isTrue(toClaspFormat(l)); }
    bool isFalse(const Literal& l) const { assert(s_.validVar(toClaspFormat(l).var())); return s_.level(toClaspFormat(l).var())==0 && s_.isFalse(toClaspFormat(l)); }
    bool isUnknown(const Literal& l) const { return (!isTrue(l) && !isFalse(l)); }

    //std::size_t numVars() const { return s_.numVars(); }

    void makeRestFalse()
    {
        //std::cout << "used " << currentVar_-1 << " of " << maxVar_ -1 << std::endl;
        for (;currentVar_ < maxVar_; ++currentVar_)
            c_.addUnary(Clasp::Literal(currentVar_,true));
    }



    void freeze(Literal l) { c_.setFrozen(l.var(),true); }

    Literal getNewLiteral(bool frozen)
    {
        if (!freeLiterals())
        {
            createNewLiterals(growth); /// magic constant
            growth*=2;
        }
        
        assert(currentVar_!=0);
        assert(c_.validVar(currentVar_));
        c_.setFrozen(currentVar_, frozen);
        return toOrderFormat(Clasp::Literal(currentVar_++, false));
    }



    Literal trueLit() const { return toOrderFormat(Clasp::posLit(0)); }
    Literal falseLit() const { return ~trueLit(); }

    bool createClause(const LitVec& lvv)
    {
        ///TODO: currently does a copy, can this be avoided ?
        /// !order::LitVec and Clasp::LitVec are different in debug mode -> PODVector
        Clasp::LitVec v;
        v.reserve(lvv.size());
        for (auto i : lvv)
        {
            //std::cout << i.sign() << "_" << i.var() << " ";
            v.push_back(toClaspFormat(i));
        }
        //std::cout << std::endl;
        //Clasp::LitVec v(vecToClaspFormat(lvv));
        return Clasp::ClauseCreator::create(s_,v,0).ok();
    }

    bool setEqual(const Literal &a, const Literal &b)
    {
        if (!createClause({a,~b})) return false;
        if (!createClause({~a,b})) return false;
        return true;
    }

    bool createCardinality(Literal v, int lb, LitVec &&lits)
    {
        Clasp::WeightLitVec wvec;
        for (auto& l : lits)
        {
            wvec.push_back(Clasp::WeightLiteral(toClaspFormat(l),1));
        }
        return Clasp::WeightConstraint::create(s_,toClaspFormat(v),wvec,lb).ok();
    }

    void unrestrictedDomainCallback(order::View) const
    {
        throw std::runtime_error("Variable was unrestricted");
    }
    
    void intermediateVariableOutOfRange() const
    {
        throw std::runtime_error("Intermediate Variable out of bounds (32bit integer)");
    }



    //const LitVec& clauses() const { /*std::cout << std::endl;*/ return clauses_; }



    /// makes sure that there are at least num free variables left
    void createNewLiterals(std::size_t num)
    {
        if (num<=maxVar_-currentVar_)
            return;
        num -= maxVar_-currentVar_;
        currentVar_ = c_.addVar(Clasp::Var_t::Atom);
        for (size_t i = 1; i < num; ++i)
        {
            Clasp::Var vnew = c_.addVar(Clasp::Var_t::Atom);
            assert(vnew==currentVar_+i);
            vnew = 0; /// disable warning in release mode
        }
        maxVar_ = currentVar_+num;
        c_.startAddConstraints();///TODO: give a guess ?

    }
    
    void addMinimize(order::Literal v, int32 weight, unsigned int level)
    {
        Clasp::WeightLiteral wl= std::make_pair(toClaspFormat(v),weight);
        c_.addMinimize(wl,level);
    }

private:

    bool freeLiterals()
    {
        return currentVar_ < maxVar_;

    }

    Clasp::SharedContext& c_;
    Clasp::Solver& s_;
    Clasp::Var currentVar_;
    Clasp::Var maxVar_;
    unsigned int growth;
};


class MyLocalSolver : public order::IncrementalSolver
{
public:
    using Literal = order::Literal;
    using LitVec = order::LitVec;
    MyLocalSolver(Clasp::Solver& s) : s_(s) {}
    bool isTrue(const Literal& l) const { return s_.isTrue(toClaspFormat(l)); }
    bool isFalse(const Literal& l) const { return s_.isFalse(toClaspFormat(l)); }
    bool isUnknown(const Literal& l) const { return (!isTrue(l) && !isFalse(l)); }

    Literal getNewLiteral() { return toOrderFormat(Clasp::Literal(s_.pushAuxVar(), false)); }
    //Literal getNewLiteral() { auto x = s_.pushAuxVar(); std::cout << x << std::endl; return toOrderFormat(Clasp::Literal(x, false)); }

    //std::size_t numVars() const { return s_.numVars(); }
    Literal trueLit() const { return toOrderFormat(Clasp::posLit(0)); }
    Literal falseLit() const { return ~trueLit(); }

private:
    Clasp::Solver& s_;


};
