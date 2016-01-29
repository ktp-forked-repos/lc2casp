/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *  Last modified:
 *     $Date: 2010-07-02 19:18:43 +1000 (Fri, 02 Jul 2010) $ by $Author: tack $
 *     $Revision: 11149 $
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "flatzinc.hh"
#include "registry.hh"

#include <vector>
#include <string>
using namespace std;

namespace FlatZinc {


void IntVar::printDefinition(FlatZincModel& m) const
{
    //std::cout << "intvar(i(" << id_ << ")," << std::endl;
    /*if (false)
    {
       std::cout << "intvar(i(" << id_ << "),";
       if (vs_->assigned)
           std::cout << vs_->i;
       else
       {
           assert(vs_->domain());
           AST::SetLit* p = vs_->domain.some();
           if (p->interval)
           {
               std::cout << p->min << ".." << p->max;
           }
           else
           {
               for (unsigned int i = 0; i < p->s.size()-1; ++i)
                   std::cout << p->s[i] << ",";
               std::cout << p->s.back();
           }
       }
       std::cout << ").\n";
        
    }
    else*/
    {
        //ss << "v" << id_;
        //std::string name(ss.str());
        std::cout << THEORY << " " << SYMBOL << " " << id_ << " " << name_.size() << " " << name_ << "\n";
        
        m.addSymbol("show");
        
        auto elem = m.getNewElement();
        std::cout << THEORY << " " << ELEMENT << " " << elem << " 1 " << id_ << " 0\n";
        std::cout << THEORY << " " << ATOM << " 0 0 " << m.getSymbolUid("show") << " 1 " << elem << "\n";
        ///TODO: domain ? i need flatZincModel

        m.addSymbol("dom");
        m.addSymbol("=");
        m.addSymbol("..");
        std::vector<unsigned int> elems;
        
        {
            const AST::SetLit* p = &dom;
            if (p->interval)
            {
                elems.emplace_back(m.getNewElement());
                m.addInteger(p->min);
                m.addInteger(p->max);
                unsigned int comp = m.getNewTerm();
                std::cout << THEORY << " " << COMPOUND << " " << comp << " " << m.getSymbolUid("..") << " 2 " << m.getIntegerUid(p->min) << " " << m.getIntegerUid(p->max) << "\n" ;
                std::cout << THEORY << " " << ELEMENT << " " << elems.back() << " 1 " << comp << " 0\n";
                //std::cout << p->min << ".." << p->max;
            }
            else
            {
                for (unsigned int i = 0; i < p->s.size(); ++i)
                {
                    elems.emplace_back(m.getNewElement());
                    unsigned int e = m.addInteger(p->s[i]);
                    std::cout << THEORY << " " << ELEMENT << " " << elems.back() << " 1 " << e << " 0\n";
                }
//                      for (unsigned int i = 0; i < p->s.size()-1; ++i)
//                          std::cout << p->s[i] << ",";
//                      std::cout << p->s.back();
            }    
            
            std::cout << THEORY << " " << ATOMWITHBOUND << " " << m.trueUid << " 0 " << m.getSymbolUid("dom") << " " << elems.size() << " ";
            for (unsigned int e : elems)
            {
                std::cout << e << " ";
            }
            std::cout << m.getSymbolUid("=") << " " << id_ << "\n";
         }
        

    }
}
  
  FlatZincModel::FlatZincModel(void)
  :  intVarCount(-1), boolVarCount(-1), setVarCount(-1), _optVar(-1),
    _solveAnnotations(NULL), uid_theory(1) {}

  void
  FlatZincModel::init(int intVars, int boolVars, int setVars) {
    std::cout << "asp 1 0 0\n"; /// start program
    std::cout << "1 0 1 1 0 0\n";
    trueUid=1;
    std::cout << "1 0 1 2 0 1 -1\n";
    falseUid=2;
    intVarCount = 0;
    iv = std::vector<IntVar>(intVars);
    iv_introduced = std::vector<bool>(intVars);
    iv_boolalias = std::vector<int>(intVars);
    boolVarCount = 0;
    bv = std::vector<BoolVar>(boolVars);
    bv_introduced = std::vector<bool>(boolVars);
    setVarCount = 0;
    sv = std::vector<SetVar>(setVars);
    sv_introduced = std::vector<bool>(setVars);
  }

  void
  FlatZincModel::newIntVar(IntVarSpec* vs, const std::string& s) {
    if (vs->alias) {
      iv[intVarCount++] = iv[vs->i];
    } else {
      //std::cerr << "create new IntVar " << intVarCount << "\n";
      /// TODO: create actual integer variable from vs
      iv[intVarCount++] = IntVar::newVar(vs,uid_theory++,s);
      iv[intVarCount-1].printDefinition(*this);
    }
    iv_introduced[intVarCount-1] = vs->introduced;
    iv_boolalias[intVarCount-1] = -1;
  }

  void
  FlatZincModel::aliasBool2Int(int iv, int bv) {
    iv_boolalias[iv] = bv;
  }
  int
  FlatZincModel::aliasBool2Int(int iv) {
    return iv_boolalias[iv];
  }

  void
  FlatZincModel::newBoolVar(BoolVarSpec* vs, const std::string& s) {
    if (vs->alias) {
      bv[boolVarCount++] = bv[vs->i];
    } else {
      //std::cerr << "create new BoolVar " << boolVarCount << "\n";
      /// TODO: create actual Boolean variable from vs
      bv[boolVarCount++] = BoolVar::newVar(vs,s);
      bv[boolVarCount-1].printDefinition(*this);
      iv_boolalias[intVarCount-1] = -1;
    }
    bv_introduced[boolVarCount-1] = vs->introduced;
  }

  void
  FlatZincModel::newSetVar(SetVarSpec* vs) {
    if (vs->alias) {
      sv[setVarCount++] = sv[vs->i];
    } else {
      std::cerr << "create new SetVar " << setVarCount << "\n";
      /// TODO: create actual set variable from vs
      sv[setVarCount++] = SetVar();
    }
    sv_introduced[setVarCount-1] = vs->introduced;
  }

  void
  FlatZincModel::postConstraint(const ConExpr& ce, AST::Node* ann) {
    try {
      registry().post(*this, ce, ann);
    } catch (AST::TypeError& e) {
      throw FlatZinc::Error("Type error", e.what());
    }
  }

  void flattenAnnotations(AST::Array* ann, std::vector<AST::Node*>& out) {
      for (unsigned int i=0; i<ann->a.size(); i++) {
        if (ann->a[i]->isCall("seq_search")) {
          AST::Call* c = ann->a[i]->getCall();
          if (c->args->isArray())
            flattenAnnotations(c->args->getArray(), out);
          else
            out.push_back(c->args);
        } else {
          out.push_back(ann->a[i]);
        }
      }
  }

  void
  FlatZincModel::createBranchers(AST::Node* ann, bool ignoreUnknown,
                                 std::ostream& err) {
    if (ann) {
      std::vector<AST::Node*> flatAnn;
      if (ann->isArray()) {
        flattenAnnotations(ann->getArray()  , flatAnn);
      } else {
        flatAnn.push_back(ann);
      }

      for (unsigned int i=0; i<flatAnn.size(); i++) {
        try {
          AST::Call *call = flatAnn[i]->getCall("int_search");
          AST::Array *args = call->getArgs(4);
          AST::Array *vars = args->a[0]->getArray();
          std::cerr << "int_search\n";
          // TODO: install search
        } catch (AST::TypeError& e) {
          (void) e;
          try {
            AST::Call *call = flatAnn[i]->getCall("bool_search");
            AST::Array *args = call->getArgs(4);
            AST::Array *vars = args->a[0]->getArray();
            std::cerr << "bool_search\n";
            // TODO: install search
          } catch (AST::TypeError& e) {
            (void) e;
            try {
              AST::Call *call = flatAnn[i]->getCall("set_search");
              AST::Array *args = call->getArgs(4);
              AST::Array *vars = args->a[0]->getArray();
              std::cerr << "set_search\n";
              // TODO: install search
            } catch (AST::TypeError& e) {
              (void) e;
              if (!ignoreUnknown) {
                err << "Warning, ignored search annotation: ";
                flatAnn[i]->print(err);
                err << std::endl;
              }
            }
          }
        }
      }
    }
    // TODO: install search for all remaining variables
  }

  AST::Array*
  FlatZincModel::solveAnnotations(void) const {
    return _solveAnnotations;
  }

  void
  FlatZincModel::solve(AST::Array* ann) {
    _method = SAT;
    _solveAnnotations = ann;
  }

  void
  FlatZincModel::minimize(int var, AST::Array* ann) {
      
    addSymbol("minimize");
    auto id = getIntVariableUid(var);
    unsigned int elem = getNewElement();
    std::cout << THEORY << " " << ELEMENT << " " << elem << " 1 " << id << " 0\n";
    std::cout << THEORY << " " << ATOM << " 0 0 " << getSymbolUid("minimize") << " ";
    std::cout << "1" << " " << elem << "\n";
    //std::cout << "TODO: minimize" << std::endl;
    //iv[var]
    /*_method = MIN;
    _optVar = var;
    _solveAnnotations = ann;
    // Branch on optimization variable to ensure that it is given a value.
    AST::Array* args = new AST::Array(4);
    args->a[0] = new AST::Array(new AST::IntVar(_optVar));
    args->a[1] = new AST::Atom("input_order");
    args->a[2] = new AST::Atom("indomain_min");
    args->a[3] = new AST::Atom("complete");
    AST::Call* c = new AST::Call("int_search", args);
    if (!ann)
      ann = new AST::Array(c);
    else
      ann->a.push_back(c);*/
  }

  void
  FlatZincModel::maximize(int var, AST::Array* ann) {
      
      /// convert to minimize (-var)
      addSymbol("minimize");
      addSymbol("-");
      auto id = getIntVariableUid(var);
      unsigned int term = getNewTerm();
      std::cout << THEORY << " " << COMPOUND << " "  << term << " " << getSymbolUid("-") << " 1 " << id << "\n";

      unsigned int elem = getNewElement();
      std::cout << THEORY << " " << ELEMENT << " " << elem << " 1 " << term << " 0\n";
      std::cout << THEORY << " " << ATOM << " 0 0 " << getSymbolUid("minimize") << " ";
      std::cout << "1" << " " << elem << "\n";
      
//    _method = MAX;
//    _optVar = var;
//    _solveAnnotations = ann;
//    // Branch on optimization variable to ensure that it is given a value.
//    AST::Array* args = new AST::Array(4);
//    args->a[0] = new AST::Array(new AST::IntVar(_optVar));
//    args->a[1] = new AST::Atom("input_order");
//    args->a[2] = new AST::Atom("indomain_min");
//    args->a[3] = new AST::Atom("complete");
//    AST::Call* c = new AST::Call("int_search", args);
//    if (!ann)
//      ann = new AST::Array(c);
//    else
//      ann->a.push_back(c);
  }

  FlatZincModel::~FlatZincModel(void) {
    delete _solveAnnotations;
    std::cout << "0\n" << std::endl;
  }

  void
  FlatZincModel::run(std::ostream& out, const Printer& p) {
    switch (_method) {
    case MIN:
    case MAX:
      //std::cerr << "start optimization search\n";
      // TODO: perform actual search
      break;
    case SAT:
      //std::cerr << "start satisfiability search\n";      
      // TODO: perform actual search
      break;
    }
  }

  FlatZincModel::Meth
  FlatZincModel::method(void) const {
    return _method;
  }

  int
  FlatZincModel::optVar(void) const {
    return _optVar;
  }

  void
  FlatZincModel::print(std::ostream& out, const Printer& p) const {
    p.print(out, *this);
  }

  void
  Printer::init(AST::Array* output) {
    _output = output;
  }

  void
  Printer::printElem(std::ostream& out, AST::Node* ai,
                     const FlatZincModel& m) const {
      //std::cout << "PRINTELEMENT" << std::endl;
    int k;
    if (ai->isInt(k)) {
      out << k;
    } else if (ai->isIntVar()) {
      // TODO: output actual variable
      out << ai->getIntVar();
    } else if (ai->isBoolVar()) {
      // TODO: output actual variable
      out << ai->getBoolVar();
    } else if (ai->isSetVar()) {
      // TODO: output actual variable
      out << ai->getSetVar();
    } else if (ai->isBool()) {
      out << (ai->getBool() ? "true" : "false");
    } else if (ai->isSet()) {
      AST::SetLit* s = ai->getSet();
      if (s->interval) {
        out << s->min << ".." << s->max;
      } else {
        out << "{";
        for (unsigned int i=0; i<s->s.size(); i++) {
          out << s->s[i] << (i < s->s.size()-1 ? ", " : "}");
        }
      }
    } else if (ai->isString()) {
      std::string s = ai->getString();
      for (unsigned int i=0; i<s.size(); i++) {
        if (s[i] == '\\' && i<s.size()-1) {
          switch (s[i+1]) {
          case 'n': out << "\n"; break;
          case '\\': out << "\\"; break;
          case 't': out << "\t"; break;
          default: out << "\\" << s[i+1];
          }
          i++;
        } else {
          out << s[i];
        }
      }
    }
  }

  void
  Printer::print(std::ostream& out, const FlatZincModel& m) const {
    if (_output == NULL)
      return;
    /*for (unsigned int i=0; i< _output->a.size(); i++) {
      AST::Node* ai = _output->a[i];
      if (ai->isArray()) {
        AST::Array* aia = ai->getArray();
        int size = aia->a.size();
        out << "[";
        for (int j=0; j<size; j++) {
          printElem(out,aia->a[j],m);
          if (j<size-1)
            out << ", ";
        }
        out << "]";
      } else {
        printElem(out,ai,m);
      }
    }*/
  }

  Printer::~Printer(void) {
    delete _output;
  }

}

// STATISTICS: flatzinc-any
