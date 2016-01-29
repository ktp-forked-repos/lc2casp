#include "theoryparser.h"
#include "clasp/logic_program.h"



namespace clingcon
{

using namespace order;

    bool TheoryParser::getConstraintType(Potassco::Id_t id, CType& t)
    {
        if (termId2constraint_.find(id) == termId2constraint_.end())
        {
            std::string s = toString(td_.getTerm(id));

            if (s=="sum")
                termId2constraint_[id]=SUM;
            else
                if (s=="dom")
                    termId2constraint_[id]=DOM;
                else
                    if (s=="show")
                        termId2constraint_[id]=SHOW;
                    else
                        if (s=="distinct")
                            termId2constraint_[id]=DISTINCT;
                        else
                            if (s=="minimize")
                                termId2constraint_[id]=MINIMIZE;
                            else // last
                                return false;
        }
        t = termId2constraint_[id];
        return true;
    }

    bool TheoryParser::getGuard(Potassco::Id_t id, order::LinearConstraint::Relation& rel)
    {
        if (termId2guard_.find(id) == termId2guard_.end())
        {
            std::string s = toString(td_.getTerm(id));

            if (s=="=")
                termId2guard_[id]=order::LinearConstraint::Relation::EQ;
            else
                if (s=="<=")
                    termId2guard_[id]=order::LinearConstraint::Relation::LE;
                else
                    if (s==">=")
                        termId2guard_[id]=order::LinearConstraint::Relation::GE;
                    else
                        if (s=="<")
                            termId2guard_[id]=order::LinearConstraint::Relation::LT;
                        else
                            if (s==">")
                                termId2guard_[id]=order::LinearConstraint::Relation::GT;
                            else
                                if (s=="!=")
                                    termId2guard_[id]=order::LinearConstraint::Relation::NE;
            else // last
                return false;
        }
        rel = termId2guard_[id];
        return true;
    }


    std::stringstream& TheoryParser::toString(std::stringstream& ss, const Potassco::TheoryData::Term& t)
    {
        if (t.type()==Potassco::Theory_t::Number)
            ss << t.number();
        else
            if (t.type()==Potassco::Theory_t::Symbol)
                ss << t.symbol();
            else
                if (t.type()==Potassco::Theory_t::Compound)
                {
                    if (t.isFunction())
                    {
                        Potassco::Id_t f = t.function();
                        //TODO: what about internal functions like + * etc...
                        toString(ss, td_.getTerm(f));
                        ss << "(";
                    }

                    for (auto i = t.begin(); i != t.end(); ++i)
                    {
                        toString(ss, td_.getTerm(*i));
                        if (i != t.end()-1)
                            ss << ",";
                    }

                    if (t.isFunction())
                    {
                        ss << ")";
                    }
                }

        return ss;
    }
    
    bool TheoryParser::isVariable(Potassco::Id_t id)
    {
        auto& a = td_.getTerm(id);
        if ((a.type()==Potassco::Theory_t::Compound || a.type()==Potassco::Theory_t::Symbol) && check(id))
            return true;
        return false;
    }

    bool TheoryParser::isNumber(Potassco::Id_t id)
    {
        auto& a = td_.getTerm(id);

        if (a.type()==Potassco::Theory_t::Number)
            return true;
        if (a.type()==Potassco::Theory_t::Compound)
        {
            if (a.isFunction())
            {
                if (toString(td_.getTerm(a.function()))=="+" && a.size()==1)
                {
                    //unary plus
                    return isNumber(*a.begin());
                }
                if (toString(td_.getTerm(a.function()))=="-" && a.size()==1)
                {
                    //unary minus
                    return isNumber(*a.begin());
                }
                if (toString(td_.getTerm(a.function()))=="+" && a.size()==2)
                {
                    //binary plus
                    return isNumber(*a.begin()) && isNumber(*(a.begin()+1));
                }
                if (toString(td_.getTerm(a.function()))=="-" && a.size()==2)
                {
                    //binary minus
                    return isNumber(*a.begin()) && isNumber(*(a.begin()+1));
                }
                if (toString(td_.getTerm(a.function()))=="*" && a.size()==2)
                {
                    //binary times
                    return isNumber(*a.begin()) && isNumber(*(a.begin()+1));
                }
            }
        }
        return false;
    }

    int TheoryParser::getNumber(Potassco::Id_t id)
    {
        assert(isNumber(id));
        auto& a = td_.getTerm(id);

        if (a.type()==Potassco::Theory_t::Number)
            return a.number();
        if (a.type()==Potassco::Theory_t::Compound)
        {
            if (a.isFunction())
            {
                if (toString(td_.getTerm(a.function()))=="+" && a.size()==1)
                {
                    //unary plus
                    return getNumber(*a.begin());
                }
                if (toString(td_.getTerm(a.function()))=="-" && a.size()==1)
                {
                    //unary minus
                    return -getNumber(*a.begin());
                }
                if (toString(td_.getTerm(a.function()))=="+" && a.size()==2)
                {
                    //binary plus
                    return getNumber(*a.begin()) + getNumber(*(a.begin()+1));
                }
                if (toString(td_.getTerm(a.function()))=="-" && a.size()==2)
                {
                    //binary minus
                    return getNumber(*a.begin()) - getNumber(*(a.begin()+1));
                }
                if (toString(td_.getTerm(a.function()))=="*" && a.size()==2)
                {
                    //binary times
                    return getNumber(*a.begin()) * getNumber(*(a.begin()+1));
                }
            }
        }
        return false;
    }


    ///
    /// \brief getView
    /// \param id
    /// \return
    /// either number -> create new var
    /// string -> create var
    /// tuple -> not allowed
    /// function
    ///        named function -> do not eval but check and create var
    ///        operator + unary ->getView of Rest
    ///        operator - unary ->getView of Rest
    ///        operator + binary -> one is number, other getView or both is number -> create Var
    ///        operator - binary -> one is number, other getView or both is number -> create Var
    ///        operator * binary -> one is number, other getView or both is number -> create Var
    ///
    bool TheoryParser::getView(Potassco::Id_t id, order::View& v)
    {
        /// already exists a view
        if (termId2View_.size() > id && termId2View_[id]!=order::View(order::InvalidVar))
        {
            v = termId2View_[id];
            return true;
        }


        auto& a = td_.getTerm(id);

        if (a.type()==Potassco::Theory_t::Number)
        {
            v = createVar(id, a.number());
        }
        else
        if (a.type()==Potassco::Theory_t::Compound)
        {
            if (a.isFunction())
            {
                std::string fname(toString(td_.getTerm(a.function())));
                if (!isalpha(fname[0]))
                {
                    if (toString(td_.getTerm(a.function()))=="+" && a.size()==1)
                    {
                        return getView(*a.begin(),v);
                    }
                    if (toString(td_.getTerm(a.function()))=="-" && a.size()==1)
                    {
                        bool b = getView(*a.begin(),v);
                        v = v*-1;
                        return b;
                    }
                    if (toString(td_.getTerm(a.function()))=="+" && a.size()==2)
                    {
                        //binary plus
                        if (isNumber(*a.begin()) && isNumber(*(a.begin()+1)))
                        {
                            v = createVar(id, getNumber(*a.begin()) + getNumber(*(a.begin()+1)));
                        }
                        else
                        {
                            if (isNumber(*a.begin()))
                            {
                                bool b = getView(*(a.begin()+1),v);
                                v.c += getNumber(*a.begin());
                                return b;
                            }
                            else
                                if (isNumber(*(a.begin()+1)))
                                {
                                    bool b = getView(*(a.begin()),v);
                                    v.c += getNumber(*(a.begin()+1));
                                    return b;
                                }
                                else
                                    return false;
                        }
                    }
                    if (toString(td_.getTerm(a.function()))=="-" && a.size()==2)
                    {
                        //binary minus
                        if (isNumber(*a.begin()) && isNumber(*(a.begin()+1)))
                        {
                            v = createVar(id, getNumber(*a.begin()) - getNumber(*(a.begin()+1)));
                        }
                        else
                        {
                            if (isNumber(*a.begin()))
                            {
                                bool b = getView(*(a.begin()+1),v);
                                v = v * -1;
                                v.c += getNumber(*a.begin());

                                return b;
                            }
                            else
                                if (isNumber(*(a.begin()+1)))
                                {
                                    bool b = getView(*(a.begin()),v);
                                    v.c -= getNumber(*(a.begin()+1));
                                    return b;
                                }
                                else
                                    return false;
                        }

                    }
                    if (toString(td_.getTerm(a.function()))=="*" && a.size()==2)
                    {
                        //binary times
                        if (isNumber(*a.begin()) && isNumber(*(a.begin()+1)))
                        {
                            v = createVar(id, getNumber(*a.begin()) * getNumber(*(a.begin()+1)));
                        }
                        else
                        {
                            if (isNumber(*a.begin()))
                            {
                                bool b = getView(*(a.begin()+1),v);
                                v = v * getNumber(*a.begin());
                                return b;
                            }
                            else
                                if (isNumber(*(a.begin()+1)))
                                {
                                    bool b = getView(*(a.begin()),v);
                                    v = v * getNumber(*(a.begin()+1));
                                    return b;
                                }
                                else
                                    return false;
                        }
                    }
                }
                else
                {
                    for (auto i = a.begin(); i != a.end(); ++i)
                        if(!check(*i))
                            return false;
                    v = createVar(id);
                }
            }
            else
                return false;
        }
        else
        if (a.type()==Potassco::Theory_t::Symbol)
        {
            v = createVar(id);
        }

        return true;
    }

    order::View TheoryParser::createVar(Potassco::Id_t id)
    {
        termId2View_.resize(std::max((unsigned int)(termId2View_.size()), (unsigned int)(id+1)), order::View(order::InvalidVar));
        assert(termId2View_[id]==order::View(order::InvalidVar));
        order::View v = n_.createView();
        termId2View_[id]=v;
        //string2View_[s]=v;
        return v;
    }

    order::View TheoryParser::createVar(Potassco::Id_t id, int32 val)
    {
        termId2View_.resize(std::max((unsigned int)(termId2View_.size()), (unsigned int)(id+1)), order::View(order::InvalidVar));
        assert(termId2View_[id]==order::InvalidVar);
        order::View v = n_.createView(order::Domain(val,val));
        termId2View_[id]=v;
        //string2Var_[s]=v;
        return v;
    }

    bool TheoryParser::check(Potassco::Id_t id)
    {
        auto& a = td_.getTerm(id);

        if (a.type()==Potassco::Theory_t::Number)
        {
            return true;
        }
        if (a.type()==Potassco::Theory_t::Symbol)
        {
            return true;
        }

        if (a.type()==Potassco::Theory_t::Compound)
        {
            if (a.isFunction())
            {
                std::string fname(toString(td_.getTerm(a.function())));
                if (!isalpha(fname[0]))
                    return false;
                for (auto i = a.begin(); i != a.end();++i)
                    if (!check(*i))
                        return false;
            }
            else // compound
            {
                for (auto i = a.begin(); i != a.end();++i)
                    if (!check(*i))
                        return false;
                return true;
            }
        }
        return false;
    }




bool TheoryParser::readConstraint(Potassco::TheoryData::atom_iterator& i)
{
    //Potassco::TheoryData::Term sum("sum");
    /// i has atom, guard, rhs, and begin/end to read next level
    //Clasp::Literal claspLit = lp->getLiteral((*i)->atom());
    Potassco::Id_t theoryTerm = (*i)->term();
    //const Potassco::TheoryData::Term& tt = td_.getTerm(theoryTerm);
    //std::cout << toString(tt) << std::endl;
    CType ct;
    if (!getConstraintType(theoryTerm, ct))
        return false;
    switch(ct)
    {

    case SUM:
    {
        order::LinearConstraint lc(order::LinearConstraint::Relation::EQ);
        for (auto elemId = (*i)->begin(); elemId != (*i)->end(); ++elemId)
        {
            auto& elem = td_.getElement(*elemId);
            if (elem.condition()!=0)
                error("Conditions on theory terms not yet supported");
            // check condition of element
            //elem->condition;
            assert(elem.size()>=1);
            /// everything more than 1 element is just for set semantics and is not used in the theory
            //for (auto single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
            auto single_elem = elem.begin();
            {
//                auto& a = td_.getTerm(*single_elem);
//                std::cout << toString(a) << " ";
//                std::cout << std::endl;
                if (isNumber(*single_elem))
                {
                    lc.addRhs(-getNumber(*single_elem));
                }
                else
                {
                    order::View v;
                    if (getView(*single_elem,v))
                        lc.add(v);
                    else
                        error("VariableView or integer expression expected",*single_elem);
                }
            }
        }

        if (!(*i)->guard())
            error("Guard expected");
        order::LinearConstraint::Relation rel;
        if (!getGuard(*(*i)->guard(),rel))
            error("Guard expected",*(*i)->guard());
        lc.setRelation(rel);


        if (!(*i)->rhs())
            error("Rhs VariableView expected");
        
        if (isNumber(*(*i)->rhs()))
        {
            lc.addRhs(getNumber(*(*i)->rhs()));            
        }
        else
        {
            order::View v;
            if (!getView(*(*i)->rhs(), v))
                error("Rhs VariableView or number expected",*(*i)->rhs());
            //if (v.reversed())
            //    lc.invert();
            v = v * -1;
            lc.add(v);
        }

        order::Literal lit = toOrderFormat(lp_->getLiteral((*i)->atom()));
        n_.addConstraint(order::ReifiedLinearConstraint(std::move(lc),lit));
        break;
    }

    case DOM:
    {
        // ((l..u) or x) = view
        order::Domain d(1,0);
        for (auto elemId = (*i)->begin(); elemId != (*i)->end(); ++elemId)
        {
            auto& elem = td_.getElement(*elemId);
            // check condition of element
            if (elem.condition()!=0)
                error("Conditions on theory terms not yet supported");
            //for (auto single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
            auto single_elem = elem.begin();
            {
//                auto& a = td_.getTerm(*single_elem);

//                std::cout << toString(a) << " ";
//                std::cout << std::endl;
                if (isNumber(*single_elem))
                {
                    d.unify(getNumber(*single_elem),getNumber(*single_elem));
                    continue;
                }

                auto& op = td_.getTerm(*single_elem);

                if (!op.isFunction())
                    error("l..u expected");
                if (toString(td_.getTerm(op.function()))==".." && op.size()==2)
                {
                    if (isNumber(*op.begin()))
                    {
                        if (isNumber(*(op.begin()+1)))
                            d.unify(getNumber(*op.begin()),getNumber(*(op.begin()+1)));
                        else
                            error("Domain bound expected",*(op.begin()+1));
                    }
                    else
                        error("Domain bound expected",*op.begin());

                }
                else
                    error("l..u expected",op.function());
            }
        }

        if (!(*i)->guard())
            error("= expected");
        order::LinearConstraint::Relation guard;
        if (!getGuard(*(*i)->guard(),guard) || guard!=order::LinearConstraint::Relation::EQ)
            error("= expected",*(*i)->guard());

        if (!(*i)->rhs())
            error("Rhs VariableView expected");
        order::View v;
        if (!getView(*(*i)->rhs(), v))
            error("Rhs VariableView expected",*(*i)->rhs());

        order::Literal lit = toOrderFormat(lp_->getLiteral((*i)->atom()));
        n_.addConstraint(order::ReifiedDomainConstraint(v,std::move(d),lit));
        break;
    }

    case DISTINCT:
    {
        // ((l..u) or x) = view
        std::vector<order::View> views;
        for (auto elemId = (*i)->begin(); elemId != (*i)->end(); ++elemId)
        {
            auto& elem = td_.getElement(*elemId);
            // check condition of element
            if (elem.condition()!=0)
                error("Conditions on theory terms not yet supported");
            //for (auto single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
            auto single_elem = elem.begin();
            {
//                auto& a = td_.getTerm(*single_elem);

//                std::cout << toString(a) << " ";
//                std::cout << std::endl;
                order::View v;
                if (getView(*single_elem,v))
                    views.emplace_back(v);
                else
                    error("VariableView expected",*single_elem);
            }
        }

        if ((*i)->guard())
            error("Did not expect a guard in distinct",*(*i)->guard());

        if ((*i)->rhs())
            error("Did not expect a rhs in distinct",*(*i)->rhs());

        order::Literal lit = toOrderFormat(lp_->getLiteral((*i)->atom()));
        n_.addConstraint(order::ReifiedAllDistinct(std::move(views),lit));
        break;
    }
        
    case SHOW:
    {
        for (auto elemId = (*i)->begin(); elemId != (*i)->end(); ++elemId)
        {
            auto& elem = td_.getElement(*elemId);
            // check condition of element
            //for (auto single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
            auto single_elem = elem.begin();
            {
//                auto& a = td_.getTerm(*single_elem);

//                std::cout << toString(a) << " ";
//                std::cout << std::endl;
                
                auto& op = td_.getTerm(*single_elem);
                
                if (op.isFunction())
                {
                    auto& term = td_.getTerm(op.function());
                    if (toString(term)=="/")
                    {
                        if (op.size()==2 && isVariable(*op.begin()) && td_.getTerm(*(op.begin()+1)).type()==Potassco::Theory_t::Number)
                        {
                            shownPred_.emplace_back(*single_elem,lp_->getLiteral(elem.condition()));
                            continue;
                        }
                        else
                            error("Variable or pred/n show expression expected",*single_elem);
                    }
                }
                
                order::View v;
                if (getView(*single_elem,v) && v.a==1 && v.c==0)
                {
                    shown_.resize(std::max((unsigned int)(v.v)+1,(unsigned int)(shown_.size())),std::make_pair(order::InvalidVar,Clasp::Literal(0,false)));
                    shown_[v.v]=std::make_pair(*single_elem,lp_->getLiteral(elem.condition()));
                }
                else
                    error("Variable or pred/n show expression expected",*single_elem);
            }
        }

        if ((*i)->guard())
            error("Did not expect a guard in show",*(*i)->guard());

        if ((*i)->rhs())
            error("Did not expect a rhs in show",*(*i)->rhs());

        break;
    }


    case MINIMIZE:
    {
        for (auto elemId = (*i)->begin(); elemId != (*i)->end(); ++elemId)
        {
            auto& elem = td_.getElement(*elemId);
            // check condition of element
            if (elem.condition()!=0)
                error("Conditions on theory terms not yet supported");
            //for (auto single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
            View v;
            unsigned int level = 0;
            auto single_elem = elem.begin();


            auto& op = td_.getTerm(*single_elem);

            bool done = false;

            if (op.isFunction())
            {
                auto& term = td_.getTerm(op.function());
                if (toString(term)=="@")
                {
                    if (op.size()==2)
                    {
                        if (getView(*op.begin(),v) && td_.getTerm(*(op.begin()+1)).type()==Potassco::Theory_t::Number)
                        {
                            level = getNumber((*(op.begin()+1)));
                            done = true;
                        }
                    }
                }
            }

            if (getView(*single_elem,v) )
            {
                level=0;
                done = true;
            }

            if (!done)
                error("VariableView or var@level expression expected",*single_elem);

            mytuple tup;
            for (single_elem = elem.begin(); single_elem != elem.end(); ++single_elem)
                tup.emplace_back(*single_elem);
            minimize_.resize(std::max((unsigned int)(minimize_.size()),level+1));
            auto it = minimize_[level].find(tup);
            if (it != minimize_[level].end())
            {
                /// already found, has same tuple identifier
                std::string s1;
                for (auto i = it->first.begin(); i != it->first.end(); ++i)
                {
                    s1 += toString(td_.getTerm(*i));
                    if (i != it->first.end()-1)
                        s1 += ",";
                }
                
                error("Having similar tuples in minimize statement is currently not supported, having " + s1);
            }
            minimize_[level][tup]=v;

        }

        if ((*i)->guard())
            error("Did not expect a guard in minimize",*(*i)->guard());

        if ((*i)->rhs())
            error("Did not expect a rhs in minimize",*(*i)->rhs());

        break;
    }
  

    };
    return true;
}


std::vector<std::pair<std::string,Clasp::Literal>> TheoryParser::postProcess()
{
    std::vector<std::pair<std::string,Clasp::Literal>> ret;
    for (auto i : shownPred_)
    {
        for (uint32 tid = 0; tid != termId2View_.size(); ++tid)
        {
            //std::cout << toString(td_.getTerm(tid)) << std::endl;
            order::View v(termId2View_[tid]);
            if (v.v!=order::InvalidVar)
            {
                auto& predTerm = td_.getTerm(i.first);
                unsigned int arity = td_.getTerm(*(predTerm.begin()+1)).number();
                Potassco::Id_t function = *predTerm.begin();
                auto& term = td_.getTerm(tid);
                if ( (term.isFunction() && term.size()==arity && term.function() == function) ||
                     (arity==0 && term.type()==Potassco::Theory_t::Symbol && tid == function) )
                {
                    shown_.resize(std::max((unsigned int)(v.v)+1,(unsigned int)(shown_.size())),std::make_pair(MAXID,Clasp::Literal(0,false)));
                    if (shown_[v.v].first == MAXID) // not already existing
                        shown_[v.v] = std::make_pair(tid,i.second);                 
                    else
                    {
                        order::View newV = n_.createView();
                        LinearConstraint lc(LinearConstraint::Relation::EQ);
                        lc.add(newV*-1);
                        lc.add(v.v);
                        n_.addConstraint(ReifiedLinearConstraint(std::move(lc),trueLit_));
                        shown_.resize(std::max((unsigned int)(newV.v)+1,(unsigned int)(shown_.size())),std::make_pair(MAXID,Clasp::Literal(0,false)));
                        shown_[newV.v] = std::make_pair(tid,i.second);
                        
                    }
                    
                }
            }
        }
    }
    ret.resize(n_.getVariableCreator().numVariables(),std::make_pair("",Clasp::Literal(0,false)));
    
    for (unsigned int i = 0; i < shown_.size(); ++i)
    {
        if (shown_[i].first!=MAXID)
            ret[i] = std::make_pair(toString(td_.getTerm(shown_[i].first)),shown_[i].second);
        //else
        //    ret[i] = std::make_pair("",Clasp::Literal(0,false));
    }
    return ret;    
}


const std::vector<TheoryParser::tuple2View>& TheoryParser::minimize() const
{
    return minimize_;
}


void TheoryParser::error(const std::string& s)
{
   throw std::runtime_error(s + ", got nothing");
}

void TheoryParser::error(const std::string& s, Potassco::Id_t id)
{
    throw std::runtime_error(s + ", got " + toString(td_.getTerm(id)));
}

}
