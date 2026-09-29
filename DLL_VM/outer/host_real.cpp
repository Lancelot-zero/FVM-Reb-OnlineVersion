// ============================================================
// host_real.cpp — 宿主类里"**自己能算**"的 27 条
//
//   文件 10 / 目录 2 / 缓冲区 10 / 日期 2 / ini 3（名单见 outer_split.h 的 HOST_REAL_LIST）
//   这一批**不需要游戏引擎**：本地文件系统 + 标准库就能干完，所以不留在 host_fn.cpp 当空壳，
//   在这里真实现。底层原语在 host/（host_file_* / host_buf_*，都是真实现），
//   本文件只负责"GML 语义"那一层：取实参槽 → 调原语 → 回值写槽。
//
//   挂的表名就是原名（file_exists / buffer_read …），**不是** impl_* ——
//   native 表（kNativeFns）直接放函数指针，没有二次派发那一层。
//
//   语义真源：
//     · GML 参考手册（本机没有）——下面每条都写清"照哪条语义"
//     · 项目里真实用法：scripts/FileUtil、scripts/save_file、scripts/scr_battle_function、
//       objects/obj_shell/Other_68、objects/obj_game_init（ini 那三条）
//
//   ⚠ 三个"跨层"的地方，先写在这：
//     1) GML 的 file_text_open_* 返回**数字句柄**，host_file_open 返回 void* →
//        本文件里有一张小句柄表（void* ↔ int，带复用队列），GML 侧只看到 int。
//     2) GML 的 buffer id 本来就是 int（宿主层池下标）→ **直接透传**，不另建表。
//     3) GML 的 buffer_write / buffer_read 是**类型化**的（buffer_u8 / buffer_s32 / buffer_string …）：
//        字节宽度 / 编码 / 小端在这里做，宿主层只收到"一段字节"。
//
//   ⚠ 本文件不 printf；参数不对就照 GML 回 undefined / 0 / -1 / 空操作，不乱置 vm_set_err。
// ============================================================
#include "outer.h"
#include "../host/host.h"
#include <cstdio>       // snprintf：把数拼成 ini 的值（不是往控制台打）
#include <string>
#include <vector>

namespace {

// ============================================================
// 小工具
// ============================================================
// 数字回值：整数存 INT、否则存 FLOAT（和 builtin.cpp 的 mk_num_v 一个规矩）
Value mk_num_v(VM_FLOAT x) {
    double d = (double)x;
    if (d >= -2147483648.0 && d <= 2147483647.0) {
        VM_INT i = (VM_INT)x;
        if ((VM_FLOAT)i == x) return mk_int(i);
    }
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}

// i64 → 槽：放得下就 INT，放不下只能按 FLOAT 存（⚠ 32 位槽，>2^24 会丢精度）
Value from_i64(long long x) {
    if (x >= (-2147483647LL - 1) && x <= 2147483647LL) return mk_int((VM_INT)x);
    return mk_num_v((VM_FLOAT)x);
}

// 取第 k 个实参当**文本**（复制一份：intern_str 可能让字符串池扩容，
//   拿着池里元素的引用往下走会悬）。调用方**必须**先守 argc。
bool arg_str(Value* mem, const uint8_t* args, int k, std::string& out) {
    Value v = arg_v(mem, args, k);
    if (v.t != MEM_STRING || !str_ok(v.v)) return false;
    out = str_at(v.v);
    return true;
}

void wr_str(Value* mem, int dst, const std::string& s) {
    wr_v(mem, dst, mk_str_sid(intern_str(s)));
}

// ============================================================
// 文件句柄表（void* ↔ int，带复用队列）
//   GML 侧只看到 int；-1 = 开失败（GML 的 file_text_open_read 读不到文件就是 -1，
//   项目里到处是 `if (_w == -1) return;`，见 scripts/FileUtil、scripts/save_file）
// ============================================================
std::vector<void*> g_fh;        // 下标 = GML 看到的句柄号
std::vector<int>   g_fh_free;   // 关掉的句柄号，等着复用

void* fh_of(int id) {
    if (id < 0 || id >= (int)g_fh.size()) return NULL;
    return g_fh[(size_t)id];
}

int fh_open(const std::string& path, int mode) {
    void* h = host_file_open(path, mode);
    if (!h) return -1;
    int id;
    if (!g_fh_free.empty()) { id = g_fh_free.back(); g_fh_free.pop_back(); }
    else { g_fh.push_back(NULL); id = (int)g_fh.size() - 1; }
    g_fh[(size_t)id] = h;
    return id;
}

void fh_close(int id) {
    void* h = fh_of(id);
    if (!h) return;                        // 没开过 / 已关过 → 空操作（别把 id 重复塞回复用队列）
    host_file_close(h);
    g_fh[(size_t)id] = NULL;
    g_fh_free.push_back(id);
}

// ============================================================
// buffer 的类型号（GML 的常量表，**照 GML 的编号**）
//   宿主层只认字节，所以"类型号 → 宽度 / 编码"这一层在本文件。
//   ⚠ 这些常量本来是 GML 的内建常量（编译成 global.buffer_u8 那种读取），
//     现在 VM 里**还没有**常量表 → 传进来可能是 undefined(0)。0 不是合法类型，
//     于是读写都按"不认识的类型"处理（回 undefined / 不写），这是**已知的上游缺口**。
//   ⚠ 12/13 是 buffer_text / buffer_u64 的编号，资料里有两种排法（u64 与 text 互换）；
//     这里取"u8..f64,bool,string,text,u64"这一套（ENIGMA 的 GML 兼容表用的是
//     u64=12、text=13，和这里相反 —— 见汇报里"拿不准"的那条）。
// ============================================================
enum {
    BUF_U8     = 1,
    BUF_S8     = 2,
    BUF_U16    = 3,
    BUF_S16    = 4,
    BUF_U32    = 5,
    BUF_S32    = 6,
    BUF_F16    = 7,
    BUF_F32    = 8,
    BUF_F64    = 9,
    BUF_BOOL   = 10,
    BUF_STRING = 11,
    BUF_TEXT   = 12,
    BUF_U64    = 13
};

// seek 的基准（GML 的 buffer_seek_start / _relative / _end）
//   ⚠ 名字前面带 SEEK_BASE_：<cstdio> 已经有 SEEK_END(2) 这个宏，直接用会撞。
enum { SEEK_BASE_START = 0, SEEK_BASE_REL = 1, SEEK_BASE_END = 2 };

int type_bytes(int type) {
    switch (type) {
    case BUF_U8: case BUF_S8: case BUF_BOOL:            return 1;
    case BUF_U16: case BUF_S16: case BUF_F16:           return 2;
    case BUF_U32: case BUF_S32: case BUF_F32:           return 4;
    case BUF_F64: case BUF_U64:                         return 8;
    default:                                            return 0;   // 字符串 / 不认识
    }
}

// ── 小端写（GML 的 buffer 默认就是小端）──
void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v & 0xFFu); p[1] = (uint8_t)((v >> 8) & 0xFFu); }
void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu); }
void put64(uint8_t* p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu); }
uint16_t get16(const uint8_t* p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
uint32_t get32(const uint8_t* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= ((uint32_t)p[i]) << (8 * i);
    return v;
}
uint64_t get64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (8 * i);
    return v;
}

// ── float32 ↔ float16（用了 buffer_f16 才走到）──
//   舍入做的是"就近进位"，**没做** ties-to-even；次正规数按能表示的范围截断。
//   项目里没人真用 f16 存数据，这里够用；要精确再照 IEEE 754 补。
uint16_t f32_to_f16(VM_FLOAT x) {
    uint32_t b;
    memcpy(&b, &x, 4);
    uint32_t sign = (b >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((b >> 23) & 0xFFu) - 127 + 15;
    uint32_t man  = b & 0x7FFFFFu;
    if (((b >> 23) & 0xFFu) == 0xFFu)                       // Inf / NaN
        return (uint16_t)(sign | 0x7C00u | (man ? 0x200u : 0u));
    if (exp <= 0) {                                        // 0 / 次正规 / 太小 → 0
        if (exp < -10) return (uint16_t)sign;
        man |= 0x800000u;
        uint32_t shift = (uint32_t)(14 - exp);
        uint32_t half  = man >> shift;
        if ((man >> (shift - 1)) & 1u) half++;              // 就近进位
        return (uint16_t)(sign | half);
    }
    if (exp >= 31) return (uint16_t)(sign | 0x7C00u);       // 溢出 → Inf
    uint32_t half = sign | ((uint32_t)exp << 10) | (man >> 13);
    if (man & 0x1000u) half++;
    return (uint16_t)half;
}

VM_FLOAT f16_to_f32(uint16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000u) << 16;
    uint32_t exp  = ((uint32_t)h >> 10) & 0x1Fu;
    uint32_t man  = (uint32_t)h & 0x3FFu;
    uint32_t b;
    if (exp == 0) {
        if (man == 0) {
            b = sign;
        } else {                                            // 次正规 → 规格化
            int e = -1;
            do { man <<= 1; e++; } while (!(man & 0x400u));
            man &= 0x3FFu;
            b = sign | ((uint32_t)(112 - e) << 23) | (man << 13);
        }
    } else if (exp == 31) {
        b = sign | 0x7F800000u | (man << 13);
    } else {
        b = sign | ((exp + 112u) << 23) | (man << 13);       // exp-15+127 = exp+112
    }
    VM_FLOAT f;
    memcpy(&f, &b, 4);
    return f;
}

// 把一个数按 GML 类型编成小端字节 → 返回字节数；0 = 类型不认识（调用方当"没写"）
int enc_num(int type, double num, uint8_t* out) {
    switch (type) {
    case BUF_U8:   out[0] = (uint8_t)(uint64_t)(long long)num;                        return 1;
    case BUF_S8:   out[0] = (uint8_t)(int8_t)(long long)num;                          return 1;
    case BUF_BOOL: out[0] = (num != 0.0) ? 1u : 0u;                                   return 1;
    case BUF_U16:  put16(out, (uint16_t)(uint64_t)(long long)num);                    return 2;
    case BUF_S16:  put16(out, (uint16_t)(int16_t)(long long)num);                     return 2;
    case BUF_F16:  put16(out, f32_to_f16((VM_FLOAT)num));                             return 2;
    case BUF_U32:  put32(out, (uint32_t)(uint64_t)(long long)num);                    return 4;
    case BUF_S32:  put32(out, (uint32_t)(int32_t)(long long)num);                     return 4;
    case BUF_F32: {
        float f = (float)num;
        uint32_t b;
        memcpy(&b, &f, 4);
        put32(out, b);
        return 4;
    }
    case BUF_F64: {
        double d = num;
        uint64_t b;
        memcpy(&b, &d, 8);
        put64(out, b);
        return 8;
    }
    case BUF_U64:  put64(out, (uint64_t)(long long)num);                              return 8;
    default:       return 0;
    }
}

// 小端字节 → 槽（type_bytes 已经保证长度够）
Value dec_num(int type, const uint8_t* in) {
    switch (type) {
    case BUF_U8:   return mk_int((VM_INT)in[0]);
    case BUF_S8:   return mk_int((VM_INT)(int8_t)in[0]);
    case BUF_BOOL: return mk_int(in[0] ? 1 : 0);
    case BUF_U16:  return mk_int((VM_INT)get16(in));
    case BUF_S16:  return mk_int((VM_INT)(int16_t)get16(in));
    case BUF_F16:  return mk_num_v(f16_to_f32(get16(in)));
    case BUF_U32:  return from_i64((long long)get32(in));
    case BUF_S32:  return mk_int((VM_INT)(int32_t)get32(in));
    case BUF_F32: {
        uint32_t b = get32(in);
        float f;
        memcpy(&f, &b, 4);
        return mk_num_v((VM_FLOAT)f);
    }
    case BUF_F64: {
        uint64_t b = get64(in);
        double d;
        memcpy(&d, &b, 8);
        return mk_num_v((VM_FLOAT)d);          // ⚠ 槽是 32 位 → 只能收窄
    }
    case BUF_U64:  return from_i64((long long)get64(in));
    default:       return mk_int(0);
    }
}

// 从当前位置读一个"以 0 结尾的串"（buffer_string / buffer_text 的读侧）
//   ⚠ 简化：GML 的 buffer_text 读侧没有长度信息，这里和 buffer_string 一样"读到 0 或到头"。
std::string buf_read_str(int id) {
    std::string s;
    for (;;) {
        uint8_t c = 0;
        if (host_buf_read(id, &c, 1) != 1) break;    // 到头了
        if (c == 0) break;                           // 结尾的 0（也吃掉）
        s.push_back((char)c);
    }
    return s;
}

// ============================================================
// ini（3 条）—— 简化点都写在下面
//   ① 只实现**写**：ini_open 只记路径，ini_write_real 攒起来，ini_close 才落盘。
//      **读侧（ini_read_real / ini_read_string / ini_key_exists）不在本次这 27 条里，没实现。**
//   ② 正因为读侧没有，写回时**必须**把原文件其它键原样保留 ——
//      否则一次 ini_write_real 就会把整份 config.ini 冲掉（项目里 obj_mute_button 只写
//      music_volume，obj_key_bind_button 只写 keybinds，冲突起来就是数据没了）。
//      所以 ini_close 走"读原文件 → 改那一个 key → 写回"，不是整份重写。
//   ③ section / key 按**原样文本**比对（GML 实际大小写不敏感，这里没做）。
//   ④ 行结束符统一成 "\n"（读进来时 CRLF 已被 host_file_readln 吃掉）。
//   ⑤ ini_write_real 的数值格式：整数 "%lld"、浮点 "%.6g"（GML 具体几位没查）。
// ============================================================
struct IniEntry { std::string sec, key, val; };

bool                  g_ini_on = false;
std::string           g_ini_path;
std::vector<IniEntry> g_ini_pending;

// ini 文件的一行：0 = 原样（注释 / 空行），1 = [section]，2 = key=value
struct IniLine { int kind; std::string a, b; };

} // namespace

// ============================================================
// 文件 10 条
// ============================================================
// file_exists(fname)：GML 里文件或**目录**都算存在 → 1 / 0
void file_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, (host_file_exists(path) || host_dir_exists(path)) ? 1 : 0);
}

// file_text_open_write(fname) → 句柄（截断重写）；失败 -1
void file_text_open_write(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, fh_open(path, HOST_FILE_WRITE));
}

// file_text_write_string(fileid, str)：不加换行（GML 无返回值 → undefined）
void file_text_write_string(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 2) return;
    void* h = fh_of((int)arg_num(mem, args, 0));
    if (!h) return;
    std::string s;
    if (!arg_str(mem, args, 1, s)) return;         // 不是字符串 → 空操作
    host_file_write(h, s);
}

// file_text_close(fileid)：关掉并把句柄号还回复用队列（关第二次是空操作）
void file_text_close(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 1) return;
    fh_close((int)arg_num(mem, args, 0));
}

// file_text_open_append(fname) → 句柄（追加）；失败 -1
void file_text_open_append(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, fh_open(path, HOST_FILE_APPEND));
}

// file_text_open_read(fname) → 句柄（文本读）；失败 -1
void file_text_open_read(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, fh_open(path, HOST_FILE_READ_TEXT));
}

// file_text_eof(fileid) → 1 / 0（句柄不对 → 当"到头了"= 1，和宿主层一致）
void file_text_eof(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 1); return; }
    void* h = fh_of((int)arg_num(mem, args, 0));
    wr_i(mem, dst, host_file_eof(h) ? 1 : 0);
}

// file_text_read_string(fileid)：读一个"非空白词"（照 GML；句柄不对 / 到头 → 空串）
void file_text_read_string(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_str(mem, dst, ""); return; }
    void* h = fh_of((int)arg_num(mem, args, 0));
    if (!h) { wr_str(mem, dst, ""); return; }
    wr_str(mem, dst, host_file_read_string(h));
}

// file_text_readln(fileid)：读一行（不含换行；GML 无返回值 → undefined）
void file_text_readln(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 1) return;
    void* h = fh_of((int)arg_num(mem, args, 0));
    if (!h) return;
    host_file_readln(h);
}

// file_text_writeln(fileid)：写一个换行（GML 无返回值 → undefined）
void file_text_writeln(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 1) return;
    void* h = fh_of((int)arg_num(mem, args, 0));
    if (!h) return;
    host_file_write(h, "\n");
}

// ============================================================
// 目录 2 条
// ============================================================
// directory_exists(path) → 1 / 0（只要是真目录）
void directory_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, host_dir_exists(path) ? 1 : 0);
}

// directory_create(path) → 1 / 0
//   ⚠ 简化：宿主层用的是 create_directories（**连中间层一起建**），
//     而 GML 的 directory_create 只建最后一级 —— 多建中间层是超集，不会少建。
void directory_create(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, host_dir_create(path) ? 1 : 0);
}

// ============================================================
// 缓冲区 10 条（id 直接透传：GML 的 buffer id 就是宿主层池下标）
// ============================================================
// buffer_create(size, type, alignment) → id
//   ⚠ 简化：alignment（第三个参数）**忽略** —— 宿主层是 std::vector，没有对齐概念。
//   ⚠ 类型号按原样交给宿主层存着，但**负数要夹到 0** ——
//     宿主层拿 `type < 0` 当"这个槽是死的"的标记，负类型会让刚建出来的 buf 立刻算死的。
void buffer_create(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    int size = (int)arg_num(mem, args, 0);
    int type = (argc >= 2) ? (int)arg_num(mem, args, 1) : 0;
    if (type < 0) type = 0;
    wr_i(mem, dst, host_buf_create(size, type));
}

// buffer_write(buffer, type, value)：照 GML 的**参数顺序**（buffer 在前、type 第二、值第三）
void buffer_write(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 3) return;
    int id   = (int)arg_num(mem, args, 0);
    int type = (int)arg_num(mem, args, 1);
    if (!host_buf_alive(id)) return;
    if (type == BUF_STRING || type == BUF_TEXT) {
        std::string s;
        if (!arg_str(mem, args, 2, s)) s.clear();          // 不是字符串 → 写空
        // buffer_string 结尾带一个 0；buffer_text **不带**（项目里写 text 之后会自己补分隔符）
        if (type == BUF_STRING) s.push_back('\0');
        if (!s.empty()) host_buf_write(id, s.data(), (int)s.size());
        return;
    }
    uint8_t tmp[8];
    int n = enc_num(type, (double)arg_num(mem, args, 2), tmp);
    if (n > 0) host_buf_write(id, tmp, n);
}

// buffer_read(buffer, type)：类型化读 → 数字 / 字符串
//   ⚠ 读到不够（到 buffer 尾）：按**补 0** 解，不报错（GML 那边是读越界，项目里靠
//      `while (buffer_tell(buf) < _size)` 自己守，见 scr_command_VM / VM_Decode）。
//   ⚠ f64 只能收窄成 32 位槽；u32 / u64 超过 int32 时按 FLOAT 存（>2^24 会丢精度）。
void buffer_read(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(mem, dst, mk_undef()); return; }
    int id   = (int)arg_num(mem, args, 0);
    int type = (int)arg_num(mem, args, 1);
    if (!host_buf_alive(id)) { wr_v(mem, dst, mk_undef()); return; }
    if (type == BUF_STRING || type == BUF_TEXT) {
        wr_str(mem, dst, buf_read_str(id));
        return;
    }
    int need = type_bytes(type);
    if (need <= 0) { wr_v(mem, dst, mk_undef()); return; }
    uint8_t tmp[8] = { 0 };
    if (host_buf_read(id, tmp, need) <= 0) { wr_i(mem, dst, 0); return; }   // 到头了
    wr_v(mem, dst, dec_num(type, tmp));
}

// buffer_load(fname) → **新** buffer 的 id；读不到 → -1
//   （宿主层只有"往已有 buf 里填"，所以这里先建一个 0 长的再填）
void buffer_load(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    std::string path;
    if (!arg_str(mem, args, 0, path)) { wr_i(mem, dst, -1); return; }
    int id = host_buf_create(0, 0);          // 类型号先给 0（fixed）；宿主层不看它
    if (!host_buf_load(id, path)) { host_buf_delete(id); wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, id);
}

// buffer_delete(buffer)：放掉（id 无效 = 空操作）
void buffer_delete(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 1) return;
    host_buf_delete((int)arg_num(mem, args, 0));
}

// buffer_get_size(buffer) → 字节数（id 无效 → 0）
void buffer_get_size(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, host_buf_size((int)arg_num(mem, args, 0)));
}

// buffer_seek(buffer, base, offset)：base = 0 头 / 1 相对 / 2 尾（GML 的 buffer_seek_*）
//   ⚠ offset 按**有符号**收（scribble 里有 buffer_seek(b, buffer_seek_relative, -1)）；
//     算出来小于 0 就夹到 0（宿主层也不接受负位置）。
void buffer_seek(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 3) return;
    int id   = (int)arg_num(mem, args, 0);
    int base = (int)arg_num(mem, args, 1);
    int off  = (int)arg_num(mem, args, 2);
    if (!host_buf_alive(id)) return;
    int pos = off;
    if (base == SEEK_BASE_REL)      pos = host_buf_tell(id) + off;
    else if (base == SEEK_BASE_END) pos = host_buf_size(id) + off;
    if (pos < 0) pos = 0;
    host_buf_seek(id, pos);
}

// buffer_tell(buffer) → 当前读写位置（id 无效 → 0）
void buffer_tell(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, host_buf_tell((int)arg_num(mem, args, 0)));
}

// buffer_copy(src, src_off, size, dest, dest_off)
//   ⚠ GML 的实参顺序是"源在前、目标在后"，宿主层是 (dst, src, src_off, dst_off, n) —— 别抄反。
void buffer_copy(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 5) return;
    int src     = (int)arg_num(mem, args, 0);
    int src_off = (int)arg_num(mem, args, 1);
    int n       = (int)arg_num(mem, args, 2);
    int dstd    = (int)arg_num(mem, args, 3);
    int dst_off = (int)arg_num(mem, args, 4);
    host_buf_copy(dstd, src, src_off, dst_off, n);
}

// buffer_poke(buffer, offset, type, value)：**不动**读写位置，往指定偏移写
void buffer_poke(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 4) return;
    int id   = (int)arg_num(mem, args, 0);
    int off  = (int)arg_num(mem, args, 1);
    int type = (int)arg_num(mem, args, 2);
    if (!host_buf_alive(id)) return;
    if (type == BUF_STRING || type == BUF_TEXT) {
        std::string s;
        if (!arg_str(mem, args, 3, s)) s.clear();
        if (type == BUF_STRING) s.push_back('\0');
        if (!s.empty()) host_buf_poke(id, off, s.data(), (int)s.size());
        return;
    }
    uint8_t tmp[8];
    int n = enc_num(type, (double)arg_num(mem, args, 3), tmp);
    if (n > 0) host_buf_poke(id, off, tmp, n);
}

// ============================================================
// 日期 2 条
//   GML 的 datetime = **1899-12-30 起的天数**（实数，本地时间）；换算在宿主层。
// ============================================================
// date_current_datetime() → 当前时间（实数）
//   ⚠ 槽是 32 位浮点：45000 天这个量级上 1 天只剩 ~0.004 的分辨率（≈5 分钟），
//     所以"秒"这一级在槽里存不住 —— 这是 VM_FLOAT 的宽度问题，不是这里的 bug。
void date_current_datetime(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_f(mem, dst, (VM_FLOAT)host_date_now());
}

// date_datetime_string(dt) → 文本
//   ⚠ 简化：GML 这条是**系统区域格式**（"September 15th 2011, 11:33.00" 那种，随机器变），
//     这里统一成 "YYYY-MM-DD hh:mm:ss"（和宿主层 host_date_current 一个格式，日志好对齐）。
void date_datetime_string(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_str(mem, dst, ""); return; }
    double dt = (double)arg_num(mem, args, 0);
    wr_str(mem, dst, host_date_to_string(dt));
}

// ============================================================
// ini 3 条（简化点见文件上半部分的说明）
// ============================================================
// ini_open(fname)：打开一个 ini（只记路径；**不读原文件**，原文件在 ini_close 时按行保留）
void ini_open(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    g_ini_on = false;
    g_ini_pending.clear();
    if (argc < 1) return;
    std::string path;
    if (!arg_str(mem, args, 0, path)) return;
    g_ini_path = path;
    g_ini_on = true;
}

// ini_write_real(section, key, value)：攒着，ini_close 才落盘
//   ⚠ 没打开就写 = 空操作（GML 那边会报"INI 文件未定义"，我们这里不报）。
void ini_write_real(Value* mem, const uint8_t* args, int argc, int dst) {
    wr_v(mem, dst, mk_undef());
    if (argc < 3 || !g_ini_on) return;
    std::string sec, key;
    if (!arg_str(mem, args, 0, sec) || !arg_str(mem, args, 1, key)) return;
    Value v = arg_v(mem, args, 2);
    char buf[64];
    if (v.t == MEM_FLOAT)      snprintf(buf, sizeof(buf), "%.6g", (double)f_of(v.v));
    else if (v.t == MEM_INT)   snprintf(buf, sizeof(buf), "%lld", (long long)v.v);
    else                       snprintf(buf, sizeof(buf), "0");      // 不是数字 → 0
    IniEntry e;
    e.sec = sec; e.key = key; e.val = buf;
    g_ini_pending.push_back(e);
}

// ini_close()：把攒下的键落盘（读原文件 → 改那一个 key → 写回，其它键不许丢）
void ini_close(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_v(mem, dst, mk_undef());
    if (!g_ini_on) return;
    g_ini_on = false;

    // 1) 原文件按行读进来（不存在 → 空表）
    std::vector<IniLine> lines;
    {
        void* f = host_file_open(g_ini_path, HOST_FILE_READ_TEXT);
        if (f) {
            while (!host_file_eof(f)) {
                std::string t = host_file_readln(f);
                IniLine L;
                L.kind = 0;
                L.a = t;
                if (t.size() >= 2 && t[0] == '[' && t[t.size() - 1] == ']') {
                    L.kind = 1;
                    L.a = t.substr(1, t.size() - 2);
                } else {
                    size_t eq = t.find('=');
                    if (eq != std::string::npos && eq > 0) {
                        L.kind = 2;
                        L.a = t.substr(0, eq);
                        L.b = t.substr(eq + 1);
                    }
                }
                lines.push_back(L);
            }
            host_file_close(f);
        }
    }

    // 2) 待写项挨个落地：同 section 同 key → 就地改；没有 → 追加到那个 section 里
    //    （section 不存在就新建一个，放在文件尾）
    for (size_t p = 0; p < g_ini_pending.size(); p++) {
        const IniEntry& e = g_ini_pending[p];
        int sec_start = -1;                       // 同名 section 的**最后一个**（后出现的赢）
        for (int i = 0; i < (int)lines.size(); i++)
            if (lines[(size_t)i].kind == 1 && lines[(size_t)i].a == e.sec) sec_start = i;
        int sec_end = (int)lines.size();
        if (sec_start >= 0) {
            for (int i = sec_start + 1; i < (int)lines.size(); i++)
                if (lines[(size_t)i].kind == 1) { sec_end = i; break; }
        }
        int hit = -1;
        for (int i = sec_start + 1; i < sec_end; i++)
            if (lines[(size_t)i].kind == 2 && lines[(size_t)i].a == e.key) { hit = i; break; }

        if (hit >= 0) { lines[(size_t)hit].b = e.val; continue; }

        IniLine nl;
        nl.kind = 2; nl.a = e.key; nl.b = e.val;
        if (sec_start >= 0) {
            lines.insert(lines.begin() + sec_end, nl);       // 插在本 section 最后
        } else {
            IniLine h;
            h.kind = 1; h.a = e.sec;
            lines.push_back(h);
            lines.push_back(nl);
        }
    }

    // 3) 写回
    void* w = host_file_open(g_ini_path, HOST_FILE_WRITE);
    if (w) {
        for (size_t i = 0; i < lines.size(); i++) {
            std::string out;
            if (lines[i].kind == 1)      out = "[" + lines[i].a + "]";
            else if (lines[i].kind == 2) out = lines[i].a + "=" + lines[i].b;
            else                         out = lines[i].a;
            host_file_writeln(w, out);
        }
        host_file_close(w);
    }
    g_ini_pending.clear();
}
