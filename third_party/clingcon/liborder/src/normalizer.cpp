
#include "normalizer.h"
#include "linearpropagator.h"
#include "types.h"
#include "translator.h"

#include <map>
#include <unordered_map>
#include <memory>


namespace order
{


void Normalizer::addConstraint(ReifiedLinearConstraint&& l)
{
    linearConstraints_.emplace_back(std::move(l));
}

void Normalizer::addConstraint(ReifiedDomainConstraint&& d)
{
    domainConstraints_.emplace_back(std::move(d));
}


/// hallsize=0 should be equal to quadratic amount of unequal ?
void Normalizer::addConstraint(ReifiedAllDistinct&& l)
{
    allDistincts_.emplace_back(std::move(l));
}

void Normalizer::addConstraint(ReifiedDisjoint&& l)
{
    disjoints_.emplace_back(std::move(l));
}

void Normalizer::addMinimize(View& v, unsigned int level)
{
    minimize_.emplace_back(v,level);
}


bool Normalizer::addLinear(ReifiedLinearConstraint&& l)
{
    s_.freeze(l.v);
    l.normalize();
    assert(l.l.getRelation()==LinearConstraint::Relation::LE || l.l.getRelation()==LinearConstraint::Relation::EQ);

    if (l.l.getRelation()==LinearConstraint::Relation::LE)
    {
        if (l.l.getConstViews().size()==1) // In this case we sometimes can make an orderLiteral out of it, if it was not yet otheriwse created
        {
            //ReifiedLinearConstraint copy(l);
            View v = *l.l.getConstViews().begin();
            int rhs = l.l.getRhs();
            //rhs = v.divide(rhs);
            Restrictor r = vc_.getRestrictor(v);
            auto it = std::upper_bound(r.begin(), r.end(), rhs);
            if (it != r.begin())
                --it;
            else
                it = r.end();
            return vc_.setLELit(it,l.v);
        }
        else
        {
            ReifiedLinearConstraint t(l);
            if (!s_.isFalse(l.v))
                addImp(std::move(l));
            if (!s_.isTrue(t.v)) /// Maybe makes problems with assumptions?
            {
                t.reverse();
                t.v = ~t.v;
                addImp(std::move(t));
            }
        }
    }
    else // EQ
    {
        assert(l.l.getRelation()==LinearConstraint::Relation::EQ);
        if (l.l.getConstViews().size()==1) // In this case we sometimes can make an orderLiteral out of it, if it was not yet otheriwse created
        {
            View v = *(l.l.getConstViews().begin());
            Restrictor r = vc_.getRestrictor(v);
            auto it = std::lower_bound(r.begin(), r.end(), l.l.getRhs());
            it = (it==r.end() || *it != l.l.getRhs()) ? r.end() : it;
            return vc_.setEqualLit(it,l.v); // otherwise, an equality will be created
        }
        Literal orig = l.v;
        ReifiedLinearConstraint u(l);
        ReifiedLinearConstraint less(l);
        ReifiedLinearConstraint more(l);
        if (!s_.isFalse(l.v))
        {
            l.l.setRelation(LinearConstraint::Relation::LE);
            addImp(std::move(l));
            u.l.setRelation(LinearConstraint::Relation::GE);
            addImp(std::move(u));
        }
        if (!s_.isTrue(orig))
        {
            Literal x = s_.getNewLiteral(true); ///TODO: maybe reuse if constraint is already used somewhere
            less.v = x;
            less.l.setRelation(LinearConstraint::Relation::LT);
            addImp(std::move(less));
            Literal y = s_.getNewLiteral(true); ///TODO: maybe reuse if constraint is already used somewhere
            more.v = y;
            more.l.setRelation(LinearConstraint::Relation::GT);
            addImp(std::move(more));
            if (!s_.createClause(LitVec{~x,~orig})) /// having orig implies not x (having x implies not orig)
                return false;
            if (!s_.createClause(LitVec{~y,~orig})) /// having orig implies not y (having y implies not orig)
                return false;
            if (!s_.createClause(LitVec{~x,~y})) /// cant be less and more at same time
                return false;
            if (!s_.createClause(LitVec{x,y,orig})) /// either equal or less or
                return false;
        }
    }
    return true;
}


std::pair<bool,bool> Normalizer::deriveSimpleDomain(ReifiedDomainConstraint& d)
{
    View v = d.getView();
    if (v.a==0)
    {
        if (d.getDomain().in(v.c))
            return std::make_pair(true,s_.setEqual(d.getLiteral(),s_.trueLit()));
        else
            return std::make_pair(true,s_.setEqual(d.getLiteral(),s_.falseLit()));
    }
    if (s_.isTrue(d.getLiteral()))
    {
        if (!vc_.intersectView(d.getView(),d.getDomain()))
            return std::make_pair(true, false);
        return std::make_pair(true, true);
    }
    else
        if (s_.isFalse(d.getLiteral()))
        {
            Domain all;
            all.remove(d.getDomain());
            if (!vc_.intersectView(d.getView(),all))
                return std::make_pair(true, false);
            return std::make_pair(true, true);
        }
    return std::make_pair(false, true);
}

std::pair<bool,bool> Normalizer::deriveSimpleDomain(const ReifiedLinearConstraint& l)
{
    std::pair<bool,bool> ret;
    if (l.l.getViews().size()> 1)
        return std::make_pair(false, true);
    if (l.l.getViews().size()==0)
    {
        Literal lit = l.v;
        if ((l.l.getRelation()==LinearConstraint::Relation::LE && 0 > l.l.getRhs())
                ||
            (l.l.getRelation()==LinearConstraint::Relation::EQ && 0 != l.l.getRhs()))
            lit = ~lit;

        return std::make_pair(true,s_.createClause(LitVec{lit}));
    }
    auto view = l.l.getViews().front();
    if (l.l.getRelation()==LinearConstraint::Relation::LE)
    {
        if (s_.isTrue(l.v))
        {
            return std::make_pair(true, vc_.constrainUpperBound(view,l.l.getRhs()));
        }
        else
            if (s_.isFalse(l.v))
            {
                return std::make_pair(true, vc_.constrainLowerBound(view,l.l.getRhs()+1));
            }
    }
    else
    {
        assert(l.l.getRelation()==LinearConstraint::Relation::EQ);
        if (s_.isTrue(l.v))
            return std::make_pair(true, vc_.constrainView(view,l.l.getRhs(), l.l.getRhs()));
        if (s_.isFalse(l.v))
            return std::make_pair(true, vc_.removeFromView(view,l.l.getRhs()));

    }
    return std::make_pair(false,true);
}


bool Normalizer::addDistinct(ReifiedAllDistinct&& l)
{
    if (conf_.pidgeon)
        if (!addPidgeonConstraint(l))
            return false;
    if (conf_.permutation)
        if (!addPermutationConstraint(l))
            return false;
    if (conf_.alldistinctCard)
        return addDistinctCardinality(std::move(l));
    //if (conf_.hallsize==0)
        return addDistinctPairwiseUnequal(std::move(l));
    //else
    //    return addDistinctHallIntervals(std::move(l));
}

/// size is sum of sizes of views
ViewDomain unify(const std::vector<View>& views, const VariableCreator& vc, uint64& size)
{
    auto it = views.begin();
    //createOrderLiterals(*it);
    ViewDomain d(vc.getViewDomain(*(it++))); // yes, explicitly use domain of the view, not of the variable
    size += d.size();
    for (;it!=views.end(); ++it)
    {
        //createOrderLiterals(*it);
        ViewDomain dd(vc.getViewDomain(*it));
        size+=dd.size();
        d.unify(dd);
    }
    return d;   
}

bool Normalizer::addPidgeonConstraint(ReifiedAllDistinct& l)
{
    auto views = l.getViews();
    if (views.size()==0) return true;
    uint64 size = 0;
    ViewDomain d = unify(views,vc_, size);    

    if (views.size()>d.size())
    {
        if (!s_.createClause(LitVec{~l.getLiteral()}))
            return false;
        return true;
    }
    if (size==d.size()) // no overlap between variables
    {
        if (!s_.createClause(LitVec{l.getLiteral()}))
            return false;
        return true;
    }

    int lower = *(d.begin()+(views.size()-1));
    int upper = *(d.end()-(views.size()+1-1));
    LitVec lowerbound{~l.getLiteral()};
    LitVec upperbound{~l.getLiteral()};
    for (const auto& i : views)
    {
        {
        LinearConstraint c(LinearConstraint::Relation::GE);
        c.add(i);
        c.addRhs(lower);
        c.normalize();
        lowerbound.emplace_back(getLitFromUnary(c));
        }

        {
        LinearConstraint c(LinearConstraint::Relation::LE);
        c.add(i);
        c.addRhs(upper);
        c.normalize();
        upperbound.emplace_back(getLitFromUnary(c));
        }
    }
    if(!s_.createClause(lowerbound))
        return false;
    if(!s_.createClause(upperbound))
        return false;
    return true;
}

/// currently only for true distinct constraint
bool Normalizer::addPermutationConstraint(ReifiedAllDistinct& l)
{
    if (!s_.isTrue(l.getLiteral()))
        return true;
    auto views = l.getViews();
    if (views.size()==0) return true;

    uint64 size = 0;
    ViewDomain d = unify(views,vc_,size);  
    
    if (views.size()>d.size())
    {
        if (!s_.createClause(LitVec{~l.getLiteral()}))
            return false;
        return true;
    }
    if (size==d.size()) // no overlap between variables
    {
        if (!s_.createClause(LitVec{l.getLiteral()}))
            return false;
        return true;
    }

    if (views.size()==d.size())
    {
        for (auto i : d)
        {
            LitVec cond;
            for (auto v : views)
            {
                cond.emplace_back(vc_.getEqualLit(v,i));
                //if (!s_.createClause(LitVec{x,~cond.back(),~l.getLiteral()}))
                //    return false;
            }
            if (!s_.createClause(cond))
                    return false;
            /*
            CHECK THIS AGAIN -> for true either x must be true or better -> just have clause {x_1=v, ... x_n=v} for all values
                    if unknown or false, just dont do it
            Literal x = s_.getNewLiteral(false);
            LitVec cond;
            for (auto v : views)
            {
                cond.emplace_back(vc_.getEqualLit(v,i));
                if (!s_.createClause(LitVec{x,~cond.back(),~l.getLiteral()}))
                    return false;
            }
            cond.emplace_back(~x);
            cond.emplace_back(~l.getLiteral());
            if (!s_.createClause(cond))
                    return false;
            ///in case constraint is false, new free variables are not allowed to vary
            if (!s_.createClause(LitVec{l.getLiteral(),x}))
                return false;
            */
        }
    }
    return true;
}


bool Normalizer::addDistinctCardinality(ReifiedAllDistinct&& l)
{

    auto views = l.getViews();
    if (views.size()==0) return true;

    uint64 size = 0;
    ViewDomain d = unify(views,vc_,size);  

    if (views.size()>d.size())
    {
        if (!s_.createClause(LitVec{~l.getLiteral()}))
            return false;
        return true;
    }
    if (size==d.size()) // no overlap between variables
    {
        if (!s_.createClause(LitVec{l.getLiteral()}))
            return false;
        return true;
    }


    LitVec conditions;
    for (auto i : d)
    {
        LitVec lits;

        for (auto v : views)
            lits.emplace_back(vc_.getEqualLit(v,i));

        Literal x(Literal::fromIndex(0));
        if (s_.isTrue(l.getLiteral()))
            x = s_.falseLit();
        else
            x = s_.getNewLiteral(false);

        conditions.emplace_back(x);
        if (!s_.createCardinality(conditions.back(),2,std::move(lits)))
            return false;
        if (!s_.createClause(LitVec{~conditions.back(),~l.getLiteral()}))
            return false;
    }

    conditions.emplace_back(l.getLiteral());
    return s_.createClause(conditions);
}

bool Normalizer::addDistinctPairwiseUnequal(ReifiedAllDistinct&& l)
{
    std::vector<LinearConstraint> inequalities;
    const auto& views = l.getViews();
    if (views.size()==1)
        return true;
    for (auto i = views.begin(); i != views.end()-1; ++i)
        for (auto j = i+1; j != views.end(); ++j)
        {
            LinearConstraint temp(LinearConstraint::Relation::NE);
            temp.add(*i);
            temp.add((*j)*-1);///reverse view
            inequalities.emplace_back(temp);
        }

    LitVec lits;
    for (auto& i : inequalities)
    {
        Literal x(0, false);
        if (s_.isTrue(l.getLiteral()))
            x = s_.trueLit();
        else
            x = s_.getNewLiteral(true);
        if (!addLinear(ReifiedLinearConstraint(LinearConstraint(i),x)))
            return false;
        lits.emplace_back(~x);
        if (!s_.createClause(LitVec{~l.getLiteral(),x}))
            return false;
    }
    lits.emplace_back(l.getLiteral());
    if (!s_.createClause(lits))
        return false;
    return true;
}
/*
bool Normalizer::addDistinctHallIntervals(ReifiedAllDistinct&& l)
{
    auto views = l.getViews();
    if (views.size()==0) return true;

    uint64 size = 0;
    Domain d = unify(views,vc_,size);  
    
    if (views.size()>d.size())
    {
        if (!s_.createClause(LitVec{~l.getLiteral()}))
            return false;
        return true;
    }
    if (size==d.size()) // no overlap between variables
    {
        if (!s_.createClause(LitVec{l.getLiteral()}))
            return false;
        return true;
    }

    ///TODO: make this a member, delete after all calls of addDistinct
    /// can be used to lookup and save variables
    std::map<std::tuple<View,int,int>, Variable> inHall;

    /// create A_ilu <=> l <= Xi <= u
    Domain::const_iterator begin = d.begin();
    Domain::const_iterator end = d.end();
    for (Domain::const_iterator i = begin; i!=end; ++i)
    {
        uint32 newHall = std::min(conf_.hallsize,uint32((uint32)(end-i)));
        for (int k = 0; k <= static_cast<int>(newHall)-1; ++k)
            for (auto v : views)
            {
                if (k==0)
                {
                    // equality l == v == i
                    auto lit = vc_.getEqualLit(i);
                    Variable alu = vc_.createVariable({0,1});
                    //std::cout << "Var:" << alu << " =:= " << "V"  << v  << "=" << *i << std::endl;
                    if (!vc_.setLELit(vc_.getRestrictor(alu).begin(),~lit)) //ReifiedLinearConstraint(std::move(alul),~lit)))
                        return false;
                    vc_.createOrderLiterals(alu);// why ?
                    inHall[std::make_tuple(v,*i, *(i+k))] = alu;
                }
                else
                {
                    Literal lblit = vc_.getGELit(i);
                    if (s_.isFalse(lblit)) continue;

                    Literal ublit = vc_.getLELit(i+k);
                    if (s_.isFalse(ublit)) continue;

                    Variable alu = vc_.createVariable({0,1});
                    //std::cout << "Var:" << alu << " =:= " << "V"  << v  << "={" << (*i) << ".." << (*(i+k)) << "}" << std::endl;
                    vc_.createOrderLiterals(alu);/// ?
                    auto r = vc_.getRestrictor(alu);
                    //std::cout << "create Hall Variable for var " << v << " in range " << (*i) << " .. " << (*(i+k)) << std::endl;
                    inHall[std::make_tuple(v,*i, *(i+k))] = alu;

                    if (!s_.createClause(LitVec{vc_.getLiteral(r.begin()),lblit})) return false;
                    if (!s_.createClause(LitVec{vc_.getLiteral(r.begin()),ublit})) return false;
                    if (!s_.createClause(LitVec{ ~(vc_.getLiteral(r.begin())), ~lblit, ~ublit})) return false;
                }
            }

    }


    ///TODO, für die a_ilu könnte man subset relationen kodieren ? also, a00 gilt immer wenn a01 auch gilt und so ...

    LitVec newLits;
    for (auto i = d.begin(); i != d.end(); ++i)
    {
        uint32 newHall = std::min(conf_.hallsize,uint32((uint32)(d.end()-i)));
        for (int k = 0; k <= static_cast<int>(newHall)-1; ++k)
        {
            LinearConstraint c(LinearConstraint::Relation::LE);
            c.addRhs(k+1);
            int low = (*i);
            int up = (*(i+k));
            for (auto v : views)
            {
                auto found = inHall.find(std::make_tuple(v,low,up));
                if (found!=inHall.end())
                {
                    c.add(View(found->second),1);
                }
            }

            if (s_.isFalse(l.getLiteral()))
            {
                c.setRelation(LinearConstraint::Relation::GT);
                newLits.emplace_back(s_.getNewLiteral(true));
                ReifiedLinearConstraint r(std::move(c), newLits.back());
                r.normalize();
                auto splitted = r.split(vc_,s_, conf_);
                assert(splitted.size());
                for (auto& i : splitted)
                    if (!addLinear(std::move(i))) /// newvar_i <-> not alldiff
                        return false;

            }
            else
            if (s_.isTrue(l.getLiteral()))
            {
                ReifiedLinearConstraint r(std::move(c), l.getLiteral());
                r.normalize();
                auto splitted = r.split(vc_,s_, conf_);
                assert(splitted.size());
                addImp(std::move(splitted.front())); /// l.v -> alldiff

                for (auto i = splitted.begin()+1; i !=splitted.end();++i)
                    if (!addLinear(std::move(*i)))
                        return false;
            }
            else
            //if (!s_.isTrue(l.getLiteral()) && !s_.isFalse(l.getLiteral()))
            {
                c.setRelation(LinearConstraint::Relation::EQ); // set if eq just for splitting purposes, saves some intermediate variables
                ReifiedLinearConstraint r(std::move(c),l.getLiteral());
                r.normalize();
                auto splitted = r.split(vc_,s_, conf_);
                assert(splitted.size());
                for (auto i = splitted.begin()+1; i !=splitted.end();++i)
                    if (!addLinear(std::move(*i)))
                        return false;

                newLits.emplace_back(s_.getNewLiteral(true));
                ReifiedLinearConstraint t(splitted.front());
                t.v = newLits.back();
                t.l.setRelation(LinearConstraint::Relation::GT);
                if (!addLinear(std::move(t))) /// newvar_i <-> not alldiff
                    return false;
                splitted.front().l.setRelation(LinearConstraint::Relation::LE);
                addImp(std::move(splitted.front()));/// l.v -> alldiff
            }

        }
    }

    if (!s_.isTrue(l.getLiteral()))
    {
        LitVec clause(newLits);
        clause.emplace_back(l.getLiteral());
        if (!s_.createClause(clause)) // either all are different or some or equal
            return false;
        for (auto i : newLits)
            if (!s_.createClause(LitVec{~l.getLiteral(), ~i})) /// l.v -> not newvar, newvar -> not l.v
                return false;
    }
    return true;
}*/


bool Normalizer::addDomainConstraint(ReifiedDomainConstraint&& d)
{
    LitVec longc;
    assert(!s_.isFalse(d.getLiteral()) && !s_.isTrue(d.getLiteral()));
    for (const auto &i : d.getDomain().getRanges())
    {
        if (i.l == i.u)
        {
            Literal u = vc_.getEqualLit(d.getView(),i.l);
            longc.emplace_back(u);
        }
        else
        {
            Literal u = s_.getNewLiteral(false);
            longc.emplace_back(u);

            Restrictor r = vc_.getRestrictor(d.getView());
            auto it = std::lower_bound(r.begin(),r.end(),i.l);
            Literal x = vc_.getGELiteral(it);
            it = std::upper_bound(it,r.end(),i.u);
            Literal y = vc_.getLELiteral(it);
            
            if (!s_.createClause(LitVec{~u,x})) return false;
            if (!s_.createClause(LitVec{~u,y})) return false;
            if (!s_.createClause(LitVec{u,~x, ~y})) return false;
        }
    }

    longc.emplace_back(~d.getLiteral());
    if (!s_.createClause(longc)) return false;
    longc.pop_back();
    for (const auto& i : longc)
        if (!s_.createClause(LitVec{~i,d.getLiteral()})) return false;

    return true;
}

bool Normalizer::addDisjoint(ReifiedDisjoint&& l)
{
    ReifiedNormalizedDisjoint d(std::move(l),s_);

    if (conf_.disjoint2distinct)
    {
        std::vector<View> views;
        bool allDiff=true;
        /// test if reified is an alldifferent
        for (auto& i : d.getViews())
        {
            if (i.size()!=1)
            {
                allDiff=false;
                break;
            }
            auto pair = i.back();
            if (!s_.isTrue(pair.second))
            {
                allDiff=false;
                break;
            }
            views.emplace_back(pair.first);
        }

        if (allDiff)
        {
            addConstraint(ReifiedAllDistinct(std::move(views),l.getLiteral()));
            return true;
        }
    }

    /// first vector stores for every tuple
    /// a map having key is the value the variable should take
    /// and DNF as two vectors
    std::vector<std::map<int,std::vector<std::vector<Literal>>>> DNF;
    for (auto &i : d.getViews())
    {
        DNF.emplace_back(std::map<int,std::vector<std::vector<Literal>>>());
        auto &map = DNF.back();
        for (auto &viewcondpair : i)
        {
            View v = viewcondpair.first;
            Literal cond = viewcondpair.second;
            auto dom = vc_.getViewDomain(v);
            for (auto val : dom)
            {
                auto it = map.lower_bound(val);
                if (it == map.end() || (it->first != val))
                    it = map.insert(it,std::make_pair(val,std::vector<std::vector<Literal>>()));
                it->second.emplace_back(std::vector<Literal>{vc_.getEqualLit(v,val), cond});
            }
        }
    }


    Domain dom(1,-1);
    std::vector<std::map<int,Literal>> conditions;
    for (auto &tuple : DNF)
    {
        conditions.emplace_back(std::map<int,Literal>());
        auto &map = conditions.back();
        for (auto &value : tuple)
        {
            ReifiedDNF d(std::move(value.second));
            Literal aux_x_y(d.tseitin(s_));             /// is true if one of the variables for the tuple has been set to "value"
            assert(map.find(value.first)==map.end()); // should only occur once
            map.insert(std::make_pair(value.first,aux_x_y));
//            auto it = map.lower_bound(value.first);
//            if (it == map.end() || (it->first != value.first))
//                it = map.insert(it, std::make_pair(value.first,aux_x_y));
//            else
//                it->second.emplace_back(aux_x_y);
            dom.unify(value.first,value.first);
        }
    }


    LitVec auxs;
    ///1. add a new literal which is reified with a cardinality constraint
    ///2. make reification with original d
    for (const auto &value : dom)
    {

        LitVec v;
        for (auto &tuple : conditions)
        {
            const auto& it = tuple.find(value);
            if (it != tuple.end())
            {
                v.emplace_back(it->second);
            }
        }
        Literal aux = s_.getNewLiteral(false);
        if (!s_.createCardinality(aux,2,std::move(v)))
            return false;
        if (!s_.createClause(LitVec{~aux,~d.getLiteral()}))
            return false;
        auxs.emplace_back(aux);
    }

    auxs.emplace_back(d.getLiteral());
    if (!s_.createClause(auxs))
        return false;

    return true;
}

bool Normalizer::calculateDomains()
{
    size_t removed = 0;
    for (size_t i = 0; i < domainConstraints_.size()-removed;)
    {
        using std::swap;
        auto& d = domainConstraints_[i];
        auto p = deriveSimpleDomain(d);
        if (!p.second) return false; // empty domain
        if (p.first) // simplified away
        {
            ++removed;
            domainConstraints_[i] = std::move(domainConstraints_[domainConstraints_.size()-removed]);
        }
        else
            ++i;
    }
    domainConstraints_.erase(domainConstraints_.begin()+(domainConstraints_.size()-removed), domainConstraints_.end());


    removed = 0;
    for (size_t i = 0; i < linearConstraints_.size()-removed;)
    {
        using std::swap;
        auto& d = linearConstraints_[i];
        d.normalize();
        auto p = deriveSimpleDomain(d);
        if (!p.second) return false; // empty domain
        if (p.first) // simplified away
        {
            ++removed;
            linearConstraints_[i] = std::move(linearConstraints_[linearConstraints_.size()-removed]);
        }
        else
            ++i;
    }
    linearConstraints_.erase(linearConstraints_.begin()+(linearConstraints_.size()-removed), linearConstraints_.end());

    /// restrict the domains according to the found equalities
    for (auto ec : ep_.equalities())
    {
        if (ec.first==ec.second->top()) /// original variable
        {
            for (auto it : ec.second->getConstraints())
            {
                //Variable v = it.first;
                EqualityClass::Edge e = it.second;
                getVariableCreator().constrainDomain(ec.second->top(),e.secondCoef,e.constant,e.firstCoef);
            }
        }
    }

    return true;
}


void Normalizer::checkDomains()
{
    ///TODO: what about views ? can be out of bound ?
    for (size_t i =0; i < vc_.numVariables(); ++i)
    {
        if (getVariableCreator().isValid(i))
        {
            auto r = vc_.getDomain(i);
            if ((r.upper()==std::numeric_limits<int>::max()-1 || r.lower()==std::numeric_limits<int>::min()) && conf_.minLitsPerVar == -1)
                //if (r.size()==std::numeric_limits<uint32>::max()-std::numeric_limits<uint32>::min())
                s_.unrestrictedDomainCallback(View(i));
            ///TODO: could be better specified, maybe i need restrictions to both directions,
            ///or maybe i impose a restriction on the size of the domain
        }
    }
}

namespace
{
uint64 allOrderLiterals(Variable v, const VariableCreator& vc)
{
    return std::max(vc.getDomainSize(v),1u)-1;
}
uint64 allLiterals(Variable v, const VariableCreator& vc)
{
    return std::max((uint64)(std::max(vc.getDomainSize(v),1u)-1) * 2,(uint64)1u) -1;
}
}

uint64 Normalizer::estimateVariables()
{
    estimate_.resize(std::max(estimate_.size(),getVariableCreator().numVariables()),0);
    checkDomains();
    uint64 sum = 0;
    
    for (auto& i : minimize_)
        estimate_[i.first.v]=allOrderLiterals(i.first.v,getVariableCreator());
    for (auto& i : linearConstraints_)
        sum += estimateVariables(i);
    for (auto& i : domainConstraints_)
        sum += i.getDomain().getRanges().size();
    for (auto& i : allDistincts_)
        sum += estimateVariables(i);
    for (auto& i : disjoints_)
        sum += estimateVariables(i);
    
    for (Variable i = 0; i <= getVariableCreator().numVariables(); ++i)
        if (getVariableCreator().isValid(i))
        {
            uint64 min = conf_.minLitsPerVar == -1 ? allOrderLiterals(i,getVariableCreator()) : std::min((uint64)(conf_.minLitsPerVar),allOrderLiterals(i,getVariableCreator()));
            sum += std::min(std::max(estimate_[i],min),allLiterals(i,getVariableCreator()));
        }
    return sum;
}

uint64 Normalizer::estimateVariables(const ReifiedLinearConstraint& i)
{
    uint64 sum = 0;
    if (i.l.getRelation()==LinearConstraint::Relation::EQ && !s_.isTrue(i.v))
        sum +=2;
    uint64 size = conf_.translateConstraints == -1 ? std::numeric_limits<uint64>::max() : conf_.translateConstraints;

    if (i.l.getViews().size()==1)
    {
        Variable v = i.l.getViews().begin()->v;
        estimate_[v] = std::min(estimate_[v]+1,allLiterals(v,getVariableCreator()));
    }
    else
    {
        uint64 product = i.l.productOfDomainsExceptLast(getVariableCreator());
        if (product<=size)
        {
            for (auto view : i.l.getViews())
                estimate_[view.v] = std::min(estimate_[view.v]+std::min(product,allLiterals(view.v,getVariableCreator())),allLiterals(view.v,getVariableCreator()));
        }
    }
    return sum;
}


namespace
{
/*
size_t recSplitHallConstraintEstimate(const CreatingSolver& s, std::vector<Domain>& vec, const Config& conf)
{
    if (vec.size()<=conf.splitsize_maxClauseSize.first || vec.size()<3)
        return 0;

    std::sort(vec.begin(), vec.end(), [](const Domain& x, const Domain& y) { return x.size() < y.size(); });

    size_t size=1;
    for (auto i = vec.begin(); i != vec.end()-1; ++i)
    {
            size*=i->size();
            if (size>conf.splitsize_maxClauseSize.second)
                break;
    }
    if (size<=conf.splitsize_maxClauseSize.second)
        return 0;

    std::vector<std::vector<Domain> > ret;
    ret.resize(conf.splitsize_maxClauseSize.first);
    std::size_t bucket = 0;
    for (auto& i : vec)
    {
        ret[bucket].emplace_back(std::move(i));
        bucket=(bucket+1)%conf.splitsize_maxClauseSize.first;
    }

    uint64 resultingSize = 0;

    for (int i = (int)(conf.splitsize_maxClauseSize.first-1); i >=0; --i)
    {
        if (ret[i].size()>=2)
        {
            Domain d = ret[i].front();
            for (std::vector<Domain>::const_iterator j = ret[i].begin()+1; j !=ret[i].end(); ++j)
            {
                d+=*j;
                if (d.overflow())
                    s.intermediateVariableOutOfRange();
            }
            resultingSize += d.size()-1;
            d.inplace_times(-1,conf.domSize);
            if (d.overflow())
                s.intermediateVariableOutOfRange();
            ret[i].emplace_back(std::move(d));
        }
    }
    for (auto i : ret)
        resultingSize += recSplitHallConstraintEstimate(s,i,conf);
    return resultingSize;
}

size_t splitHallConstraintEstimate(const CreatingSolver& s, size_t x, const Config& conf)
{
    std::vector<Domain> vec(x,Domain(0,1));
    return recSplitHallConstraintEstimate(s, vec, conf);
}*/

}


uint64 Normalizer::estimateVariables(const ReifiedAllDistinct& c)
{
    auto views = c.getViews();
    if (views.size()<=1) return 0;

    uint64 size = 0;
    ViewDomain d = unify(views,vc_,size);
    
    if (views.size()>d.size())
        return 0;

    if (size==d.size()) // no overlap between variables
        return 0;
    
    if (conf_.pidgeon)
    {
        int lower = *(d.begin()+(views.size()-1));
        int upper = *(d.end()-(views.size()+1-1));
        for (auto& i : views)
            if (getVariableCreator().isValid(i.v))
                estimate_[i.v]= std::min(allLiterals(i.v,getVariableCreator()), (uint64)(upper-lower+1)*2 + estimate_[i.v]);
    }

    uint64 perm = 0;
    if (conf_.permutation)
    {
        if (views.size()==d.size())
        {
            for (auto& i : views)
                if (getVariableCreator().isValid(i.v))
                    estimate_[i.v]= allLiterals(i.v,getVariableCreator());
            perm = d.size();
        }
    }

    if (conf_.alldistinctCard)
    {
        for (auto& i : views)
            estimate_[i.v]= std::min(uint64(estimate_[i.v]) + uint64(d.size()-1)*3,allLiterals(i.v,getVariableCreator()));
        return (s_.isTrue(c.getLiteral()) ? 0 : d.size()) + perm;
    }
    else// pairwise inequality
    {
        uint64 size = conf_.translateConstraints == -1 ? std::numeric_limits<uint64>::max() : conf_.translateConstraints;
                /// if we translate the inequalities, we need some order lits
        uint64 max = 0;
        for (auto i : views)
        {
            uint64 n = allLiterals(i.v,getVariableCreator());
            if (n <= size)
                max = std::max(max,n);
        }
        for (auto i : views)
        {
            uint64 n = allLiterals(i.v,getVariableCreator());
            if (n <= size)
                estimate_[i.v] = std::min(estimate_[i.v] + n,allLiterals(i.v,getVariableCreator()));
            else
                estimate_[i.v] = std::min(estimate_[i.v] + max,allLiterals(i.v,getVariableCreator()));
        }
        return ((views.size()*views.size() + 1)/2)*2 + (s_.isTrue(c.getLiteral()) ? 0 : ((views.size()*views.size() + 1) / 2)) + perm;
    }
    //return d.size()*conf_.hallsize*views.size() +  d.size()*conf_.hallsize*splitHallConstraintEstimate(s_, views.size(),conf_) + permutation;
}



uint64 Normalizer::estimateVariables(const ReifiedDisjoint& d)
{
    uint64 sum = 0;
    ///tseitin of conditions
    for (const auto &i : d.getViews())
        for (const auto &j : i)
            sum += j.second.estimateVariables();

    std::vector<View> vars;
    bool allDiff=true;
    /// test if reified is an alldifferent
    for (const auto& i : d.getViews())
    {
        if (i.size()!=1)
        {
            allDiff=false;
            break;
        }
        auto pair = i.back();
        vars.emplace_back(pair.first);
    }

    if (allDiff && conf_.disjoint2distinct)
    {
        return sum + estimateVariables(ReifiedAllDistinct(std::move(vars),d.getLiteral()));
    }

    ViewDomain dom(1,-1);
    for (const auto &i : d.getViews())
    {
        for (auto &varcondpair : i)
        {
            View v = varcondpair.first;
            auto d = vc_.getViewDomain(v);
            estimate_[v.v] = std::max(estimate_[v.v] + (uint64)(std::max(d.size(),(uint64)1u)-1)*3-1,allLiterals(v.v,getVariableCreator()));
            dom.unify(d);
        }
    }
    return sum += dom.size();
}






bool Normalizer::prepare()
{
    if (conf_.equalityProcessing)
        if (!equalityPreprocessing())
            return false;
    
    /// calculate very first domains for easy constraints and remove them
    if (!calculateDomains())
        return false;

    /// split constraints
    std::size_t csize = linearConstraints_.size();
    for (std::size_t i = 0; i < csize; ++i)
    {
        ///TODO: sugar does propagation while splitting, this can change order/everything
        linearConstraints_[i].normalize();
        std::vector<ReifiedLinearConstraint> splitted = linearConstraints_[i].split(vc_, s_, conf_);
        linearImplications_.reserve(splitted.size()+linearConstraints_.size()-1);// necessary? insert could do this
        assert(splitted.size()>0);
        linearConstraints_[i] = std::move(*splitted.begin());

        for (auto j = splitted.begin()+1; j < splitted.end(); ++j)
            addConstraint(std::move(*j));
    }

    /// do propagation on true/false literals
    LinearPropagator p(s_, vc_);
    for (auto& i : linearConstraints_)
    {
        i.l.normalize();
        if (!s_.isTrue(i.v) && !s_.isFalse(i.v))
            continue;
        if (i.l.getRelation()==LinearConstraint::Relation::LE )
        {
            ReifiedLinearConstraint l(i); // make a copy

            if (s_.isTrue(l.v))
            {
                l.sort(vc_);
                p.addImp(std::move(l));
            }
            else
                if (s_.isFalse(l.v)) /// Maybe makes problems with assumptions?
                {
                    l.reverse();
                    l.v = ~l.v;
                    l.sort(vc_);
                    p.addImp(std::move(l));
                }
        }
        if (i.l.getRelation()==LinearConstraint::Relation::EQ && s_.isTrue(i.v))
        {
            ReifiedLinearConstraint l(i); // make a copy
            l.l.setRelation(LinearConstraint::Relation::LE);
            l.sort(vc_);
            p.addImp(std::move(l));
            ReifiedLinearConstraint u(i); // make a copy
            u.l.setRelation(LinearConstraint::Relation::GE);
            u.sort(vc_);
            p.addImp(std::move(u));
        }
    }
    if(!p.propagate())
        return false;

    ///update the domain
    for (std::size_t i = 0; i < vc_.numVariables(); ++i)
    {
        if (getVariableCreator().isValid(i))
        {
            auto r = p.getVariableStorage().getCurrentRestrictor(i);
            vc_.constrainView(View(i),r.lower(), r.upper()); /// should be the same as intersect (but cheaper),
        }
        /// as i only do bound propagation
    }

    return true;
}

void Normalizer::addMinimize()
{
    for (auto p : minimize_)
    {
        View v = p.first;
        unsigned int level = p.second;
        auto& vc = getVariableCreator();
        auto res = vc.getRestrictor(v);
        order::uint64 before = 0;
        for (auto it = res.begin(); it != res.end(); ++it)
        {
            int32 w = ((*it) - before);
            before = *it;
            s_.addMinimize(vc.getGELiteral(it),w,level);
        }
    }
}

bool Normalizer::createClauses()
{
    //for (std::size_t i = 0; i < linearConstraints_.size(); ++i)
    //    std::cout << i << ":\t" << linearConstraints_[i].l << " << " << linearConstraints_[i].v.asUint() << std::endl;
    /// 1st: add all linear constraints
    for (auto& i : linearConstraints_)
        if (!addLinear(std::move(i))) /// adds it to the constraints
            return false;
    linearConstraints_.clear();
    for (auto& i : domainConstraints_)
    {
        if (!addDomainConstraint(std::move(i)))
            return false;
    }
    domainConstraints_.clear();
    //for (std::size_t i = 0; i < linearImplications_.size(); ++i)
    //    std::cout << i << ":\t" << linearImplications_[i].l << " << " << linearImplications_[i].v.asUint() << std::endl;

    for (auto& i : disjoints_)
        if (!addDisjoint(std::move(i)))
            return false;
    disjoints_.clear();
    
    /// add even more constraints and also some orderVariables for hallIntervals
    for (auto& i : allDistincts_)
        if (!addDistinct(std::move(i)))
            return false;
    allDistincts_.clear();
    
    /// remove duplicates
    sort( linearImplications_.begin(), linearImplications_.end() );
    linearImplications_.erase( unique( linearImplications_.begin(), linearImplications_.end() ), linearImplications_.end() );


    // TODO do propagate only if added allDistinct constraints
    ///Cant do propagation here, as it does not propagate to clasp -> can restrict domains
    /// -> this can result in unused orderLits -> make them false in createOrderLits
    ///So either not propagate -> more order lits
    /// propagate and add clauses for clasp manually that apply after creating order literals
    /// cant use propagateWithReason as it uses orderLiterals

    ///TODO: do restrict variables if equality preprocessing restricts integer variables
    if (!vc_.restrictDomainsAccordingToLiterals())
        return false;

    LinearPropagator p3(s_,vc_);
    p3.addImp(std::move(linearImplications_));

    // do propagate all original constraints
    if(!p3.propagate())
    {
        return false;
    }
    ///update the domain
    for (std::size_t i = 0; i < vc_.numVariables(); ++i)
    {
        if (getVariableCreator().isValid(i))
        {
            const auto& r = p3.getVariableStorage().getCurrentRestrictor(i);
            if (!vc_.constrainView(View(i), r.lower(), r.upper()))
                return false;
        }
    }
    linearImplications_ = p3.removeConstraints();


    vc_.prepareOrderLitMemory();

    if (!createEqualClauses())
        return false;
    
    if (!vc_.createOrderLiterals())
        return false;

    if (!translate(s_,getVariableCreator(),constraints(),conf_))
        return false;

    if (conf_.explicitBinaryOrderClausesIfPossible && !createOrderClauses())
        return false;
    
    addMinimize();

    //for (std::size_t i = 0; i < constraints_.size(); ++i)
    //    std::cout << i << ":\t" << constraints_[i].l << " << " << constraints_[i].v.asUint() << std::endl;

    /// make all unecessary ones false
    s_.makeRestFalse();
    

    return true;
}

void Normalizer::addImp(ReifiedLinearConstraint&& l)
{
    l.normalize();
    l.l.sort(vc_);
    assert(l.l.getRelation()==LinearConstraint::Relation::LE);
    linearImplications_.emplace_back(std::move(l));
}


bool Normalizer::createOrderClauses()
{
    /// how to decide this incrementally?  recreate per var ?
    for (Variable var = 0; var <vc_.numVariables(); ++var)
    {
        if (getVariableCreator().isValid(var))
        {
            const auto& lr = vc_.getRestrictor(View(var));
            if (lr.size()>=3)
            {
                auto start = pure_LELiteral_iterator(lr.begin(), vc_.getStorage(var), true);
                auto end =   pure_LELiteral_iterator(lr.end()-2, vc_.getStorage(var), false);
                for (auto next = start; next != end;)
                {
                    auto old = next;
                    ++next;
                    if (old.isValid() && next.isValid() && old.numElement()+1 == next.numElement())
                        if (!s_.createClause(LitVec{~(*old),*next}))
                            return false;
                    //if (vc_.hasLELiteral(v) && vc_.hasLELiteral(v+1))
                    //if (!s_.createClause(LitVec{~(vc_.getLELiteral(v)),vc_.getLELiteral(v+1)}))// could use GELiteral instead
                    //    return false;
                    //++v;
                }
            }
        }
    }
    return true;
}

bool Normalizer::createEqualClauses()
{
    return vc_.createEqualClauses();
}


bool Normalizer::equalityPreprocessing()
{
    if (!ep_.process(linearConstraints_))
        return false;
    for (auto& i : allDistincts_)
        if (!ep_.substitute(i))
            return false;
    for (auto& i : domainConstraints_)
        if (!ep_.substitute(i))
            return false;
    for (auto& i : disjoints_)
        if (!ep_.substitute(i))
            return false;
    for (Variable v = 0; v != getVariableCreator().numVariables(); ++v)
        if (!ep_.isValid(v))
            getVariableCreator().removeVar(v);
    return true;
}

}
