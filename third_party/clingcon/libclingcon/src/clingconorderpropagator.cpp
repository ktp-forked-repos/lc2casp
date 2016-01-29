#include "clingconorderpropagator.h"
#include "order/variable.h"


namespace clingcon
{


Clasp::Constraint::PropResult ClingconOrderPropagator::propagate(Clasp::Solver& s, Clasp::Literal p, uint32& data)
{
    /// only called if p is gets true (otherwise ~p gets true)
    assert(s_.isTrue(p));


    if (dls_.back()!=s.decisionLevel())
    {
        //std::cout << "new level " << s.decisionLevel() << std::endl;
        dls_.emplace_back(s.decisionLevel());
        p_.addLevel();
        s_.addUndoWatch(s_.decisionLevel(), this);
        //std::cout << "Variable storage before getting to the next level " << p_.getVVS().getVariableStorage() << std::endl;
    }

    DataBlob blob(DataBlob::fromRep(data));
    if (blob.sign())
    {
        ///order literal
        assert(propVar2cspVar_.find(p.var())!=propVar2cspVar_.end());
        auto cspVars = propVar2cspVar_[p.var()];


        for (auto cspVar : cspVars)
        {
            uint64 bound = abs(cspVar.second)-1;
            bool sign = cspVar.second < 0;
//            std::cout << "Propagate order Literal" << std::endl;
//            std::cout << "Variable: " << cspVar.first << " with bound/sign/p,sign" << bound << "/" << sign << "/" << p.sign() << " on dl" << s_.decisionLevel() << std::endl;
//            std::cout << "Literal: " << p.rep() << "<-litnumber "<< p.var() << "," << p.sign() << " @"<< s_.decisionLevel() << std::endl;
            order::Restrictor lr = p_.getVVS().getVariableStorage().getRestrictor(cspVar.first); //(*orderLits_[cspVar.first]);
            assert(p_.getVVS().getVariableStorage().getDomain(cspVar.first).size() > bound); /// assure that the given bound is in range
            assert(toClaspFormat(p_.getVVS().getVariableStorage().getLELiteral(lr.begin()+bound)).var() == p.var()); /// given the cspvar+bound we can reinfer the literal which should be p again

            if ((p.sign() && !sign) || (!p.sign() && sign))
            {   /// cspVar.first > bound
                if (p_.getVVS().getVariableStorage().getCurrentRestrictor(cspVar.first).begin() < (lr.begin()+bound+1))
                {
                    bool nonempty = p_.constrainLowerBound(lr.begin()+bound+1); /// we got not (a<=x) ->the new lower bound is x+1
                    //assert(nonempty); // can happen that variables are propagated in a way that clasp is not yet aware of the conflict, but should find it during this unit propagation
                    if (!nonempty)
                    { /*std::cout << "now" << std::endl;*/ assertConflict_ = true; }
                }

                /// the start of the free range was (probably) shifted to the right
                /// if range was restricted from 5...100 to 10..100, we make 8,7,6,5 false with reason x>9
                if (conf_.minLitsPerVar >= 0 || !conf_.explicitBinaryOrderClausesIfPossible)
                {
                    order::pure_LELiteral_iterator newit(lr.begin()+bound,p_.getVVS().getVariableStorage().getOrderStorage(cspVar.first),false);
                    while((--newit).isValid())
                    {
                        if (s_.isFalse(toClaspFormat(*newit)))
                            break;
                        if (!s_.force(toClaspFormat(~(*newit)), Clasp::Antecedent(p)))
                            return PropResult(false, true);
                        /// if we have binary order clauses and reached a native variable,
                        /// we leave the rest to clasp -> NO, this loop should be faster and has smarter reason
                        //if (conf_.explicitBinaryOrderClausesIfPossible && !s_.auxVar(toClaspFormat(*newit).var()))
                        //        break;
                    }
                }

            }
            else
            {   ///cspVar.first <= bound
                if (p_.getVVS().getVariableStorage().getCurrentRestrictor(cspVar.first).end() > (lr.begin()+bound))
                {
                    bool nonempty = p_.constrainUpperBound(lr.begin()+bound+1); /// we got a <= x, the new end restrictor is at x+1
                    //assert(nonempty);
                    if (!nonempty) {  /*std::cout << "now" << std::endl;*/ assertConflict_ = true; }
                }
                /// the end of the free range was (probably) shifted to the left
                /// if range was restricted from 5...100 to 5..95, we make 96,97,98,99 true with reason x<95

                if (conf_.minLitsPerVar >= 0 || !conf_.explicitBinaryOrderClausesIfPossible)
                {
                    order::pure_LELiteral_iterator newit(lr.begin()+bound,p_.getVVS().getVariableStorage().getOrderStorage(cspVar.first),true);
                    while((++newit).isValid())
                    {
                        if (s_.isTrue(toClaspFormat(*newit)))
                            break;
                        if (!s_.force(toClaspFormat(*newit), Clasp::Antecedent(p)))
                            return PropResult(false, true);
                        /// if we have binary order clauses and reached a native variable,
                        /// we leave the rest to clasp
                        //if (conf_.explicitBinaryOrderClausesIfPossible && !s_.auxVar(toClaspFormat(*newit).var()))
                        //        break;
                    }
                }
            }
        }
    }
    else
    {
        /// reification literal
        //std::cout << "received reification lit " << p.rep() << std::endl;
        p_.queueConstraint(static_cast<std::size_t>(blob.var()));
    }
    return PropResult(true, true);
}


void ClingconOrderPropagator::reason(Clasp::Solver& s, Clasp::Literal p, Clasp::LitVec& lits)
{
    if (conflict_.size())
    {
        lits.insert(lits.end(),conflict_.begin(), conflict_.end());
        conflict_.clear();
    }
    else
        lits.insert(lits.end(),reasons_[p.var()].begin(), reasons_[p.var()].end());
}


bool ClingconOrderPropagator::init(Clasp::Solver& s)
{
    return true;
}


bool ClingconOrderPropagator::propagateFixpoint(Clasp::Solver& , PostPropagator*)
{
    assert(!assertConflict_);
    assert(orderLitsAreOK());
    while (!p_.atFixPoint())
    {
        //if (!p_.propagateSingleStep()){ return false; }
        std::vector<order::LinearLiteralPropagator::LinearConstraintClause> clauses = p_.propagateSingleStep();
        if (clauses.size())
        {
            for (auto clause : clauses)
            {
                auto myclause = clause.getClause(*ms_.get(), p_.getVVS(), conf_.createOnConflict);

                if (clause.addedNewLiteral())
                {
                    auto varit = clause.getAddedIterator(p_.getVVS().getVariableStorage());
                    
                    //std::cout << "Added new variable by propagation V" << varit.view().v << "*" << varit.view().a << "+" << varit.view().c << "<=" << *varit << std::endl;
                    addWatch(varit.view().v,toClaspFormat(p_.getVVS().getVariableStorage().getLELiteral(varit)),varit.numElement());
                }


                Clasp::LitVec claspClause;
                claspClause.reserve(myclause.size());
                for (auto l : myclause)
                    claspClause.push_back(toClaspFormat(l));
                ////1=true, 2=false, 0=free
                /// all should be isFalse(i)==true, one can be unknown or false
//                              std::cout << "Give clasp the clause "  << std::endl;
//                              for (auto i : claspClause)
//                                  std::cout << i.rep() << "@" << s_.level(i.var()) << "isFalse?" << s_.isFalse(i) << " isTrue" << s_.isTrue(i) << "  ,   ";
//                              std::cout << " on level " << s_.decisionLevel();
//                              std::cout << std::endl;
                assert(std::count_if(claspClause.begin(), claspClause.end(), [&](Clasp::Literal i){ return s_.isFalse(i); } )>=claspClause.size()-1);
                assert(std::count_if(claspClause.begin(), claspClause.end(), [&](Clasp::Literal i){ return s_.isFalse(i) && (s_.level(i.var()) == s_.decisionLevel()); } )>=1);

                if (conf_.learnClauses)
                {
                    if (!Clasp::ClauseCreator::create(s_,claspClause, Clasp::ClauseCreator::clause_force_simplify, Clasp::ClauseInfo(Clasp::Constraint_t::Other)).ok())
                        return false;
                }
                else
                {

                    ///TODO: what about the return value
                    Clasp::ClauseCreator::prepare(s_, claspClause, Clasp::ClauseCreator::clause_force_simplify);

                    if (claspClause.size()==0) /// top level conflict
                        claspClause.push_back(Clasp::negLit(0));

                    /// in case of a conflict we do not want to override reason of the same literal
                    if (s_.isFalse(*claspClause.begin()))
                    {
                        conflict_.clear();
                        for (auto i = claspClause.begin()+1; i != claspClause.end(); ++i)
                            conflict_.push_back(~(*i));
                    }
                    else
                    {
                        reasons_[claspClause.begin()->var()].clear();
                        conflict_.clear();
                        for (auto i = claspClause.begin()+1; i != claspClause.end(); ++i)
                            reasons_[claspClause.begin()->var()].push_back(~(*i));
                    }
                    if (!s_.force(*claspClause.begin(),this))
                        return false;
                }
            }
            if (!s_.propagateUntil(this))         { return false; }
            assert(orderLitsAreOK());
        }
    }
    return true;
}


void ClingconOrderPropagator::addWatch(const order::Variable& var, const Clasp::Literal& cl, unsigned int step)
{
    DataBlob blob(0, true);
    s_.addWatch(cl, this, blob.rep());
    s_.addWatch(~cl, this, blob.rep());
    int32 x = cl.sign() ? int32(step+1)*-1 : int32(step+1);
    propVar2cspVar_[cl.var()].emplace_back(std::make_pair(var,x));
}


/// debug function to check if stored domain restrictions are in line with order literal assignment in clasp solver
/// furthermore check if current bound has literals (upper and lower must have lits)
bool ClingconOrderPropagator::orderLitsAreOK()
{
    /*auto& vs = p_.getVariableStorage();
    auto& vc = vs.getVariableCreator();
    for (order::Variable var = 0; var < vc.numVariables(); ++var)
    {
    if (vc.isValid(var)
    {
        auto currentlr = vs.getCurrentLiteralRestrictor(var);
        auto baselr = vc.getLiteralRestrictor(var);

        /// everything left of open domain must be false or not have a literal
        for (auto i = baselr.begin(); i < currentlr.begin(); ++i)
            if (vc.hasLiteral(i) && !s_.isFalse(toClaspFormat(vc.getLiteral(i))))
                return false;
        /// everything open must be undecided or not have a literal
        for (auto i = currentlr.begin(); i < currentlr.end()-1; ++i)
            if (vc.hasLiteral(i) && (s_.isFalse(toClaspFormat(vc.getLiteral(i))) || s_.isTrue(toClaspFormat(vc.getLiteral(i)))))
                return false;
        /// everything to the right must be true or not have a literal
        for (auto i = currentlr.end()-1; i < baselr.end(); ++i)
            if (vc.hasLiteral(i) && !s_.isTrue(toClaspFormat(vc.getLiteral(i))))
                return false;

        if (!(currentlr.begin().isAtFirstLiteral() || (vc.hasLiteral(currentlr.begin()-1) && s_.isFalse(toClaspFormat(vc.getLiteral(currentlr.begin()-1))))))
            return false;
        if (!(vc.hasLiteral(currentlr.end()-1) && s_.isTrue(toClaspFormat(vc.getLiteral(currentlr.end()-1)))))
            return false;
    }

    }*/
    return true;
}





void ClingconOrderPropagator::reset()
{
    //std::cout << "reset on dl " << s_.decisionLevel() << std::endl;
    assertConflict_ = false;
    if (s_.decisionLevel()==dls_.back())
    {
        p_.removeLevel();
        p_.addLevel();
    }
}


void ClingconOrderPropagator::undoLevel(Clasp::Solver& s)
{
    //std::cout << "undo dl " << s_.decisionLevel() << std::endl;
    assertConflict_ = false;
    p_.removeLevel();
    dls_.pop_back();
}


namespace
{

/// finds first literal that is true

template<class ForwardIt>
ForwardIt my_upper_bound(ForwardIt first, ForwardIt last, Clasp::Solver& s, const order::orderStorage& os)
{
    ForwardIt it;
    typename std::iterator_traits<ForwardIt>::difference_type count, step;
    count = std::distance(first,last);
 
    while (count > 0) {
        it = first; 
        step = count / 2; 
        std::advance(it, step);
        if (s.isFalse(toClaspFormat(os.getLiteral(it.numElement())))) {  /// if (lit is false)
            first = ++it;
            count -= step + 1;
        } else count = step;
    }
    return first;
}

}

bool ClingconOrderPropagator::isModel(Clasp::Solver& s)
{
    //std::cout << "Is probably a model ?" << " at dl " << s_.decisionLevel() << std::endl;
    auto& vs = p_.getVVS().getVariableStorage();

    order::Variable unrestrictedVariable(order::InvalidVar);
    unsigned int maxSize=1;
    for (unsigned int i = 0; i < vs.numVariables(); ++i)
    {
        if (p_.getVVS().getVariableStorage().isValid(i) && watched_[i])
        {
            auto lr = vs.getCurrentRestrictor(i);
    
            if (lr.size()>maxSize)
            {
                maxSize=lr.size();
                unrestrictedVariable=i;
            }
        }
    }

    if (maxSize>1) /// there is some unknownness
    {
        auto lr = vs.getCurrentRestrictor(order::View(unrestrictedVariable));
        auto it = lr.begin() + ((maxSize-1)/2);
        order::Literal l = p_.getSolver().getNewLiteral();
        p_.getVVS().setLELit(it,l);
        //std::cout << "Added V" << unrestrictedVariable << "<=" << *it << std::endl;
        addWatch(unrestrictedVariable,toClaspFormat(l),it.numElement());
        return false;
    } 
    else
    {
        /// store the model to be printed later
        for (order::Variable v = 0; v < p_.getVVS().getVariableStorage().numVariables(); ++v)
        {
            if (names_==nullptr || names_->size() <= v || s_.isFalse((*names_)[v].second))
            {
                lastModel_[v].con=0;
                continue;
            }
            order::EqualityClass::Edge e(1,1,0);
            order::ViewIterator it;
            const order::EqualityProcessor::EqualityClassMap::iterator eqsit;
            order::Variable var(v);
            if (!p_.getVVS().getVariableStorage().isValid(v))
            {
                auto eqsit = eqs_.find(v);
                assert(eqsit != eqs_.end());// should be inside, otherwise variable is valid
                assert(eqsit->second->top()!=v); // should not be top variable -> otherwise variable is valid
                auto& constraints = eqsit->second->getConstraints();
                assert(constraints.find(v) != constraints.end());
                e = constraints.find(v)->second;   
                var = eqsit->second->top();
            }
        
            if (watched_[v])
            {
                order::Restrictor lr;
                lr = p_.getVVS().getVariableStorage().getCurrentRestrictor(var);
                assert(lr.size()==1);
                it = lr.begin();
            }
            else /// not watched, need to search for value
            {
                auto rs = p_.getVVS().getVariableStorage().getRestrictor(order::View(var));
                it = my_upper_bound(rs.begin(), rs.end(),s_,p_.getVVS().getVariableStorage().getOrderStorage(var));
            }
            int32 value = (((int64)(e.secondCoef) * (int64)(*it)) + (int64)(e.constant))/(int64)(e.firstCoef);
            assert((((int64)(e.secondCoef) * (int64)(*it)) + (int64)(e.constant)) % (int64)(e.firstCoef) == 0);
            lastModel_[v].val=value; /// how to use 1 bit for check
            lastModel_[v].con = 1;
        }
        
    }

    return true;
}



const char* ClingconOrderPropagator::printModel(order::Variable v, const std::string& name)
{
    //std::cout << "enter printModel " << v << " " << name << std::endl;
    if (lastModel_[v].con==0)
        return 0;
    outputbuf_ = name;
    outputbuf_ += "=";
    outputbuf_ += std::to_string(lastModel_[v].val);
    return outputbuf_.c_str();  
}

}

