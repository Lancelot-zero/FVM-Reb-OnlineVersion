// ============================================================
// host_shared.cpp — 出站共享内存：**VM 往后写、宿主引擎往前读**的一条单向通道
//
//   和 host_buffer.cpp 的区别（别混）：
//     host_buffer  一个 buf 一个句柄、读写位置在句柄上 —— 是 **GML 自己的** buffer_* 语义，
//                  VM 想 seek / 想读回来都行（因为那是虚拟机自己的数据）
//     host_shared  **进程级就这一块**、没有句柄、只有"追加"和"写游标" ——
//                  这是给宿主引擎看的：VM 把"发生了什么"按顺序倒出来，
//                  引擎拿 base + cursor 两样东西就能读走（不需要回调、不需要跨 DLL 传对象）
//
//   为什么是"只增不减的游标 + 引擎自己复位"：
//     单向通道的两端各自只关心一件事 —— VM 只管"我再写多少"，引擎只管"我读到哪"。
//     要是 VM 能看见读位置（或者能复位），就会出现"引擎还没读走、VM 把它冲掉"的竞态；
//     现在 VM 连 reset 都调不到（没暴露给它），从接口上就杜绝了这种踩踏。
//     **复位时机 = "引擎已经消费完这一段了"** —— 这个信息只有引擎有，所以只有引擎该调。
//
//   内存策略：初始 64KB → 不够**翻倍** → 上限 16MB，到顶就 write 返回 false（**不截断**）。
//     为什么翻倍而不是一步到 16MB：绝大多数帧只写几十字节，一上来 16MB 白占；
//     为什么上限 16MB：这是"宿主读走"的临时区，不是存档，给太大不如让引擎早点读。
//
//   ⚠ 字节规则（编码）只在本文件里：VM 侧的 VM_HostPush 只负责"照 Value 的类型挑一支调"。
// ============================================================
#include "host.h"
#include "host_layout.h"        // HOST_LAYOUT_VERSION（契约版本；由 tools/gen_host_layout.py 生成）
#include <cstring>
#include <vector>

namespace {

const int SHM_INIT = 64 * 1024;             // 初始容量 64KB
const int SHM_MAX  = 16 * 1024 * 1024;      // 上限 16MB（翻倍的终点；到这里还装不下就拒绝）

// 唯一的那块内存 + 唯一的那个写游标。
//   ⚠ 只增不减：g_cursor 只有 host_shm_reset 会动，各支 push / write 只会往前推。
//   ⚠ 初始就按 64KB 建好（不 lazy）：host_shm_base() 因此**永远**不是 NULL，
//     引擎启动时就能把地址记下来，不用等 VM 写过一次再问。
std::vector<uint8_t> g_shm((size_t)SHM_INIT, 0);
int                  g_cursor = 0;

// ============================================================
// ★【编码格式】★ —— 整个通道的字节规则**只有这一块**，格式要改就改这里
//
//   ⚠⚠ **包里没有类型信息** ⚠⚠
//     写进内存的就是**纯 payload**，一个类型字节都没有：每种值怎么解读，靠
//     "宿主引擎 ↔ DLL 侧"**事先约定的布局**（哪个调用点按什么顺序、什么类型传）。
//     好处：省字节、读写两边各自按约定直接搬，不用先解一层 TLV。
//     代价（**这是前提，不是 bug**）：约定错了不会被发现 —— 调用方必须严格按约定的
//     顺序和类型传参；传错类型不会报错，对方只会照着错的口径解读出一堆垃圾。
//     所以这份注释就是"约定"的落点：改这里 = 改协议，两端一起改。
//
//   每种值编成（**全部小端**）：
//
//     int       → s32（4 字节）                      host_shm_push_i32
//     float     → f32（4 字节，IEEE 位模式原样）      host_shm_push_f32
//     string    → u32 长度（**字节数**）+ UTF-8 字节  host_shm_push_bytes
//     buf       → u32 长度（**字节数**）+ 裸字节      host_shm_push_bytes（和 string 共用）
//     undefined → **什么都不写**（0 字节）            哪支都不调
//     其它类型（数组 / map / grid / list …）→ 一律当 undefined → 也是 0 字节
//
//   ⚠ 长度是 **u32**：调用方（VM 侧）要把 size_t 收成 uint32_t 再传，
//     真出现"一段超过 4GB"这种（64 位下才可能）→ VM 那边直接算失败，不截断、不谎报长度。
//   ⚠ 一次调用的边界 = **VM_HostPush 的返回值（写了几字节）**：流里没有边界标记，
//     引擎按约定自己往下走。
// ============================================================

// 小端写 4 字节（s32 位模式 / f32 位模式 / string·buf 的 u32 长度都走它）
void put_le32(uint8_t* out, uint32_t x) {
    out[0] = (uint8_t)( x        & 0xFF);
    out[1] = (uint8_t)((x >>  8) & 0xFF);
    out[2] = (uint8_t)((x >> 16) & 0xFF);
    out[3] = (uint8_t)((x >> 24) & 0xFF);
}

// 这一笔（再写 extra 字节）还在 16MB 以内吗
bool room_for(long long extra) { return (long long)g_cursor + extra <= (long long)SHM_MAX; }

// 把一块内存**原样**追加到游标后面（不够就翻倍扩容）。**前提：调用方已经确认装得下** ——
//   所以它自己不会失败：这样"先 check 再 append"两步合起来就是**原子的一笔**。
//   扩容会换地址 → 引擎每次消费前重新问 host_shm_base()（见 host.h 的注释）。
void append_raw(const void* p, size_t n) {
    if (n == 0) return;
    size_t need = (size_t)g_cursor + n;
    if (need > g_shm.size()) {
        size_t cap = g_shm.size();
        while (cap < need) cap *= 2;                      // 翻倍扩容
        if (cap > (size_t)SHM_MAX) cap = (size_t)SHM_MAX; // 封顶（need <= MAX 时这行不会触发）
        g_shm.resize(cap, 0);                             // 新扩的那截补 0
    }
    memcpy(&g_shm[(size_t)g_cursor], p, n);
    g_cursor += (int)n;
}

}  // namespace

// 游标归零（数据不清 —— 引擎只看 [0, cursor)，清不清无所谓，省一次 memset）
void host_shm_reset() { g_cursor = 0; }

// 首地址：给引擎的那个指针。std::vector 扩容会换地址 → 引擎**每次消费前**重新问一次
//   （注释写在这，是因为"缓存了 base 之后扩容"是这块内存唯一能踩的坑）
uint8_t* host_shm_base() { return g_shm.empty() ? NULL : &g_shm[0]; }

int host_shm_size() { return (int)g_shm.size(); }

int host_shm_cursor() { return g_cursor; }

// 往游标后面追加 n 字节。成功 1 / 失败 0（**失败时游标和内容一个字节都不动**）
//   失败只有一种：这一笔会让游标越过 16MB 上限 —— 宁可让调用方响亮报错，也不静默截断
//   （截断 = 引擎按约定的布局解析到一半突然断了，比"这次没写进去"难查得多）
bool host_shm_write(const void* p, int n) {
    if (n < 0 || (!p && n > 0)) return false;
    if (n == 0) return true;                                  // 写 0 字节 = 成功，不动游标
    if (!room_for(n)) return false;                           // 超上限：不写、不截断、游标不动
    append_raw(p, (size_t)n);
    return true;
}

// ── 按类型追加的那三支（宿主/引擎侧直接可用；VM 侧只是挑一支调）──

// int → s32（4 字节小端）
bool host_shm_push_i32(int32_t v) {
    if (!room_for(4)) return false;
    uint8_t b[4];
    put_le32(b, (uint32_t)v);                                 // 槽里就是 s32 的位模式，直接搬
    append_raw(b, sizeof(b));
    return true;
}

// float → f32（4 字节小端，IEEE 位模式原样）
bool host_shm_push_f32(float v) {
    if (!room_for(4)) return false;
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));                           // 原样取位模式（不做数值转换）
    uint8_t b[4];
    put_le32(b, bits);
    append_raw(b, sizeof(b));
    return true;
}

// string / buf → u32 长度（字节数，小端）+ n 字节
//   ⚠ **长度和内容一起判、一起写**：装不下就一个字节都不写（不然流里会留一个"长度说有 N 字节、
//     后面却没有"的假头，引擎照长度跳过去就整条错位）。所以这里不拆成两次 append 判断。
bool host_shm_push_bytes(const void* p, uint32_t n) {
    if (!p && n > 0) return false;
    if (!room_for(4 + (long long)n)) return false;             // 长度字段 + 内容，一起原子
    uint8_t b[4];
    put_le32(b, n);
    append_raw(b, sizeof(b));
    append_raw(p, (size_t)n);                                  // n == 0 时 append_raw 直接返回
    return true;
}

// ============================================================
// 布局契约版本：引擎开工前**对暗号**用的那一个导出（理由见 host.h 第 8 节）
//
//   为什么和共享内存放同一个编译单元：这套约定（B1 状态 / B2 命令 / A 请求流）
//   就是"不链接、只约定"的那份东西，版本号跟它绑在一起最不容易走散 ——
//   以后真拆成两个文件，第一个忘更新的也是它。
//   函数体就一句：读的是生成器写死的编译期常量，没有运行期状态、没有初始化顺序问题。
// ============================================================
HOST_LAYOUT_API int host_layout_version() {
    return HOST_LAYOUT_VERSION;
}
