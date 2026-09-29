// ============================================================
// host_state.cpp — B1 状态区：**宿主引擎每帧覆盖着写、VM 按槽号读**的那块内存
//
//   这是第三块共享内存（前两块是 host_shared.cpp 的出站流 / host_packet.cpp 的入站包）：
//     出站 host_shared  VM 往后写，引擎拿 base + cursor 自己读
//     入站 host_packet  引擎递一包，VM 当场回调解掉
//     B1   host_state   **引擎每帧把整块重写**，VM 想什么时候读就什么时候读（没有回调、没有游标）
//
//   为什么是"覆盖式"、连个清都不用：
//     输入本来就是"这一帧的输入长什么样"——引擎每帧重写整块，就不存在"上一帧的残留"，
//     也就不需要"增量追加 + 消费游标"那套（那套要两个写者/读者对齐，输入这种高频小数据不值当）。
//     谁想清（比如引擎自己收了输入子系统想归零）就调 host_state_clear()，这里只给口。
//
//   为什么这块**不在本文件里定布局**：
//     槽号 / 字段类型 / 位图规则全在契约 host/host_layout.h（生成物，
//     真源 tools/gen_host_layout.py）。本文件只做一件事：**把这块内存拿出来**，
//     因为"约定"必须只有一份真源 —— 在这儿再抄一遍槽号就是给以后埋一个静默读错。
//
//   ⚠ 分层：本文件属于 host 层，**不许** include 内层头（core/vm_def.h）——
//     所以"一个槽长什么样"用 host.h 里的 HostStateSlot（同布局 POD）重述，见那里的注释。
// ============================================================
#include "host.h"
#include "host_layout.h"        // HOST_LAYOUT_STATE_SLOTS（契约；由 tools/gen_host_layout.py 生成）

namespace {

// 唯一的那块 B1 —— 进程级、静态、**全 0 初始化**。
//   为什么不 lazy 建：host_state_base() 应该**永远**不是 NULL（和 host_shm_base() 一个道理），
//   引擎启动时就能把地址记下来，不用等谁先写一次。
//   槽全 0 = {t = 0（MEM_INT）, v = 0}：是个**确定的**初值 —— "还没接引擎"时读出来是
//   "没有按键、鼠标在 0"（0/0 是合法输入），而不是随机的垃圾。
HostStateSlot g_state[HOST_LAYOUT_STATE_SLOTS];

}  // namespace

// B1 首地址（槽数组）。引擎拿到它，按 host_layout.h 的槽号直接写：
//   base[HOST_LAYOUT_STATE_MOUSE_GUI_X] = {类型, 值};
//   ⚠ 这块地址**不会变**（就是上面那个静态数组）—— 但引擎照旧每次开工前现取一次，
//     和 host_shm_base() 保持同一个用法，将来真改成可搬迁的内存也不用改引擎。
HostStateSlot* host_state_base() { return g_state; }

// 槽数（= 契约里的 HOST_LAYOUT_STATE_SLOTS）。
//   给引擎一个"我这边认的块有多大"的回答：和 host_layout_version() 一起，
//   引擎开工前拿这两个数对一下暗号，比"按自己记的 64 去写"稳。
int host_state_slots() { return HOST_LAYOUT_STATE_SLOTS; }

// 全部槽清成 undefined（引擎想用才用；覆盖式写的场合**不需要**每帧清）。
//   ⚠ 只清成 undefined，**不**清成"没按 / 坐标 0"：清成 0 会让
//     "引擎还没写"和"引擎写的就是 0"分不出来（比如 mouse_check_button 会在没输入时
//     报"左键按着"）。undefined 在 VM 侧读出来一律当"没有"，语义正好。
//   ⚠ 这里写的类型号 HOST_STATE_T_UNDEFINED 必须等于内层的 MEM_UNDEFINED ——
//     由外层（outer/host_state_in.cpp）拿 static_assert 钉死，抄岔了编译期就炸。
void host_state_clear() {
    for (int i = 0; i < HOST_LAYOUT_STATE_SLOTS; i++) {
        g_state[i].t = HOST_STATE_T_UNDEFINED;
        g_state[i].v = 0;
    }
}
