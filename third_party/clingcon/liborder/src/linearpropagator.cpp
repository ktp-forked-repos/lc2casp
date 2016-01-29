#include "linearpropagator.h"

namespace order
{
/*
namespace
{
std::pair<int64, int64>& operator +=(std::pair<int64, int64>& x, const std::pair<int64, int64>& y) {
    x.first += y.first; x.second += y.second;
    return x;
}

std::pair<int64, int64> operator -(const std::pair<int64, int64>& x, const std::pair<int64, int64>& y) {
    return std::make_pair(x.first - y.first, x.second - y.second);
}

std::pair<int64, int64> operator +(const std::pair<int64, int64>& x, const std::pair<int64, int64>& y) {
    return std::make_pair(x.first + y.first, x.second + y.second);
}

}*/



LitVec LinearLiteralPropagator::LinearConstraintClause::getClause(IncrementalSolver& s, VolatileVariableStorage& vs, bool createOnConflict) const
{
    LitVec ret;
    for (unsigned int i = 0; i < its_.size(); ++i)
    {
        if (vs.getVariableStorage().hasGELiteral(its_[i]))
        {
            ret.emplace_back(~vs.getVariableStorage().getGELiteral(its_[i]));
        }
        else
        {
            assert(conclusion_==i);
            /////// use pure LE lit iterator, but obey the direction of the view, if its reversed i have to go to the other direction etc...           
            Literal l = (!conflict_ || createOnConflict) ? s.getNewLiteral() : s.trueLit();
            newLit_ = l != s.trueLit();
            
            if (its_[i].view().reversed())
            {
                auto varit = ViewIterator::viewToVarIterator(its_[i]);

                if (conflict_)
                {
                    if (createOnConflict)
                        vs.setLELit(varit,l); // on view
                    auto it = pure_LELiteral_iterator(varit,vs.getVariableStorage().getOrderStorage(varit.view().v),true);
                    --it;
                    if (it.isValid())
                        ret.emplace_back(~*it);
                    else
                    {
                        assert(false);
                        ret.emplace_back(s.trueLit()); /// huh ?
                    }
                }
                else
                {
                    vs.setLELit(varit,l); // on view
                    ret.emplace_back(~l);
                }
            }
            else
            {
                auto varit = ViewIterator::viewToVarIterator(its_[i]-1);

                if (conflict_)
                {
                    if (createOnConflict)
                        vs.setLELit(varit,l); // on view
                    auto it = pure_LELiteral_iterator(varit,vs.getVariableStorage().getOrderStorage(varit.view().v),true);
                    ++it;
                    assert(it.isValid());
                    assert(s.isFalse(*it));
                    ret.emplace_back(*it);                                        
                }
                else
                {
                    vs.setLELit(varit,l); // on view
                    ret.emplace_back(l);
                }
            }
            
            
           /* auto it = ViewIterator::viewToVarIterator(its_[i]-1);

            pure_LELiteral_iterator pit(it,orderStorage,true);
            assert(pit.isValid());
            assert(*pit == l); ///should be a direct hit, and return the same literal
            //VariableCreator::pure_literal_iterator pit = vc.setLELiteral(its_[i]-1,~l);
            //Literal l = vc.createLELitOnline(its_[i]-1,s);
            if (!reversed())
                l = ~l;
            if (conflict_)
            {
                /// just look for the next literal which is already introduced,
                /// should simulate resolution on this literal
                if (reversed)
                {
                    --pit;
                    if (pit.isValid())
                        l = *pit;
                    else
                        true does not make much sense;
                }
                else
                {
                    ++pit;/// next valid literal
                    if (pit.isValid())
                        l = ~(*pit);
                    else
                        l = s_.falseLit();
                   
                        
                }
                assert(s.isFalse(l));
            }
            ret.emplace_back(l);*/
            
            
        }
    }
    ret.emplace_back(~constraint_.v);
    return ret;
}



LinearLiteralPropagator::LinearConstraintClause::iter LinearLiteralPropagator::LinearConstraintClause::getAddedIterator(const VariableStorage& vs)
{
    assert(conclusion_<its_.size());
    if (its_[conclusion_].view().reversed())
    {
        assert(vs.hasLELiteral(ViewIterator::viewToVarIterator(its_[conclusion_])));
        return ViewIterator::viewToVarIterator(its_[conclusion_]);
    }
    else
    {
        assert(vs.hasLELiteral(ViewIterator::viewToVarIterator(its_[conclusion_]-1)));
        return ViewIterator::viewToVarIterator(its_[conclusion_]-1);
    }
}


void ConstraintStorage::addImp(ReifiedLinearConstraint&& l)
{
    l.normalize();
    assert(l.l.getRelation()==LinearConstraint::Relation::LE);
    l.l.setFlag(true);
    linearImpConstraints_.emplace_back(std::move(l));
    auto id = linearImpConstraints_.size()-1;
    toProcess_.emplace_back(id);
    for (auto i : l.l.getConstViews())
    {
        assert(i.a!=0);
        
        /// can sometimes add a constraint twice for the same variable, should not be a problem
        /// TODO: find a place to call unqiue ?
        if (!i.reversed())
        {
            lbChanges_.resize(std::max(i.v+1,(unsigned int)(lbChanges_.size())));
            lbChanges_[i.v].emplace_back(id);
        }else
        {
            ubChanges_.resize(std::max(i.v+1,(unsigned int)(ubChanges_.size())));
            ubChanges_[i.v].emplace_back(id);
        }
    }

}

void ConstraintStorage::addImp(std::vector<ReifiedLinearConstraint>&& vl)
{
    for (auto& l : vl)
        addImp(std::move(l));
}

void ConstraintStorage::removeLevel()
{
    for (auto i: toProcess_)
        linearImpConstraints_[i].l.setFlag(false);
    toProcess_.clear();
}


/// return false if the domain is empty
void ConstraintStorage::constrainUpperBound(const View &view, const Solver& s)
{
    Variable v = view.v;
    
    if (ubChanges_.size()>v)
    for (auto i : ubChanges_[v])
    {
        bool check = (s.isTrue(linearImpConstraints_[i].v) && !view.reversed()) || (s.isFalse(linearImpConstraints_[i].v) && view.reversed());
        if (!linearImpConstraints_[i].l.getFlag() && (check || s.isUnknown(linearImpConstraints_[i].v)))
        {
            linearImpConstraints_[i].l.setFlag(true);
            toProcess_.emplace_back(i);
        }
    }

    if (lbChanges_.size()>v)
    for (auto i : lbChanges_[v])
    {
        bool check = (s.isTrue(linearImpConstraints_[i].v) && view.reversed()) || (s.isFalse(linearImpConstraints_[i].v) && !view.reversed());
        if (!linearImpConstraints_[i].l.getFlag() && (check || s.isUnknown(linearImpConstraints_[i].v)))
        {
            linearImpConstraints_[i].l.setFlag(true);
            toProcess_.emplace_back(i);
        }
    }
}

/// return false if the domain is empty
void ConstraintStorage::constrainLowerBound(const View &u, const Solver& s)
{
    constrainUpperBound(u*-1,s);
}


/// remove all constraints,
/// moves the list of all reified implications out of the object
std::vector<ReifiedLinearConstraint> ConstraintStorage::removeConstraints()
{
    auto ret = std::move(linearImpConstraints_);
    linearImpConstraints_.clear();
    lbChanges_.clear();
    ubChanges_.clear();
    for (auto i : toProcess_)
        ret[i].l.setFlag(false);
    toProcess_.clear();
    return ret;
}


/// propagate, but not until a fixpoint
bool LinearPropagator::propagateSingleStep()
{
    if (storage_.toProcess_.size())
    {
        auto& lc = storage_.linearImpConstraints_[storage_.toProcess_.back()];
        storage_.toProcess_.pop_back();
        lc.l.setFlag(false);
        if (s_.isTrue(lc.v))
        {
            if (!propagate_true(lc.l))
                return false;
        }
        if (s_.isUnknown(lc.v))
        {
            if (!propagate_impl(lc))
                return false;
        }
    }
    return true;
}


/// return false if a domain gets empty
bool LinearPropagator::propagate()
{
    //process one of the list (others including itself maybe added), itself has to be removed beforehand
    while (storage_.toProcess_.size())
    {
        if (!propagateSingleStep())
            return false;
    }
    return true;
}



/// propagate, but not until a fixpoint
/// returns a set of new clauses
std::vector<LinearLiteralPropagator::LinearConstraintClause> LinearLiteralPropagator::propagateSingleStep()
{
    if (storage_.toProcess_.size())
    {
        auto& lc = storage_.linearImpConstraints_[storage_.toProcess_.back()];
        storage_.toProcess_.pop_back();
        lc.l.setFlag(false);
        if (s_.isTrue(lc.v))
            return propagate_true(lc);
        else
            if (s_.isUnknown(lc.v))
                return propagate_impl(lc);
    }
    return {};
}


std::pair<int64,int64> LinearPropagator::computeMinMax(const LinearConstraint& l)
{
    auto views = l.getViews();
    std::pair<int64,int64> minmax(0,0);
    for (auto i : views)
    {
        auto r = vs_.getCurrentRestrictor(i);
        minmax.first += r.lower();
        minmax.second += r.upper();
    }
    return minmax;
}


std::pair<int64,int64> LinearLiteralPropagator::computeMinMax(const LinearConstraint& l, LinearConstraintClause::itervec& clause)
{
    auto views = l.getViews();
    std::pair<int64,int64> minmax(0,0);

    for (auto i : views)
    {
        auto r = vs_.getVariableStorage().getCurrentRestrictor(i);
        assert(!r.isEmpty());
        minmax.first += r.lower();
        minmax.second += r.upper();
        clause.emplace_back(r.begin());
    }
    return minmax;
}


bool LinearPropagator::propagate_true(const LinearConstraint& l)
{
    assert(l.getRelation()==LinearConstraint::Relation::LE);
    auto minmax = computeMinMax(l);
    if (minmax.second <= l.getRhs())
        return true;

    for (auto i : l.getViews())
    {
        auto r = vs_.getCurrentRestrictor(i);
        auto wholeRange = vs_.getRestrictor(i); 
        std::pair<int64,int64> mm;
        mm.first = minmax.first - r.lower();
        mm.second = minmax.second - r.upper();

            int64 up = l.getRhs() - mm.first;
            if (up < r.lower())
            {
                //std::cout << "Constrain Variable " << i.first << " with new upper bound " << up << std::endl;
                return false;
            }
            if (up < r.upper())
            {
                //std::cout << "Constrain Variable " << i.first << " with new upper bound " << up << std::endl;
                //auto newUpper = std::lower_bound(wholeRange.begin(), wholeRange.end(), up);
                auto newUpper = std::upper_bound(wholeRange.begin(), wholeRange.end(), up);
                if (!constrainUpperBound(newUpper)) // +1 is needed, as it is in iterator pointing after the element
                    return false;
                minmax.first = mm.first + r.lower();
                minmax.second = mm.second + *(newUpper-1);
            }
    }
    return true;
}

std::vector<LinearLiteralPropagator::LinearConstraintClause> LinearLiteralPropagator::propagate_true(const ReifiedLinearConstraint& rl)
{
    const LinearConstraint& l = rl.l;
    assert(l.getRelation()==LinearConstraint::Relation::LE);
    std::vector<LinearConstraintClause> ret;
    LinearConstraintClause::itervec clause;
    auto minmax = computeMinMax(l, clause);
    if (minmax.second <= l.getRhs())
        return ret;

    //std::cout << "trying to propagate " << l << std::endl;
    auto views = l.getViews();
    for (std::size_t index=0; index < views.size(); ++index)
    {
        auto& i = views[index];
        auto wholeRange = vs_.getVariableStorage().getRestrictor(i);
        assert(wholeRange.size()>0);
        auto r = vs_.getVariableStorage().getCurrentRestrictor(i);
        std::pair<int64,int64> mm;
        mm.first = minmax.first - r.lower();
        mm.second = minmax.second - r.upper();
        
        //Literal prop = s_.falseLit();
        bool prop = false;
        Restrictor::ViewIterator propIt(wholeRange.begin());
        bool conflict = false;

        int64 up = l.getRhs() - mm.first;
        
        
        //// check if this returns the correct literals,
        /// see translatorClauseChecker::add, there is a positive and a negative add
        if (up < wholeRange.lower()) // derive false
        {
            //std::cout << "Constrain View " << i.v << "*" << i.a << "+" << i.c << " with new upper bound " << up << std::endl;
            propIt=wholeRange.begin();//can be removed
            conflict = true;
        }
        else
            if (up < r.upper())
            {
                //std::cout << "Constrain Variable " << i.v << "*" << i.a << "+" << i.c << " with new upper bound " << up << std::endl;
                auto newUpper = std::upper_bound(wholeRange.begin(), wholeRange.end(), up, [](int64 val, int64 it){ return it > val; });
                //assert(newUpper != r.end()); /// should never be able to happen, as up < r.upper().first, so there is something which is smaller, this means we do not need r ?
                if (newUpper==wholeRange.begin())
                {
                    propIt = newUpper;
                    conflict=true;
                }
                else
                {
                    propIt = newUpper;
                    --newUpper;
                    prop = true;
                    //prop = vs_.getVariableCreator().getLiteral(newUpper);
                    conflict = !constrainUpperBound((newUpper+1)); // +1 is needed, as it is an iterator pointing after the element
                    minmax.first = mm.first + r.lower();
                    minmax.second =  mm.second + *newUpper;
                    //minmax = mm + std::minmax(i.second*(int64)r.lower(),i.second*(int64)((*newUpper)));
                }
            }

        if (prop || conflict)
        {
            LinearConstraintClause c(rl);
            LinearConstraintClause::itervec aux(clause);
            aux[index] = propIt;
            c.setClause(std::move(aux),index);
            c.setConflict(conflict);
            ret.emplace_back(c);
        }
        if (conflict)
            break;
    }
    //When i changed a bound, the reason for the next ones can change, right ? No! only the upper/lower bound is changes, the other bound is used for reason
    return ret;
}




bool LinearPropagator::propagate_impl(ReifiedLinearConstraint &rl)
{
    const LinearConstraint& l = rl.l;
    assert(l.getRelation()==LinearConstraint::Relation::LE);
    auto minmax = computeMinMax(l);

    if (minmax.first>l.getRhs())
        return s_.createClause(LitVec{~rl.v});

    return true;
}


std::vector<LinearLiteralPropagator::LinearConstraintClause> LinearLiteralPropagator::propagate_impl(ReifiedLinearConstraint& rl)
{
    std::vector<LinearLiteralPropagator::LinearConstraintClause> ret;
    const LinearConstraint& l = rl.l;
    assert(l.getRelation()==LinearConstraint::Relation::LE);

    LinearConstraintClause::itervec clause;
    auto minmax = computeMinMax(l, clause);
    if (minmax.first>l.getRhs())
    {
        //clause.emplace_back(~rl.v);
        LinearConstraintClause c(rl);
        c.setClause(clause);
        ret.emplace_back(std::move(c));
    }
    return ret;
}


/// return false if the domain is empty
bool LinearPropagator::constrainUpperBound(const ViewIterator &u)
{
    storage_.constrainUpperBound(u.view(),s_);
    return vs_.constrainUpperBound(u);
}


/// return false if the domain is empty
bool LinearLiteralPropagator::constrainUpperBound(const ViewIterator &u)
{
    storage_.constrainUpperBound(u.view(),s_);
    return vs_.getVariableStorage().constrainUpperBound(u);
}

/// return false if the domain is empty
bool LinearLiteralPropagator::constrainLowerBound(const ViewIterator &l)
{   
    storage_.constrainLowerBound(l.view(),s_);
    return vs_.getVariableStorage().constrainLowerBound(l);
}



void LinearLiteralPropagator::queueConstraint(std::size_t id)
{
    assert(id < storage_.linearImpConstraints_.size());
    if (!storage_.linearImpConstraints_[id].l.getFlag())
    {
        storage_.toProcess_.emplace_back(id);
        storage_.linearImpConstraints_[id].l.setFlag(true);
    }

}


}
