#include <cclingo.h>
#include <clingo/clingocontrol.hh>
#include <gringo/input/nongroundparser.hh>

using namespace Gringo;
using namespace Gringo::Input;

extern "C" void clingo_free(void *p) {
    ::operator delete[](p);
}

// {{{1 error

namespace {

struct ClingoError : std::exception {
    ClingoError(clingo_error_t err) : err(err) { }
    virtual ~ClingoError() noexcept = default;
    virtual const char* what() const noexcept {
        return clingo_error_str(err);
    }
    clingo_error_t const err;
};

}

extern "C" char const *clingo_error_str(clingo_error_t err) {
    switch (err) {
        case clingo_error_success:   { return nullptr; }
        case clingo_error_runtime:   { return "runtime error"; }
        case clingo_error_bad_alloc: { return "bad allocation"; }
        case clingo_error_logic:     { return "logic error"; }
        case clingo_error_unknown:   { return "unknown error"; }
    }
    return nullptr;
}

#define CCLINGO_TRY try {
#define CCLINGO_CATCH } \
catch(ClingoError &e)        { return e.err; } \
catch(std::bad_alloc &e)     { return clingo_error_bad_alloc; } \
catch(std::runtime_error &e) { return clingo_error_runtime; } \
catch(std::logic_error &e)   { return clingo_error_logic; } \
catch(...)                   { return clingo_error_unknown; } \
return clingo_error_success

// {{{1 value

namespace {

void init_value(Gringo::Value v, clingo_value_t *val) {
    Gringo::Value::POD p = v;
    val->a = p.type;
    val->b = p.value;
}

} // namespace

Gringo::Value toVal(clingo_value_t a) {
    return Gringo::Value::POD{a.a, a.b};
}

clingo_value_t toVal(Gringo::Value a) {
    Gringo::Value::POD v = a;
    return {v.type, v.value};
}

extern "C" void clingo_value_new_num(int num, clingo_value_t *val) {
    init_value(Gringo::Value::createNum(num), val);
}

extern "C" void clingo_value_new_sup(clingo_value_t *val) {
    init_value(Gringo::Value::createSup(), val);
}

extern "C" void clingo_value_new_inf(clingo_value *val) {
    init_value(Gringo::Value::createInf(), val);
}

extern "C" clingo_error_t clingo_value_new_str(char const *str, clingo_value *val) {
    CCLINGO_TRY
        init_value(Gringo::Value::createStr(str), val);
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_value_new_id(char const *id, bool_t sign, clingo_value *val) {
    CCLINGO_TRY
        init_value(Gringo::Value::createId(id, sign), val);
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_value_new_fun(char const *name, clingo_value_span_t args, bool_t sign, clingo_value *val) {
    CCLINGO_TRY
        Gringo::ValVec vals;
        vals.reserve(args.size);
        for (auto it = args.begin, ie = it + args.size; it != ie; ++it) {
            vals.emplace_back(Gringo::Value::POD{it->a, it->b});
        }
        init_value(Gringo::Value::createFun(name, vals, sign), val);
    CCLINGO_CATCH;
}

extern "C" int clingo_value_num(clingo_value_t val) {
    return toVal(val).num();
}

extern "C" char const *clingo_value_name(clingo_value_t val) {
    return toVal(val).name()->c_str();
}

extern "C" char const *clingo_value_str(clingo_value_t val) {
    return toVal(val).string()->c_str();
}

extern "C" bool_t clingo_value_sign(clingo_value_t val) {
    return toVal(val).sign();
}

extern "C" clingo_value_span_t clingo_value_args(clingo_value_t val) {
    auto args = toVal(val).args();
    return {&reinterpret_cast<clingo_value_t const&>(*args.begin()), args.size()};
}

extern "C" clingo_value_type_t clingo_value_type(clingo_value_t val) {
    return static_cast<clingo_value_type_t>(toVal(val).type());
}

extern "C" clingo_error_t clingo_value_to_string(clingo_value_t val, char **str) {
    CCLINGO_TRY
        std::ostringstream oss;
        toVal(val).print(oss);
        std::string s = oss.str();
        *str = reinterpret_cast<char *>(::operator new[](sizeof(char) * (s.size() + 1)));
        std::strcpy(*str, s.c_str());
    CCLINGO_CATCH;
}

extern "C" bool_t clingo_value_eq(clingo_value_t a, clingo_value_t b) {
    return toVal(a) == toVal(b);
}

extern "C" bool_t clingo_value_lt(clingo_value_t a, clingo_value_t b) {
    return toVal(a) < toVal(b);
}

// {{{1 module

struct clingo_module : DefaultGringoModule { };

extern "C" clingo_error_t clingo_module_new(clingo_module_t **mod) {
    CCLINGO_TRY
        *mod = new clingo_module();
    CCLINGO_CATCH;
}

extern "C" void clingo_module_free(clingo_module_t *mod) {
    delete mod;
}

// {{{1 model

struct clingo_model : Gringo::Model { };

extern "C" bool_t clingo_model_contains(clingo_model_t *m, clingo_value_t atom) {
    return m->contains(toVal(atom));
}

extern "C" clingo_error_t clingo_model_atoms(clingo_model_t *m, clingo_show_type_t show, clingo_value_span_t *ret) {
    CCLINGO_TRY
        Gringo::ValVec const &atoms = m->atoms(show);
        ret->begin = reinterpret_cast<clingo_value_t const*>(atoms.data());
        ret->size = atoms.size();
    CCLINGO_CATCH;
}

// {{{1 solve iter

struct clingo_solve_iter : Gringo::SolveIter { };

extern "C" clingo_error_t clingo_solve_iter_next(clingo_solve_iter_t *it, clingo_model **m) {
    CCLINGO_TRY
        *m = reinterpret_cast<clingo_model*>(const_cast<Gringo::Model*>(it->next()));
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_solve_iter_get(clingo_solve_iter_t *it, clingo_solve_result_t *ret) {
    CCLINGO_TRY
        *ret = static_cast<int>(it->get());
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_solve_iter_close(clingo_solve_iter_t *it) {
    CCLINGO_TRY
        it->close();
    CCLINGO_CATCH;
}

// {{{1 statistics

// {{{1 configurations

// {{{1 ast type

extern "C" char const *clingo_ast_type_str(clingo_ast_type_t type) {
    switch (type) {
        case clingo_ast_type_term_function: { return "function"; }
        case clingo_ast_type_term_variable: { return  "variable"; }
        case clingo_ast_type_term_value:    { return "value"; }
    }
    return nullptr;
}

// {{{1 ast

class ASTBuilder : public Gringo::Input::INongroundProgramBuilder {
    using NodeList = std::forward_list<clingo_ast>;
    using NodeVecList = std::forward_list<std::vector<clingo_ast>>;
    using TermUidVec = std::vector<TermUid>;
    using TermUidVecVec = std::vector<TermVecUid>;
    using Callback = std::function<void (clingo_ast &ast)>;
public:
    ASTBuilder(Callback cb) : cb_(cb) { }
    clingo_ast_t &newNode(clingo_ast_type_t type, Location const &loc, Value val) {
        nodes_.emplace_front();
        memset(&nodes_.front(), 0, sizeof(clingo_ast_t));
        nodes_.front().type = type;
        nodes_.front().location = {
            loc.beginFilename->c_str(), loc.endFilename->c_str(),
            loc.beginLine, loc.endLine,
            loc.beginColumn, loc.endColumn
        };
        nodes_.front().value = toVal(val);
        return nodes_.front();
    }
    // {{{2 terms
    TermUid term(Location const &loc, Value val) override {
        auto &node = newNode(clingo_ast_type_term_value, loc, val);
        return terms_.insert(&node);
    }
    TermUid term(Location const &loc, FWString name) override {
        auto &node = newNode(clingo_ast_type_term_variable, loc, Value::createId(name));
        return terms_.insert(&node);
    }
    TermUid term(Location const &loc, UnOp op, TermUid a) override {
        Value val;
        switch (op) {
            case UnOp::NOT: { val = Value::createId("~"); break;}
            case UnOp::NEG: { val = Value::createId("-"); break;}
            case UnOp::ABS: { val = Value::createId("|"); break;}
        }
        auto &node = newNode(clingo_ast_type_term_function, loc, val);
        node.children.size = 1;
        node.children.begin = terms_.erase(a);
        return terms_.insert(&node);
    }
    TermUid pool_(Location const &loc, TermUidVec const &vec) {
        if (vec.size() == 1) { return vec.front(); }
        else {
            nodeVecs_.emplace_front();
            auto &nodeVec = nodeVecs_.front();
            for (auto &term : vec) {
                nodeVec.emplace_back(*terms_.erase(term));
            }
            auto &node = newNode(clingo_ast_type_term_function, loc, Value::createId(";"));
            node.children.size = nodeVec.size();
            node.children.begin = nodeVec.data();
            return terms_.insert(&node);
        }
    }
    TermUid term(Location const &loc, UnOp op, TermVecUid a) override {
        return term(loc, op, pool_(loc, termvecs_.erase(a)));
    }
    TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) override {
        Value val;
        switch (op) {
            case BinOp::ADD: { val = Value::createId("+"); break; }
            case BinOp::OR:  { val = Value::createId("?");  break; }
            case BinOp::SUB: { val = Value::createId("-"); break; }
            case BinOp::MOD: { val = Value::createId("\\"); break; }
            case BinOp::MUL: { val = Value::createId("*"); break; }
            case BinOp::XOR: { val = Value::createId("^"); break; }
            case BinOp::POW: { val = Value::createId("**"); break; }
            case BinOp::DIV: { val = Value::createId("/"); break; }
            case BinOp::AND: { val = Value::createId("&"); break; }
        }
        auto &node = newNode(clingo_ast_type_term_function, loc, val);
        nodeVecs_.emplace_front();
        nodeVecs_.front().emplace_back(*terms_.erase(a));
        nodeVecs_.front().emplace_back(*terms_.erase(b));
        node.children.size = 2;
        node.children.begin = nodeVecs_.front().data();
        return terms_.insert(&node);
    }
    TermUid term(Location const &loc, TermUid a, TermUid b) override {
        auto &node = newNode(clingo_ast_type_term_function, loc, Value::createId(".."));
        nodeVecs_.emplace_front();
        nodeVecs_.front().emplace_back(*terms_.erase(a));
        nodeVecs_.front().emplace_back(*terms_.erase(b));
        node.children.size = 2;
        node.children.begin = nodeVecs_.front().data();
        return terms_.insert(&node);
    }
    TermUid lua_(Location const &loc, clingo_ast_t &child, bool lua) {
        if (!lua) { return terms_.insert(&child); }
        else {
            auto &node = newNode(clingo_ast_type_term_function, loc, Value::createId("@"));
            node.children.size = 1;
            node.children.begin = &child;
            return terms_.insert(&node);
        }
    }
    TermUid fun_(Location const &loc, FWString name, TermVecUid a, bool lua) {
        auto args = termvecs_.erase(a);
        for (auto &arg : args) {
            nodeVecs_.front().emplace_back(*terms_.erase(arg));
        }
        auto &node = newNode(clingo_ast_type_term_function, loc, Value::createId(name->c_str()));
        node.children.size = args.size();
        node.children.begin = nodeVecs_.front().data();
        return lua_(loc, node, lua);
    }
    TermUid term(Location const &loc, FWString name, TermVecVecUid a, bool lua) override {
        TermUidVec pool;
        for (auto &args : termvecvecs_.erase(a)) {
            pool.emplace_back(fun_(loc, name, args, lua));
        }
        return pool_(loc, pool);
    }
    TermUid term(Location const &loc, TermVecUid a, bool forceTuple) override {
        auto args = termvecs_.erase(a);
        if (args.size() == 1 && !forceTuple) {
            return args.front();
        }
        else {
            auto args = termvecs_.erase(a);
            for (auto &arg : args) {
                nodeVecs_.front().emplace_back(*terms_.erase(arg));
            }
            auto &node = newNode(clingo_ast_type_term_function, loc, Value::createId(","));
            node.children.size = args.size();
            node.children.begin = nodeVecs_.front().data();
            return terms_.insert(&node);
        }
    }
    TermUid pool(Location const &loc, TermVecUid a) override {
        return pool_(loc, termvecs_.erase(a));
    }
    // {{{2 csp
    CSPMulTermUid cspmulterm(Location const &loc, TermUid coe, TermUid var) override {
        (void)loc;
        (void)coe;
        (void)var;
        throw std::logic_error("implement me!!!");
    }

    CSPMulTermUid cspmulterm(Location const &loc, TermUid coe) override {
        (void)loc;
        (void)coe;
        throw std::logic_error("implement me!!!");
    }

    CSPAddTermUid cspaddterm(Location const &loc, CSPAddTermUid a, CSPMulTermUid b, bool add) override {
        (void)loc;
        (void)a;
        (void)b;
        (void)add;
        throw std::logic_error("implement me!!!");
    }

    CSPAddTermUid cspaddterm(Location const &loc, CSPMulTermUid a) override {
        (void)loc;
        (void)a;
        throw std::logic_error("implement me!!!");
    }

    LitUid csplit(CSPLitUid a) override {
        (void)a;
        throw std::logic_error("implement me!!!");
    }

    CSPLitUid csplit(Location const &loc, CSPLitUid a, Relation rel, CSPAddTermUid b) override {
        (void)loc;
        (void)a;
        (void)rel;
        (void)b;
        throw std::logic_error("implement me!!!");
    }

    CSPLitUid csplit(Location const &loc, CSPAddTermUid a, Relation rel, CSPAddTermUid b) override {
        (void)loc;
        (void)a;
        (void)rel;
        (void)b;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 id vectors
    IdVecUid idvec() override {
        throw std::logic_error("implement me!!!");
    }

    IdVecUid idvec(IdVecUid uid, Location const &loc, FWString id) override {
        (void)uid;
        (void)loc;
        (void)id;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 term vectors
    TermVecUid termvec() override {
        throw std::logic_error("implement me!!!");
    }

    TermVecUid termvec(TermVecUid uid, TermUid term) override {
        (void)uid;
        (void)term;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 term vector vectors
    TermVecVecUid termvecvec() override {
        throw std::logic_error("implement me!!!");
    }

    TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) override {
        (void)uid;
        (void)termvecUid;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 literals
    LitUid boollit(Location const &loc, bool type) override {
        (void)loc;
        (void)type;
        throw std::logic_error("implement me!!!");
    }

    LitUid predlit(Location const &loc, NAF naf, bool neg, FWString name, TermVecVecUid argvecvecUid) override {
        (void)loc;
        (void)naf;
        (void)neg;
        (void)name;
        (void)argvecvecUid;
        throw std::logic_error("implement me!!!");
    }

    LitUid rellit(Location const &loc, Relation rel, TermUid termUidLeft, TermUid termUidRight) override {
        (void)loc;
        (void)rel;
        (void)termUidLeft;
        (void)termUidRight;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 literal vectors
    LitVecUid litvec() override {
        throw std::logic_error("implement me!!!");
    }

    LitVecUid litvec(LitVecUid uid, LitUid literalUid) override {
        (void)uid;
        (void)literalUid;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 conditional literals
    CondLitVecUid condlitvec() override {
        throw std::logic_error("implement me!!!");
    }

    CondLitVecUid condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) override {
        (void)uid;
        (void)lit;
        (void)litvec;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 body aggregate elements
    BdAggrElemVecUid bodyaggrelemvec() override {
        throw std::logic_error("implement me!!!");
    }

    BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) override {
        (void)uid;
        (void)termvec;
        (void)litvec;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 head aggregate elements
    HdAggrElemVecUid headaggrelemvec() override {
        throw std::logic_error("implement me!!!");
    }

    HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) override {
        (void)uid;
        (void)termvec;
        (void)lit;
        (void)litvec;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 bounds
    BoundVecUid boundvec() override {
        throw std::logic_error("implement me!!!");
    }

    BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) override {
        (void)uid;
        (void)rel;
        (void)term;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 heads
    HdLitUid headlit(LitUid lit) override {
        (void)lit;
        throw std::logic_error("implement me!!!");
    }

    HdLitUid headaggr(Location const &loc, TheoryAtomUid atom) override {
        (void)loc;
        (void)atom;
        throw std::logic_error("implement me!!!");
    }

    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) override {
        (void)loc;
        (void)fun;
        (void)bounds;
        (void)headaggrelemvec;
        throw std::logic_error("implement me!!!");
    }

    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid headaggrelemvec) override {
        (void)loc;
        (void)fun;
        (void)bounds;
        (void)headaggrelemvec;
        throw std::logic_error("implement me!!!");
    }

    HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) override {
        (void)loc;
        (void)condlitvec;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 bodies
    BdLitVecUid body() override {
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) override {
        (void)body;
        (void)bodylit;
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) override {
        (void)body;
        (void)loc;
        (void)naf;
        (void)atom;
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) override {
        (void)body;
        (void)loc;
        (void)naf;
        (void)fun;
        (void)bounds;
        (void)bodyaggrelemvec;
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) override {
        (void)body;
        (void)loc;
        (void)naf;
        (void)fun;
        (void)bounds;
        (void)bodyaggrelemvec;
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) override {
        (void)body;
        (void)loc;
        (void)head;
        (void)litvec;
        throw std::logic_error("implement me!!!");
    }

    BdLitVecUid disjoint(BdLitVecUid body, Location const &loc, NAF naf, CSPElemVecUid elem) override {
        (void)body;
        (void)loc;
        (void)naf;
        (void)elem;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 csp constraint elements
    CSPElemVecUid cspelemvec() override {
        throw std::logic_error("implement me!!!");
    }

    CSPElemVecUid cspelemvec(CSPElemVecUid uid, Location const &loc, TermVecUid termvec, CSPAddTermUid addterm, LitVecUid litvec) override {
        (void)uid;
        (void)loc;
        (void)termvec;
        (void)addterm;
        (void)litvec;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 statements
    void rule(Location const &loc, HdLitUid head) override {
        (void)loc;
        (void)head;
        throw std::logic_error("implement me!!!");
    }

    void rule(Location const &loc, HdLitUid head, BdLitVecUid body) override {
        (void)loc;
        (void)head;
        (void)body;
        throw std::logic_error("implement me!!!");
    }

    void define(Location const &loc, FWString name, TermUid value, bool defaultDef) override {
        (void)loc;
        (void)name;
        (void)value;
        (void)defaultDef;
        throw std::logic_error("implement me!!!");
    }

    void optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) override {
        (void)loc;
        (void)weight;
        (void)priority;
        (void)cond;
        (void)body;
        throw std::logic_error("implement me!!!");
    }

    void showsig(Location const &loc, FWSignature sig, bool csp) override {
        (void)loc;
        (void)sig;
        (void)csp;
        throw std::logic_error("implement me!!!");
    }

    void show(Location const &loc, TermUid t, BdLitVecUid body, bool csp) override {
        (void)loc;
        (void)t;
        (void)body;
        (void)csp;
        throw std::logic_error("implement me!!!");
    }

    void python(Location const &loc, FWString code) override {
        (void)loc;
        (void)code;
        throw std::logic_error("implement me!!!");
    }

    void lua(Location const &loc, FWString code) override {
        (void)loc;
        (void)code;
        throw std::logic_error("implement me!!!");
    }

    void block(Location const &loc, FWString name, IdVecUid args) override {
        (void)loc;
        (void)name;
        (void)args;
        throw std::logic_error("implement me!!!");
    }

    void external(Location const &loc, LitUid head, BdLitVecUid body) override {
        (void)loc;
        (void)head;
        (void)body;
        throw std::logic_error("implement me!!!");
    }

    void edge(Location const &loc, TermVecVecUid edges, BdLitVecUid body) override {
        (void)loc;
        (void)edges;
        (void)body;
        throw std::logic_error("implement me!!!");
    }

    void heuristic(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) override {
        (void)loc;
        (void)neg;
        (void)name;
        (void)tvvUid;
        (void)body;
        (void)a;
        (void)b;
        (void)mod;
        throw std::logic_error("implement me!!!");
    }

    void project(Location const &loc, bool neg, FWString name, TermVecVecUid tvvUid, BdLitVecUid body) override {
        (void)loc;
        (void)neg;
        (void)name;
        (void)tvvUid;
        (void)body;
        throw std::logic_error("implement me!!!");
    }

    void project(Location const &loc, FWSignature sig) override {
        (void)loc;
        (void)sig;
        throw std::logic_error("implement me!!!");
    }

    // {{{2 theory atoms

    TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) override {
        (void)loc;
        (void)args;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) override {
        (void)loc;
        (void)args;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) override {
        (void)loc;
        (void)opterm;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) override {
        (void)loc;
        (void)args;
        throw std::logic_error("implement me!!!");

    }

    TheoryTermUid theorytermfun(Location const &loc, FWString name, TheoryOptermVecUid args) override {
        (void)loc;
        (void)name;
        (void)args;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermUid theorytermvalue(Location const &loc, Value val) override {
        (void)loc;
        (void)val;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermUid theorytermvar(Location const &loc, FWString var) override {
        (void)loc;
        (void)var;
        throw std::logic_error("implement me!!!");
    }

    TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) override {
        (void)ops;
        (void)term;
        throw std::logic_error("implement me!!!");
    }

    TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) override {
        (void)opterm;
        (void)ops;
        (void)term;
        throw std::logic_error("implement me!!!");
    }

    TheoryOpVecUid theoryops() override {
        throw std::logic_error("implement me!!!");
    }

    TheoryOpVecUid theoryops(TheoryOpVecUid ops, FWString op) override {
        (void)ops;
        (void)op;
        throw std::logic_error("implement me!!!");
    }

    TheoryOptermVecUid theoryopterms() override {
        throw std::logic_error("implement me!!!");
    }

    TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, TheoryOptermUid opterm) override {
        (void)opterms;
        (void)opterm;
        throw std::logic_error("implement me!!!");
    }

    TheoryOptermVecUid theoryopterms(TheoryOptermUid opterm, TheoryOptermVecUid opterms) override {
        (void)opterm;
        (void)opterms;
        throw std::logic_error("implement me!!!");
    }

    TheoryElemVecUid theoryelems() override {
        throw std::logic_error("implement me!!!");
    }

    TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) override {
        (void)elems;
        (void)opterms;
        (void)cond;
        throw std::logic_error("implement me!!!");
    }


    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) override {
        (void)term;
        (void)elems;
        throw std::logic_error("implement me!!!");
    }

    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, FWString op, TheoryOptermUid opterm) override {
        (void)term;
        (void)elems;
        (void)op;
        (void)opterm;
        throw std::logic_error("implement me!!!");
    }


    // {{{2 theory definitions

    TheoryOpDefUid theoryopdef(Location const &loc, FWString op, unsigned priority, TheoryOperatorType type) override {
        (void)loc;
        (void)op;
        (void)priority;
        (void)type;
        throw std::logic_error("implement me!!!");
    }

    TheoryOpDefVecUid theoryopdefs() override {
        throw std::logic_error("implement me!!!");
    }

    TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) override {
        (void)defs;
        (void)def;
        throw std::logic_error("implement me!!!");
    }

    TheoryTermDefUid theorytermdef(Location const &loc, FWString name, TheoryOpDefVecUid defs) override {
        (void)loc;
        (void)name;
        (void)defs;
        throw std::logic_error("implement me!!!");
    }

    TheoryAtomDefUid theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type) override {
        (void)loc;
        (void)name;
        (void)arity;
        (void)termDef;
        (void)type;
        throw std::logic_error("implement me!!!");
    }

    TheoryAtomDefUid theoryatomdef(Location const &loc, FWString name, unsigned arity, FWString termDef, TheoryAtomType type, TheoryOpVecUid ops, FWString guardDef) override {
        (void)loc;
        (void)name;
        (void)arity;
        (void)termDef;
        (void)type;
        (void)ops;
        (void)guardDef;
        throw std::logic_error("implement me!!!");
    }

    TheoryDefVecUid theorydefs() override {
        throw std::logic_error("implement me!!!");
    }

    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) override {
        (void)defs;
        (void)def;
        throw std::logic_error("implement me!!!");
    }

    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) override {
        (void)defs;
        (void)def;
        throw std::logic_error("implement me!!!");
    }

    void theorydef(Location const &loc, FWString name, TheoryDefVecUid defs) override {
        (void)loc;
        (void)name;
        (void)defs;
        throw std::logic_error("implement me!!!");
    }

    // }}}2

    virtual ~ASTBuilder() noexcept = default;
private:
    using Terms            = Indexed<clingo_ast*, TermUid>;
    using TermVecs         = Indexed<TermUidVec, TermVecUid>;
    using TermVecVecs      = Indexed<TermUidVecVec, TermVecVecUid>;
    using IdVecs           = Indexed<clingo_ast*, IdVecUid>;
    using Lits             = Indexed<clingo_ast*, LitUid>;
    using LitVecs          = Indexed<clingo_ast*, LitVecUid>;
    using BodyAggrElemVecs = Indexed<clingo_ast*, BdAggrElemVecUid>;
    using CondLitVecs      = Indexed<clingo_ast*, CondLitVecUid>;
    using HeadAggrElemVecs = Indexed<clingo_ast*, HdAggrElemVecUid>;
    using Bodies           = Indexed<clingo_ast*, BdLitVecUid>;
    using Heads            = Indexed<clingo_ast*, HdLitUid>;
    using CSPLits          = Indexed<clingo_ast*, CSPLitUid>;
    using CSPAddTerms      = Indexed<clingo_ast*, CSPAddTermUid>;
    using CSPMulTerms      = Indexed<clingo_ast*, CSPMulTermUid>;
    using CSPElems         = Indexed<clingo_ast*, CSPElemVecUid>;
    using Bounds           = Indexed<clingo_ast*, BoundVecUid>;

    using TheoryOpVecs      = Indexed<clingo_ast*, TheoryOpVecUid>;
    using TheoryTerms       = Indexed<clingo_ast*, TheoryTermUid>;
    using RawTheoryTerms    = Indexed<clingo_ast*, TheoryOptermUid>;
    using RawTheoryTermVecs = Indexed<clingo_ast*, TheoryOptermVecUid>;
    using TheoryElementVecs = Indexed<clingo_ast*, TheoryElemVecUid>;
    using TheoryAtoms       = Indexed<clingo_ast*, TheoryAtomUid>;

    using TheoryOpDefs    = Indexed<clingo_ast*, TheoryOpDefUid>;
    using TheoryOpDefVecs = Indexed<clingo_ast*, TheoryOpDefVecUid>;
    using TheoryTermDefs  = Indexed<clingo_ast*, TheoryTermDefUid>;
    using TheoryAtomDefs  = Indexed<clingo_ast*, TheoryAtomDefUid>;
    using TheoryDefVecs   = Indexed<clingo_ast*, TheoryDefVecUid>;

    Callback            cb_;
    NodeList            nodes_;
    NodeVecList         nodeVecs_;
    Terms               terms_;
    TermVecs            termvecs_;
    TermVecVecs         termvecvecs_;
    IdVecs              idvecs_;
    Lits                lits_;
    LitVecs             litvecs_;
    BodyAggrElemVecs    bodyaggrelemvecs_;
    HeadAggrElemVecs    headaggrelemvecs_;
    CondLitVecs         condlitvecs_;
    Bounds              bounds_;
    Bodies              bodies_;
    Heads               heads_;
    CSPLits             csplits_;
    CSPAddTerms         cspaddterms_;
    CSPMulTerms         cspmulterms_;
    CSPElems            cspelems_;
    TheoryOpVecs        theoryOpVecs_;
    TheoryTerms         theoryTerms_;
    RawTheoryTerms      theoryOpterms_;
    RawTheoryTermVecs   theoryOptermVecs_;
    TheoryElementVecs   theoryElems_;
    TheoryAtoms         theoryAtoms_;
    TheoryOpDefs        theoryOpDefs_;
    TheoryOpDefVecs     theoryOpDefVecs_;
    TheoryTermDefs      theoryTermDefs_;
    TheoryAtomDefs      theoryAtomDefs_;
    TheoryDefVecs       theoryDefVecs_;
};

// {{{1 domain

// {{{1 control

struct clingo_control : Gringo::Control { };

extern "C" clingo_error_t clingo_control_new(clingo_module_t *mod, int argc, char const **argv, clingo_control_t **ctl) {
    CCLINGO_TRY
        *ctl = static_cast<clingo_control_t*>(mod->newControl(argc, argv));
    CCLINGO_CATCH;
}

extern "C" void clingo_control_free(clingo_control_t *ctl) {
    delete ctl;
}

extern "C" clingo_error_t clingo_control_add(clingo_control_t *ctl, char const *name, char const **params, char const *part) {
    CCLINGO_TRY
        Gringo::FWStringVec p;
        for (char const **param = params; *param; ++param) { p.emplace_back(*param); }
        ctl->add(name, p, part);
    CCLINGO_CATCH;
}

namespace {

struct ClingoContext : Gringo::Context {
    ClingoContext(clingo_ground_callback_t *cb, void *data)
    : cb(cb)
    , data(data) {}

    bool callable(Gringo::FWString) const override {
        return cb;
    }

    Gringo::ValVec call(Gringo::Location const &, Gringo::FWString name, Gringo::ValVec const &args) override {
        assert(cb);
        clingo_value_span_t args_c;
        args_c.size = args.size();
        args_c.begin = reinterpret_cast<clingo_value_t const*>(args.data());
        clingo_value_span_t ret_c;
        auto err = cb(name->c_str(), args_c, data, &ret_c);
        if (err != 0) { throw ClingoError(err); }
        Gringo::ValVec ret;
        for (auto it = ret_c.begin, ie = it + ret_c.size; it != ie; ++it) {
            ret.emplace_back(toVal(*it));
        }
        return ret;
    }
    virtual ~ClingoContext() noexcept = default;

    clingo_ground_callback_t *cb;
    void *data;
};

}

extern "C" clingo_error_t clingo_control_ground(clingo_control_t *ctl, clingo_part_span_t vec, clingo_ground_callback_t *cb, void *data) {
    CCLINGO_TRY
        Gringo::Control::GroundVec gv;
        gv.reserve(vec.size);
        for (auto it = vec.begin, ie = it + vec.size; it != ie; ++it) {
            Gringo::ValVec params;
            params.reserve(it->params.size);
            for (auto jt = it->params.begin, je = jt + it->params.size; jt != je; ++jt) {
                params.emplace_back(Gringo::Value::POD{jt->a, jt->b});
            }
            gv.emplace_back(it->name, params);
        }
        ClingoContext cctx(cb, data);
        ctl->ground(gv, cb ? &cctx : nullptr);
    CCLINGO_CATCH;
}

Gringo::Control::Assumptions toAss(clingo_symbolic_literal_span_t *assumptions) {
    Gringo::Control::Assumptions ass;
    if (assumptions) {
        for (auto it = assumptions->begin, ie = it + assumptions->size; it != ie; ++it) {
            ass.emplace_back(toVal(it->atom), !it->sign);
        }
    }
    return ass;
}

extern "C" clingo_error_t clingo_control_solve(clingo_control_t *ctl, clingo_symbolic_literal_span_t *assumptions, clingo_model_handler_t *model_handler, void *data, clingo_solve_result_t *ret) {
    CCLINGO_TRY
        *ret = static_cast<clingo_solve_result_t>(ctl->solve([model_handler, data](Gringo::Model const &m) {
            return model_handler(static_cast<clingo_model*>(const_cast<Gringo::Model*>(&m)), data);
        }, toAss(assumptions)));
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_control_solve_iter(clingo_control_t *ctl, clingo_symbolic_literal_span_t *assumptions, clingo_solve_iter_t **it) {
    CCLINGO_TRY
        *it = reinterpret_cast<clingo_solve_iter_t*>(ctl->solveIter(toAss(assumptions)));
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_control_assign_external(clingo_control_t *ctl, clingo_value_t atom, clingo_truth_value_t value) {
    CCLINGO_TRY
        ctl->assignExternal(toVal(atom), static_cast<Potassco::Value_t>(value));
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_control_release_external(clingo_control_t *ctl, clingo_value_t atom) {
    CCLINGO_TRY
        ctl->assignExternal(toVal(atom), Potassco::Value_t::Release);
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_control_parse(clingo_control_t *ctl, char const *program, clingo_parse_callback_t *cb, void *data) {
    CCLINGO_TRY
        (void)ctl; // Note: not used yet
        ASTBuilder builder([data, cb](clingo_ast_t &ast) {
            auto ret = cb(&ast, data);
            if (ret != 0) { throw ClingoError(ret); }
        });
        NonGroundParser parser(builder);
        parser.pushStream("<string>", gringo_make_unique<std::istringstream>(program));
        parser.parse();
    CCLINGO_CATCH;
}

extern "C" clingo_error_t clingo_control_add_ast(clingo_control_t *ctl, clingo_add_ast_callback_t *cb, void *data) {
    CCLINGO_TRY
        for (;;) {
            clingo_ast_t *node = nullptr;
            auto ret = cb(&node, data);
            if (ret != 0) { return ret; }
            if (node != nullptr) {
                (void)ctl;
                throw std::logic_error("implement me!!!");
            }
            else { break; }
        }
    CCLINGO_CATCH;
}

// }}}1
