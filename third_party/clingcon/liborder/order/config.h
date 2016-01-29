#pragma once
#include "types.h"


namespace order
{

struct Config
{
public:
    Config(bool redundantClauseCheck,
           unsigned int domSize, bool break_symmetries,
           std::pair<unsigned int,unsigned int> splitsize_maxClauseSize,
           bool pidgeon, bool permutation, bool disjoint2distinct,
           bool alldistinctCard, bool explicitBinaryOrderClauses,
           bool learnClauses, unsigned int dlprop,
           bool createOnConflict, int64 translateConstraints, int64 minLitsPerVar,
           bool equalityProcessing) :
        redundantClauseCheck(redundantClauseCheck),
        domSize(domSize), break_symmetries(break_symmetries),
        splitsize_maxClauseSize(splitsize_maxClauseSize),
        pidgeon(pidgeon), permutation(permutation), disjoint2distinct(disjoint2distinct),
        alldistinctCard(alldistinctCard), explicitBinaryOrderClausesIfPossible(explicitBinaryOrderClauses),
        learnClauses(learnClauses), dlprop(dlprop),
        createOnConflict(createOnConflict), translateConstraints(translateConstraints),
        minLitsPerVar(minLitsPerVar), equalityProcessing(equalityProcessing)
    {
        this->splitsize_maxClauseSize.first = std::max((unsigned int)(3),this->splitsize_maxClauseSize.first);
    }
    //Config() : hallsize(0), redundantClauseCheck(true), domSize(10000), break_symmetries(false), splitsize_maxClauseSize{3,1024}, pidgeon(true), permutation(true),
    //           disjoint2distinct(true), alldistinctCard(false), explicitBinaryOrderClauses(true) {}

    bool redundantClauseCheck; /// activate check for redundant clauses while translating linear constraints
    unsigned int domSize; /// the maximum number of chunks a domain can have when multiplied (if avoidable)
    bool break_symmetries; /// necessary to avoid double solutions, can be set to false when only computing 1 solution or using projection
                     /// on the visible variables
    std::pair<unsigned int,uint64> splitsize_maxClauseSize; /// constraints are splitted into this size, if the expected number of clauses is larger then maxClauseSize
                            /// "minimum should be 3!
    //unsigned int maxClauseSize; /// see splitsize
    bool pidgeon; /// apply pidgeon hole optimization
    bool permutation; /// apply permutation constraint to alldifferent cosntraints
    bool disjoint2distinct; /// try to convert a disjoint constraint to an alldistinct constraint
    bool alldistinctCard; /// translate alldistinct with cardinality constraints
    bool explicitBinaryOrderClausesIfPossible; /// have the order clauses explicit or in a propagator
    /// be careful with explicitBinaryOrderClauses as it is not compatible with bool translate which has to be implemented yet
    bool learnClauses; /// learn clauses while propagating, default true
    unsigned int dlprop; /// 0 = no difference logic propagator, 1 =dl prop comes before linear order prop, 2 =dl prop comes after
    bool createOnConflict; /// if lazyLiterals is true only, creates aux variables on conflicts, also they are not used
    int64 translateConstraints; // translate constraint if expected number of clauses is less than this number (-1 = all)
    int64 minLitsPerVar; // precreate at least this number of literals per variable (-1 = all)
    bool equalityProcessing; // enable equality processing
};

//for testing
static Config lazySolveConfig = Config(true,10000,false,{3,1024},true,true,true,false,true,true,2,true,1000,1000,true);
static Config nonlazySolveConfig = Config(true,10000,false,{3,1024},true,true,true,false,true,true,2,true,0,-1,true);
static Config lazyDiffSolveConfig = Config(true,10000,false,{3,1024},true,true,true,false,true,true,1,true,0,1000,true);
static Config translateConfig = Config(true,10000,false,{3,1024},true,true,true,false,true,true,0,true,-1,1000,true);
}
