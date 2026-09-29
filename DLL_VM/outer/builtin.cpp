// ============================================================
// builtin.cpp — 内置类（42 条）[自举]
//
//   数学 / 类型判断 / 字符串 / 全局变量 / 坐标 / 随机 / 颜色
//   这些只用 VM 自己的数据（槽 + 字符串池 + 容器池）就能算完，不碰引擎。
//
//   ⚠ 字符串类（string_* / string / chr / ord / json_*）要**文本**：
//     从池里取文本 → 算 → 再 intern 回池。
//     也就是说这几条是"运行时碰文本"的例外（算完立刻变回池下标），别在别处学样。
//
//   语义真源：老代码 transformGML2txt/vm_builtin.cpp（+ vm_container.cpp 的 merge_colour）。
//   老代码里没有的几条（json_parse / string_length / string_copy / string_replace_all /
//   ord / real / string_starts_with / variable_global_set）在这里照 GML 语义补齐，
//   能解析 / 能算的常见形状写在各自函数上面。
//
//   本文件不往控制台打任何东西；出错只置错误码。
// ============================================================
#include "outer.h"
#include <cmath>
#include <cstdio>       // snprintf：把数字拼进字符串缓冲（不是往控制台打）
#include <cstdlib>      // rand / strtod

// ============================================================
// 本文件的小工具（static 在匿名命名空间里，不污染别处）
// ============================================================
namespace {

// 数字结果写槽：整数存 INT、否则存 FLOAT（和老的 mk_num 一个规矩）
//   槽宽度是 VM_FLOAT（现在 32 位），所以量程外 / 有小数部分的一律按 FLOAT 存。
Value mk_num_v(VM_FLOAT x) {
    if (std::floor(x) == x && x >= (VM_FLOAT)-2147483647.0 && x <= (VM_FLOAT)2147483647.0)
        return mk_int((VM_INT)x);
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}
void wr_num (Value* mem, int dst, VM_FLOAT x) { wr_v(mem, dst, mk_num_v(x)); }
void wr_bool(Value* mem, int dst, bool b)     { wr_i(mem, dst, b ? 1 : 0); }

// 返回值用的空串池下标。
//   为什么**不**进 const_str.h 的名单：这是函数返回的**空串值**，不是"名字"，
//   名单管的是代码要按名字引用的字符串；它也不该占一个 S_xxx 常量。
//   池里没有现成的空串条目（名单第 0 项 X(empty) 的文本是 "empty"，不是 ""），
//   所以在这儿 intern 一次、缓存住下标（intern_str 本来就按文本去重，等价于每次现调）。
VM_INT empty_str_sid() {
    static VM_INT sid = -1;
    if (sid < 0) sid = intern_str("");
    return sid;
}

// 取第 k 个实参当**文本**（复制一份，别拿着池里元素的引用去 intern：
//   intern 可能让 g_strings 扩容，老引用就悬了）。
bool arg_str(Value* mem, const uint8_t* args, int k, std::string& out) {
    Value v = arg_v(mem, args, k);
    if (v.t != MEM_STRING || !str_ok(v.v)) return false;
    out = str_at(v.v);
    return true;
}

// 码点 → UTF-8（chr 和 json 的 \uXXXX 都用它；cp == 0 不写，越界也不写）
void utf8_put(std::string& t, unsigned cp) {
    if (cp == 0) return;
    if (cp < 0x80)        t.push_back((char)cp);
    else if (cp < 0x800) {
        t.push_back((char)(0xC0 | (cp >> 6)));
        t.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        t.push_back((char)(0xE0 | (cp >> 12)));
        t.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        t.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        t.push_back((char)(0xF0 | (cp >> 18)));
        t.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        t.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        t.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

// 容器只读访问（json_stringify 用；容器池在内层，这里只看不拥有）
VM_INT cont_size(const Value& v) {
    if (!container_alive(v)) return 0;
    if (v.t == MEM_ARRAY) return (VM_INT)g_arr_pool[(size_t)v.v].size();
    if (v.t == MEM_LIST)  return (VM_INT)g_list_pool[(size_t)v.v].size();
    return 0;
}
Value cont_at(const Value& v, VM_INT k) {
    if (!container_alive(v)) return mk_undef();
    if (v.t == MEM_ARRAY && k >= 0 && k < (VM_INT)g_arr_pool[(size_t)v.v].size())
        return g_arr_pool[(size_t)v.v][(size_t)k];
    if (v.t == MEM_LIST && k >= 0 && k < (VM_INT)g_list_pool[(size_t)v.v].size())
        return g_list_pool[(size_t)v.v][(size_t)k];
    return mk_undef();
}

// ============================================================
// JSON 工具（json_stringify / json_parse 共用）
// ============================================================
static void json_esc(std::string& out, const std::string& s) {
    out += '"';
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')      { out += '\\'; out += (char)c; }
        else if (c == '\n')             out += "\\n";
        else if (c == '\r')             out += "\\r";
        else if (c == '\t')             out += "\\t";
        else if (c < 0x20)              { /* 其它控制字符：丢掉（JSON 里不好带） */ }
        else                            out += (char)c;
    }
    out += '"';
}
static void json_num(std::string& out, const Value& v) {
    char buf[64];
    if (v.t == MEM_FLOAT) snprintf(buf, sizeof(buf), "%.6g", (double)f_of(v.v));   // 槽是 32 位，6 位有效数字够
    else                  snprintf(buf, sizeof(buf), "%lld", (long long)v.v);
    out += buf;
}
// 容器嵌套有深度上限：真成环时也不至于把 C++ 栈爆掉
static void json_val(const Value& v, std::string& out, int depth) {
    if (depth > 16) { out += str_at(S_null); return; }
    switch (v.t) {
    case MEM_STRING:
        if (str_ok(v.v)) json_esc(out, str_at(v.v));
        else             out += "\"\"";
        return;
    case MEM_FLOAT:
    case MEM_INT:
        json_num(out, v); return;
    case MEM_UNDEFINED:
        out += str_at(S_null); return;
    case MEM_ARRAY:
    case MEM_LIST: {
        out += '[';
        VM_INT n = cont_size(v);
        for (VM_INT i = 0; i < n; i++) {
            if (i) out += ',';
            json_val(cont_at(v, i), out, depth + 1);
        }
        out += ']';
        return;
    }
    case MEM_MAP: {
        out += '{';
        if (container_alive(v)) {
            const std::map<Value, Value, ValueLess>& mp = g_map_pool[(size_t)v.v];
            bool first = true;
            for (std::map<Value, Value, ValueLess>::const_iterator it = mp.begin(); it != mp.end(); ++it) {
                if (!first) out += ',';
                first = false;
                // 键：字符串就转义，数字 / 其它原样（和老的 json_val 一致）
                if (it->first.t == MEM_STRING && str_ok(it->first.v))
                    json_esc(out, str_at(it->first.v));
                else
                    json_num(out, it->first);
                out += ':';
                json_val(it->second, out, depth + 1);
            }
        }
        out += '}';
        return;
    }
    default:                              // 未知类型（grid 等）→ null（和老的 default 一致）
        out += str_at(S_null); return;
    }
}

// ── 下面这些是 json_parse 的递归下降 ──
static void j_ws(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
}
static bool j_hex4(const std::string& s, size_t& i, unsigned& cp) {
    if (i + 4 > s.size()) return false;
    cp = 0;
    for (int k = 0; k < 4; k++) {
        char c = s[i + (size_t)k];
        unsigned d;
        if (c >= '0' && c <= '9')      d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else return false;
        cp = (cp << 4) | d;
    }
    i += 4;
    return true;
}
static bool j_string(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    i++;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return true;
        if (c != '\\') { out.push_back(c); continue; }
        if (i >= s.size()) return false;
        char e = s[i++];
        switch (e) {
        case '"':  out.push_back('"');  break;
        case '\\': out.push_back('\\'); break;
        case '/':  out.push_back('/');  break;
        case 'b':  out.push_back('\b'); break;
        case 'f':  out.push_back('\f'); break;
        case 'n':  out.push_back('\n'); break;
        case 'r':  out.push_back('\r'); break;
        case 't':  out.push_back('\t'); break;
        case 'u': {
            unsigned cp = 0;
            if (!j_hex4(s, i, cp)) return false;
            // 代理对：高代理后面紧跟 \uDC00..\uDFFF → 合成一个码点（UTF-16 的常见形状）
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                size_t save = i;
                i += 2;
                unsigned lo = 0;
                if (j_hex4(s, i, lo) && lo >= 0xDC00 && lo <= 0xDFFF)
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                else
                    i = save;                       // 不是合法低代理：退回，单个码点照写
            }
            utf8_put(out, cp);
            break;
        }
        default: return false;
        }
    }
    return false;                                   // 引号没闭合
}
static bool j_value(const std::string& s, size_t& i, Value& out, int depth) {
    if (depth > 32) return false;
    j_ws(s, i);
    if (i >= s.size()) return false;
    char c = s[i];

    if (c == '"') {                                 // 字符串
        std::string t;
        if (!j_string(s, i, t)) return false;
        out = mk_str_sid(intern_str(t));
        return true;
    }
    if (c == '[') {                                 // 数组 → MEM_ARRAY
        i++;
        VM_INT idx = arr_new();
        Value h; h.t = MEM_ARRAY; h.v = idx;
        j_ws(s, i);
        if (i < s.size() && s[i] == ']') { i++; out = h; return true; }
        while (true) {
            Value el;
            if (!j_value(s, i, el, depth + 1)) { release(h); return false; }
            // 元素归数组持有：retain 一份再放掉解析器手里那份（标量是空操作）
            if (allow_store(el, h)) { retain(el); g_arr_pool[(size_t)idx].push_back(el); }
            release(el);
            j_ws(s, i);
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == ']') { i++; out = h; return true; }
            release(h);
            return false;
        }
    }
    if (c == '{') {                                 // 对象 → MEM_MAP（结构体就是它）
        i++;
        VM_INT idx = map_new();
        Value h; h.t = MEM_MAP; h.v = idx;
        j_ws(s, i);
        if (i < s.size() && s[i] == '}') { i++; out = h; return true; }
        while (true) {
            j_ws(s, i);
            std::string key;
            if (!j_string(s, i, key)) { release(h); return false; }
            Value kv = mk_str_sid(intern_str(key));
            j_ws(s, i);
            if (i >= s.size() || s[i] != ':') { release(h); return false; }
            i++;
            Value val;
            if (!j_value(s, i, val, depth + 1)) { release(h); return false; }
            // 重复键：先放掉旧值，再放新值（键本身不带引用）
            std::map<Value, Value, ValueLess>& mp = g_map_pool[(size_t)idx];
            std::map<Value, Value, ValueLess>::iterator old = mp.find(kv);
            if (old != mp.end()) release(old->second);
            retain(val);
            mp[kv] = val;
            release(val);
            j_ws(s, i);
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == '}') { i++; out = h; return true; }
            release(h);
            return false;
        }
    }
    if (c == 't' && s.compare(i, 4, "true") == 0)  { i += 4; out = mk_int(1); return true; }
    if (c == 'f' && s.compare(i, 5, "false") == 0) { i += 5; out = mk_int(0); return true; }
    if (c == 'n' && s.compare(i, 4, "null") == 0)  { i += 4; out = mk_undef(); return true; }

    // 数字：扫出合法字符，交给 strtod（C 库只给 double，立刻收窄回 VM_FLOAT）
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
        size_t st = i;
        if (s[i] == '-' || s[i] == '+') i++;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' ||
                                s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) i++;
        std::string tok = s.substr(st, i - st);
        char* endp = NULL;
        double d = strtod(tok.c_str(), &endp);
        if (endp == tok.c_str() || *endp != '\0') return false;   // 不是合法数字
        out = mk_num_v((VM_FLOAT)d);
        return true;
    }
    return false;
}

} // namespace

// ============================================================
// 数学
// ============================================================
// 数值一律按 VM_FLOAT 算（槽就是 32 位浮点），写回去时整数存 INT、否则存 FLOAT
void floor(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, std::floor(arg_num(mem, args, 0)));
}
// GML 的 round 是"四舍六入五成双"（银行家舍入）：round(0.5)=0、round(1.5)=2、round(2.5)=2
void round(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT x = arg_num(mem, args, 0);
    VM_FLOAT f = std::floor(x);
    VM_FLOAT d = x - f;
    VM_FLOAT r;
    if (d > (VM_FLOAT)0.5)      r = f + (VM_FLOAT)1;
    else if (d < (VM_FLOAT)0.5) r = f;
    else                        r = (((long long)f % 2) == 0) ? f : f + (VM_FLOAT)1;
    wr_num(mem, dst, r);
}
void ceil(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, std::ceil(arg_num(mem, args, 0)));
}
void abs(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, std::fabs(arg_num(mem, args, 0)));
}
// max / min：GML 可以给多个参数
void max(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_num(mem, dst, (VM_FLOAT)0); return; }
    VM_FLOAT r = arg_num(mem, args, 0);
    for (int i = 1; i < argc; i++) { VM_FLOAT x = arg_num(mem, args, i); if (x > r) r = x; }
    wr_num(mem, dst, r);
}
void min(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_num(mem, dst, (VM_FLOAT)0); return; }
    VM_FLOAT r = arg_num(mem, args, 0);
    for (int i = 1; i < argc; i++) { VM_FLOAT x = arg_num(mem, args, i); if (x < r) r = x; }
    wr_num(mem, dst, r);
}
void clamp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT x = arg_num(mem, args, 0), lo = arg_num(mem, args, 1), hi = arg_num(mem, args, 2);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    wr_num(mem, dst, x);
}
void lerp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT a = arg_num(mem, args, 0), b = arg_num(mem, args, 1), t = arg_num(mem, args, 2);
    wr_num(mem, dst, a + (b - a) * t);
}

// ── 随机数：GM 用自己的 RNG，我们先用标准 rand()（宿主以后要换自己的流也容易）──
void random(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT n = arg_num(mem, args, 0);
    wr_num(mem, dst, ((VM_FLOAT)rand() / (VM_FLOAT)RAND_MAX) * n);
}
void random_range(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT a = arg_num(mem, args, 0), b = arg_num(mem, args, 1);
    if (a > b) { VM_FLOAT t = a; a = b; b = t; }
    wr_num(mem, dst, a + ((VM_FLOAT)rand() / (VM_FLOAT)RAND_MAX) * (b - a));
}
// irandom(n)：0..n 的整数（含两端）—— floor(random(n+1))，和 GM 一致
void irandom(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT n = arg_num(mem, args, 0);
    if (!(n > (VM_FLOAT)0)) { wr_num(mem, dst, (VM_FLOAT)0); return; }   // n<=0 / nan → 0
    VM_INT v = (VM_INT)(((VM_FLOAT)rand() / (VM_FLOAT)RAND_MAX) * (n + (VM_FLOAT)1));
    if (v > (VM_INT)n) v = (VM_INT)n;                                    // rand()==RAND_MAX 时防越界
    wr_num(mem, dst, (VM_FLOAT)v);
}
// irandom_range(a, b)：闭区间整数，a、b 反了也能用（跟 GML 一样）
void irandom_range(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT a = (VM_INT)arg_num(mem, args, 0), b = (VM_INT)arg_num(mem, args, 1);
    if (a > b) { VM_INT t = a; a = b; b = t; }
    VM_INT span = b - a + 1;
    if (span <= 0) { wr_i(mem, dst, a); return; }
    wr_i(mem, dst, a + (VM_INT)(rand() % span));
}

// ── 三角 / 向量：GML 里"角度"用度、"sin/cos"收弧度；y 轴朝下 ──
void sin(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, std::sin(arg_num(mem, args, 0)));
}
void cos(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, std::cos(arg_num(mem, args, 0)));
}
void degtorad(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(mem, dst, arg_num(mem, args, 0) * ((VM_FLOAT)3.14159265358979323846 / (VM_FLOAT)180.0));
}
void lengthdir_x(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT len = arg_num(mem, args, 0);
    VM_FLOAT dir = arg_num(mem, args, 1) * ((VM_FLOAT)3.14159265358979323846 / (VM_FLOAT)180.0);
    wr_num(mem, dst, len * std::cos(dir));
}
// GML 的 y 轴朝下：lengthdir_y = -len * sin(dir)
void lengthdir_y(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT len = arg_num(mem, args, 0);
    VM_FLOAT dir = arg_num(mem, args, 1) * ((VM_FLOAT)3.14159265358979323846 / (VM_FLOAT)180.0);
    wr_num(mem, dst, -len * std::sin(dir));
}
void point_direction(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT x1 = arg_num(mem, args, 0), y1 = arg_num(mem, args, 1);
    VM_FLOAT x2 = arg_num(mem, args, 2), y2 = arg_num(mem, args, 3);
    VM_FLOAT d = std::atan2(y1 - y2, x2 - x1) * ((VM_FLOAT)180.0 / (VM_FLOAT)3.14159265358979323846);
    if (d < (VM_FLOAT)0) d += (VM_FLOAT)360;
    wr_num(mem, dst, d);
}
void point_distance(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_FLOAT dx = arg_num(mem, args, 2) - arg_num(mem, args, 0);
    VM_FLOAT dy = arg_num(mem, args, 3) - arg_num(mem, args, 1);
    wr_num(mem, dst, std::sqrt(dx * dx + dy * dy));
}

// ============================================================
// 类型判断
//   注意 is_struct：我们这边结构体和 ds_map 是同一种（键值对池），分不开，
//   所以 is_struct 对 ds_map 也会返回真。项目里没见两者混用，先这么定。
// ============================================================
void is_real(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT t = rd_t(mem, argslot(args, 0));
    wr_bool(mem, dst, t == MEM_INT || t == MEM_FLOAT);
}
void is_string(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(mem, dst, rd_t(mem, argslot(args, 0)) == MEM_STRING);
}
void is_array(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(mem, dst, rd_t(mem, argslot(args, 0)) == MEM_ARRAY);
}
void is_struct(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(mem, dst, rd_t(mem, argslot(args, 0)) == MEM_MAP);
}
void is_undefined(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(mem, dst, rd_t(mem, argslot(args, 0)) == MEM_UNDEFINED);
}

// ============================================================
// 字符串（唯一允许运行时碰文本的一批）
// ============================================================
// string(x)：GML 里 real 转字符串保留 2 位小数（string(1.8) = "1.80"），整数不带小数点
//   ⚠ 格式化本身**不在这**：唯一实现在内层 value_to_text（core/pool.cpp），
//     解释器的 OP_ADD 拼串用的是同一份 —— 两边各写一套的话，
//     `"a" + x` 和 `"a" + string(x)` 迟早会给出两种文本。
void string(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    Value v = rd_v(mem, slot);
    if (v.t == MEM_STRING) { wr_v(mem, dst, v); return; }      // 本来就是字符串：原样转发（还是同一个池下标）
    wr_v(mem, dst, mk_str_sid(intern_str(value_to_text(v))));
}

// string_length(str)：**按字节**数（和 string_copy / string_pos / string_delete 的
//   下标语义对齐 —— 老代码这族全是按 std::string 的字节下标走的；池里存的就是 UTF-8 字节）
void string_length(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string t;
    if (!arg_str(mem, args, 0, t)) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, (VM_INT)t.size());
}

// string_copy(str, pos, count)：pos 从 **1** 起；pos<1 当 1，count<0 当 0，越界自动截断
void string_copy(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string t;
    if (!arg_str(mem, args, 0, t)) { wr_v(mem, dst, mk_str_sid(empty_str_sid())); return; }
    VM_INT pos = (VM_INT)arg_num(mem, args, 1);
    VM_INT cnt = (VM_INT)arg_num(mem, args, 2);
    if (pos < 1) pos = 1;
    if (cnt < 0) cnt = 0;
    size_t start = (size_t)(pos - 1);
    std::string o = (start >= t.size()) ? std::string() : t.substr(start, (size_t)cnt);
    wr_v(mem, dst, mk_str_sid(intern_str(o)));
}

// string_delete(str, pos, count)：从 1 起的 pos 开始删 count 个字符
void string_delete(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = arg_v(mem, args, 0);
    VM_INT pos = (VM_INT)arg_num(mem, args, 1), cnt = (VM_INT)arg_num(mem, args, 2);
    if (a.t != MEM_STRING || !str_ok(a.v)) { wr_v(mem, dst, a); return; }
    std::string t = str_at(a.v);
    VM_INT i = pos - 1;
    if (i < 0) i = 0;
    if (i < (VM_INT)t.size() && cnt > 0) t.erase((size_t)i, (size_t)cnt);
    wr_v(mem, dst, mk_str_sid(intern_str(t)));
}

// string_replace_all(str, find, rep)：把所有 find 换成 rep
//   find 是空串 → 原样返回（不然会无限插）
void string_replace_all(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string t, f, r;
    if (!arg_str(mem, args, 0, t)) { wr_v(mem, dst, arg_v(mem, args, 0)); return; }
    if (!arg_str(mem, args, 1, f) || !arg_str(mem, args, 2, r) || f.empty()) {
        wr_v(mem, dst, mk_str_sid(intern_str(t)));
        return;
    }
    size_t p = 0;
    while ((p = t.find(f, p)) != std::string::npos) {
        t.replace(p, f.size(), r);
        p += r.size();                    // 跳过刚插进去的，避免 rep 里含 find 时反复替换
    }
    wr_v(mem, dst, mk_str_sid(intern_str(t)));
}

// string_pos(substr, str)：子串位置（**1 起**，找不到 → 0）
void string_pos(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = arg_v(mem, args, 0), b = arg_v(mem, args, 1);
    if (a.t != MEM_STRING || b.t != MEM_STRING || !str_ok(a.v) || !str_ok(b.v)) { wr_num(mem, dst, (VM_FLOAT)0); return; }
    size_t p = str_at(b.v).find(str_at(a.v));
    wr_num(mem, dst, (p == std::string::npos) ? (VM_FLOAT)0 : (VM_FLOAT)(p + 1));
}

// string_starts_with(str, prefix)：前缀判断 → 1 / 0（GMS2 的布尔就是 0/1 的数）
void string_starts_with(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string t, p;
    if (!arg_str(mem, args, 0, t) || !arg_str(mem, args, 1, p)) { wr_bool(mem, dst, false); return; }
    wr_bool(mem, dst, t.size() >= p.size() && t.compare(0, p.size(), p) == 0);
}

// chr(n)：码点 → 单字符字符串（UTF-8 编码；n<=0 → 空串）
void chr(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT c = (VM_INT)arg_num(mem, args, 0);
    std::string t;
    if (c > 0) utf8_put(t, (unsigned)c);
    wr_v(mem, dst, mk_str_sid(intern_str(t)));
}

// ord(str)：首字符的码点（UTF-8 解码；空串 → 0）
//   和 chr 互逆：chr(ord(s)) == s 的首字符
void ord(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string t;
    if (!arg_str(mem, args, 0, t) || t.empty()) { wr_i(mem, dst, 0); return; }
    const unsigned char* p = (const unsigned char*)t.data();
    size_t n = t.size();
    unsigned c0 = p[0], cp = c0;
    if ((c0 & 0x80) == 0)            cp = c0;
    else if ((c0 & 0xE0) == 0xC0 && n >= 2) cp = ((c0 & 0x1Fu) << 6)  | (p[1] & 0x3Fu);
    else if ((c0 & 0xF0) == 0xE0 && n >= 3) cp = ((c0 & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
    else if ((c0 & 0xF8) == 0xF0 && n >= 4) cp = ((c0 & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu);
    wr_i(mem, dst, (VM_INT)cp);
}

// real(x)：字符串 → 数；不是合法数字给 0（老代码 num_at 的规矩：不做隐式转换、不报错）
//   整数写 INT、带小数写 FLOAT（和 GML 的 real 结果一致）
void real(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = arg_v(mem, args, 0);
    if (a.t == MEM_STRING) {
        if (!str_ok(a.v)) { wr_num(mem, dst, (VM_FLOAT)0); return; }
        char* endp = NULL;
        double d = strtod(str_at(a.v).c_str(), &endp);
        if (endp == str_at(a.v).c_str()) { wr_num(mem, dst, (VM_FLOAT)0); return; }
        wr_num(mem, dst, (VM_FLOAT)d);
        return;
    }
    wr_num(mem, dst, arg_num(mem, args, 0));
}

// ── json_stringify：结构体 / 数组 → JSON 文本（Step 里联机广播属性用）──
//   数字：整数原样，浮点 %.6g；undefined → null；容器嵌套有深度上限（见 json_val）
void json_stringify(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string out;
    json_val(arg_v(mem, args, 0), out, 0);
    wr_v(mem, dst, mk_str_sid(intern_str(out)));
}

// ── json_parse：JSON 文本 → 结构体 / 数组 ──
//   老代码里这条没实现（旧表里是空壳），这里照 GML 语义补一个：
//     · object → 结构体（MEM_MAP，键是字符串池下标）；array → MEM_ARRAY
//     · number → 整数存 INT、否则 FLOAT；true/false → 1/0；null → undefined
//     · 字符串支持 " \\ \/ \b \f \n \r \t 和 \uXXXX（含 UTF-16 代理对）
//     · 能解析的常见形状 = 上面这些；解析失败（截断 / 语法错 / 尾随垃圾）→ undefined
//   容器按池的引用计数规矩来：建出来 refs=1，进父容器 retain 一份、放掉自己那份，
//   根节点最后 give_handle 交给 dst。
void json_parse(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value sv = arg_v(mem, args, 0);
    if (sv.t != MEM_STRING || !str_ok(sv.v)) { wr_v(mem, dst, mk_undef()); return; }
    // 必须复制一份文本：解析过程中 intern_str 会让 g_strings 扩容，
    // 拿着池里元素的引用往下走就悬了。
    std::string txt = str_at(sv.v);
    size_t i = 0;
    Value out;
    if (!j_value(txt, i, out, 0)) { wr_v(mem, dst, mk_undef()); return; }
    j_ws(txt, i);
    if (i != txt.size()) { release(out); wr_v(mem, dst, mk_undef()); return; }   // 后面还有垃圾 → 不算成功
    give_handle(mem, dst, out);
}

// ============================================================
// 全局变量（g_globals，进程级：VM 挂掉也不清）
//   下标就是名字的池下标（同一个文本永远同一个 sid）
// ============================================================
void variable_global_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    if (rd_t(mem, slot) != MEM_STRING) { wr_bool(mem, dst, false); return; }
    VM_INT si = rd_i(mem, slot);
    if (!str_ok(si)) { wr_bool(mem, dst, false); return; }
    wr_bool(mem, dst, global_exists_sid(si));
}
void variable_global_get(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    Value* p = NULL;
    if (rd_t(mem, slot) == MEM_STRING) {
        VM_INT si = rd_i(mem, slot);
        if (str_ok(si)) p = global_var_sid(si);
    }
    if (!p) { wr_v(mem, dst, mk_undef()); return; }
    wr_v(mem, dst, *p);
}
// variable_global_set(name, value)：写全局
//   引用计数的账由内层 global_var_set_sid 结（retain 新的 / release 旧的）——
//   全局表是进程级的、没人来收，写进去的容器句柄必须由它自己持一份引用。
void variable_global_set(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    if (argc < 2) return;
    int slot = argslot(args, 0);
    if (rd_t(mem, slot) != MEM_STRING) return;
    VM_INT si = rd_i(mem, slot);
    if (!str_ok(si)) return;
    global_var_set_sid(si, arg_v(mem, args, 1));
}

// ============================================================
// 颜色
// ============================================================
// merge_color(col1, col2, amount)：GM 的颜色是 BGR 三个字节，按分量线性插值
//   （英式拼法 merge_colour 是本体；两个名字一个实现，见 vm_container.cpp）
void merge_colour(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT   c1 = arg_i(mem, args, 0);
    VM_INT   c2 = arg_i(mem, args, 1);
    VM_FLOAT amount = arg_num(mem, args, 2);
    if (amount < (VM_FLOAT)0) amount = (VM_FLOAT)0;
    if (amount > (VM_FLOAT)1) amount = (VM_FLOAT)1;
    VM_INT out = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        VM_INT a = (c1 >> shift) & 255;
        VM_INT b = (c2 >> shift) & 255;
        VM_INT v = (VM_INT)((VM_FLOAT)a + ((VM_FLOAT)b - (VM_FLOAT)a) * amount + (VM_FLOAT)0.5);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        out |= v << shift;
    }
    wr_i(mem, dst, out);
}
void merge_color(Value* mem, const uint8_t* args, int argc, int dst) {
    merge_colour(mem, args, argc, dst);
}
// make_color_rgb(r, g, b)：GML 的调色板是 BGR 排的 → r | g<<8 | b<<16
//   （hot_card_slot.cpp 里当场手算的就是这个；那处可以不再手算）
void make_color_rgb(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT r = (VM_INT)arg_num(mem, args, 0);
    VM_INT g = (VM_INT)arg_num(mem, args, 1);
    VM_INT b = (VM_INT)arg_num(mem, args, 2);
    wr_i(mem, dst, r | (g << 8) | (b << 16));
}
