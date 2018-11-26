#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <exception>
#include <unordered_map>

using std::string;
using std::vector;
using std::unique_ptr;
using std::make_pair;
using std::pair;
using std::unordered_map;
using std::make_unique;
using std::runtime_error;
using std::cin;
using std::cout;
using std::endl;

// To do

typedef struct {
    const char* name;
    void* pointer;
} symbol_t;

void jit_compile_expression_to_arm(
        const char* expression, const symbol_t* externs, void* out_buffer);

// Some functions

bool is_digit(char c) {
    return '0' <= c and c <= '9';
}
bool is_letter(char c) {
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z');
}

// Tokens

enum TokenType {TOKEN_NUMBER, TOKEN_PUNCT, TOKEN_OPER, TOKEN_NAMED};
enum OperType {PLUS, MINUS, MUL, UNAR_MINUS};
enum PunctType {OPEN, CLOSE, COMMA};

struct Token {
    virtual ~Token() = default;
    virtual TokenType type() = 0;
};

struct TokenNumber;
struct TokenPunct;
struct TokenOper;
struct TokenNamed;

typedef vector<unique_ptr<Token> > ArrayOfTokens;

OperType get_oper_type(Token&);
PunctType get_punct_type(Token&);
string get_name(Token&);

ArrayOfTokens tokenate(const string& expr);

// Tree

enum NodeType {NODE_CONST, NODE_VAR, NODE_FUNC, NODE_OPER};

struct Node {
    virtual ~Node() = default;

    virtual NodeType type() = 0;
};

struct NodeConst;
struct NodeVar;
struct NodeFunc;
struct NodeOper;

void clear(Node* node);

pair<Node*, int> scan_expr_plus(const ArrayOfTokens& tokens, int i);
pair<Node*, int> scan_expr_mult(const ArrayOfTokens& tokens, int i);
pair<Node*, int> scan_expr_sub(const ArrayOfTokens& tokens, int i);
Node* build_tree(const ArrayOfTokens& tokens);

class TreePrinter {
public:
    void print(Node*);
    void print(Node* node, int parent);
private:
    int n;
};

// Assembler code

struct Symbol {
    string name;
    void* ptr;

    Symbol(symbol_t s): name(s.name), ptr(s.pointer) {}
};

vector<Symbol> convert_externs (const symbol_t*);

int32_t add(int32_t x, int32_t y);
int32_t sub(int32_t x, int32_t y);
int32_t mul(int32_t x, int32_t y);
int32_t unar_minus(int32_t x);

uint32_t instr_push(int reg);
uint32_t instr_pop(int reg);
uint32_t instr_bx_lr();
uint32_t instr_bx(int reg);
uint32_t instr_mov(int to, int from);
uint32_t instr_mov_pc(int to);
uint32_t instr_ldr(int to, int from_addr);
uint32_t instr_skip();
uint32_t instr_add_const(int reg, int num);

void write_instr(uint32_t instr, void*& buff);

void gen(Node* node, void*& buff, unordered_map<string, void*>& externs);
void gen_load_value(void*& buff, int32_t value);
void gen_const(NodeConst* node, void*& buff);
void gen_var(NodeVar* node, void*& buff, void* varptr);
void gen_callable(vector<Node*>, void*&, void* funcptr, unordered_map<string, void*>&);
void gen_oper(NodeOper* node, void*& buff, unordered_map<string, void*>& externs);
void gen_func(NodeFunc* node, void*& buff, unordered_map<string, void*>& externs);
void gen_epilogue(void*& buff);
void gen_prologue(void*& buff);

// STRUCT IMPLEMENTATION

struct TokenNumber : public Token {
    TokenType type() override { return TOKEN_NUMBER; }
    int c;
    explicit TokenNumber(int c): c(c) {}
};

struct TokenPunct : public Token {
    TokenType type() override { return TOKEN_PUNCT; }
    PunctType pt;
    explicit TokenPunct(PunctType bt): pt(bt) {}
};

struct TokenOper : public Token {
    TokenType type() override { return TOKEN_OPER; }
    OperType opt;
    explicit TokenOper(OperType opt): opt(opt) {}
};

struct TokenNamed : public Token {
    TokenType type() override { return TOKEN_NAMED; }
    string name;
    explicit TokenNamed(const string& name): name(name) {}
};

struct NodeConst : public Node {
    NodeType type() override { return NODE_CONST; }
    int c;
    explicit NodeConst(int c):
            c(c) {}
};

struct NodeVar : public Node {
    NodeType type() override { return NODE_VAR; }
    string name;
    explicit NodeVar(const string& name):
            name(name) {}
};

struct NodeFunc : public Node {
    NodeType type() override { return NODE_FUNC; }
    string name;
    vector <Node*> childs;
    NodeFunc(const string& name, const vector<Node*>& childs):
            name(name), childs(childs) {}
};

struct NodeOper : public Node {
    NodeType type() override { return NODE_OPER; }
    OperType oper_type;
    vector <Node*> childs;
    NodeOper(OperType opt, const vector<Node*>& childs):
            oper_type(opt), childs(childs) {}
};

// IMPLEMENTATION

void clear(Node* node) {
    if (node->type() == NODE_OPER) {
        auto casted = static_cast<NodeOper*>(node);
        for (Node* u : casted->childs)
            clear(u);
    }
    else if (node->type() == NODE_FUNC) {
        auto casted = static_cast<NodeFunc*>(node);
        for (Node* u : casted->childs)
            clear(u);
    }

    delete node;
}

ArrayOfTokens tokenate(const string& expr) {
    enum Condition {basic, start_number, start_word};
    Condition q = basic;
    ArrayOfTokens result;
    int cur_num = 0;
    string cur_word;

    for (char c: expr) {
        if (q == start_number) { // !!!!!!
            if (is_digit(c)) {
                cur_num *= 10;
                cur_num += c - '0';
                continue;
            }
            else {
                result.emplace_back(new TokenNumber(cur_num));
                q = basic;
            }
        }

        if (q == start_word) { // !!!!!!
            if (is_letter(c)) {
                cur_word.push_back(c);
                continue;
            }
            else {
                result.emplace_back(new TokenNamed(cur_word));
                q = basic;
            }
        }

        if (c == '+') {
            result.emplace_back(new TokenOper(PLUS));
        }
        else if (c == '-') {
            result.emplace_back(new TokenOper(MINUS));
        }
        else if (c == '*') {
            result.emplace_back(new TokenOper(MUL));
        }
        else if (c == '(') {
            result.emplace_back(new TokenPunct(OPEN));
        }
        else if (c == ')') {
            result.emplace_back(new TokenPunct(CLOSE));
        }
        else if (c == ',') {
            result.emplace_back(new TokenPunct(COMMA));
        }
        else if (is_letter(c)) {
            cur_word.clear();
            cur_word.push_back(c);
            q = start_word;
        }
        else if (is_digit(c)) {
            cur_num = c - '0';
            q = start_number;
        }
        else if (c != ' ') {
            throw runtime_error("tokenate: uncorrect symbol in expression");
        }
    }

    if (q == start_number)
        result.emplace_back(new TokenNumber(cur_num));

    if (q == start_word) {
        result.emplace_back(new TokenNamed(cur_word));
    }

    return result;
}

OperType get_oper_type(Token &token) {
    return dynamic_cast<TokenOper&>(token).opt;
}

PunctType get_punct_type(Token &token) {
    return dynamic_cast<TokenPunct&>(token).pt;
}

string get_name(Token& token) {
    return dynamic_cast<TokenNamed&>(token).name;
}

pair<Node*, int> scan_expr_plus(const ArrayOfTokens& tokens, int i) {
    if (i == tokens.size())
        throw runtime_error("scan_expr_plus: called with empty suffix");

    bool minus = false;
    if (tokens[i]->type() == TOKEN_OPER and get_oper_type(*tokens[i]) == MINUS) {
        i++;
        minus = true;
    }

    auto p = scan_expr_mult(tokens, i);
    Node* node = p.first;
    i = p.second;
    if (minus) {
        node = new NodeOper(UNAR_MINUS, {node});
    }

    while (i != tokens.size()) {
        if (tokens[i]->type() == TOKEN_OPER and
                (get_oper_type(*tokens[i]) == PLUS or
                 get_oper_type(*tokens[i]) == MINUS)) {
            OperType opt = get_oper_type(*tokens[i]);
            i++;
            p = scan_expr_mult(tokens, i);

            node = new NodeOper(opt, {node, p.first});
            i = p.second;
        }
        else {
            break;
        }
    }

    return make_pair(node, i);
};


pair<Node*, int> scan_expr_mult(const ArrayOfTokens& tokens, int i) {
    if (i == tokens.size())
        throw runtime_error("scan_expr_mult: called with empty suffix");

    auto p = scan_expr_sub(tokens, i);
    Node* node = p.first;
    i = p.second;

    while (i != tokens.size()) {
        if (tokens[i]->type() == TOKEN_OPER and get_oper_type(*tokens[i]) == MUL) {
            i++;
            p = scan_expr_sub(tokens, i);

            node = new NodeOper(MUL, {node, p.first});
            i = p.second;
        }
        else {
            break;
        }
    }

    return make_pair(node, i);
};

pair<Node*, int> scan_expr_sub(const ArrayOfTokens& tokens, int i) {
    if (i == tokens.size())
        throw runtime_error("scan_expr_sub: called with empty suffix");

    if (tokens[i]->type() == TOKEN_NUMBER) {
        auto token = dynamic_cast<TokenNumber*>(tokens[i].get());
        Node* node = new NodeConst(token->c);
        return make_pair(node, i+1);
    }
    else if (tokens[i]->type() == TOKEN_NAMED) {
        if (i+1 != tokens.size() and tokens[i+1]->type() == TOKEN_PUNCT and
                get_punct_type(*tokens[i + 1]) == OPEN) {
            string name = get_name(*tokens[i]);
            i += 2;
            vector<Node*> childs;
            while (true) {
                auto p = scan_expr_plus(tokens, i);
                childs.push_back(p.first);
                i = p.second;

                if (tokens[i]->type() == TOKEN_PUNCT) {
                    if (get_punct_type(*tokens[i]) == CLOSE) {
                        i++;
                        break;
                    }
                    else if (get_punct_type(*tokens[i]) == COMMA) {
                        i++;
                        continue;
                    }
                }
                throw runtime_error("scan_expr_sub: expeted , or )");
            }
            Node* node = new NodeFunc(name, childs);
            return make_pair(node, i);
        }
        else {
            Node* node = new NodeVar(get_name(*tokens[i]));
            return make_pair(node, i+1);
        }
    }
    else if (tokens[i]->type() == TOKEN_PUNCT and get_punct_type(*tokens[i]) == OPEN) {
        ++i;
        auto p = scan_expr_plus(tokens, i);
        i = p.second;
        if (i >= tokens.size() or
                tokens[i]->type() != TOKEN_PUNCT or
                get_punct_type(*tokens[i]) != CLOSE) {
            throw runtime_error("scan_expr_sub: expected )");
        }
        p.second++;
        return p;
    }
};

Node* build_tree(const ArrayOfTokens& tokens) {
    auto p = scan_expr_plus(tokens, 0);
    if (p.second != tokens.size())
        throw runtime_error("build_tree: invalid expression");

    return p.first;
}

void TreePrinter::print(Node* node) {
    n = 0;
    print(node, 0);
}

void TreePrinter::print(Node* node, int parent) {
    int v = n;
    n++;
    cout << parent << " ";

    if (node->type() == NODE_CONST) {
        auto node2 = dynamic_cast<NodeConst*>(node);
        cout << "num " << node2->c << endl;
    }
    else if (node->type() == NODE_OPER) {
        auto node2 = dynamic_cast<NodeOper*>(node);
        char c;
        if (node2->oper_type == PLUS)
            c = '+';
        if (node2->oper_type == MINUS)
            c = '-';
        if (node2->oper_type == MUL)
            c = '*';
        if (node2->oper_type == UNAR_MINUS)
            c = 'm';
        cout << "oper " << c << endl;
        for (Node* child : node2->childs)
            print(child, v);
    }
    else if (node->type() == NODE_VAR) {
        auto node2 = dynamic_cast<NodeVar*>(node);
        cout << "var " << node2->name << endl;
    }
    else if (node->type() == NODE_FUNC) {
        auto node2 = dynamic_cast<NodeFunc*>(node);
        cout << "func " << node2->name << endl;
        for (Node* child : node2->childs)
            print(child, v);
    }
}

uint32_t instr_push(int reg);
uint32_t instr_pop(int reg);
uint32_t instr_bx_lr();
uint32_t instr_bx(int reg);
uint32_t instr_mov(int to, int from);

uint32_t instr_mov_pc(int to) {
    return 0xe1a0000f + to*0x1000;
}

uint32_t instr_ldr(int to, int from_addr);
uint32_t instr_skip();
uint32_t instr_add_const(int reg, int num);

void write_instr(uint32_t instr, void*& buff) {
    uint32_t* bufint = (uint32_t*)buff;
    *bufint = instr;
    buff += 4;
}

void gen(Node* node, void*& buff, unordered_map<string, void*>& externs) {
    if (node->type() == NODE_FUNC) {
        auto node2 = dynamic_cast<NodeFunc*>(node);
        return gen_func(node2, buff, externs);
    }
    else if (node->type() == NODE_VAR) {
        auto node2 = dynamic_cast<NodeVar*>(node);
        return gen_var(node2, buff, externs[node2->name]);
    }
    else if (node->type() == NODE_OPER) {
        auto node2 = dynamic_cast<NodeOper*>(node);
        return gen_oper(node2, buff, externs);
    }
    else if (node->type() == NODE_CONST) {
        auto node2 = dynamic_cast<NodeConst*>(node);
        return gen_const(node2, buff);
    }
}

/* mov r0, pc
 * add r0, 8
 * ldr r0, [r0]
 * skip
 * <NUM>
 */

void gen_load_value(void*& buff, int32_t value) {
    write_instr(instr_mov_pc(0), buff);
    write_instr(instr_add_const(0, 8), buff);
    write_instr(instr_ldr(0, 0), buff);
    write_instr(instr_skip(), buff);
    *static_cast<int32_t*>(buff) = value;
    buff += 4;
}

/* <GEN LOAD NUM>
 * push r0
 */

void gen_const(NodeConst* node, void*& buff) {
    gen_load_value(buff, node->c);
    write_instr(instr_push(0), buff);
}

/* <GEN LOAD PTR>
 * ldr r0, [r0]
 * push r0
 */

void gen_var(NodeVar* node, void*& buff, void* varptr) {
    gen_load_value(buff, (int32_t)varptr);
    write_instr(instr_ldr(0, 0), buff);
    write_instr(instr_push(0), buff);
}

/* <DO f1>
 * ...
 * <DO fk>
 * <GEN LOAD PTR>
 * mov r4, r0
 * pop rk
 * ...
 * pop r0
 * bx r4
 * push r0
 */


void gen_callable(vector<Node*> childs, void*& buff, void* funcptr,
                  unordered_map<string, void*>& externs) {
    for (int i = 0; i < childs.size(); ++i)
        gen(childs[i], buff, externs);

    gen_load_value(buff, (int32_t)funcptr);
    write_instr(instr_mov(4, 0), buff);

    for (int i = (int)childs.size() - 1; i >= 0; --i)
        write_instr(instr_pop(i), buff);

    write_instr(instr_bx(4), buff);
    write_instr(instr_push(0), buff);
}

void gen_func(NodeFunc* node, void*& buff, unordered_map<string, void*>& externs) {
    gen_callable(node->childs, buff, externs[node->name], externs);
}

void gen_oper(NodeOper* node, void*& buff, unordered_map<string, void*>& externs) {
    if (node->oper_type == PLUS) {
        gen_callable(node->childs, buff, &add, externs);
    }
    else if (node->oper_type == MINUS) {
        gen_callable(node->childs, buff, &sub, externs);
    }
    else if (node->oper_type == MUL) {
        gen_callable(node->childs, buff, &mul, externs);
    }
    else if (node->oper_type == UNAR_MINUS) {
        gen_callable(node->childs, buff, &unar_minus, externs);
    }
}

/* pop r0
 * pop r7
 * ...
 * pop r4
 * bx lr
 */

void gen_epilogue(void*& buff) {
    write_instr(instr_pop(0), buff);
    write_instr(instr_pop(7), buff);
    write_instr(instr_pop(6), buff);
    write_instr(instr_pop(5), buff);
    write_instr(instr_pop(4), buff);
    write_instr(instr_bx_lr(), buff);
}

/* push r4
 * ...
 * push r7
 */

void gen_prologue(void*& buff) {
    write_instr(instr_push(4), buff);
    write_instr(instr_push(5), buff);
    write_instr(instr_push(6), buff);
    write_instr(instr_push(7), buff);
}

void jit_compile_expression_to_arm(
        const char* expression, const symbol_t* externs, void* out_buffer) {
    string expr(expression);
    auto tokens = tokenate(expr);
    auto tree = build_tree(tokens);

    vector<Symbol> ext_vec = convert_externs(externs);
    unordered_map<string, void*> ext_map;
    for (Symbol& s : ext_vec)
        ext_map[s.name] = s.ptr;

    gen_prologue(out_buffer);
    gen(tree, out_buffer, ext_map);
    gen_epilogue(out_buffer);
}

int main() {
    printf("%x\n", instr_mov_pc(2));
};
