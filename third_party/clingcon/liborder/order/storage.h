#pragma once

#include "variable.h"
#include "domain.h"
#include "config.h"


#include <set>
#include <vector>
#include <iostream>
#include <memory>
#include <map>
#include <unordered_map>
#include <cmath>


namespace order
{

struct ReifiedLinearConstraint;
class LinearConstraint;
class VariableStorage;
class VolatileVariableStorage;
class pure_LELiteral_iterator;




class orderStorage
{
public:
    friend pure_LELiteral_iterator;
    using vector = LitVec;
    using map = std::map<unsigned int, Literal>;
private:
    enum store : unsigned int {hasvector=1, hasmap=2};
    unsigned int store_;
    /// \brief vector
    /// we may have a vector and/or a map (non-exclusive or but at least one)
    ///
    vector vector_;
    //std::unique_ptr<std::unordered_map<unsigned int,Literal>> hashMap;
    map map_;
    unsigned int maxSize_;
public:
    orderStorage() : store_(0), maxSize_(0) {}
    
    void useVector() { store_ = store_ | hasvector; }
    void useMap() { store_ = store_ | hasmap; }
    
    bool hasVector() const { return store_ & hasvector; }
    bool hasMap() const { return store_ & hasmap; }
    
    /// returns the number of literals already created
    uint32 numLits() const
    {
        if (hasMap())
            return map_.size();
        Literal l(0,false);
        l.flag();
        if (hasVector())
            return std::count_if(vector_.begin(),vector_.end(),[&l](const Literal& in){ return in.asUint()!=l.asUint();});
            
        return 0;        
    }
    
    bool isPrepared() const { return (hasMap() || hasVector()) && maxSize_>0 && store_>0; }
    //pre: hasVector
    vector& getVector() { assert(isPrepared()); return vector_; }
    //pre: hasVector
    const vector& getVector() const { assert(isPrepared()); return vector_; }
    //pre: hasMap
    map& getMap() { assert(isPrepared()); return map_; }
    //pre: hasMap
    const map& getMap() const { assert(isPrepared()); return map_; }
    
    // just increases/shrinks the vector
    // does not care for setting truthvalues !
    void setSize(unsigned int s)
    {
        maxSize_ = s;
        if (hasVector())
        {
            Literal l(0,false);
            l.flag();
            vector_.resize(maxSize_,l);
        }
    }
    
    bool hasNoLiteral(unsigned int index) const
    {
        assert(isPrepared());
        assert(index < maxSize_);
        if (store_ & hasvector)
            return vector_[index].flagged();
        else
            return map_.find(index) == map_.end();
    }
    
    void setLiteral(unsigned int index, const Literal &l)
    {
        assert(isPrepared());
        if (store_ & hasvector)
        {
            //std::cout << "create lit in vector " << l.var() << std::endl;
            vector_[index]=l;
        }

        /// if map is a third of the vector, remove it
        if ((store_ == (hasvector & hasmap)) && map_.size()*3>=maxSize_)
        {
            map_.clear();
            store_ = hasvector;
        }

        if (store_ & hasmap)
        {
            //std::cout << "create lit in hasmap " << l.var() << std::endl;
            map_.emplace(index,l);
        }

        if (store_ == hasmap && map_.size()*3>=maxSize_) /// convert map to vector if it gets too large
        {
            store_ = store_ | hasvector; 
            Literal l(0,false);
            l.flag();
            vector_.resize(maxSize_, l);
            while(!map_.empty())
            {
                vector_[map_.begin()->first] = map_.begin()->second;
                map_.erase(map_.begin());
            }
        }
    }
    
    Literal getLiteral(unsigned int index) const
    {
        assert(isPrepared());
        if (store_ & hasvector)
            return vector_[index];
        else
        {
            assert(map_.find(index)!=map_.end());
            return map_.find(index)->second;
        }
    }
};


/// iterates only over literals, avoids non created literals
/// works on variables only, view has to be simple!
class pure_LELiteral_iterator : public std::iterator<std::random_access_iterator_tag, std::pair<int,Literal> >
{
public:

    /// creates a plr with an iterator pointing to the next valid literal which is
    /// up=true: >= it
    /// up=false:<=it
    /// pre: it.view().reversed()==false
    /// storage s should be storage(vc.orderLitMemory_[it.view().v])
    pure_LELiteral_iterator(const Restrictor::ViewIterator& it, const orderStorage& s, bool up) :
        storage_(s), valid_(false)
    {
        assert(!it.view().reversed());
        assert(it.view().c==0); // not necessary, but does not make much sense ?
        
        unsigned int realIndex = it.numElement();
        if (storage_.hasMap())
        {
            
            mapit_ = storage_.getMap().lower_bound(realIndex);
            valid_ = mapit_ != storage_.getMap().end();
            if (!up)
            {
                valid_ = true;

                if (mapit_ == storage_.getMap().end())
                    --mapit_;
                else 
                if (mapit_->first!=realIndex)
                {
                    if (mapit_==storage_.getMap().begin())
                        valid_ = false;
                    else
                        --mapit_;
                }
            }
        }
        else
        {
            assert(storage_.hasVector());
            vectorit_ = storage_.getVector().begin()+realIndex;
            if (up)
            {
                while(vectorit_!=storage_.getVector().end() && vectorit_->flagged())
                    ++vectorit_;
                valid_ = vectorit_ != storage_.getVector().end();
            }
            else
            {
                if (vectorit_==storage_.getVector().end())
                    --vectorit_;
                while(vectorit_!=storage_.getVector().begin() && vectorit_->flagged())
                    --vectorit_;
                valid_ = vectorit_!=storage_.getVector().begin() || !vectorit_->flagged();
            }
        }
    }



    bool isValid() const { return valid_; }
    pure_LELiteral_iterator(const pure_LELiteral_iterator& ) = default;
    pure_LELiteral_iterator& operator=(const pure_LELiteral_iterator& it)
    {
        assert(&storage_ == &it.storage_);
        mapit_ = it.mapit_;
        vectorit_ = it.vectorit_;
        valid_ = it.valid_;
        return *this;
    }

    // please do only compare iterators of the same domain
    bool operator==(pure_LELiteral_iterator const &x) const { assert(storage_.hasMap() == x.storage_.hasMap()); assert(valid_); return (storage_.hasMap() ? mapit_ == x.mapit_ : vectorit_ == x.vectorit_); }
    bool operator!=(pure_LELiteral_iterator const &x) const { assert(storage_.hasMap() == x.storage_.hasMap()); assert(valid_); return (storage_.hasMap() ? mapit_ != x.mapit_ : vectorit_ != x.vectorit_); }
    
    bool operator<(const pure_LELiteral_iterator& x) const { assert(storage_.hasMap() == x.storage_.hasMap()); assert(valid_);  return (storage_.hasMap() ? mapit_->first < x.mapit_->first : vectorit_ < x.vectorit_); }
    bool operator<=(const pure_LELiteral_iterator& x) const { assert(storage_.hasMap() == x.storage_.hasMap()); assert(valid_);  return (storage_.hasMap() ? mapit_->first <= x.mapit_->first : vectorit_ <= x.vectorit_); }

    pure_LELiteral_iterator& operator++()
    {                
        assert(valid_);
        if (storage_.hasMap())
        {
            ++mapit_;
            valid_ = mapit_ != storage_.getMap().end();
        }
        else
        {
            while(vectorit_ != storage_.getVector().end())
            {
                ++vectorit_;
                if (!vectorit_->flagged())
                    break;
            }
            valid_ = vectorit_ != storage_.getVector().end();
        }
        return *this;
    }

    pure_LELiteral_iterator operator++(int)
    {
        auto temp(*this);
        assert(valid_);
        if (storage_.hasMap())
        {
            ++mapit_;
            valid_ = mapit_ != storage_.getMap().end();
        }
        else
        {
            while(vectorit_ != storage_.getVector().end())
            {
                ++vectorit_;
                if (!vectorit_->flagged())
                    break;
            }
            valid_ = vectorit_ != storage_.getVector().end();
        }
        return temp;
    }

    pure_LELiteral_iterator& operator--()
    {
        assert(valid_);
        if (storage_.hasMap())
        {
            if (mapit_ == storage_.getMap().begin())
            {
                valid_ = false;
            }
            else
                --mapit_;
        }
        else
        {
            while(vectorit_ != storage_.getVector().begin())
            {
                --vectorit_;
                if (!vectorit_->flagged())
                    break;
            }
            valid_ = !vectorit_->flagged();
        }
        return *this;
    }
    pure_LELiteral_iterator operator--(int)
    {
        auto temp(*this);
        assert(valid_);
        if (storage_.hasMap())
        {
            if (mapit_ == storage_.getMap().begin())
            {
                valid_ = false;
            }
            else
                --mapit_;
        }
        else
        {
            while(vectorit_ != storage_.getVector().begin())
            {
                --vectorit_;
                if (!vectorit_->flagged())
                    break;
            }
            valid_ = !vectorit_->flagged();
        }
        return temp;
    }

    unsigned int numElement() const { assert(valid_); return storage_.hasMap() ? mapit_->first : vectorit_ - storage_.getVector().begin(); }


    /// returns a literal
    Literal operator*() const { assert(valid_); return (storage_.hasMap() ? mapit_->second : *vectorit_); }
    Literal operator->() const { assert(valid_); return (storage_.hasMap() ? mapit_->second : *vectorit_); }
private:

    using Mapit = orderStorage::map::const_iterator;
    //using Hashit = VariableCreator::orderStorage::hashMap::element_type::iterator;
    using Vectorit = orderStorage::vector::const_iterator;
    const orderStorage & storage_;
    Mapit mapit_;
    Vectorit vectorit_;
    bool valid_;
};





/*
inline orderStorage::store operator|(orderStorage::store a, orderStorage::store b)
{return static_cast<orderStorage::store>(static_cast<int>(a) | static_cast<int>(b));}
inline orderStorage::store operator&(orderStorage::store a, orderStorage::store b)
{return static_cast<orderStorage::store>(static_cast<int>(a) & static_cast<int>(b));}
explicit operator bool(orderStorage::store a) { return static_cast<bool>(a); }
*/

class VariableCreator
{

public:
    friend VariableStorage;
    friend VolatileVariableStorage;

    VariableCreator(CreatingSolver& s, Config conf) : s_(s), conf_(conf) {}
    std::size_t numVariables() const { return domains_.size(); }

    ViewDomain getViewDomain(const View& v) const
    {
        assert(isValid(v.v));
        assert(v.a!=0);
        /*Domain d = *domains_[v.v];
        d.inplace_times(v.a,d.getRanges().size()); /// view domains must be correct,
                                                   /// otherwise getDomainSize() is incorrect
                                                   /// and i can also not compare View numElement with Var numElement
        d +=(v.c);
        */
        ViewDomain d(*domains_[v.v],v);
        return d;
    }

    /// restrict the values i of the domain of v
    /// to fullfill: (times*i + c)%div == 0
    bool constrainDomain(const Variable& v, int32 times, int32 c, int32 div);
    
    const Domain& getDomain(const Variable& v) const
    {
        assert(isValid(v));
        return *domains_[v];
    }
    
    unsigned int getDomainSize(const View& v) const
    {
        assert(isValid(v.v));
        return domains_[v.v]->size();
    }
//    const Domain& getDomain(const Variable& v) const
//    {
//        assert(isValid(v.v));
//        return *domains_[v];
//    }

    /// only possible before creating literals for it
    void removeVar(const Variable& v)
    {
        assert(isValid(v));
        domains_[v].reset(nullptr);
        assert(!hasOrderLitMemory(v));
        //assert(equalLits_.find(v)==equalLits_.end());
    }

    bool isValid(const Variable& v) const { return (v < domains_.size() && domains_[v]!=nullptr); }

    Variable createVariable(const Domain& d = Domain())
    {
        domains_.emplace_back(new Domain(d));
        Variable v = domains_.size()-1;
        return v;
    }

    /// intersects the domain of v with d
    /// pre: the variable must be valid
    /// returns false on empty domain
    bool intersectView(const View& v, Domain d);


    /// constrains the view by lower/upper
    /// pre: the variable must be valid
    /// returns false on empty domain or unsat
    /// function can potentially produce clauses
    bool constrainView(const View& v, int64 lower, int64 upper)
    {
        if (lower>upper)
            return false;
        Variable var = v.v;
        assert(isValid(v.v));
        int newlower = v.divide(v.reversed() ? upper : lower);
        int newupper = v.divide(v.reversed() ? lower : upper);
        if (domains_[var]->lower()==newlower && domains_[var]->upper()==newupper)
            return true;
        if (!domainChange(var,newlower,newupper)) return false;
        return domains_[var]->intersect(newlower,newupper);
    }


    /// remove x from the domain
    /// return false if the domain is empty
    bool removeFromView(const View& v, int x);



    /// return false if the domain is empty
    /// function can potentially produce clauses
    bool constrainUpperBound(const View& v, int u) {
        if (v.reversed())
            return constrainView(v,v.multiply(domains_[v.v]->upper()),u);
        else
            return constrainView(v,v.multiply(domains_[v.v]->lower()),u);
    }

    /// return false if the domain is empty
    /// function can potentially produce clauses
    bool constrainLowerBound(const View& v, int l) {
        if (v.reversed())
            return constrainView(v,l,v.multiply(domains_[v.v]->lower()));
        else
            return constrainView(v,l,v.multiply(domains_[v.v]->upper()));
    }


    /// returns a restrictor for inspection
    Restrictor getRestrictor(const View& v) const
    {
        assert(isValid(v.v));
        return Restrictor(v,*domains_[v.v]);
    }

    /// reification lit will be equal to the order literal afterwards
    /// will be equal to false, if used on end iterator
    bool setLELit(const Restrictor::ViewIterator &it, Literal l);
    
    /// reification lit will be equal to the order literal afterwards
    /// /// will be equal to false, if used on end iterator
    bool setGELit(const Restrictor::ViewIterator &it, Literal l);

   
    /// true if there exists an order literal
    bool hasLELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
                    return hasGELiteral(ViewIterator::viewToVarIterator(it));
        assert(isValid(it.view().v));
        if (getDomainSize(it.view()) == it.numElement()) return true;
        /// its either the iterator just before end, which is true, or it has an order literal
        return (getDomainSize(it.view())-1 == it.numElement()) || (hasOrderLitMemory(it.view().v) && !isFlagged(it));
    }
    
    /// true if there exists a literal =:= x>*it
    bool hasGELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
                    return hasLELiteral(ViewIterator::viewToVarIterator(it));
        if (getDomainSize(it.view()) == it.numElement()) return true;
        assert(isValid(it.view().v));
        /// its either the iterator at begin, which is true, or it has an order literal
        return (0 == it.numElement()) || (hasOrderLitMemory(it.view().v) && !isFlagged(it-1));
    }
    
    
    /// can generate a literal
    Literal getLELiteral(Restrictor::ViewIterator it)
    {
        if (it.view().reversed())
                    return getGELiteral(ViewIterator::viewToVarIterator(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return s_.trueLit();

        Variable v = it.view().v;
        assert(isValid(v));
        prepareOrderLitMemory(v);
        if (isFlagged(it))
            orderLitMemory_[v].setLiteral(it.numElement(), s_.getNewLiteral(true));
        return orderLitMemory_[v].getLiteral(it.numElement());
    }
    
    /// can't generate a literal
    /// literal must exist -> pre: hasLELiteral
    Literal getLELiteral(const Restrictor::ViewIterator& it) const 
    {
        if (it.view().reversed())
                    return getGELiteral(ViewIterator::viewToVarIterator(it));
        assert(hasLELiteral(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return s_.trueLit();

        Variable v = it.view().v;
        if (hasOrderLitMemory(v))
            return orderLitMemory_[v].getLiteral(it.numElement());
        else
        {
            assert(getDomainSize(it.view())-1 == it.numElement());
            return s_.trueLit();
        }
    }
    
    /// can generate a literal
    Literal getGELiteral(Restrictor::ViewIterator it)
    {
        if (it.view().reversed())
                    return getLELiteral(ViewIterator::viewToVarIterator(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return s_.falseLit();
        if (it.numElement()==0)
            return s_.trueLit();

        Variable v = it.view().v;
        assert(isValid(v));
        prepareOrderLitMemory(v);
        if (isFlagged(it-1))
            orderLitMemory_[v].setLiteral((it-1).numElement(), s_.getNewLiteral(true));
        return ~orderLitMemory_[v].getLiteral((it-1).numElement());
    }
       
    /// cant create literal
    Literal getGELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
                    return getLELiteral(ViewIterator::viewToVarIterator(it));
        assert(hasGELiteral(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return s_.falseLit();
        if (it.numElement()==0)
            return s_.trueLit();
        else
            return ~getLELiteral(it-1);
    }
     
    
    
  /*  TODO, if view is reversed, then negate the literal!!!
    Literal getLELit(const View& v, int32 rhs)
    {
        assert(c.getViews().size()==1);
        assert(c.getRelation()==LinearConstraint::Relation::LE);
        auto r = vc_.getRestrictor(view);
        auto found = std::upper_bound(r.begin(), r.end(), rhs);
        
        Is this correct here ? should i have a -1 interface
        if (found!=r.begin())
            return vc_.getLiteral(view.first, found-1);
        else
            return ~trueLit_;
    }
    */   

    /// restrict the domain of all variables according to the literals that are already set in the solver
    /// as orderliterals
    bool restrictDomainsAccordingToLiterals();

    /// returns true if there is a special literal for denoting var=i
    /// the literal is the second return
    std::pair<bool,Literal> hasEqualLit(Restrictor::ViewIterator it) const;

    /// returns the literal which is equivalent to var=i, creates one if necessary
    Literal getEqualLit(Restrictor::ViewIterator it)
    {
        it = ViewIterator::viewToVarIterator(it);
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return s_.falseLit();
        auto f = hasEqualLit(it);
        if (f.first) return f.second;
        return createEqualLit(it);
    }
    
    Literal getEqualLit(const View& v, int32 i)
    {
        Restrictor r = getRestrictor(v);
        auto it = std::lower_bound(r.begin(), r.end(), i);
        it = (it==r.end() || *it != i) ? r.end() : it;
        return getEqualLit(it);
    }

    /// stores unary eq/ne constraints as literals for reuse
    /// if the literal already exists, posts boolean equality of the two
    bool setEqualLit(Restrictor::ViewIterator it, Literal l);

    bool createOrderLiterals();

    bool createOrderLiterals(const Variable& i);

    /// could in theory also work incrementally ?
    bool createEqualClauses();

    void prepareOrderLitMemory();

    /// intern use only for pureLELiteralIterator's
    const orderStorage& getStorage(const Variable& v) const
    {
        assert(hasOrderLitMemory(v));
        return orderLitMemory_[v];
    }

private:
        
    bool isFlagged(const Restrictor::ViewIterator& it) const 
    {
        /// ensure that this is called for LE literal requests
        assert(!it.view().reversed());
        //it = ViewIterator::viewToVarIterator(it);
        Variable v = it.view().v;
        assert(hasOrderLitMemory(v));
        assert(domains_[v]->size() > it.numElement());
        return orderLitMemory_[v].hasNoLiteral(it.numElement());
    }
    
    /// return a Literal l=:=(v=i)
    /// pre: hasEqualLit(it).first = false
    Literal createEqualLit(Restrictor::ViewIterator it);
    
    bool hasOrderLitMemory(const Variable& var) const
    {
        if (!isValid(var)) return false;
        orderLitMemory_.resize(numVariables());
        return orderLitMemory_[var].isPrepared();
    }

    /// prepare the orderlitMemory with sentinels if not already created
    void prepareOrderLitMemory(const Variable& var, std::size_t size);
    void prepareOrderLitMemory(const Variable& var) { prepareOrderLitMemory(var, getDomainSize(View(var))); }

    /// always call this before domain will be restricted on lower/upper bound,
    /// if orderLitMemory was allocated this will reoder it
    /// and post the necessary constraints for the variables that have been removed
    /// note that wholes are not detected and are not allowed,
    /// so the new domain is not allowed to have new holes!
    bool domainChange(const Variable& var, int newLower, int newUpper);

    /// always call this before domain will be restricted to a smallereq domain,
    /// if orderLitMemory was allocated this will reoder it
    /// and post the necessary constraints for the variables that have been removed
    /// new holes are detected and allowed,
    /// so the new domain is not allowed to have new holes!
    bool domainChange(const Variable& var, const Domain& d);

    CreatingSolver& s_;
    // have to use unique pointers, otherwise pointers to domains will get invalid on resize
    std::vector<std::unique_ptr<Domain> > domains_;    

    mutable std::vector<orderStorage> orderLitMemory_; /// these unique pointers just manage the memory of the order literals
                                                       /// on very large domains > 1.000.000 and lazy_literals true we use a map instead
    std::map<std::pair<Variable,int>,Literal> equalLits_; // given a variable and a number, may contain a literal
                                                          // (var = int) == lit
                                                          // if the literal is not flagged, we still need to introduce equality constraints
                                                          // TODO: if equality lits are created on the fly, flag them ?
    Config conf_;

};  


///this calss has read only access to orderLiterals and the equalLiterals and the domain
class VariableStorage
{
public:
    /// directly get the literals and domain access from the VC
    VariableStorage(const VariableCreator& vc, Literal trueLit);
    /// directly give them access
    VariableStorage(const std::vector<std::unique_ptr<Domain> >& domains,
                    const std::vector<orderStorage>& orderLitMemory,            
                    /*const std::map<std::pair<Variable,int>,Literal>& equalLits,*/
                    Literal trueLit);
    
    //const VariableCreator& getVariableCreator() { return vc_; }

    /// interface for variableCreator
    std::size_t numVariables() const { return domains_.size(); }

    const Domain& getDomain(const Variable& v) const { return *domains_[v]; }

    bool isValid(const Variable& v) const { return v < domains_.size() && domains_[v]!=nullptr; }

    friend std::ostream& operator<< (std::ostream& stream, const VariableStorage& s);

    void addLevel() { levelSets_.push_back(VarSet()); }
    void removeLevel();
    
    /// return false if the domain is empty
    /// the iterator points to the first element not in the domain (or end)
    bool constrainUpperBound(const Restrictor::ViewIterator& u) {
        Variable v = u.view().v;
        ViewIterator toVar = ViewIterator::viewToVarIterator(u);
        if(u.view().reversed())
            return constrainLowerBound(toVar+1); // since upper bound is the element after it, we have to take one before it
        
        if (toVar.numElement()==domains_[v]->size()) // end iterator, should not change anything
            return true;

        if (toVar < rs_[v].back().begin())
            constrainVariable(Restrictor(rs_[v].back().begin(),rs_[v].back().begin()));
        else
            constrainVariable(Restrictor(rs_[v].back().begin(),toVar));
        return rs_[v].back().size()>0;
    }

    /// return false if the domain is empty
    bool constrainLowerBound(Restrictor::ViewIterator l) {
        Variable v = l.view().v;
        ViewIterator toVar = ViewIterator::viewToVarIterator(l);
        if(l.view().reversed())
            return constrainUpperBound(toVar);
        
        if (toVar > rs_[v].back().end())
            constrainVariable(Restrictor(rs_[v].back().end(),rs_[v].back().end()));
        else
            constrainVariable(Restrictor(toVar,rs_[v].back().end()));
        return rs_[v].back().size()>0;
    }


    /// returns a restrictor for inspection, to change it, call constrainVariable
    Restrictor getCurrentRestrictor(const View& v) const
    {
        assert(isValid(v.v));
        return Restrictor(v,rs_[v.v].back());
    }
    
    /// returns a restrictor for inspection, to change it, call constrainVariable
    const Restrictor& getCurrentRestrictor(const Variable& v) const
    {
        assert(isValid(v));
        return rs_[v].back();
    }
    
    /// returns a restrictor for inspection, to change it, call constrainVariable
    Restrictor getRestrictor(const View& v) const
    {
        assert(isValid(v.v));
        return Restrictor(v,getDomain(v.v));
    }
    
    const orderStorage& getOrderStorage(Variable v) const { assert(isValid(v)); return orderLitMemory_[v]; }
    
    /// true if there exists an order literal
    bool hasLELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
            return hasGELiteral(ViewIterator::viewToVarIterator(it));
        assert(isValid(it.view().v));
        if (getDomainSize(it.view()) == it.numElement()) return true;
        /// its either the iterator just before end, which is true, or it has an order literal
        return (getDomainSize(it.view())-1 == it.numElement()) || (!isFlagged(it));
    }
    
    
    /// true if there exists a literal =:= x>*it
    bool hasGELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
            return hasLELiteral(ViewIterator::viewToVarIterator(it));
        if (getDomainSize(it.view()) == it.numElement()) return true;
        assert(isValid(it.view().v));
        /// its either the iterator at begin, which is true, or it has an order literal
        return ((0 == it.numElement()) || (!isFlagged(it-1)));
    }
    
    
    // can't generate a literal
    /// literal must exist -> pre: hasLELiteral
    Literal getLELiteral(const Restrictor::ViewIterator& it) const 
    {
        if (it.view().reversed())
            return getGELiteral(ViewIterator::viewToVarIterator(it));
        assert(hasLELiteral(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return trueLit_;
        
        Variable v = it.view().v;
        return orderLitMemory_[v].getLiteral(it.numElement()); /// also has a last lit which is true
    }
    
    
    /// cant create literal
    Literal getGELiteral(const Restrictor::ViewIterator& it) const
    {
        if (it.view().reversed())
            return getLELiteral(ViewIterator::viewToVarIterator(it));
        assert(hasGELiteral(it));
        if (getDomainSize(it.view()) == it.numElement()) /// end element
            return ~trueLit_;
        if (it.numElement()==0)
            return trueLit_;
        else
            return ~getLELiteral(it-1);
    }
    

private:
    
    void init();
    
    unsigned int getDomainSize(const View& v) const
    {
        assert(isValid(v.v));
        return domains_[v.v]->size();
    }
    
    bool isFlagged(const Restrictor::ViewIterator& it) const 
    {
        /// ensure that this is called for LE literal requests
        assert(!it.view().reversed());
        //it = ViewIterator::viewToVarIterator(it);
        Variable v = it.view().v;
        assert(domains_[v]->size() > it.numElement());
        return orderLitMemory_[v].hasNoLiteral(it.numElement());
    }
    
    /// constrains the view on the current level by r
    /// pre: the variable must be valid
    /// pre: the View must be simple, i.e. a=1 and c=0
    /// pre: r must be "setsmaller" than the previous r on the same level and
    /// the r on the level before
    void constrainVariable(const Restrictor& r);
    

    Literal trueLit_;
    using ResVec = std::vector<Restrictor>;
    std::vector<ResVec> rs_; /// for each variable, there is a stack of restrictors (added whenever there was a change)
                             /// the restrictors need to have a simple view, ie a = 1, c = 0
    using VarSet = std::set<Variable>;
    std::vector<VarSet> levelSets_; /// for each level we have a set of stored variables, TODO: bad data structure ?
    //const VariableCreator& vc_;
    const std::vector<std::unique_ptr<Domain> >& domains_; // this is just a reference to the global domains
    const std::vector<orderStorage>& orderLitMemory_;
    //const std::map<std::pair<Variable,int>,Literal>& equalLits_;
                                                          
    
};

class VolatileVariableStorage
{
public:
    VolatileVariableStorage(const VariableCreator& vc, Literal trueLit) : volOrderLitMemory_(vc.orderLitMemory_),
                                                                          vs_(vc.domains_, volOrderLitMemory_, trueLit)
    {}
    
    VariableStorage& getVariableStorage() { return vs_; }
    
    /// reification lit will be equal to the order literal afterwards
    /// pre: it isFlagged (not created before)
    /// pre: it != end
    bool setLELit(const Restrictor::ViewIterator &it, Literal l);
    /// pre: it != begin
    bool setGELit(const Restrictor::ViewIterator& it, Literal l);

private:
      std::vector<orderStorage> volOrderLitMemory_;
      std::map<std::pair<Variable,int>,Literal> volEqualLit_;
      VariableStorage vs_;
};


inline std::ostream& operator<< (std::ostream& stream, const VariableStorage& s)
{
    for (std::size_t i = 0; i != s.rs_.size(); ++i)
    {
        if (s.isValid(i))
        {
            stream << "Var " << i << ":\t";
            for (auto r = s.rs_[i].begin(); r != s.rs_[i].end(); ++r)
                stream << *r;
            stream << std::endl;
        }
    }
    return stream;
}
}
