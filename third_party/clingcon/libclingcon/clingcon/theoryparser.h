#pragma once
#include "solver.h"
#include "order/normalizer.h"
#include "clingconorderpropagator.h"
#include "potassco/theory_data.h"
#include <unordered_map>
#include <sstream>



namespace clingcon
{




class TheoryParser
{
public:
    enum CType {SUM, DOM, DISTINCT, SHOW, MINIMIZE};
    using mytuple = std::vector<Potassco::Id_t>;   /// a tuple identifier
    using tuple2View = std::map<mytuple, order::View>; // could be unordered

    TheoryParser(order::Normalizer& n, Potassco::TheoryData& td, Clasp::Asp::LogicProgram* lp, order::Literal trueLit) :
        n_(n), td_(td), lp_(lp), trueLit_(trueLit)
    {}

    /// returns false, if not a constraint of this theory
    /// throws string with error if error occurs
    bool readConstraint(Potassco::TheoryData::atom_iterator& i);
    /// turn show predicates to variables
    std::vector<std::pair<std::string, Clasp::Literal> > postProcess();
    const std::vector<tuple2View>& minimize() const;

private:

    void error(const std::string& s);
    void error(const std::string& s, Potassco::Id_t id);


    bool getConstraintType(Potassco::Id_t id, CType& t);
    bool getGuard(Potassco::Id_t id, order::LinearConstraint::Relation& rel);


    std::string toString(const Potassco::TheoryData::Term& t)
    {
        std::stringstream ss;
        return toString(ss,t).str();
    }

    std::stringstream& toString(std::stringstream& ss, const Potassco::TheoryData::Term& t);

    bool isNumber(Potassco::Id_t id);

    int getNumber(Potassco::Id_t id);

    ///
    /// \brief getView
    /// \param id
    /// \return
    /// either number -> create new var
    /// string -> create var
    /// tuple -> not allowed
    /// function
    ///        named function -> eval and create var
    ///        operator + unary ->getView of Rest
    ///        operator - unary ->getView of Rest
    ///        operator + binary -> one is number, other getView or both is number -> create Var
    ///        operator - binary -> one is number, other getView or both is number -> create Var
    ///        operator * binary -> one is number, other getView or both is number -> create Var
    ///
    bool getView(Potassco::Id_t id, order::View& v);

    order::View createVar(Potassco::Id_t id);

    order::View createVar(Potassco::Id_t id, int32 val);

    
private:
    
    bool check(Potassco::Id_t id);
    bool isVariable(Potassco::Id_t id);
    

    std::unordered_map<Potassco::Id_t, CType>  termId2constraint_;
    std::unordered_map<Potassco::Id_t, order::LinearConstraint::Relation>  termId2guard_;
    std::vector<std::pair<Potassco::Id_t,Clasp::Literal>> shown_; /// order::Variable to TermId + condition literal
    std::vector<std::pair<Potassco::Id_t,Clasp::Literal>> shownPred_; /// a list of p/3 predicates to be shown + condition literal
    std::vector<tuple2View> minimize_;                /// for each level
    
    std::vector<order::View>  termId2View_;
    order::Normalizer& n_;
    Potassco::TheoryData& td_;
    Clasp::Asp::LogicProgram* lp_;
    order::Literal trueLit_;
    const Potassco::Id_t MAXID = std::numeric_limits<Potassco::Id_t>::max();
};

}
