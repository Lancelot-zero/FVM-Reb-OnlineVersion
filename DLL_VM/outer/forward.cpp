// ============================================================
// forward.cpp — 转发类（79 条）
//
//   这批 native **没有 C++ 实现**（也没往表里写实现）：交给"名字 → 实现"的统一查找 ——
//     ① DLL 函数表命中 → **直通**：同一份 mem / args / dst，一次间接调用，零搬运
//     ② 否则 callbin 表（借公共内存的帧跑字节码）→ 要么答上，要么明确说"没这个函数"
//   目的：**不留静默空操作**，而且给哪条写实现 = 注册一个同名函数，表顺序一个字不用动。
//
//   ⚠ 不要用 mem 里的临时槽拼参数表：
//     OP_NATIVE 也会从 **callbin 帧**（mem = g_call_mem + 偏移，只有 cb.value 个槽）里发出来，
//     往 SCRATCH_BASE 那种"高位槽"写就落到帧外（轻则踩公共内存空闲区，重则越过整块公共内存）。
//     直通那条路本来就不占槽；callbin 那条路把实参收进 C++ 栈上的数组。
// ============================================================
#include "outer.h"

void native_forward(Value* mem, const uint8_t* args, int argc, int dst, VM_INT name_sid) {
    vm_call_by_name(name_sid, mem, args, argc, dst);
}

#define FWD_FN(NAME) \
    void NAME(Value* mem, const uint8_t* args, int argc, int dst) { \
        native_forward(mem, args, argc, dst, S_##NAME); \
    }
FORWARD_LIST(FWD_FN)
#undef FWD_FN
