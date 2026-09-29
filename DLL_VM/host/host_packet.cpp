// ============================================================
// host_packet.cpp — 入站包：**宿主引擎把一包字节递进 VM** 的那条单向通道
//
//   和 host_shared.cpp（出站）配成一对，方向相反：
//     host_shared   VM 往后写，引擎拿 base + cursor **自己读**（没有回调）
//     host_packet   引擎把 buf + size 递进来，调 host_packet_apply() —— **一次回调**
//
//   为什么是回调、不是"引擎写进一块共享内存、VM 轮询"：
//     入站的东西是**指令**（设一个全局 / 调一次函数），不是数据流。指令要当场生效，
//     而"轮到 VM 时"只有 VM 自己知道 —— 引擎只要调一下，这一包就在这一刻做完。
//     反过来做（引擎写、VM 轮询）就得多一份队列 + 一个"什么时候轮询"的约定，
//     等于把同步问题推给别人；这里一步到位。
//
//   为什么这儿只有"一个函数指针"、别的什么都不判：
//     host 层照旧**不认识 Value / 槽 / VM**（见 host.h 的分层说明）。包里的字节规则
//     完全归 VM 侧（outer/packet.cpp）。这一层只保管"引擎该调谁"。
//     buf / size 的合法性也**故意不在这儿拦**：那一层才能 vm_set_err 响亮报错，
//     在这儿先 return 负数的话，"坏包"就只剩一个没人看得见的返回值了 —— 用户明确要求
//     坏包必须响亮，所以这层只做**注册判断**。
//
//   ⚠ 幂等：注册口可以重复调（VM 可能被装载两次 / 测试里反复初始化），
//     就是一次赋值，没有副作用。传 NULL = 撤销注册（插件 DLL 卸载前那种场景）。
//   ⚠ 没注册就调 → 返回负数、**绝不崩**：这是给引擎的"这条路还没接上"的确定回答。
// ============================================================
#include "host.h"

namespace {

// 引擎注册进来的那个 handler。**零初始化**：跨编译单元的静态初始化顺序未定义，
//   用 POD 静态指针（不是全局对象），谁先碰都对。
HostPacketFn g_packet_fn = NULL;

// 没注册 handler 时的返回值。约定：<0 = 失败（和 handler 自己的失败约定一致），
//   ⚠ 这个值**不是** VM 的错误码 —— host 层不认识 VM 的错误码（也不该认识）。
const int HOST_PACKET_NO_HANDLER = -1;

}  // namespace

// 注册 / 换 / 撤销 handler（幂等；可重复调）
void host_packet_set_handler(HostPacketFn fn) {
    g_packet_fn = fn;
}

// 引擎调这个把一包交给 VM：
//   注册过 → **原样转发**给 handler，返回它的结果（0 = 成功，<0 = 失败）
//   没注册 → 返回 HOST_PACKET_NO_HANDLER（负数），一个字节都不碰、不崩
//   ⚠ buf / size 不在这儿判（理由见文件头）：handler 得有机会 vm_set_err
int host_packet_apply(void* buf, int size) {
    if (!g_packet_fn) return HOST_PACKET_NO_HANDLER;
    return g_packet_fn(buf, size);
}
