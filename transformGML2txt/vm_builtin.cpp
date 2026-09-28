// ============================================================
// vm_builtin.cpp — 常规内置函数（数学 / 类型判断 / 字符串 / 全局变量 / 坐标换算）
//
//   这些是"VM 自己就能算出来"的那批：纯计算，或者只查 VM 自己的表
//   （实例表、全局变量表、容器池）。跟引擎打交道的（精灵、碰撞、绘制、
//   音频、网络、shader）不在这儿 —— 那些留空壳给宿主。
//
//   写新条目的规矩：
//     · 语义照 GML 手册，别"顺手优化"（比如 round 是四舍六入五成双、真值是 > 0.5）
//     · 参数从槽里读（argslot + rd_*），结果写 dst（wr_v / wr_i，dst < 0 时自动不写）
//     · 在这里写实了，就要把名字从 vm.h 的 NATIVE_FN_LIST 拿掉、挂到 vm_native.cpp 表尾
//     · 数值一律按 double 算，写回去时"整数存 int、否则存 float"（跟 hot/ 里一个规矩）
// ============================================================

#include "vm.h"
#define NOMINMAX                 // windows.h 的 max/min 宏会撞我们的 void max/min
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdlib>

namespace {

// ── 小工具（只在本文件用）──

// 读一个数值参数：float 按位取回，int 直接转；字符串给 0
//   （GML 会尝试把字符串转成数，我们的翻译器只在明确是数字的地方用算术，不做隐式转换）

double num_at(Vm& vm, const uint8_t* args, int k) {
    int slot = argslot(args, k);
    int t = rd_t(vm, slot), v = rd_i(vm, slot);
    if (t == MEM_FLOAT) { float f; memcpy(&f, &v, 4); return (double)f; }
    return (double)v;
}

Value mk_num(double x) {
    Value v;
    if (x == (double)(int)x) { v.t = MEM_INT;   v.v = (int)x; }
    else                     { v.t = MEM_FLOAT; float f = (float)x; memcpy(&v.v, &f, 4); }
    return v;
}

void wr_num(Vm& vm, int dst, double x)  { wr_v(vm, dst, mk_num(x)); }
void wr_bool(Vm& vm, int dst, bool b)   { wr_i(vm, dst, b ? 1 : 0); }

Value mk_str(Vm& vm, const char* s) {
    Value v; v.t = MEM_STRING; v.v = intern_str(s);
    return v;
}

// 按槽号数组调一个已实现的内部函数（我们自己拼参数表时用，比如 ds_map_create/set）
void call_slots(Vm& vm, Fn fn, const int* slots, int n, int dst) {
    int32_t buf[8];
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) buf[i] = slots[i];
    fn(vm, (const uint8_t*)buf, n, dst);
}

// 全局变量（宿主往 g_globals 里塞的那些：grid_offset_x / grid_cols / ...）
double gnum(const char* name) {
    Value* p = global_var(name);
    if (!p) return 0;
    if (p->t == MEM_FLOAT) { float f; int iv = p->v; memcpy(&f, &iv, 4); return (double)f; }
    return (double)p->v;
}
double gnum_sid(int sid) { return gnum(str_at(sid).c_str()); }


// 结构体（GM 的 {a: 1, b: 2} 在我们这儿就是键是字符串的 map）
void struct_begin(Vm& vm, int slot) {
    call_slots(vm, ds_map_create, NULL, 0, slot);
}
void struct_put(Vm& vm, int map_slot, int key_slot, int val_slot) {
    int a[3];
    a[0] = map_slot; a[1] = key_slot; a[2] = val_slot;
    call_slots(vm, ds_map_set, a, 3, -1);
}

} // namespace

// ============================================================
// 数学
// ============================================================
void floor(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, std::floor(num_at(vm, args, 0)));
}
// GML 的 round 是"四舍六入五成双"（银行家舍入）：round(0.5)=0、round(1.5)=2、round(2.5)=2
void round(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double x = num_at(vm, args, 0);
    double f = std::floor(x);
    double d = x - f;
    double r;
    if (d > 0.5)      r = f + 1;
    else if (d < 0.5) r = f;
    else              r = (((long long)f % 2) == 0) ? f : f + 1;
    wr_num(vm, dst, r);
}
void ceil(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, std::ceil(num_at(vm, args, 0)));
}
void abs(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, std::fabs(num_at(vm, args, 0)));
}
// max / min：GML 可以给多个参数
void max(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_num(vm, dst, 0); return; }
    double r = num_at(vm, args, 0);
    for (int i = 1; i < argc; i++) { double x = num_at(vm, args, i); if (x > r) r = x; }
    wr_num(vm, dst, r);
}
void min(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_num(vm, dst, 0); return; }
    double r = num_at(vm, args, 0);
    for (int i = 1; i < argc; i++) { double x = num_at(vm, args, i); if (x < r) r = x; }
    wr_num(vm, dst, r);
}
void clamp(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double x = num_at(vm, args, 0), lo = num_at(vm, args, 1), hi = num_at(vm, args, 2);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    wr_num(vm, dst, x);
}
void lerp(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double a = num_at(vm, args, 0), b = num_at(vm, args, 1), t = num_at(vm, args, 2);
    wr_num(vm, dst, a + (b - a) * t);
}

// ── 随机数：GM 用自己的 RNG，我们先用标准 rand()（宿主以后要换自己的流也容易）──
void random(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double n = num_at(vm, args, 0);
    wr_num(vm, dst, ((double)rand() / (double)RAND_MAX) * n);
}
// ── 补的几个 GM 内建（翻译器会用到，之前没登记）──

// chr(n)：码点 → 单字符字符串
void chr(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int c = (int)num_at(vm, args, 0);
    std::string t;
    if (c > 0 && c < 128) t.push_back((char)c);
    else if (c > 0) {                                   // 非 ASCII：按 UTF-8 编
        if (c < 0x800) { t.push_back((char)(0xC0 | (c >> 6))); t.push_back((char)(0x80 | (c & 0x3F))); }
        else { t.push_back((char)(0xE0 | (c >> 12))); t.push_back((char)(0x80 | ((c >> 6) & 0x3F))); t.push_back((char)(0x80 | (c & 0x3F))); }
    }
    Value v; v.t = MEM_STRING; v.v = intern_str(t);
    wr_v(vm, dst, v);
}

// string_pos(substr, str)：子串位置（**1 起**，找不到 → 0）
void string_pos(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = rd_v(vm, argslot(args, 0)), b = rd_v(vm, argslot(args, 1));
    if (a.t != MEM_STRING || b.t != MEM_STRING || !str_ok(a.v) || !str_ok(b.v)) { wr_num(vm, dst, 0); return; }
    size_t p = str_at(b.v).find(str_at(a.v));
    wr_num(vm, dst, (p == std::string::npos) ? 0.0 : (double)(p + 1));
}

// file_exists(name)：按工作目录找（宿主要沙箱的话以后换成宿主回调）
void file_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = rd_v(vm, argslot(args, 0));
    if (a.t != MEM_STRING || !str_ok(a.v)) { wr_num(vm, dst, 0); return; }
    FILE* fp = fopen(str_at(a.v).c_str(), "rb");
    wr_num(vm, dst, fp ? 1.0 : 0.0);
    if (fp) fclose(fp);
}

// camera_get_view_x(view)：还没有相机系统 → 0（只提示一次，别静默）
void camera_get_view_x(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool warned = false;
    if (!warned) { warned = true; printf("[vm] camera_get_view_x: 还没有相机系统，先返回 0\n"); }
    wr_num(vm, dst, 0);
}


// string_delete(str, pos, count)：从 1 起的 pos 开始删 count 个字符
void string_delete(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = rd_v(vm, argslot(args, 0));
    int pos = (int)num_at(vm, args, 1), cnt = (int)num_at(vm, args, 2);
    if (a.t != MEM_STRING || !str_ok(a.v)) { wr_v(vm, dst, a); return; }
    std::string t = str_at(a.v);
    int i = pos - 1;
    if (i < 0) i = 0;
    if (i < (int)t.size() && cnt > 0) t.erase((size_t)i, (size_t)cnt);
    Value o; o.t = MEM_STRING; o.v = intern_str(t);
    wr_v(vm, dst, o);
}

// ── file_text_*：真做（宿主以后要沙箱的话换成宿主回调）──
static FILE* g_ft[32];
void file_text_open_write(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = rd_v(vm, argslot(args, 0));
    if (a.t != MEM_STRING || !str_ok(a.v)) { wr_num(vm, dst, -1); return; }
    int h = -1;
    for (int i = 0; i < 32; i++) if (!g_ft[i]) { h = i; break; }
    if (h < 0) { printf("[vm] file_text_open_write: 打开的文件太多（>32）\n"); wr_num(vm, dst, -1); return; }
    g_ft[h] = fopen(str_at(a.v).c_str(), "wb");
    if (!g_ft[h]) { printf("[vm] file_text_open_write: 打不开 %s\n", str_at(a.v).c_str()); wr_num(vm, dst, -1); return; }
    wr_num(vm, dst, (double)h);
}
void file_text_write_string(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int h = (int)num_at(vm, args, 0);
    Value a = rd_v(vm, argslot(args, 1));
    if (h < 0 || h >= 32 || !g_ft[h]) { printf("[vm] file_text_write_string: 无效句柄 %d\n", h); return; }
    if (a.t == MEM_STRING && str_ok(a.v)) fwrite(str_at(a.v).data(), 1, str_at(a.v).size(), g_ft[h]);
}
void file_text_close(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int h = (int)num_at(vm, args, 0);
    if (h < 0 || h >= 32 || !g_ft[h]) { printf("[vm] file_text_close: 无效句柄 %d\n", h); return; }
    fclose(g_ft[h]); g_ft[h] = NULL;
}

// ── 相机 / 音频：引擎侧能力，还没有 —— 返回 0 并**只提示一次**（不静默）──
void camera_get_view_y(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false; if (!w) { w = true; printf("[vm] camera_get_view_y: 还没有相机系统，先返回 0\n"); }
    wr_num(vm, dst, 0);
}
void camera_get_view_width(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false; if (!w) { w = true; printf("[vm] camera_get_view_width: 还没有相机系统，先返回 0\n"); }
    wr_num(vm, dst, 0);
}
void camera_get_view_height(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false; if (!w) { w = true; printf("[vm] camera_get_view_height: 还没有相机系统，先返回 0\n"); }
    wr_num(vm, dst, 0);
}
void audio_create_stream(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false; if (!w) { w = true; printf("[vm] audio_create_stream: 还没有音频系统，先返回 -1\n"); }
    wr_num(vm, dst, -1);
}


// ── 补的一批 GM 内建（翻译器要）──

// room_exists(room)：VM 里没有房间资源表 → 一律当"存在"（只提示一次）。
//   ⚠ 语义上是近似：调用方通常用它做保护（不存在就 return），返回 1 会让流程继续走，
//     接下来 room_goto 也是宿主的事。
void room_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false;
    if (!w) { w = true; printf("[vm] room_exists: 还没有房间资源表，一律当存在\n"); }
    wr_num(vm, dst, 1);
}

// directory_exists(name) / directory_create(name)：真做（和 file_exists 一样走文件系统）
void directory_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value a = rd_v(vm, argslot(args, 0));
    if (a.t != MEM_STRING || !str_ok(a.v)) { wr_num(vm, dst, 0); return; }
    DWORD at = GetFileAttributesA(str_at(a.v).c_str());
    wr_num(vm, dst, (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) ? 1.0 : 0.0);
}
void directory_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    Value a = rd_v(vm, argslot(args, 0));
    if (a.t != MEM_STRING || !str_ok(a.v)) return;
    CreateDirectoryA(str_at(a.v).c_str(), NULL);        // 已存在也算成功（GM 同）
}

// network_destroy(socket)：没有网络层 → 只提示一次
void network_destroy(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; (void)dst;
    static bool w = false;
    if (!w) { w = true; printf("[vm] network_destroy: 还没有网络层，忽略\n"); }
}

// buffer_create(...)：还没有缓冲区系统 → 返回 -1（只提示一次）
void buffer_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    static bool w = false;
    if (!w) { w = true; printf("[vm] buffer_create: 还没有缓冲区系统，先返回 -1\n"); }
    wr_num(vm, dst, -1);
}

// irandom(n)：0..n 的整数（含两端）—— floor(random(n+1))，和 GM 一致
void irandom(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double n = num_at(vm, args, 0);
    if (!(n > 0)) { wr_num(vm, dst, 0); return; }        // n<=0 / nan → 0
    int v = (int)(((double)rand() / (double)RAND_MAX) * (n + 1.0));
    if (v > (int)n) v = (int)n;                          // rand()==RAND_MAX 时防越界
    wr_num(vm, dst, (double)v);
}
void random_range(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double a = num_at(vm, args, 0), b = num_at(vm, args, 1);
    if (a > b) { double t = a; a = b; b = t; }
    wr_num(vm, dst, a + ((double)rand() / (double)RAND_MAX) * (b - a));
}
// irandom_range(a, b)：闭区间整数，a、b 反了也能用（跟 GML 一样）
void irandom_range(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = (int)num_at(vm, args, 0), b = (int)num_at(vm, args, 1);
    if (a > b) { int t = a; a = b; b = t; }
    int span = b - a + 1;
    if (span <= 0) { wr_i(vm, dst, a); return; }
    wr_i(vm, dst, a + (int)(rand() % span));
}

// ── 三角 / 向量：GML 里"角度"用度、"sin/cos"收弧度；y 轴朝下 ──
void sin(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, std::sin(num_at(vm, args, 0)));
}
void cos(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, std::cos(num_at(vm, args, 0)));
}
void degtorad(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_num(vm, dst, num_at(vm, args, 0) * (3.14159265358979323846 / 180.0));
}
void lengthdir_x(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double len = num_at(vm, args, 0), dir = num_at(vm, args, 1) * (3.14159265358979323846 / 180.0);
    wr_num(vm, dst, len * std::cos(dir));
}
// GML 的 y 轴朝下：lengthdir_y = -len * sin(dir)
void lengthdir_y(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double len = num_at(vm, args, 0), dir = num_at(vm, args, 1) * (3.14159265358979323846 / 180.0);
    wr_num(vm, dst, -len * std::sin(dir));
}
void point_direction(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double x1 = num_at(vm, args, 0), y1 = num_at(vm, args, 1);
    double x2 = num_at(vm, args, 2), y2 = num_at(vm, args, 3);
    double d = std::atan2(y1 - y2, x2 - x1) * (180.0 / 3.14159265358979323846);
    if (d < 0) d += 360;
    wr_num(vm, dst, d);
}
void point_distance(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double dx = num_at(vm, args, 2) - num_at(vm, args, 0);
    double dy = num_at(vm, args, 3) - num_at(vm, args, 1);
    wr_num(vm, dst, std::sqrt(dx * dx + dy * dy));
}

// ============================================================
// 类型判断
//   注意 is_struct：我们这边结构体和 ds_map 是同一种（键值对池），分不开，
//   所以 is_struct 对 ds_map 也会返回真。项目里没见两者混用，先这么定。
// ============================================================
void is_real(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int t = rd_t(vm, argslot(args, 0));
    wr_bool(vm, dst, t == MEM_INT || t == MEM_FLOAT);
}
void is_string(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(vm, dst, rd_t(vm, argslot(args, 0)) == MEM_STRING);
}
void is_array(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(vm, dst, rd_t(vm, argslot(args, 0)) == MEM_ARRAY);
}
void is_struct(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(vm, dst, rd_t(vm, argslot(args, 0)) == MEM_MAP);
}
void is_undefined(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    wr_bool(vm, dst, rd_t(vm, argslot(args, 0)) == MEM_UNDEFINED);
}

// ============================================================
// 字符串
// ============================================================
// string(x)：GML 里 real 转字符串保留 2 位小数（string(1.8) = "1.80"），整数不带小数点
void string(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    int t = rd_t(vm, slot), v = rd_i(vm, slot);
    char buf[64];
    if (t == MEM_STRING) { wr_v(vm, dst, rd_v(vm, slot)); return; }
    if (t == MEM_FLOAT)          { float f; memcpy(&f, &v, 4); snprintf(buf, sizeof(buf), "%.2f", (double)f); }
    else if (t == MEM_UNDEFINED) snprintf(buf, sizeof(buf), "%s", str_at(S_undefined).c_str());
    else if (t == MEM_ARRAY)     snprintf(buf, sizeof(buf), "[array]");
    else if (t == MEM_MAP)       snprintf(buf, sizeof(buf), "[struct]");
    else if (t == MEM_GRID)      snprintf(buf, sizeof(buf), "[grid]");
    else if (t == MEM_LIST)      snprintf(buf, sizeof(buf), "[list]");
    else                         snprintf(buf, sizeof(buf), "%d", v);
    wr_v(vm, dst, mk_str(vm, buf));
}

// ── json_stringify：结构体 / 数组 → JSON 文本（Step 里联机广播属性用）──
//   数字：整数原样，浮点 %.6g（我们槽是 32 位 float，6 位有效数字够）
//   undefined → null（JSON 里没有 undefined）；容器嵌套有深度上限，防真成环时爆栈
static void json_num(char* out, size_t cap, Value v) {
    if (v.t == MEM_FLOAT) { float f; int iv = v.v; memcpy(&f, &iv, 4); snprintf(out, cap, "%.6g", (double)f); }
    else snprintf(out, cap, "%d", v.v);
}
static void json_esc(std::string& out, const std::string& s) {
    out += '"';
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')    { out += '\\'; out += (char)c; }
        else if (c == '\n')           out += "\\n";
        else if (c == '\r')           out += "\\r";
        else if (c == '\t')           out += "\\t";
        else if (c < 0x20)            { /* 其它控制字符：丢掉 */ }
        else                          out += (char)c;
    }
    out += '"';
}
static void json_val(Vm& vm, Value v, std::string& out, int depth) {
    char buf[64];
    if (depth > 16) { out += str_at(S_null); return; }
    switch (v.t) {
    case MEM_STRING:
        if (str_ok(v.v)) json_esc(out, str_at(v.v));
        else out += "\"\"";
        return;
    case MEM_FLOAT:
    case MEM_INT:
        json_num(buf, sizeof(buf), v); out += buf; return;
    case MEM_UNDEFINED:
        out += str_at(S_null); return;
    case MEM_ARRAY:
    case MEM_LIST: {
        out += '[';
        int n = container_size(v);
        for (int i = 0; i < n; i++) {
            if (i) out += ',';
            json_val(vm, container_at(v, i), out, depth + 1);
        }
        out += ']';
        return;
    }
    case MEM_MAP: {
        out += '{';
        std::vector<std::pair<Value, Value> > kv = map_pairs(v);
        for (size_t i = 0; i < kv.size(); i++) {
            if (i) out += ',';
            if (kv[i].first.t == MEM_STRING && kv[i].first.v >= 0 && kv[i].first.v < (int)g_strings.size())
                json_esc(out, str_at(kv[i].first.v));
            else { json_num(buf, sizeof(buf), kv[i].first); out += buf; }
            out += ':';
            json_val(vm, kv[i].second, out, depth + 1);
        }
        out += '}';
        return;
    }
    default:
        out += str_at(S_null); return;
    }
}
void json_stringify(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    std::string out;
    json_val(vm, rd_v(vm, argslot(args, 0)), out, 0);
    wr_v(vm, dst, mk_str(vm, out.c_str()));
}

// ============================================================
// 全局变量（g_globals，进程级：VM 挂掉也不清）
// ============================================================
void variable_global_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    if (rd_t(vm, slot) != MEM_STRING) { wr_bool(vm, dst, false); return; }
    int si = rd_i(vm, slot);
    if (!str_ok(si)) { wr_bool(vm, dst, false); return; }
    wr_bool(vm, dst, global_exists(str_at(si)));
}
void variable_global_get(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int slot = argslot(args, 0);
    Value* p = NULL;
    if (rd_t(vm, slot) == MEM_STRING) {
        int si = rd_i(vm, slot);
        if (str_ok(si)) p = global_var(str_at(si));
    }
    if (!p) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return; }
    wr_v(vm, dst, *p);
}

// ============================================================
// 颜色
// ============================================================
void merge_color(Vm& vm, const uint8_t* args, int argc, int dst) {
    merge_colour(vm, args, argc, dst);       // 实现在 vm_container.cpp（英式拼法那个名字）
}
// make_color_rgb(r, g, b)：GML 的调色板是 BGR 排的 → r | g<<8 | b<<16
//   （hot_card_slot.cpp 里当场手算的就是这个；那处可以不再手算）
void make_color_rgb(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    long long r = (long long)num_at(vm, args, 0);
    long long g = (long long)num_at(vm, args, 1);
    long long b = (long long)num_at(vm, args, 2);
    wr_num(vm, dst, (double)(r | (g << 8) | (b << 16)));
}

// ============================================================
// 项目脚本函数（scripts/ 里的那批；翻译器把它们当内部函数发）
// ============================================================
// damage_enemy(inst, dmg, dmg_type) —— scr_command_VM.gml:2765
//   走**敌人自己的受击事件**（闪白 + 音效 + 护盾结算，含各敌人自己重写的 Other_10），
//   和原版子弹命中时做的事完全一致。别改成"直接扣 hp"——那样会绕过护盾和自定义受击逻辑。
//   返回 1 = 已结算，0 = 实例不存在
void damage_enemy(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int inst = rd_i(vm, argslot(args, 0));
    double dmg = num_at(vm, args, 1);
    std::string type = str_at(S_normal);                     // undefined / 空串 → "normal"
    int ts = argslot(args, 2);
    if (rd_t(vm, ts) == MEM_STRING) {
        int si = rd_i(vm, ts);
        if (str_ok(si) && !str_at(si).empty())
            type = str_at(si);
    }
    if (!inst_get(inst)) { wr_i(vm, dst, 0); return; }
    Value* hs = inst_var_sid(inst, S_hit_sound);         // 有 hit_sound 就播（音频是宿主的 → 空实现）
    if (hs) {
        wr_v(vm, SCRATCH_BASE + 4, *hs);
        wr_i(vm, SCRATCH_BASE + 5, 0);
        wr_i(vm, SCRATCH_BASE + 6, 0);
        int sa[3]; sa[0] = SCRATCH_BASE + 4; sa[1] = SCRATCH_BASE + 5; sa[2] = SCRATCH_BASE + 6;
        call_slots(vm, audio_play_sound, sa, 3, -1);
    }
    inst_var_set_sid(inst, S_damage_amount, mk_num(dmg));
    inst_var_set_sid(inst, S_damage_type, mk_str(vm, type.c_str()));
    // with (inst) { ... event_user(0) } —— 事件号 0 → _OBJECT_USER_EVENT_0
    wr_i(vm, SCRATCH_BASE + 4, inst);
    wr_i(vm, SCRATCH_BASE + 5, 0);
    int ea[2]; ea[0] = SCRATCH_BASE + 4; ea[1] = SCRATCH_BASE + 5;
    call_slots(vm, run_event_user, ea, 2, -1);
    wr_i(vm, dst, 1);
}

// ============================================================
// 网格 / 坐标
//   照 scripts/get_grid_position_from_world 和 scripts/card_depth 抄的；
//   几何参数从全局变量拿（宿主设置）：grid_offset_x/y、grid_cell_size_x/y、grid_cols/rows
// ============================================================
static void put_cell_struct(Vm& vm, int dst, double col, double row, double cx, double cy) {
    int map = SCRATCH_BASE + 16, ks = SCRATCH_BASE + 17, vs = SCRATCH_BASE + 18;
    struct_begin(vm, map);
    wr_v(vm, ks, mk_str_sid(S_col)); wr_num(vm, vs, col); struct_put(vm, map, ks, vs);
    wr_v(vm, ks, mk_str_sid(S_row)); wr_num(vm, vs, row); struct_put(vm, map, ks, vs);
    wr_v(vm, ks, mk_str_sid(S_x));   wr_num(vm, vs, cx);  struct_put(vm, map, ks, vs);
    wr_v(vm, ks, mk_str_sid(S_y));   wr_num(vm, vs, cy);  struct_put(vm, map, ks, vs);
    Value h = rd_v(vm, map);
    wr_v(vm, dst, h);                        // dst 接一份
    Value z; z.t = MEM_INT; z.v = 0;
    slot_write(vm, map, z);                  // 临时槽还回它那份
}

void get_world_position_from_grid(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double col = num_at(vm, args, 0), row = num_at(vm, args, 1);
    double csx = gnum_sid(S_grid_cell_size_x), csy = gnum_sid(S_grid_cell_size_y);
    double cx = gnum_sid(S_grid_offset_x) + col * csx + csx / 2;
    double cy = gnum_sid(S_grid_offset_y) + row * csy + csy / 2;
    put_cell_struct(vm, dst, col, row, cx, cy);
}

void get_grid_position_from_world(Vm& vm, const uint8_t* args, int argc, int dst) {
    double wx = num_at(vm, args, 0), wy = num_at(vm, args, 1);
    bool raw = (argc >= 3) && (num_at(vm, args, 2) != 0);
    double ox = gnum_sid(S_grid_offset_x), oy = gnum_sid(S_grid_offset_y);
    double csx = gnum_sid(S_grid_cell_size_x), csy = gnum_sid(S_grid_cell_size_y);
    double cols = gnum_sid(S_grid_cols), rows = gnum_sid(S_grid_rows);
    double col = std::floor((wx - ox) / csx);
    double row = std::floor((wy - oy) / csy);
    if (!raw) {                              // 回绕到扩展区（照 GML 抄）
        if (col < 0)          col = col + cols + 64;
        if (col >= cols + 64) col = col - cols - 64;
        if (row < 0)          row = row + rows + 64;
        if (row >= rows + 64) row = row - rows - 64;
    }
    double cx = ox + col * csx + csx / 2;
    double cy = oy + row * csy + csy / 2;
    put_cell_struct(vm, dst, col, row, cx, cy);
}

// calculate_plant_depth(col, row, plant_type)
//   层序 lilypad → coffee 每层往后挪 1 点，最后夹在 -330..-10
void calculate_plant_depth(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    double col = num_at(vm, args, 0), row = num_at(vm, args, 1);
    double base = -10 - (row * 45) - (col * 5);
    double offset = 0;
    int slot = argslot(args, 2);
    if (rd_t(vm, slot) == MEM_STRING) {
        int si = rd_i(vm, slot);
        if (str_ok(si)) {
            static const int order[] = { S_lilypad, S_shield_inner, S_normal, S_shield_outer, S_coffee };
            const std::string& t = str_at(si);
            for (int i = 0; i < 5; i++)
                if (t == str_at(order[i])) { offset = -i; break; }
        }
    }
    double d = base + offset;
    if (d < -330) d = -330;
    if (d > -10)  d = -10;
    wr_num(vm, dst, d);
}

// ============================================================
// 引擎侧占位（宿主那头的活；VM 里没有精灵 / 网络 / 音频，只能什么都不做）
//   为什么写实而不是留空壳：这几个在 GML 里本来就没返回值，
//   留空壳是"没写"，写实能在这里留一句话说明是"有意不做事"。
// ============================================================
void audio_play_sound(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(vm, dst, 0);                        // 没有声音系统 → 返回 0（无效 id）
}
void send_message(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)vm; (void)args; (void)argc; (void)dst;   // 网络是宿主的
}
void show_debug_message(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int slot = argslot(args, 0);
    int t = rd_t(vm, slot), v = rd_i(vm, slot);
    if (t == MEM_STRING && v >= 0 && v < (int)g_strings.size())
        printf("[vm] %s\n", str_at(v).c_str());
    else
        printf("[vm] show_debug_message: non-string argument\n");
}

// ============================================================
// 宿主负责画 / 量的这批：**先只写签名，实体留空**
//   Player / Effects 那 59 个对象要用；名字挂在 vm_native.cpp 表尾，追加在末尾，
//   所以已有的 native id 一个都没动（老的 361 个 bin 原样有效）。
//   ⚠ 空实体 = 不写 dst → 取返回值的地方拿到 0：
//     · 绘制族（draw_*）：0 无所谓，画是宿主的事；
//     · string_width_ext / string_height_ext：要字体系统量，宿主给；
//     · network_broadcast_active_skill：网络是宿主的（同 send_message）。
//   （ceil / make_color_rgb 是纯数学，已经写实了，见上面"数学"/"颜色"两节）
// ============================================================
void draw_set_color(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_set_alpha(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_set_font(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_set_halign(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_set_valign(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_rectangle(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_rectangle_color(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void draw_roundrect(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void string_width_ext(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void string_height_ext(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
void network_broadcast_active_skill(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
