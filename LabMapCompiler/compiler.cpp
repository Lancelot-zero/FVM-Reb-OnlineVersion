#include "compiler_defs.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <io.h>

using namespace std;

// ============================================================
// Token 定义
// ============================================================
enum TokenType {
    TK_EOF,
    TK_ERROR,
    TK_IDENT,
    TK_INT,
    TK_FLOAT,
    TK_STRING,
    TK_LBRACE, TK_RBRACE,
    TK_LPAREN, TK_RPAREN,
    TK_ASSIGN, TK_COMMA,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_MOD,
    TK_GT, TK_LT, TK_EQ, TK_NEQ, TK_GTE, TK_LTE,
    TK_ANDAND, TK_OROR,
};

static const char* tk_name(TokenType t) {
    switch (t) {
        case TK_EOF: return "EOF";
        case TK_ERROR: return "invalid character";
        case TK_IDENT: return "identifier";
        case TK_INT: return "integer";
        case TK_FLOAT: return "float";
        case TK_STRING: return "string";
        case TK_LBRACE: return "{"; case TK_RBRACE: return "}";
        case TK_LPAREN: return "("; case TK_RPAREN: return ")";
        case TK_ASSIGN: return "="; case TK_COMMA: return ",";
        case TK_PLUS: return "+"; case TK_MINUS: return "-";
        case TK_STAR: return "*"; case TK_SLASH: return "/";
        case TK_MOD: return "%";
        case TK_GT: return ">"; case TK_LT: return "<";
        case TK_EQ: return "=="; case TK_NEQ: return "!=";
        case TK_GTE: return ">="; case TK_LTE: return "<=";
        case TK_ANDAND: return "&&"; case TK_OROR: return "||";
        default: return "?";
    }
}

struct Token {
    TokenType type = TK_EOF;
    string str_val;
    int int_val = 0;
    float float_val = 0;
    int line = 0;
};

// ============================================================
// 字符串池
// ============================================================
struct StringPool {
    vector<string> strs;
    int add(const string& s) {
        for (int i = 0; i < (int)strs.size(); i++)
            if (strs[i] == s) return i;
        strs.push_back(s);
        return (int)strs.size() - 1;
    }
    int size() const { return (int)strs.size(); }
};

// ============================================================
// 变量表 — 所有变量分配内存槽位
// ============================================================
struct VarTable {
    map<string, int> name_to_slot;
    int next_slot = 0;

    int alloc(const string& name) {
        auto it = name_to_slot.find(name);
        if (it != name_to_slot.end()) return it->second;
        int s = next_slot++;
        name_to_slot[name] = s;
        return s;
    }
    int get(const string& name) const {
        auto it = name_to_slot.find(name);
        return (it != name_to_slot.end()) ? it->second : -1;
    }
    int count() const { return next_slot; }
};

// ============================================================
// ByteBuf — 指令缓冲区
// ============================================================
struct ByteBuf {
    vector<uint8_t> buf;
    void u8(uint8_t v)  { buf.push_back(v); }
    void u16(uint16_t v) {
        buf.push_back((uint8_t)(v & 0xFF));
        buf.push_back((uint8_t)((v >> 8) & 0xFF));
    }
    void s32(int32_t v) {
        buf.push_back((uint8_t)(v & 0xFF));
        buf.push_back((uint8_t)((v >> 8) & 0xFF));
        buf.push_back((uint8_t)((v >> 16) & 0xFF));
        buf.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    int tell() const { return (int)buf.size(); }
    void patch_s32(int offset, int32_t val) {
        buf[offset]     = (uint8_t)(val & 0xFF);
        buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
        buf[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
        buf[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
    }
};

// 语句级临时槽位作用域：语句结束时回收临时槽位（临时值不跨语句存活）
struct TempScope {
    int& base;
    int saved;
    TempScope(int& b) : base(b), saved(b) {}
    ~TempScope() { base = saved; }
};

// ============================================================
// 编译器
// ============================================================
class Compiler {
public:
    Compiler(const string& src) : src_(src) {}

    bool compile(string& err_msg) {
        // ---- 第一遍：收集字符串/变量，校验结构，检测常量 ----
        // 源码总行数（行号表用它兜底：块末尾回放/内联那段会跑到文件外，行号是假的，不记）
        total_lines_ = 1;
        for (char _c : src_) { if (_c == '\n') total_lines_++; }
        src_copy_ = src_;
        // 跳过 UTF-8 BOM（部分 Windows 编辑器保存时自动添加）
        if (src_copy_.size() >= 3 && (uint8_t)src_copy_[0] == 0xEF &&
            (uint8_t)src_copy_[1] == 0xBB && (uint8_t)src_copy_[2] == 0xBF)
            src_copy_.erase(0, 3);
        pos_ = 0; line_ = 1; next();
        if (!collect()) { err_msg = first_err_; return false; }

        // ---- 第二遍：生成字节码 ----
        pos_ = 0; line_ = 1; next();
        if (!generate()) { err_msg = first_err_; return false; }

        return true;
    }

    const StringPool& strings() const { return strings_; }
    const vector<pair<int, vector<uint8_t>>>& blocks() const { return blocks_; }
    const vector<pair<int, vector<pair<int,int>>>>& block_lines() const { return block_lines_; }
    const vector<string>& const_names() const { return const_names_; }
    int literal_count() const { return (int)literal_pool_.size(); }

private:
    string src_;
    string src_copy_;
    int pos_ = 0;
    int line_ = 1;
    Token cur_;

    // ---- 错误机制：记录第一个错误，编译最终失败 ----
    bool failed_ = false;
    string first_err_;
    void error(int line, const string& msg) {
        if (!failed_) { failed_ = true; first_err_ = "Line " + to_string(line) + ": " + msg; }
        cerr << "Line " << line << ": " << msg << endl;
    }

    StringPool strings_;
    VarTable vars_;
    vector<pair<int, vector<uint8_t>>> blocks_;
    ByteBuf* cur_buf_ = nullptr;
    // 行号表：gen_statement 每条语句记一次 (字节偏移, 源行号)，gen_block 收工时按块存进 block_lines_。
    // 编译成功后由 write_binary 写成同名 .lines（侧挂文件，不改 bin 格式；VM 端只在报错时才读）
    bool                                     rec_lines_ = false;
    vector<pair<int,int>>                    cur_lines_;
    vector<pair<int, vector<pair<int,int>>>> block_lines_;
    int                                      total_lines_ = 0;   // 源码总行数（行号表兜底用）
    bool                                     want_lines_ = true; // 是否把行号表打进 bin（--no-lines 关闭）
public:
    void set_lines(bool v) { want_lines_ = v; }
private:
    int temp_base_ = 0;        // 临时槽位起点（= 用户变量数）
    map<int, int> slot_type_;     // 槽位 → 类型 (MEM_INT/MEM_FLOAT/MEM_STRING)
    map<int, string> slot_str_;   // 槽位 → 字面量字符串值（仅字面量槽位有）
    int paren_ = 0;               // 第一遍收集时的括号层级（参数/条件边界判定）；表达式一律单行，不允许跨行
    int loop_depth_ = 0;          // 循环嵌套层级（第一遍 break/continue 的“在循环内”校验）

    // ---- 常量检测：同一变量 ≥2 处被赋完全相同且纯字面量的值 → 常量 ----
    //   · 单个字面量（可带负号）→ 常量传播：名字直接替换成池槽，不占变量槽
    //   · 复合字面量表达式（如 5+5）→ 保留变量槽，在 _VM_CONST_INIT 中 COPY 一次
    struct DefSite {
        string canon;         // 表达式 token 序列指纹
        bool literal_only;    // 是否纯字面量（无变量/函数调用）
        int type;             // MEM_INT/MEM_FLOAT/MEM_STRING
        bool single_lit;      // 表达式是否为单个（可负）字面量
        string lit_key;       // single_lit 时对应的字面量池键
    };
    map<string, vector<DefSite>> defs_;
    set<string> consts_;                 // 复合字面量常量 → 变量槽 + COPY
    map<string, string> const_alias_;    // 单字面量常量 → 池键（引用处直接替换）
    map<string, int> const_type_;
    map<string, bool> const_emitted_;
    vector<string> const_names_;
    ByteBuf const_buf_;       // _VM_CONST_INIT 块字节码（复合常量 COPY 部分，最后插到 blocks_[0]）
    ByteBuf scratch_buf_;     // 常量重复定义点丢弃字节码用

    // ---- while 循环上下文栈：break/continue 跳转指令的修补点 ----
    struct LoopCtx {
        int start_ip;               // 条件字节码起点（回跳与 continue 目标）
        vector<int> break_patches;  // 各 break 处 OP_JMP 操作数偏移
        vector<int> cont_patches;   // 各 continue 处 OP_JMP 操作数偏移
    };
    vector<LoopCtx> loops_;

    // ---- 自定义块 _DEFINE_BLOCK_xxx ----
    //   只有内部块（_OBJECT_* / _VM_*）能调用；调用处把被调块的字节码内联到本块末尾，
    //   调用点跳到拷贝开头，拷贝执行完（或 exit）再跳回调用点的下一条。
    struct LexState { int pos; int line; Token cur; };
    struct CustomDef { LexState def; int line; };
    map<string, CustomDef> custom_defs_;   // 名字 → 定义点词法现场（第一遍记录，第二遍回放展开）
    int  collect_custom_depth_ = 0;        // 第一遍：正在收集自定义块体的层数（>0 时禁止再调用块）
    bool in_custom_ = false;               // 第二遍：正在展开自定义块体
    int  custom_ret_ip_ = 0;               // 第二遍：展开中 exit/末尾跳回的地址
    struct PendingCall { string name; int patch; int ret_ip; int line; };
    vector<PendingCall> pending_calls_;    // 当前块里待展开的调用（块生成完统一内联）

    LexState lex_save() const { return LexState{pos_, line_, cur_}; }
    void lex_restore(const LexState& s) { pos_ = s.pos; line_ = s.line; cur_ = s.cur; }

    // ---- 字面量池：每个不同的裸字面量分配一个常驻槽，在 _VM_CONST_INIT 中统一初始化，
    //      之后任何引用直接传池槽（VM 参数槽只读前提） ----
    // 键按内容（与字符串池索引无关，避免两遍扫描入池顺序不同导致索引错位）
    struct PoolEntry {
        string key;           // "i:<值>" / "f:<浮点位>" / "s:<字符串内容>"
        int type;             // MEM_INT/MEM_FLOAT/MEM_STRING
        int32_t bits;         // int 值 / float 位
        string str_val;       // MEM_STRING: 内容（供字面量校验与发射）
    };
    vector<PoolEntry> literal_pool_;   // 按首次出现顺序
    set<string> literal_keys_;
    map<string, int> literal_slot_;    // key → 常驻槽号（第二遍生成前分配）

    void pool_add(const string& key, int type, int32_t bits, const string& str_val) {
        if (literal_keys_.count(key)) return;
        literal_keys_.insert(key);
        literal_pool_.push_back({key, type, bits, str_val});
    }
    int lit_slot(const string& key) {
        auto it = literal_slot_.find(key);
        if (it == literal_slot_.end()) {
            error(cur_.line, "internal: literal pool miss '" + key + "'");
            return alloc_temp();
        }
        return it->second;
    }

    void set_type(int slot, int t) { slot_type_[slot] = t; }
    int  get_type(int slot) { auto it = slot_type_.find(slot); return (it != slot_type_.end()) ? it->second : MEM_INT; }
    void set_str(int slot, const string& s) { slot_str_[slot] = s; }
    bool get_str(int slot, string& out) { auto it = slot_str_.find(slot); if (it != slot_str_.end()) { out = it->second; return true; } return false; }
    bool is_num(int t) { return t == MEM_INT || t == MEM_FLOAT || t == PT_ANY; }
    int  infer_bin_type(int ta, int tb) {
        if (!is_num(ta) || !is_num(tb)) {
            error(cur_.line, "strings cannot participate in arithmetic");
            return MEM_INT;
        }
        return (ta == MEM_FLOAT || tb == MEM_FLOAT) ? MEM_FLOAT : MEM_INT;
    }
    bool is_keyword(const string& s) {
        return s == "if" || s == "elif" || s == "else" || s == "halt" || s == "exit" ||
               s == "while" || s == "break" || s == "continue";
    }

    // ========== Tokenizer ==========
    // 空白、注释；';' 视作回车（行号+1）
    void skip_ws_comment() {
        while (pos_ < (int)src_copy_.size()) {
            char c = src_copy_[pos_];
            if (c == ' ' || c == '\t' || c == '\r') { pos_++; continue; }
            if (c == '\n' || c == ';') { pos_++; line_++; continue; }
            if (c == '#' || (c == '/' && pos_ + 1 < (int)src_copy_.size() && src_copy_[pos_ + 1] == '/')) {
                while (pos_ < (int)src_copy_.size() && src_copy_[pos_] != '\n') pos_++;
                continue;
            }
            break;
        }
    }

    Token read_token() {
        skip_ws_comment();
        Token t;
        t.line = line_;
        if (pos_ >= (int)src_copy_.size()) { t.type = TK_EOF; return t; }

        char c = src_copy_[pos_];

        // 数字（手写解析，避免 atoi 溢出 UB / atof 的 locale 依赖）
        if (c >= '0' && c <= '9') {
            long long iv = 0;
            bool overflow = false;
            while (pos_ < (int)src_copy_.size() && src_copy_[pos_] >= '0' && src_copy_[pos_] <= '9') {
                iv = iv * 10 + (src_copy_[pos_] - '0');
                if (iv > 0x7FFFFFFFLL) overflow = true;
                pos_++;
            }
            if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '.') {
                pos_++;
                double frac = 0, scale = 1;
                while (pos_ < (int)src_copy_.size() && src_copy_[pos_] >= '0' && src_copy_[pos_] <= '9') {
                    frac = frac * 10 + (src_copy_[pos_] - '0');
                    scale *= 10;
                    pos_++;
                }
                if (overflow) error(t.line, "float literal out of range");
                t.type = TK_FLOAT;
                t.float_val = (float)((double)iv + frac / scale);
            } else {
                if (overflow) error(t.line, "integer literal out of range (max 2147483647)");
                t.type = TK_INT;
                t.int_val = (int)iv;
            }
            return t;
        }

        if (c == '"') {
            pos_++;
            string s;
            bool closed = false;
            while (pos_ < (int)src_copy_.size()) {
                char ch = src_copy_[pos_];
                if (ch == '"') { pos_++; closed = true; break; }
                if (ch == '\\' && pos_ + 1 < (int)src_copy_.size()) {
                    pos_++;
                    char esc = src_copy_[pos_];
                    if (esc == 'n') s += '\n';
                    else if (esc == 't') s += '\t';
                    else if (esc == '"') s += '"';
                    else if (esc == '\\') s += '\\';
                    else { s += '\\'; s += esc; }
                    pos_++;
                    continue;
                }
                s += ch;
                pos_++;
            }
            if (!closed) error(t.line, "unterminated string literal");
            t.type = TK_STRING;
            t.str_val = s;
            return t;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c & 0x80)) {
            int start = pos_;
            while (pos_ < (int)src_copy_.size()) {
                char ch = src_copy_[pos_];
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || (ch & 0x80)) pos_++;
                else break;
            }
            t.type = TK_IDENT;
            t.str_val = src_copy_.substr(start, pos_ - start);
            return t;
        }

        pos_++;
        switch (c) {
            case '{': t.type = TK_LBRACE;  return t;
            case '}': t.type = TK_RBRACE;  return t;
            case '(': t.type = TK_LPAREN;  return t;
            case ')': t.type = TK_RPAREN;  return t;
            case '=':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '=') { pos_++; t.type = TK_EQ; }
                else t.type = TK_ASSIGN;
                return t;
            case ',': t.type = TK_COMMA;  return t;
            case '+': t.type = TK_PLUS;   return t;
            case '-': t.type = TK_MINUS;  return t;
            case '*': t.type = TK_STAR;   return t;
            case '/': t.type = TK_SLASH;  return t;
            case '%': t.type = TK_MOD;    return t;
            case '>':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '=') { pos_++; t.type = TK_GTE; }
                else t.type = TK_GT;
                return t;
            case '<':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '=') { pos_++; t.type = TK_LTE; }
                else t.type = TK_LT;
                return t;
            case '!':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '=') { pos_++; t.type = TK_NEQ; }
                else {
                    t.type = TK_ERROR;
                    error(t.line, "unexpected '!' (this language has no logical-not; use == 0 / != 0)");
                }
                return t;
            case '&':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '&') { pos_++; t.type = TK_ANDAND; }
                else {
                    t.type = TK_ERROR;
                    error(t.line, "unexpected '&' (did you mean '&&'?)");
                }
                return t;
            case '|':
                if (pos_ < (int)src_copy_.size() && src_copy_[pos_] == '|') { pos_++; t.type = TK_OROR; }
                else {
                    t.type = TK_ERROR;
                    error(t.line, "unexpected '|' (did you mean '||'?)");
                }
                return t;
            default:
                t.type = TK_ERROR;
                error(t.line, string("unexpected character '") + c + "'");
                return t;
        }
    }

    Token next() { Token t = cur_; cur_ = read_token(); return t; }
    bool check(TokenType t) { return cur_.type == t; }
    bool match(TokenType t) { if (check(t)) { next(); return true; } return false; }
    bool expect(TokenType t, const char* what) {
        if (check(t)) { next(); return true; }
        error(cur_.line, string("expected ") + what + ", got " + tk_name(cur_.type));
        return false;
    }

    // 表达式必须单行：只有同一行才允许继续
    bool expr_can_continue(int start_line) {
        return cur_.line == start_line;
    }

    // 语句结束：同行不得有多余 token
    void check_eol(int start_line) {
        if (check(TK_EOF) || check(TK_RBRACE)) return;
        if (cur_.line == start_line)
            error(cur_.line, "unexpected token after statement — one statement per line (or separate with ;)");
    }

    // ========== 临时槽位管理 ==========
    int alloc_temp() {
        int t = temp_base_++;
        slot_type_.erase(t);  // 槽位复用后清掉旧类型记录
        slot_str_.erase(t);
        return t;
    }

    // ========== 指令发射 ==========
    void emit_bin_op(int op, int dst, int a, int b) {
        cur_buf_->u8(op);
        cur_buf_->s32(dst);
        cur_buf_->s32(a);
        cur_buf_->s32(b);
    }

    void emit_call(int func_id, const vector<int>& args, int dst) {
        cur_buf_->u8(OP_CALL);
        cur_buf_->u16((uint16_t)func_id);
        cur_buf_->u8((uint8_t)args.size());
        cur_buf_->s32(dst);
        for (int a : args) {
            cur_buf_->s32(a);
        }
    }

    // ========== 参数校验（错误即编译失败） ==========
    void check_call_args(int func_id, const vector<int>& args, int line) {
        auto& def = FUNC_DEFS[func_id];
        int expected = def.param_count;

        // 变长函数不校验
        if (expected < 0) return;

        if ((int)args.size() != expected) {
            error(line, string(def.name) + " expects " + to_string(expected) +
                        " args, got " + to_string(args.size()));
            return;
        }

        if (expected == 0) return;
        auto& ptypes = def.param_types;
        for (int i = 0; i < expected; i++) {
            int expect_t = (i < (int)ptypes.size()) ? ptypes[i] : PT_INT;
            int actual_t = get_type(args[i]);
            if (expect_t == PT_ANY || actual_t == PT_ANY) continue;  // 参数接受任意 / 实参类型未知（GetProp、ArrayGet 返回值）
            if (actual_t != expect_t) {
                // int 实参可自动提升为 float
                if (!(expect_t == PT_FLOAT && actual_t == MEM_INT)) {
                    error(line, string(def.name) + " arg " + to_string(i + 1) + " expects " +
                                param_type_name(expect_t) + ", got " + param_type_name(actual_t));
                    continue;
                }
            }
            // 字符串参数：校验字面量值是否在合法集合中
            if (expect_t == PT_STRING) {
                string sval;
                if (!get_str(args[i], sval)) continue;
                const vector<const char*>* pset = nullptr;
                switch (func_id) {
                    case 0:                    // VM_BanCard → 卡片ID
                    case 6:                    // VM_SpawnPlant
                    case 90: pset = &VALID_CARD_IDS; break;   // VM_CannelBanCard
                    case 7:                     // VM_SpawnEnemy
                    case 8:  pset = &VALID_ENEMY_IDS; break;  // VM_SpawnBoss
                    case 10: pset = &VALID_OBJECT_NAMES; break; // VM_SpawnObject
                }
                if (pset && !str_in_set(sval, *pset)) {
                    error(line, string(def.name) + " arg " + to_string(i + 1) + " \"" + sval +
                                "\" is not a valid value");
                }
            }
        }
    }

    // ====================================================================
    // 第一遍：收集字符串 & 变量，校验结构，记录赋值点
    // ====================================================================
    bool collect() {
        while (!check(TK_EOF)) {
            if (check(TK_ERROR)) { next(); continue; }
            if (check(TK_RBRACE)) {
                error(cur_.line, "unexpected } at top level");
                next();
                continue;
            }
            if (check(TK_IDENT)) {
                string name = cur_.str_val;
                if (find_block(name) >= 0) { collect_block(); continue; }
                if (is_custom_block_name(name)) { collect_custom_block(name); continue; }
                if (name == "_VM_CONST_INIT")
                    error(cur_.line, "_VM_CONST_INIT is generated automatically — constants are detected from repeated identical definitions");
                else
                    error(cur_.line, "unexpected '" + name + "' at top level — only block definitions are allowed here");
                next();
                continue;
            }
            error(cur_.line, string("unexpected token at top level: ") + tk_name(cur_.type));
            next();
        }
        detect_consts();
        return !failed_;
    }

    void detect_consts() {
        for (auto& kv : defs_) {
            const string& name = kv.first;
            const vector<DefSite>& ds = kv.second;
            if (ds.size() < 2) continue;
            bool ok = true;
            for (auto& d : ds) {
                if (!d.literal_only || d.canon != ds[0].canon) { ok = false; break; }
            }
            if (!ok) continue;
            const_names_.push_back(name);
            if (ds[0].single_lit) {
                // 单个字面量 → 常量传播：引用处直接替换成池槽
                const_alias_[name] = ds[0].lit_key;
            } else {
                // 复合字面量 → 保留变量槽，_VM_CONST_INIT 中 COPY 一次
                consts_.insert(name);
                const_type_[name] = ds[0].type;
            }
        }
    }

    void collect_block() {
        strings_.add(cur_.str_val);
        next();
        if (!expect(TK_LBRACE, "{ after block name")) return;
        while (!check(TK_RBRACE) && !check(TK_EOF)) collect_statement();
        int brace_line = cur_.line;
        if (!match(TK_RBRACE)) { error(cur_.line, "missing } at end of block"); return; }
        if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
            error(cur_.line, "unexpected token after block — start a new line or use ;");
    }

    // 自定义块定义：记下定义点的词法现场（第二遍按调用次数回放展开），再按普通块收集一遍
    void collect_custom_block(const string& name) {
        LexState st = lex_save();          // cur_ = 块名
        int line = cur_.line;
        if (custom_defs_.count(name)) error(line, "duplicate custom block '" + name + "'");
        custom_defs_[name] = CustomDef{st, line};
        collect_custom_depth_++;
        collect_block();
        collect_custom_depth_--;
    }

    void collect_statement() {
        if (check(TK_EOF) || check(TK_RBRACE)) return;
        if (check(TK_ERROR)) { next(); return; }
        int start_line = cur_.line;
        if (check(TK_IDENT)) {
            string name = cur_.str_val;
            if (name == "halt" || name == "exit") { next(); check_eol(start_line); return; }  // exit 与 halt 同义
            if (name == "if") { collect_if(); return; }
            if (name == "while") { collect_while(); return; }
            if (name == "break") {
                if (loop_depth_ == 0) error(start_line, "'break' outside of a loop");
                next();
                check_eol(start_line);
                return;
            }
            if (name == "continue") {
                if (loop_depth_ == 0) error(start_line, "'continue' outside of a loop");
                next();
                check_eol(start_line);
                return;
            }
            if (name == "elif") { error(cur_.line, "'elif' without matching if"); next(); return; }
            if (name == "else") { error(cur_.line, "'else' without matching if"); next(); return; }
            if (find_block(name) >= 0) { error(cur_.line, "block '" + name + "' must be at top level"); next(); return; }
            next();
            if (check(TK_ASSIGN)) {
                if (find_func(name) >= 0)
                    error(start_line, "cannot assign to function name '" + name + "'");
                if (is_keyword(name))
                    error(start_line, "'" + name + "' is a keyword and cannot be assigned");
                if (is_custom_block_name(name))
                    error(start_line, "cannot assign to custom block name '" + name + "'");
                next();  // =
                vars_.alloc(name);
                string canon;
                bool lit = true;
                int et = MEM_INT;
                bool sl = false;
                string lk;
                collect_expr(&canon, &lit, &et, &sl, &lk);
                defs_[name].push_back({canon, lit, et, sl, lk});
                check_eol(start_line);
                return;
            }
            if (check(TK_LPAREN)) {
                if (is_custom_block_name(name)) {
                    if (collect_custom_depth_ > 0)
                        error(start_line, "custom block '" + name + "' cannot be called from another custom block");
                    next();  // (
                    paren_++;
                    collect_args();
                    paren_--;
                    if (!expect(TK_RPAREN, ")")) return;
                    check_eol(start_line);
                    return;
                }
                int fi = find_func(name);
                if (fi < 0) error(start_line, "unknown function '" + name + "'");
                next();  // (
                paren_++;
                collect_args();
                paren_--;
                if (!expect(TK_RPAREN, ")")) return;
                check_eol(start_line);
                return;
            }
            if (is_custom_block_name(name)) {
                error(start_line, "unexpected custom block name '" + name +
                                  "' — define it at top level, or call it as " + name + "()");
                return;
            }
            error(cur_.line, "unexpected identifier '" + name + "'");
            return;
        }
        error(cur_.line, string("unexpected token: ") + tk_name(cur_.type));
        next();
    }

    void collect_if() {  // cur_ = if 或 elif
        next();
        if (!expect(TK_LPAREN, "(")) return;
        paren_++;
        collect_expr(nullptr, nullptr, nullptr);
        paren_--;
        if (!expect(TK_RPAREN, ")")) return;
        if (!expect(TK_LBRACE, "{ (if body must use braces)")) return;
        while (!check(TK_RBRACE) && !check(TK_EOF)) collect_statement();
        int brace_line = cur_.line;
        if (!match(TK_RBRACE)) { error(cur_.line, "missing } in if body"); return; }

        if (check(TK_IDENT) && cur_.str_val == "elif") {
            collect_if();  // 递归处理 elif 及其后续链
            return;
        }
        if (check(TK_IDENT) && cur_.str_val == "else") {
            next();
            if (check(TK_IDENT) && cur_.str_val == "if") {
                collect_if();  // else if 等价于 elif
                return;
            }
            if (!expect(TK_LBRACE, "{ (else body must use braces)")) return;
            while (!check(TK_RBRACE) && !check(TK_EOF)) collect_statement();
            brace_line = cur_.line;
            if (!match(TK_RBRACE)) { error(cur_.line, "missing } in else body"); return;
            }
        }
        if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
            error(cur_.line, "unexpected token after if/else block — start a new line or use ;");
    }

    void collect_while() {
        next();
        if (!expect(TK_LPAREN, "(")) return;
        paren_++;
        collect_expr(nullptr, nullptr, nullptr);
        paren_--;
        if (!expect(TK_RPAREN, ")")) return;
        if (!expect(TK_LBRACE, "{ (while body must use braces)")) return;
        loop_depth_++;
        while (!check(TK_RBRACE) && !check(TK_EOF)) collect_statement();
        loop_depth_--;
        int brace_line = cur_.line;
        if (!match(TK_RBRACE)) { error(cur_.line, "missing } in while body"); return; }
        if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
            error(cur_.line, "unexpected token after while block — start a new line or use ;");
    }

    // 收集表达式 token：字符串入池、标识符入变量表、输出指纹/类型/单字面量信息
    void collect_expr(string* canon, bool* literal_only, int* expr_type,
                      bool* single_lit = nullptr, string* lit_key = nullptr) {
        int start_line = cur_.line;
        int entry = paren_;
        int etype = MEM_INT;
        bool expect_operand = true;  // 识别一元负号，把负数字面量也登记进池
        bool neg_pending = false;
        int tok_count = 0;           // 单字面量跟踪：首个 token 前的一元负号视为前缀
        bool single = true;
        bool neg_prefix = false;
        bool seen_val = false;
        string skey;
        while (!check(TK_EOF)) {
            TokenType t = cur_.type;
            if (t == TK_LBRACE || t == TK_RBRACE) { paren_ = entry; break; }
            if ((t == TK_RPAREN || t == TK_COMMA) && paren_ == entry) break;
            if (cur_.line != start_line) { paren_ = entry; break; }  // 表达式一律单行

            if (t == TK_STRING) {
                strings_.add(cur_.str_val);
                pool_add("s:" + cur_.str_val, MEM_STRING, 0, cur_.str_val);
                etype = MEM_STRING;
                if (!seen_val) {
                    seen_val = true;
                    skey = "s:" + cur_.str_val;
                } else single = false;
                if (canon) { *canon += 's'; *canon += cur_.str_val; *canon += ';'; }
            } else if (t == TK_INT) {
                pool_add("i:" + to_string(cur_.int_val), MEM_INT, cur_.int_val, "");
                if (neg_pending) pool_add("i:" + to_string(-cur_.int_val), MEM_INT, -cur_.int_val, "");
                if (!seen_val) {
                    seen_val = true;
                    skey = "i:" + to_string(neg_prefix ? -cur_.int_val : cur_.int_val);
                } else single = false;
                if (canon) { *canon += 'i'; *canon += to_string(cur_.int_val); *canon += ';'; }
            } else if (t == TK_FLOAT) {
                uint32_t bits;
                memcpy(&bits, &cur_.float_val, 4);
                pool_add("f:" + to_string(bits), MEM_FLOAT, (int32_t)bits, "");
                if (neg_pending) {
                    float nv = -cur_.float_val;
                    uint32_t nbits;
                    memcpy(&nbits, &nv, 4);
                    pool_add("f:" + to_string(nbits), MEM_FLOAT, (int32_t)nbits, "");
                }
                if (!seen_val) {
                    seen_val = true;
                    if (neg_prefix) {
                        float nv = -cur_.float_val;
                        uint32_t nbits;
                        memcpy(&nbits, &nv, 4);
                        skey = "f:" + to_string(nbits);
                    } else {
                        skey = "f:" + to_string(bits);
                    }
                } else single = false;
                etype = MEM_FLOAT;
                if (canon) {
                    *canon += 'f';
                    *canon += to_string(bits);
                    *canon += ';';
                }
            } else if (t == TK_IDENT) {
                string name = cur_.str_val;
                if (literal_only) *literal_only = false;
                if (find_func(name) < 0 && find_block(name) < 0 && !is_keyword(name) &&
                    !is_custom_block_name(name))
                    vars_.alloc(name);
                single = false;
                if (canon) { *canon += 'v'; *canon += name; *canon += ';'; }
            } else {
                if (t == TK_MINUS && tok_count == 0 && !neg_prefix) {
                    neg_prefix = true;  // 首个 token 的一元负号：单字面量前缀
                } else {
                    single = false;
                }
                if (canon) *canon += tk_name(t);
            }
            if (t == TK_LPAREN) paren_++;
            else if (t == TK_RPAREN) { if (paren_ > entry) paren_--; }
            // 一元负号跟踪（- 后面跟字面量时，负值也入池）
            if (t == TK_MINUS && expect_operand) neg_pending = true;
            else neg_pending = false;
            if (t == TK_INT || t == TK_FLOAT || t == TK_STRING || t == TK_IDENT || t == TK_RPAREN)
                expect_operand = false;
            else
                expect_operand = true;
            tok_count++;
            next();
        }
        if (expr_type) *expr_type = etype;
        if (single_lit) *single_lit = single && seen_val;
        if (lit_key && single && seen_val) *lit_key = skey;
    }

    void collect_args() {
        while (!check(TK_RPAREN) && !check(TK_EOF)) {
            collect_expr(nullptr, nullptr, nullptr);
            if (!match(TK_COMMA)) break;
        }
    }

    // ====================================================================
    // 第二遍：生成字节码
    // ====================================================================
    bool generate() {
        // 字面量池常驻槽：用户变量之后、临时槽之前
        int pool_base = vars_.count();
        for (int i = 0; i < (int)literal_pool_.size(); i++) {
            auto& e = literal_pool_[i];
            int slot = pool_base + i;
            literal_slot_[e.key] = slot;
            set_type(slot, e.type);
            if (e.type == MEM_STRING) set_str(slot, e.str_val);
        }
        temp_base_ = pool_base + (int)literal_pool_.size();
        if (temp_base_ > 4096) {
            error(1, "variables (" + to_string(pool_base) + ") + literal pool (" +
                      to_string(literal_pool_.size()) + ") exceed 4096 slots");
            return false;
        }

        const_buf_.buf.clear();
        scratch_buf_.buf.clear();

        // 预置常量槽位类型（块的生成顺序可能先于常量定义点）
        for (auto& n : consts_) {
            int slot = vars_.get(n);
            if (slot >= 0) set_type(slot, const_type_[n]);
        }

        while (!check(TK_EOF)) {
            while (match(TK_RBRACE)) {}
            if (check(TK_EOF)) break;
            if (check(TK_IDENT)) {
                string name = cur_.str_val;
                int bi = find_block(name);
                if (bi >= 0) { gen_block(bi); continue; }
                // 自定义块只在被调用处展开，这里跳过定义本身
                if (is_custom_block_name(name)) { skip_custom_block_tokens(); continue; }
            }
            next();
        }

        // _VM_CONST_INIT 永远作为第一个块输出（VM 读入即执行）：
        // 先初始化字面量池，再 COPY 命名常量（命名常量引用池槽）
        if (!const_buf_.buf.empty() || !literal_pool_.empty()) {
            ByteBuf init;
            for (int i = 0; i < (int)literal_pool_.size(); i++) {
                auto& e = literal_pool_[i];
                init.u8(OP_ASSIGN);
                init.s32(pool_base + i);
                init.u8((uint8_t)e.type);
                if (e.type == MEM_STRING) {
                    int si = strings_.add(e.str_val);  // 发射时才定索引，与引用方一致
                    init.u16((uint16_t)si);
                } else {
                    init.s32(e.bits);
                }
            }
            init.buf.insert(init.buf.end(), const_buf_.buf.begin(), const_buf_.buf.end());
            int si = strings_.add("_VM_CONST_INIT");
            blocks_.insert(blocks_.begin(), make_pair(si, init.buf));
        }

        // ── 行号表打包成伪块（方案 C，二进制）────────────────────
        // 块名 __LINES__，数据：u32 段数；每段 u32 块名下标 + u32 条数；每条 u32 字节偏移 + u32 行号。
        // 复用现成的"长度 + 名字下标 + 数据"块结构 → bin 格式零变化；
        // 老 VM 读新 bin 只会多一个永不执行的块，新 VM 读老 bin 没有这块（自动退回"块名+位置"）。
        if (want_lines_ && !block_lines_.empty()) {
            vector<uint8_t> lb;
            auto w32 = [&](uint32_t v) {
                lb.push_back((uint8_t)(v & 0xFF));
                lb.push_back((uint8_t)((v >> 8) & 0xFF));
                lb.push_back((uint8_t)((v >> 16) & 0xFF));
                lb.push_back((uint8_t)((v >> 24) & 0xFF));
            };
            w32((uint32_t)block_lines_.size());
            for (auto& bl : block_lines_) {
                w32((uint32_t)bl.first);
                w32((uint32_t)bl.second.size());
                for (auto& e : bl.second) { w32((uint32_t)e.first); w32((uint32_t)e.second); }
            }
            int li = strings_.add("__LINES__");
            blocks_.push_back({li, lb});
        }
        return !failed_;
    }

    void gen_block(int block_name_idx) {
        ByteBuf bb;
        cur_buf_ = &bb;
        temp_base_ = vars_.count() + (int)literal_pool_.size();  // 临时槽位在用户变量+字面量池之后

        next();                     // 块名
        match(TK_LBRACE);

        rec_lines_ = want_lines_;   // 本块开始记行号表
        cur_lines_.clear();
        while (!check(TK_RBRACE) && !check(TK_EOF)) {
            gen_statement();
        }
        rec_lines_ = false;
        int brace_line = cur_.line;
        match(TK_RBRACE);
        if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
            error(cur_.line, "unexpected token after block");

        // 自定义块体内联到本块末尾：写的是同一个 buffer（cur_buf_ 还是 bb），偏移天然对得上，
        // 所以这里也要记行号 —— 否则自定义块里报错会落到"内联之前最后一条普通语句"的行上
        rec_lines_ = want_lines_;
        gen_inline_calls();         // 本块调用的自定义块内联到本块末尾
        rec_lines_ = false;

        int name_str_idx = strings_.add(BLOCK_NAMES[block_name_idx]);
        blocks_.push_back({name_str_idx, bb.buf});
        // 同一偏移的连续多条（不产生字节码的语句）只留最后一条：
        // 查表规则是"起始偏移 ≤ 目标字节的最后一条"，留着前者会把行号指到一句压根没发射字节码的语句上
        vector<pair<int,int>> cleaned;
        for (auto& e : cur_lines_) {
            if (!cleaned.empty() && cleaned.back().first == e.first) cleaned.back() = e;
            else cleaned.push_back(e);
        }
        block_lines_.push_back({name_str_idx, cleaned});   // 本块的行号表（和 blocks_ 同序）
        cur_lines_.clear();
        cur_buf_ = nullptr;
    }

    // 跳过自定义块定义的 token（第二遍不生成它）
    void skip_custom_block_tokens() {
        next();                     // 块名
        if (!match(TK_LBRACE)) return;
        int depth = 1;
        while (depth > 0 && !check(TK_EOF)) {
            if (check(TK_LBRACE)) depth++;
            else if (check(TK_RBRACE)) depth--;
            next();
        }
    }

    // 调用点的跳转占位（第二遍记下，块生成完统一展开）
    void gen_custom_call(const string& name, int start_line) {
        next();                                        // (
        if (!expect(TK_RPAREN, ")")) return;           // 自定义块无参数
        if (in_custom_) {
            error(start_line, "custom block '" + name + "' cannot be called from another custom block");
            check_eol(start_line);
            return;
        }
        if (!custom_defs_.count(name)) {
            error(start_line, "unknown custom block '" + name + "'");
            check_eol(start_line);
            return;
        }
        cur_buf_->u8(OP_JMP);
        int patch = cur_buf_->tell();
        cur_buf_->s32(0);                              // 占位：拷贝开头，展开时回填
        pending_calls_.push_back({name, patch, cur_buf_->tell(), start_line});
        check_eol(start_line);
    }

    // 内部块末尾：为每个调用点拷一份被调块的字节码，前后各一条跳转
    //   调用点 --JMP--> 拷贝开头 ... 拷贝末尾/exit --JMP--> 调用点下一条
    void gen_inline_calls() {
        if (pending_calls_.empty()) return;
        vector<PendingCall> calls;
        calls.swap(pending_calls_);

        cur_buf_->u8(OP_JMP);                          // 块本体执行完跳过所有拷贝
        int skip_patch = cur_buf_->tell();
        cur_buf_->s32(0);

        for (auto& c : calls) {
            int start = cur_buf_->tell();
            cur_buf_->patch_s32(c.patch, start);
            gen_custom_body(c.name, c.ret_ip);
            cur_buf_->u8(OP_JMP);
            cur_buf_->s32(c.ret_ip);
        }
        cur_buf_->patch_s32(skip_patch, cur_buf_->tell());
    }

    // 回放某个自定义块的定义现场，生成一份拷贝（exit 跳 ret_ip）
    void gen_custom_body(const string& name, int ret_ip) {
        auto it = custom_defs_.find(name);
        if (it == custom_defs_.end()) return;

        LexState caller = lex_save();
        lex_restore(it->second.def);

        bool bak_in = in_custom_;
        int  bak_ret = custom_ret_ip_;
        vector<LoopCtx> bak_loops;
        bak_loops.swap(loops_);                        // 自定义块里的 break/continue 只认自己的循环
        in_custom_ = true;
        custom_ret_ip_ = ret_ip;

        next();                                        // 块名
        match(TK_LBRACE);
        while (!check(TK_RBRACE) && !check(TK_EOF)) gen_statement();
        match(TK_RBRACE);

        in_custom_ = bak_in;
        custom_ret_ip_ = bak_ret;
        bak_loops.swap(loops_);
        lex_restore(caller);
    }

    // ========== 语句 ==========
    void gen_statement() {
        if (check(TK_EOF) || check(TK_RBRACE)) return;
        if (check(TK_ERROR)) { next(); return; }
        TempScope ts(temp_base_);  // 语句级临时槽位回收
        int start_line = cur_.line;
        // 行号表：这条语句的字节码从当前偏移开始，源码在 start_line 行
        // 兜底：只记落在源码行数内的（正常不会越界，防的是编译器前后版本对不上的极端情况）
        if (rec_lines_ && cur_buf_ && start_line <= total_lines_)
            cur_lines_.push_back({(int)cur_buf_->buf.size(), start_line});

        if (check(TK_IDENT)) {
            string name = cur_.str_val;

            if (name == "halt" || name == "exit") {  // exit 与 halt 同义
                next();
                if (in_custom_) {                    // 自定义块：exit = 跳回调用处
                    cur_buf_->u8(OP_JMP);
                    cur_buf_->s32(custom_ret_ip_);
                } else {
                    cur_buf_->u8(OP_HALT);           // 内部块：exit = 结束整个块
                }
                check_eol(start_line);
                return;
            }
            if (name == "if") {
                gen_if();
                return;
            }
            if (name == "while") {
                gen_while();
                return;
            }
            if (name == "break") {
                gen_break(start_line);
                return;
            }
            if (name == "continue") {
                gen_continue(start_line);
                return;
            }
            if (name == "elif") {
                error(cur_.line, "'elif' without matching if");
                next();
                return;
            }
            if (name == "else") {
                error(cur_.line, "'else' without matching if");
                next();
                return;
            }
            if (find_block(name) >= 0) {
                error(cur_.line, "block '" + name + "' must be at top level");
                next();
                return;
            }

            next();  // 吃掉标识符
            if (check(TK_ASSIGN)) {
                // 变量 = 表达式
                int var_slot = vars_.get(name);
                if (var_slot < 0) {
                    error(start_line, "internal: unknown variable '" + name + "'");
                    next();
                    return;
                }
                next();  // 吃掉 =

                if (const_alias_.count(name)) {
                    // 单字面量常量：定义点不发射任何代码，只消费表达式
                    ByteBuf* saved = cur_buf_;
                    scratch_buf_.buf.clear();
                    cur_buf_ = &scratch_buf_;
                    gen_expr();
                    cur_buf_ = saved;
                    check_eol(start_line);
                    return;
                }

                if (consts_.count(name)) {
                    // 常量定义点：首个站点发射进 _VM_CONST_INIT，其余站点丢弃
                    ByteBuf* saved = cur_buf_;
                    if (const_emitted_[name]) {
                        scratch_buf_.buf.clear();
                        cur_buf_ = &scratch_buf_;
                    } else {
                        cur_buf_ = &const_buf_;
                    }
                    int src_slot = gen_expr();
                    cur_buf_ = saved;
                    if (!const_emitted_[name]) {
                        const_emitted_[name] = true;
                        if (src_slot != var_slot) {
                            const_buf_.u8(OP_COPY);
                            const_buf_.s32(var_slot);
                            const_buf_.s32(src_slot);
                        }
                    }
                    set_type(var_slot, get_type(src_slot));
                    check_eol(start_line);
                    return;
                }

                int src_slot = gen_expr();
                if (src_slot != var_slot) {
                    cur_buf_->u8(OP_COPY);
                    cur_buf_->s32(var_slot);
                    cur_buf_->s32(src_slot);
                }
                set_type(var_slot, get_type(src_slot));
                check_eol(start_line);
                return;
            }
            if (check(TK_LPAREN)) {
                if (is_custom_block_name(name)) { gen_custom_call(name, start_line); return; }
                // 函数调用（语句级，丢弃返回值）
                int fid = find_func(name);
                if (fid < 0) { error(start_line, "unknown function '" + name + "'"); next(); return; }
                next();  // 吃掉 (
                vector<int> args;
                gen_args(args);
                if (!expect(TK_RPAREN, ")")) return;
                check_call_args(fid, args, start_line);
                emit_call(fid, args, VOID_DST);
                check_eol(start_line);
                return;
            }
            error(cur_.line, "unexpected identifier '" + name + "'");
            return;
        }
        error(cur_.line, string("unexpected token: ") + tk_name(cur_.type));
        next();
    }

    // ========== if / elif 链 ==========
    void gen_if() {  // cur_ = if 或 elif
        next();
        if (!expect(TK_LPAREN, "(")) return;
        int cond_slot = gen_expr(); // 条件
        if (!expect(TK_RPAREN, ")")) return;
        if (!expect(TK_LBRACE, "{ (if body must use braces)")) return;

        cur_buf_->u8(OP_IF);
        cur_buf_->s32(cond_slot);
        int true_patch  = cur_buf_->tell(); cur_buf_->s32(0);
        int false_patch = cur_buf_->tell(); cur_buf_->s32(0);
        int ip_true = cur_buf_->tell();  // true 分支紧跟其后

        while (!check(TK_RBRACE) && !check(TK_EOF)) gen_statement();
        int brace_line = cur_.line;
        if (!match(TK_RBRACE)) { error(cur_.line, "missing } in if body"); return; }

        // 后续有 elif / else 时，true 分支末尾跳过其余分支
        bool has_chain = (check(TK_IDENT) &&
                          (cur_.str_val == "elif" || cur_.str_val == "else"));
        int jmp_patch = -1;
        if (has_chain) {
            cur_buf_->u8(OP_JMP);
            jmp_patch = cur_buf_->tell();
            cur_buf_->s32(0);
        }

        int ip_false;
        if (check(TK_IDENT) && cur_.str_val == "elif") {
            // elif 递归处理（其内部自行处理后续链与行尾校验）
            ip_false = cur_buf_->tell();
            gen_if();
        } else if (check(TK_IDENT) && cur_.str_val == "else") {
            ip_false = cur_buf_->tell();
            next();  // else
            if (check(TK_IDENT) && cur_.str_val == "if") {
                gen_if();  // else if 等价于 elif
            } else {
                if (!expect(TK_LBRACE, "{ (else body must use braces)")) return;
                while (!check(TK_RBRACE) && !check(TK_EOF)) gen_statement();
                brace_line = cur_.line;
                if (!match(TK_RBRACE)) { error(cur_.line, "missing } in else body"); return; }
                if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
                    error(cur_.line, "unexpected token after if/else block");
            }
        } else {
            ip_false = cur_buf_->tell();
            if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
                error(cur_.line, "unexpected token after if/else block");
        }

        cur_buf_->patch_s32(true_patch, ip_true);
        cur_buf_->patch_s32(false_patch, ip_false);
        if (jmp_patch >= 0) {
            cur_buf_->patch_s32(jmp_patch, cur_buf_->tell());
        }
    }

    // ========== while 循环（含 break/continue） ==========
    void gen_while() {
        next();
        if (!expect(TK_LPAREN, "(")) return;

        // 条件字节码在循环内部：每次迭代重新求值（回跳与 continue 均指向这里）。
        // 条件为裸变量/池槽时无字节码，loop_start 即 OP_IF 本身，语义同样正确。
        int loop_start = cur_buf_->tell();
        int cond_slot = gen_expr();
        if (!expect(TK_RPAREN, ")")) return;
        if (!expect(TK_LBRACE, "{ (while body must use braces)")) return;

        cur_buf_->u8(OP_IF);
        cur_buf_->s32(cond_slot);
        int body_patch = cur_buf_->tell(); cur_buf_->s32(0);
        int end_patch  = cur_buf_->tell(); cur_buf_->s32(0);
        int ip_body = cur_buf_->tell();  // 条件为真时进入循环体

        LoopCtx ctx;
        ctx.start_ip = loop_start;
        loops_.push_back(ctx);

        while (!check(TK_RBRACE) && !check(TK_EOF)) gen_statement();
        int brace_line = cur_.line;
        if (!match(TK_RBRACE)) {
            loops_.pop_back();
            error(cur_.line, "missing } in while body");
            return;
        }

        // 循环尾无条件跳回条件重算
        cur_buf_->u8(OP_JMP);
        cur_buf_->s32(loop_start);

        int ip_end = cur_buf_->tell();  // 循环出口（break 目标）
        cur_buf_->patch_s32(body_patch, ip_body);
        cur_buf_->patch_s32(end_patch, ip_end);

        // 修补本层循环体内记录的 break / continue（嵌套循环已在其内部自行修补并出栈）
        LoopCtx done = loops_.back();
        loops_.pop_back();
        for (int p : done.break_patches) cur_buf_->patch_s32(p, ip_end);
        for (int p : done.cont_patches)  cur_buf_->patch_s32(p, loop_start);

        if (!check(TK_EOF) && !check(TK_RBRACE) && cur_.line == brace_line)
            error(cur_.line, "unexpected token after while block — start a new line or use ;");
    }

    void gen_break(int start_line) {
        if (loops_.empty()) {
            error(start_line, "'break' outside of a loop");
            next();
            return;
        }
        cur_buf_->u8(OP_JMP);
        loops_.back().break_patches.push_back(cur_buf_->tell());
        cur_buf_->s32(0);  // 占位，循环编译结束时回填出口地址
        next();
        check_eol(start_line);
    }

    void gen_continue(int start_line) {
        if (loops_.empty()) {
            error(start_line, "'continue' outside of a loop");
            next();
            return;
        }
        cur_buf_->u8(OP_JMP);
        loops_.back().cont_patches.push_back(cur_buf_->tell());
        cur_buf_->s32(0);  // 占位，循环编译结束时回填条件重算地址
        next();
        check_eol(start_line);
    }

    // ========== 表达式 — 返回结果槽位 ==========
    int gen_expr()  { return gen_or(); }

    // 逻辑或（短路）：a || b → 1/0。左边为真就不算右边。
    int gen_or() {
        int start_line = cur_.line;
        int left = gen_and();
        while (check(TK_OROR)) {
            if (!expr_can_continue(start_line)) break;
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return left;
            }
            int temp = alloc_temp();
            set_type(temp, MEM_INT);
            cur_buf_->u8(OP_ASSIGN); cur_buf_->s32(temp); cur_buf_->u8(MEM_INT); cur_buf_->s32(0);
            cur_buf_->u8(OP_IF); cur_buf_->s32(left);
            int t1 = cur_buf_->tell(); cur_buf_->s32(0);   // 真 → 置 1
            int f1 = cur_buf_->tell(); cur_buf_->s32(0);   // 假 → 继续算右边
            int ip_next = cur_buf_->tell();
            int right = gen_and();
            cur_buf_->u8(OP_IF); cur_buf_->s32(right);
            int t2 = cur_buf_->tell(); cur_buf_->s32(0);   // 真 → 置 1
            int f2 = cur_buf_->tell(); cur_buf_->s32(0);   // 假 → 结束（保持 0）
            int ip_true = cur_buf_->tell();
            cur_buf_->u8(OP_ASSIGN); cur_buf_->s32(temp); cur_buf_->u8(MEM_INT); cur_buf_->s32(1);
            int ip_end = cur_buf_->tell();
            cur_buf_->patch_s32(t1, ip_true);
            cur_buf_->patch_s32(f1, ip_next);
            cur_buf_->patch_s32(t2, ip_true);
            cur_buf_->patch_s32(f2, ip_end);
            left = temp;
        }
        return left;
    }

    // 逻辑与（短路）：a && b → 1/0。左边为假就不算右边。
    int gen_and() {
        int start_line = cur_.line;
        int left = gen_cmp();
        while (check(TK_ANDAND)) {
            if (!expr_can_continue(start_line)) break;
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return left;
            }
            int temp = alloc_temp();
            set_type(temp, MEM_INT);
            cur_buf_->u8(OP_ASSIGN); cur_buf_->s32(temp); cur_buf_->u8(MEM_INT); cur_buf_->s32(0);
            cur_buf_->u8(OP_IF); cur_buf_->s32(left);
            int t1 = cur_buf_->tell(); cur_buf_->s32(0);   // 真 → 继续算右边
            int f1 = cur_buf_->tell(); cur_buf_->s32(0);   // 假 → 结束（保持 0）
            int ip_next = cur_buf_->tell();
            int right = gen_cmp();
            cur_buf_->u8(OP_IF); cur_buf_->s32(right);
            int t2 = cur_buf_->tell(); cur_buf_->s32(0);   // 真 → 置 1
            int f2 = cur_buf_->tell(); cur_buf_->s32(0);   // 假 → 结束（保持 0）
            int ip_true = cur_buf_->tell();
            cur_buf_->u8(OP_ASSIGN); cur_buf_->s32(temp); cur_buf_->u8(MEM_INT); cur_buf_->s32(1);
            int ip_end = cur_buf_->tell();
            cur_buf_->patch_s32(t1, ip_next);
            cur_buf_->patch_s32(f1, ip_end);
            cur_buf_->patch_s32(t2, ip_true);
            cur_buf_->patch_s32(f2, ip_end);
            left = temp;
        }
        return left;
    }

    // 比较
    int gen_cmp() {
        int start_line = cur_.line;
        int left = gen_add();
        while (true) {
            int op = -1;
            if (check(TK_EQ))  op = OP_EQ;
            else if (check(TK_NEQ)) op = OP_NEQ;
            else if (check(TK_GT))  op = OP_GT;
            else if (check(TK_GTE)) op = OP_GTE;
            else if (check(TK_LT))  op = OP_LT;
            else if (check(TK_LTE)) op = OP_LTE;
            else break;
            if (!expr_can_continue(start_line)) break;  // 换行且不在括号内 → 表达式结束
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return left;
            }
            int right = gen_add();
            int temp = alloc_temp();
            set_type(temp, MEM_INT);  // 比较结果总是 int
            emit_bin_op(op, temp, left, right);
            left = temp;
        }
        return left;
    }

    // 加减
    int gen_add() {
        int start_line = cur_.line;
        int left = gen_mul();
        while (check(TK_PLUS) || check(TK_MINUS)) {
            int op = check(TK_PLUS) ? OP_ADD : OP_SUB;
            if (!expr_can_continue(start_line)) break;
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return left;
            }
            int right = gen_mul();
            int t = infer_bin_type(get_type(left), get_type(right));
            int temp = alloc_temp();
            set_type(temp, t);
            emit_bin_op(op, temp, left, right);
            left = temp;
        }
        return left;
    }

    // 乘除模
    int gen_mul() {
        int start_line = cur_.line;
        int left = gen_primary();
        while (check(TK_STAR) || check(TK_SLASH) || check(TK_MOD)) {
            int op = 0;
            if (check(TK_STAR)) op = OP_MUL;
            else if (check(TK_SLASH)) op = OP_DIV;
            else op = OP_MOD;
            if (!expr_can_continue(start_line)) break;
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return left;
            }
            int right = gen_primary();
            int t = (op == OP_MOD) ? MEM_INT : infer_bin_type(get_type(left), get_type(right));
            int temp = alloc_temp();
            set_type(temp, t);
            emit_bin_op(op, temp, left, right);
            left = temp;
        }
        return left;
    }

    // 基础项：返回结果所在的内存槽位
    int gen_primary() {
        int start_line = cur_.line;
        if (check(TK_EOF)) {
            error(cur_.line, "unexpected end of file in expression");
            return alloc_temp();
        }
        if (check(TK_ERROR)) { next(); return alloc_temp(); }
        // 一元负号: -5 → 直接引用池槽(-5)；-变量/-表达式 → 0-变量
        if (check(TK_MINUS)) {
            next();
            if (!expr_can_continue(start_line)) {
                error(cur_.line, "expression cannot span lines");
                return alloc_temp();
            }
            if (check(TK_INT)) {
                int val = -cur_.int_val;
                next();
                return lit_slot("i:" + to_string(val));
            }
            if (check(TK_FLOAT)) {
                float val = -cur_.float_val;
                next();
                uint32_t bits; memcpy(&bits, &val, 4);   // 必须用 uint32_t：池里的键是 to_string(uint32_t)
                return lit_slot("f:" + to_string(bits));
            }
            int zero = alloc_temp(); set_type(zero, MEM_INT);
            cur_buf_->u8(OP_ASSIGN); cur_buf_->s32(zero); cur_buf_->u8(MEM_INT); cur_buf_->s32(0);
            int right = gen_primary();
            int t = alloc_temp(); set_type(t, get_type(right));
            emit_bin_op(OP_SUB, t, zero, right);
            return t;
        }
        if (check(TK_INT)) {
            int val = cur_.int_val;
            next();
            return lit_slot("i:" + to_string(val));
        }
        if (check(TK_FLOAT)) {
            float val = cur_.float_val;
            next();
            uint32_t bits; memcpy(&bits, &val, 4);   // 必须用 uint32_t：池里的键是 to_string(uint32_t)
            return lit_slot("f:" + to_string(bits));
        }
        if (check(TK_STRING)) {
            string str_val = cur_.str_val;
            strings_.add(str_val);
            next();
            return lit_slot("s:" + str_val);
        }
        if (check(TK_IDENT)) {
            string name = cur_.str_val;
            int fi = find_func(name);
            if (fi >= 0) {
                // 函数调用 → 返回值放临时槽
                next();
                if (!expect(TK_LPAREN, "(")) return alloc_temp();
                vector<int> args;
                gen_args(args);
                if (!expect(TK_RPAREN, ")")) return alloc_temp();
                check_call_args(fi, args, start_line);
                int t = alloc_temp();
                emit_call(fi, args, t);
                set_type(t, FUNC_DEFS[fi].return_type);
                return t;
            }
            if (is_keyword(name)) {
                error(cur_.line, "'" + name + "' is a keyword and cannot appear in an expression");
                next();
                return alloc_temp();
            }
            if (is_custom_block_name(name)) {
                error(cur_.line, "custom block '" + name + "' can only be called as a statement");
                next();
                return alloc_temp();
            }
            // 常量传播：单字面量常量直接替换成字面量池槽
            auto ai = const_alias_.find(name);
            if (ai != const_alias_.end()) {
                next();
                return literal_slot_[ai->second];
            }
            // 变量 → 直接返回其槽位
            int slot = vars_.get(name);
            if (slot < 0) {
                error(cur_.line, "internal: unknown variable '" + name + "'");
                next();
                return alloc_temp();
            }
            next();
            return slot;
        }
        if (match(TK_LPAREN)) {
            int result = gen_expr();
            expect(TK_RPAREN, ")");
            return result;
        }
        error(cur_.line, string("unexpected token in expression: ") + tk_name(cur_.type));
        next();
        return alloc_temp();
    }

    // ========== 参数列表 — 支持任意表达式 ==========
    void gen_args(vector<int>& args) {
        while (!check(TK_RPAREN) && !check(TK_EOF)) {
            args.push_back(gen_expr());
            if (!match(TK_COMMA)) break;
        }
    }

};

// ============================================================
// 二进制写入
// ============================================================
void write_binary(const string& path, const StringPool& strings,
                  const vector<pair<int, vector<uint8_t>>>& blocks,
                  const vector<pair<int, vector<pair<int,int>>>>& block_lines) {
    ofstream out(path, ios::binary);
    if (!out) { cerr << "Error: cannot write file " << path << endl; return; }

    auto w_int = [&](int32_t v) {
        out.put((char)(v & 0xFF));
        out.put((char)((v >> 8) & 0xFF));
        out.put((char)((v >> 16) & 0xFF));
        out.put((char)((v >> 24) & 0xFF));
    };

    // 字符串池
    w_int(strings.size());
    for (int i = 0; i < strings.size(); i++) {
        const string& s = strings.strs[i];
        w_int((int32_t)s.size());
        out.write(s.c_str(), s.size());
    }

    // 块数据
    for (auto& blk : blocks) {
        int32_t bytecode_len = (int32_t)blk.second.size();
        w_int(bytecode_len);
        w_int(blk.first);
        out.write((const char*)blk.second.data(), blk.second.size());
    }

    out.close();

    cout << "Compiled: " << path << " (" << blocks.size() << " blocks, "
         << strings.size() << " strings)" << endl;
}

// ============================================================
// 辅助
// ============================================================
static string derive_output(const string& input) {
    size_t pos = input.rfind(".txt");
    if (pos != string::npos && pos == input.size() - 4)
        return input.substr(0, pos) + ".bin";
    return input + ".bin";
}

static bool g_lines = true;   // true = 把行号表作为 __LINES__ 伪块打进 bin（--no-lines 关闭）

static int compile_one(const string& src_path, const string& out_path) {    ifstream in(src_path);
    if (!in) {
        cerr << "Error: cannot read " << src_path << endl;
        return 1;
    }
    stringstream ss;
    ss << in.rdbuf();
    string src = ss.str();
    in.close();
    Compiler compiler(src);
    compiler.set_lines(g_lines);
    string err;
    if (!compiler.compile(err)) {
        cerr << "Compile error (" << src_path << "): " << err << endl;
        return 1;
    }
    const vector<string>& cn = compiler.const_names();
    if (!cn.empty()) {
        cout << "Constants -> _VM_CONST_INIT:";
        for (auto& n : cn) cout << " " << n;
        cout << endl;
    }
    if (compiler.literal_count() > 0)
        cout << "Literal pool: " << compiler.literal_count() << " values" << endl;
    write_binary(out_path, compiler.strings(), compiler.blocks(), compiler.block_lines());
    return 0;
}

// ============================================================
// 入口
// ============================================================
int main(int argc, char* argv[]) {
    // ---- 开关：--lines / --no-lines；其余按位置参数 ----
    vector<string> _pos;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--no-lines") { g_lines = false; continue; }
        if (a == "--lines")    { g_lines = true;  continue; }
        if (a.rfind("--", 0) == 0) continue;   // 其它未知开关忽略
        _pos.push_back(a);
    }
    // ---- 无参数：遍历当前目录下所有 .txt ----
    if (_pos.empty()) {
        int total = 0, ok = 0;
        _finddata_t fd;
        intptr_t hFind = _findfirst("*.txt", &fd);
        if (hFind != -1) {
            do {
                string name = fd.name;
                string out = derive_output(name);
                cout << "Compiling: " << name << " -> " << out << endl;
                int ret = compile_one(name, out);
                if (ret == 0) ok++;
                total++;
            } while (_findnext(hFind, &fd) == 0);
            _findclose(hFind);
        }
        if (total == 0) {
            cerr << "No .txt files in current directory" << endl;
            return 1;
        }
        cout << "Done: " << ok << "/" << total << " files" << endl;
        return (ok == total) ? 0 : 1;
    }

    // ---- 1 或 2 个位置参数 ----
    string in_path = _pos[0];
    string out_path = (_pos.size() >= 2) ? _pos[1] : derive_output(in_path);
    return compile_one(in_path, out_path);
}
