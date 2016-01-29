#pragma once
#include <algorithm>
#include <map>
#include <cmath>


#include "constraint.h"
#include "storage.h"
#include "solver.h"



namespace order
{

class LinearPropagator;
class LinearLiteralPropagator;

/// weird interface
class ConstraintStorage
{
private:
    friend LinearPropagator;
    friend LinearLiteralPropagator;
    /// add an implication constraint l.v -> l.l
    void addImp(ReifiedLinearConstraint&& l);
    /// add several implication constraints l.v -> l.l
    void addImp(std::vector<ReifiedLinearConstraint>&& l);
    /// remove all constraints,
    /// moves the list of all reified implications out of the object
    std::vector<ReifiedLinearConstraint> removeConstraints();
    void addLevel() { assert(toProcess_.empty()); }
    void removeLevel();   
    /// true if we are at a fixpoint
    bool atFixPoint() { return toProcess_.empty(); }
    /// return false if the domain is empty
    void constrainUpperBound(const View &view, const Solver &s);
    void constrainLowerBound(const View &view, const Solver &s);
private:
    /// a list of all constraints
    std::vector<ReifiedLinearConstraint> linearImpConstraints_;
    std::vector<std::size_t> toProcess_; // a list of constraints that need to be processed
    // a list of constraints that have to be processed if the bound of the variable changes and the constraint is TRUE (opposite case for false, and dont care for unknown)
    std::vector<std::vector<std::size_t> > lbChanges_;
    std::vector<std::vector<std::size_t> > ubChanges_;
};


class LinearPropagator
{
public:
    LinearPropagator(CreatingSolver& s, const VariableCreator& vc) :
        s_(s), vs_(vc,s.trueLit()) {}

    CreatingSolver& getSolver() { return s_; }


    void addImp(ReifiedLinearConstraint&& l) { storage_.addImp(std::move(l)); }
    /// add several implication constraints l.v -> l.l
    void addImp(std::vector<ReifiedLinearConstraint>&& l) {storage_.addImp(std::move(l)); }
    /// remove all constraints,
    /// moves the list of all reified implications out of the object
    std::vector<ReifiedLinearConstraint> removeConstraints() { return storage_.removeConstraints(); }
    void addLevel() { storage_.addLevel(); vs_.addLevel(); }
    void removeLevel() {storage_.removeLevel(); vs_.removeLevel();}
  
    /// propagate all added constraints to a fixpoint
    /// return false if a domain gets empty
    /// propagates some literals using a creatingSolver
    bool propagate();

    const VariableStorage& getVariableStorage() const { return vs_; }
private:
    
    /// return false if the domain is empty
    /// iterator u points to the first element not in the domain
    bool constrainUpperBound(const ViewIterator &u);
    /// propagate only a bit (not fixpoint),
    /// return false if a domain gets empty
    bool propagateSingleStep();

    /// computes the min/maximum of the lhs
    std::pair<int64,int64> computeMinMax(const LinearConstraint& l);

    /// propagates directly, thinks the constraint is true
    /// can result in an empty domain, if so it returns false
    /// can only handle LE constraints
    /// DOES NOT GUARANTEE A FIXPOINT (just not sure)(but reshedules if not)
    /// Remarks: uses double for floor/ceil -> to compatible with 64bit integers
    bool propagate_true(const LinearConstraint& l);

    /// propagates the truthvalue of the constraint if it can be directly inferred
    /// can only handle LE constraints
    bool propagate_impl(ReifiedLinearConstraint &rl);
private:

    ConstraintStorage storage_;
    CreatingSolver& s_;
    VariableStorage vs_;
};



class LinearLiteralPropagator
{
public:
    class LinearConstraintClause
    {
    public:
        LinearConstraintClause(const ReifiedLinearConstraint& l) : constraint_(l), conflict_(false), newLit_(false) {}
        using iter = Restrictor::ViewIterator;
        using itervec = std::vector<iter>;
        LitVec getClause(IncrementalSolver &s, VolatileVariableStorage &vs, bool createOnConflict) const;

        /// pre getClause was called
        bool addedNewLiteral() const { return newLit_; }

        /// pre getClause was called
        /// returns an iterator with a simple variable view, to the position of the added literal
        iter getAddedIterator(const VariableStorage& vs);

        /// vec represents the restrictions according to the constraint variables,
        /// conclusion is the index of the variable that shall be assigned
        void setClause(itervec&& vec, unsigned int conclusion) { its_ = vec; conclusion_= conclusion; newLit_=false; }
        /// is no conclusion is given, the constraint will be assigned
        void setClause(itervec& vec) { its_ = vec; conclusion_= vec.size(); newLit_=false; }

        void setConflict(bool b) { conflict_ = b; }

    private:
        const ReifiedLinearConstraint constraint_;
        itervec its_;
        unsigned int conclusion_;
        bool conflict_;
        mutable bool newLit_;

    };

public:
    LinearLiteralPropagator(IncrementalSolver& s, const VariableCreator& vs) :
        s_(s), vs_(vs, s.trueLit()) {}

    IncrementalSolver& getSolver() { return s_; }
    VolatileVariableStorage& getVVS() { return vs_; } 


    void addImp(ReifiedLinearConstraint&& l) { storage_.addImp(std::move(l)); }
    /// add several implication constraints l.v -> l.l
    void addImp(std::vector<ReifiedLinearConstraint>&& l) { storage_.addImp(std::move(l)); }
    /// remove all constraints,
    /// moves the list of all reified implications out of the object
    std::vector<ReifiedLinearConstraint> removeConstraints() { return storage_.removeConstraints(); }
    void addLevel() { storage_.addLevel(); vs_.getVariableStorage().addLevel();}
    void removeLevel() {storage_.removeLevel(); vs_.getVariableStorage().removeLevel();}

    /// true if we are at a fixpoint, propagateSingleStep does not do anything anymore
    bool atFixPoint() { return storage_.atFixPoint(); }
    /// the same but generates a set of reasons
    std::vector<LinearConstraintClause> propagateSingleStep();
    /// propagate all added constraints to a fixpoint
    /// return false if a domain gets empty
    bool propagate();
    
    /// return false if the domain is empty
    /// iterator points to element after the bound
    bool constrainUpperBound(const ViewIterator& u);
    /// return false if the domain is empty
    bool constrainLowerBound(const ViewIterator& l);
    
    /// add a constraint (identified by id) to the propagation queue
    void queueConstraint(std::size_t id);
    
    //VariableStorage& getVariableStorage() { return vs_; }

private:
    
    /// computes the min/maximum of the lhs
    std::pair<int64,int64> computeMinMax(const LinearConstraint& l, LinearConstraintClause::itervec &clause);

    /// propagates directly, thinks the constraint is true
    /// can result in an empty domain, if so it returns false
    /// can only handle LE constraints
    /// DOES NOT GUARANTEE A FIXPOINT (just not sure)(but reshedules if not)
    /// Remarks: uses double for floor/ceil -> to compatible with 64bit integers
    std::vector<LinearConstraintClause> propagate_true(const ReifiedLinearConstraint& l);

    /// propagates the truthvalue of the constraint if it can be directly inferred
    /// can only handle LE constraints
    std::vector<LinearConstraintClause> propagate_impl(ReifiedLinearConstraint &rl);
    


private:
    ConstraintStorage storage_;
    IncrementalSolver& s_;
    VolatileVariableStorage vs_;
};


}
