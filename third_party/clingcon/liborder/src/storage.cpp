#include "storage.h"
#include "constraint.h"



namespace order
{



bool VariableCreator::constrainDomain(const Variable& v, int32 times, int32 c, int32 div)
{
    assert(isValid(v));
    std::unique_ptr<Domain> copy(new Domain(*domains_[v]));
    if (!copy->constrainDomain(times, c, div))
        return false;
    domainChange(v,*copy);
    domains_[v] = std::move(copy);
    return true;
}

bool VariableCreator::intersectView(const View &v, Domain d)
{
    assert(isValid(v.v));
    std::unique_ptr<Domain> copy(new Domain(*domains_[v.v]));
    
    d += -v.c;
    ///TODO: need to have solver for this
    //if (copy->overflow())
        //s.intermediateVariableOutOfRange();
    d.inplace_divide(v.a);
    
    if (!copy->intersect(d))
        return false;
    if (!domainChange(v.v,*copy))
        return false;
    domains_[v.v] = std::move(copy);
    return true;
    //return domains_[v]->intersect(d);
}


bool VariableCreator::removeFromView(const View& v, int x)
{
    assert(isValid(v.v));
    
    x -= v.c;
    if (x%v.a!=0)
        return true;
    x /= v.a;
    
    std::unique_ptr<Domain> copy(new Domain(*domains_[v.v]));
    if (!copy->remove(x))
        return false;
    if (!domainChange(v.v,*copy))
        return false;
    domains_[v.v] = std::move(copy);
    return true;
}

bool VariableCreator::setLELit(const Restrictor::ViewIterator& it, Literal l)
{
    if (it.view().reversed())
                return setGELit(ViewIterator::viewToVarIterator(it),l);
    if (getDomainSize(it.view()) == it.numElement()) /// end iterator, after range
        return s_.setEqual(l,s_.falseLit());
    Variable v = it.view().v;
    isValid(v);
    prepareOrderLitMemory(v);
    if (isFlagged(it))
        orderLitMemory_[v].setLiteral(it.numElement(), l);
    else
        if (!s_.setEqual(l,getLELiteral(it)))
            return false;
    if (s_.isTrue(l))
        return constrainUpperBound(it.view(),*it);
    if (s_.isFalse(l))
        return constrainLowerBound(it.view(),*(it+1));
    return true;
}

bool VariableCreator::setGELit(const Restrictor::ViewIterator& it, Literal l)
{
    if (it.view().reversed())
                return setLELit(ViewIterator::viewToVarIterator(it),l);
    if (getDomainSize(it.view()) == it.numElement()) /// end iterator, after range
        return s_.setEqual(l,s_.falseLit());
    Variable v = it.view().v;
    isValid(v);
    
    if (it.numElement()==0)
        return s_.setEqual(s_.trueLit(), l);
    prepareOrderLitMemory(v);
    if (isFlagged(it-1))
        orderLitMemory_[v].setLiteral((it-1).numElement(), ~l);
    else
        if (!s_.setEqual(l,getGELiteral(it)))
            return false;
    if (s_.isTrue(l))
        return constrainLowerBound(it.view(),*it);
    if (s_.isFalse(l))
        return constrainUpperBound(it.view(),*(it-1));
    return true;
}


std::pair<bool,Literal> VariableCreator::hasEqualLit(Restrictor::ViewIterator it) const
{
    it = ViewIterator::viewToVarIterator(it);
    if (getDomainSize(it.view()) == it.numElement()) /// end element
        return std::make_pair(true,s_.falseLit());
    Variable v = it.view().v;
    assert(isValid(v));
    ViewDomain d = getViewDomain(it.view());
    if (!d.in(*it))
        return std::make_pair(true,s_.falseLit());
    auto f = equalLits_.find(std::make_pair(v,*it));
    if (f!= equalLits_.end())
        return std::make_pair(true,f->second);
    else
    {
        /// if it is the lowest value in the domain
        /// then le and eq are equal
        if (d.lower()==*it && hasLELiteral(it))
            return std::make_pair(true,getLELiteral(it));
        if (d.upper()==*it && hasGELiteral(it))
            return std::make_pair(true,getGELiteral(it));
        
        return std::make_pair(false,s_.trueLit());
    }
}


bool VariableCreator::setEqualLit(Restrictor::ViewIterator it, Literal l)
{
    it = ViewIterator::viewToVarIterator(it);
    if (it.numElement()==getDomainSize(it.view()))
        return s_.setEqual(l,s_.falseLit());
    auto f = hasEqualLit(it);
    if (f.first) // already exists
    {
        return s_.setEqual(l,f.second);
    }
    else
    {
        /// it may happen that we do not have order literals but this is actually equivalent to an order literal
        if (it.numElement()==0)
            return setLELit(it,l);
        if (it.numElement()==getDomainSize(it.view()) - 1)
            return setGELit(it,l);
        equalLits_.insert(std::make_pair(std::make_pair(it.view().v,*it), l));
        return true;
    }
}


bool VariableCreator::createOrderLiterals()
{
    orderLitMemory_.resize(numVariables());
    // create order-literals for the domains
    for (std::size_t i = 0; i < numVariables(); ++i)
        if (isValid(i))
            if (!createOrderLiterals(i)) return false;
    return true;
}


bool VariableCreator::createOrderLiterals(const Variable& i)
{
    assert(i < numVariables());
    orderLitMemory_.resize(numVariables());
    std::size_t size = getDomainSize(View(i));
    assert(orderLitMemory_[i].isPrepared());
    ///prepareOrderLitMemory(i,size);

    //assert(orderLitMemory_[i].hasVector()); /// should always be a vector, as we create all orderLiterals
    //                                        /// post-priori, so we always want to use a vector

    if (size && conf_.minLitsPerVar)
    {
        unsigned int step = std::max(((int64)(size)/(conf_.minLitsPerVar-1)),(int64)1);
        for (size_t j = 0; j < size-1; j+=step)
            if (orderLitMemory_[i].hasNoLiteral(j))
            {
                orderLitMemory_[i].setLiteral(j,s_.getNewLiteral(true));
            }
    }
    /// the last one is always true
    if (orderLitMemory_[i].hasNoLiteral(size-1))
        orderLitMemory_[i].setLiteral(size-1, s_.trueLit());
    else///TODO: gets executed every time, but only necessary on changes
        if (!s_.createClause(LitVec{orderLitMemory_[i].getLiteral(size-1)}))
            return false;
    return true;
}


bool VariableCreator::restrictDomainsAccordingToLiterals()
{
    for (std::size_t i = 0; i < numVariables(); ++i)
    {
        if (isValid(i) && hasOrderLitMemory(i))
        {
            auto r = getRestrictor(View(i));
            if (r.size()==1)
                continue;
            pure_LELiteral_iterator pitbegin(r.begin(),orderLitMemory_[i],true);
            pure_LELiteral_iterator pitend(r.end(),orderLitMemory_[i],false);
            assert(pitbegin.isValid());
            assert(pitend.isValid());
            
            int32 lower = r.lower();// can be int32 here, as i only have Variables
            int32 upper = r.upper();
            
            while(pitbegin<=pitend)
            {
                assert(pitbegin.isValid());
                if (s_.isFalse(*pitbegin))
                {
                    lower = (*(r.begin() + pitbegin.numElement())) + 1;
                }
                else
                if (s_.isTrue(*pitbegin))
                {
                    upper = (*(r.begin() + pitbegin.numElement()));
                    break;
                }
                ++pitbegin;
            }
            if (!constrainView(View(i),lower,upper))
                return false;
        }
    }
    return true;
}


Literal VariableCreator::createEqualLit(Restrictor::ViewIterator it)
{
    it = ViewIterator::viewToVarIterator(it);
    assert(hasEqualLit(it).first==false);
    ViewDomain d = getViewDomain(it.view());
    if (d.lower()==*it)
        return getLELiteral(it);
    if (d.upper()==*it)
        return getGELiteral(it);
    Literal aux = s_.getNewLiteral(true);
    aux.clearFlag(); /// i do need to introduce clauses for this
    equalLits_.insert(std::make_pair(std::make_pair(it.view().v,*it), aux));
    return aux;
}


bool VariableCreator::createEqualClauses()
{
    for (auto& i : equalLits_)
    {
        Variable v = i.first.first;
        int value =  i.first.second;
        Literal l  =  i.second;
        if (l.flagged()) 
            continue;

        i.second.flag();  /// mark as already proceeded
        const auto res = getRestrictor(View(v));
        Literal a(s_.trueLit()); // shall be    x <= v    (exact)
        Literal b(s_.falseLit()); // shall be    x <= v-1  (or lower)
        const auto found = std::lower_bound(res.begin(), res.end(), value, [](int it, int val){ return it < val; });
        if (found==res.end() || (*found)!=value) /// if out of range or in a hole
        {
            if (!s_.createClause(LitVec{~l}))
                return false;
            continue;
        }
        else
        {
            assert((*found)==value);
            a = getLELiteral(found);
        }


        b = ~getGELiteral(found);

        if (!s_.createClause({l,~a,b}))
            return false;
        if (!s_.createClause({~l,~b}))
            return false;
        if (!s_.createClause({~l,a}))
            return false;
    }
    return true;
}

void VariableCreator::prepareOrderLitMemory()
{
    for (unsigned int i = 0; i < numVariables(); ++i)
        if (isValid(i))
            prepareOrderLitMemory(i);
}


/// prepare the orderlitMemory with sentinels if not already created
void VariableCreator::prepareOrderLitMemory(const Variable& var, std::size_t size)
{
    if (!hasOrderLitMemory(var))
    {
        ///TODO: what about the 50 here
        /// on lazy literals we always have the map in the beginning if the variable domain is > 50
        if (conf_.minLitsPerVar >= 0 && size>0)
        {
            orderLitMemory_[var].useMap();
        }

        /// we have the vector if we are not lazy
        /// if we are lazy we only have it for small sizes
        if (size<1000000 || conf_.minLitsPerVar == -1)
        {
            orderLitMemory_[var].useVector();
        }

        orderLitMemory_[var].setSize(size);
        orderLitMemory_[var].setLiteral(size-1,s_.trueLit());
    }
}


bool VariableCreator::domainChange(const Variable& var, int newLower, int newUpper)
{
    if (newUpper<newLower)
    {
        s_.createClause(LitVec{s_.falseLit()});
        return false;
    }
    if (hasOrderLitMemory(var))
    {
        auto r = getRestrictor(View(var));
        if (r.lower()==newLower && r.upper()==newUpper) return true;

        auto it = std::lower_bound(r.begin(),r.end(),newLower);
        size_t start = it - r.begin();
        
        it = std::upper_bound(it,r.end(),newUpper);
        size_t end = (it - r.begin());
        
        /// we prefer the map if available to create the clauses (and keep it up to date)
        if (orderLitMemory_[var].hasMap())
        {
            orderStorage::map& m = orderLitMemory_[var].getMap();
            for (auto h = m.begin(); h != m.end(); ++h)
            {
                /// everyting before start to false
                if (h->first < start)
                {
                    if (!s_.createClause(LitVec{~(h->second)}))
                        return false;
                    m.erase(h);
                }
                /// everyting after end to true
                if (h->first >= end)
                {
                    if (!s_.createClause(LitVec{h->second}))
                        return false;
                    m.erase(h);
                }
            }
        }
        else
        {
            assert(orderLitMemory_[var].hasVector());
            for (size_t i = 0; i < start; ++i) /// everyting before start to false
                if (!orderLitMemory_[var].hasNoLiteral(i))
                    if (!s_.createClause(LitVec{~(orderLitMemory_[var].getLiteral(i))}))
                        return false;
            for (size_t i = end; i < r.size(); ++i) /// everyting after end to true
                if (!orderLitMemory_[var].hasNoLiteral(i))
                    if (!s_.createClause(LitVec{orderLitMemory_[var].getLiteral(i)}))
                        return false;
        }

        /// keep the vector up to date
        if (orderLitMemory_[var].hasVector())
        {
            orderStorage::vector& v = orderLitMemory_[var].getVector();
            std::move(v.begin()+start, v.begin()+end, v.begin());
            assert(end-start<=v.size());
            //orderLitMemory_[var].vector->resize(end-start, Literal(0,false));
        }
        orderLitMemory_[var].setSize(end-start);
    }
    return true;
}

bool VariableCreator::domainChange(const Variable& var, const Domain& d)
{
    if (d.empty())
    {
        s_.createClause(LitVec{s_.falseLit()});
        return false;
    }

    if (hasOrderLitMemory(var))
    {
        auto r = getRestrictor(View(var));
        auto oldDom = getDomain(var);
        if (oldDom==d) return true;

        auto it = std::lower_bound(r.begin(),r.end(),d.lower());
        size_t start = it - r.begin();
        

        /// handle new holes
        /// i go through the new domain
        std::vector<ViewIterator> keep; //[) (second points after the hole)
        keep.emplace_back(it);
        auto i = d.begin();
        while (i != d.end())
        {
            auto hole = it;
            while (*i!=*it && it!=r.end()) // there seems to be a new hole
            {
                if (hasEqualLit(it).first)
                {
                    if (!setEqualLit(it,s_.falseLit()))
                        return false;
                }
                ++it;
            }
            if (hole!=it)
            {
                keep.emplace_back(hole); /// end of last part to keep
                keep.emplace_back(it); /// start of next part to keep
            }
            ++i;
            ++it;
        }

        auto endit = std::upper_bound(it,r.end(),d.upper());
        keep.emplace_back(endit);

        
        size_t end = (endit - r.begin());
        

        if (orderLitMemory_[var].hasMap())
        {
            auto& m = orderLitMemory_[var].getMap();
            auto k = keep.begin()+1;
            for (auto h = m.begin(); h != m.end(); ++h)
            {
                /// everyting before start to false
                if (h->first < start)
                {
                    Literal l = h->second;
                    m.erase(h);
                    if (!s_.createClause(LitVec{~l}))
                        return false;
                }
                else
                {
                    /// everyting after end to true
                    if (h->first >= end)
                    {
                        Literal l = h->second;
                        m.erase(h);
                        if (!s_.createClause(LitVec{l}))
                            return false;
                    }
                    else
                    {
                        if (h->first >= *k-r.begin() && h->first < (*(k+1))-r.begin())
                        {
                            m.erase(h);
                        }
                    }
                }
            }
        }


        if (orderLitMemory_[var].hasVector())
        {
            if (!orderLitMemory_[var].hasMap())
            {
                for (size_t i = 0; i < start; ++i) /// everyting before start to false
                    if (!(orderLitMemory_[var].hasNoLiteral(i)))
                        if (!s_.createClause(LitVec{~(orderLitMemory_[var].getLiteral(i))}))
                            return false;

                for (size_t i = end; i < r.size(); ++i) /// everyting after end to true
                    if (!(orderLitMemory_[var].hasNoLiteral(i)))
                        if (!s_.createClause(LitVec{orderLitMemory_[var].getLiteral(i)}))
                            return false;
            }

            ///keep x,x+1 and throw away the rest
            assert(keep.size()%2==0);
            auto& v = orderLitMemory_[var].getVector();
            auto insert = v.begin();
            for (auto i = keep.begin(); i != keep.end(); ++i)
            {
                size_t offset_start = (*i-r.begin());
                size_t offset = ((*(i+1)-r.begin()))-offset_start;
                std::move(v.begin()+offset_start,v.begin()+offset_start+offset,insert);
                insert+=offset;
                ++i;
            }

            assert(end-start<=v.size());                
        }

        orderLitMemory_[var].setSize(d.size());
        if (!orderLitMemory_[var].hasNoLiteral(d.size()-1))
            if (!s_.createClause(LitVec{orderLitMemory_[var].getLiteral(d.size()-1)}))
                return false;

    }
    return true;
}


VariableStorage::VariableStorage(const VariableCreator& vc, Literal trueLit) : trueLit_(trueLit), domains_(vc.domains_), orderLitMemory_(vc.orderLitMemory_)
{
    init();
}

VariableStorage::VariableStorage(const std::vector<std::unique_ptr<Domain> >& domains,
                                 const std::vector<orderStorage>& orderLitMemory,
                                 Literal trueLit) : trueLit_(trueLit), domains_(domains), orderLitMemory_(orderLitMemory)/*, equalLits_(equalLits)*/
{
    init();
}

void VariableStorage::init()
{
    addLevel();
    for (std::size_t i = 0; i < numVariables(); ++i)
    {
        rs_.emplace_back();
        if (isValid(i))
        {
            rs_.back().emplace_back(getRestrictor(View(i)));
            levelSets_.back().insert(i);
        }
    }
}


void VariableStorage::removeLevel()
{
    assert(levelSets_.size());
    for (auto i : levelSets_.back())
        rs_[i].pop_back();
    levelSets_.pop_back();
}


void VariableStorage::constrainVariable(const Restrictor& r)
{
    assert(r.begin().view().a==1);
    assert(r.begin().view().c==0);
    Variable v = r.begin().view().v;
    assert(isValid(v));
    assert(levelSets_.size());
    assert(r.begin()>=rs_[v].back().begin());
    assert(r.end()<=rs_[v].back().end());
    assert(r.isSubsetEQOf(rs_[v].back()));
    auto ret = levelSets_.back().insert(v);
    if (ret.second) // has newly been inserted
        rs_[v].emplace_back(r);
    else
        rs_[v].back() = r;
}


bool VolatileVariableStorage::setLELit(const Restrictor::ViewIterator &it, Literal l)
{ 
    if (it.view().reversed())
        return setGELit(ViewIterator::viewToVarIterator(it),l);
    assert (vs_.getDomain(it.view().v).size() > it.numElement()); /// end iterator, after range 
    Variable v = it.view().v;
    vs_.isValid(v);
    assert (volOrderLitMemory_[v].hasNoLiteral(it.numElement()));
    volOrderLitMemory_[v].setLiteral(it.numElement(), l);
    return true;
}


bool VolatileVariableStorage::setGELit(const Restrictor::ViewIterator& it, Literal l)
{
    if (it.view().reversed())
        return setLELit(ViewIterator::viewToVarIterator(it),l);
    assert (vs_.getDomain(it.view().v).size() > it.numElement()); /// end iterator, after range
    Variable v = it.view().v;
    vs_.isValid(v);
    assert (it.numElement()>0);
    assert (volOrderLitMemory_[v].hasNoLiteral((it-1).numElement()));
    volOrderLitMemory_[v].setLiteral((it-1).numElement(), l);
    return true;
}

}
