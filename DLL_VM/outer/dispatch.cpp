// ============================================================
// dispatch.cpp — 派发类（3 条）：run_block / run_event_user / VM_Execute
//
//   批 native 干的事是"跑另一个块"：先看有没有 C++ 热块，没有就把字节码块交给 VM_RUN。
//   "当前实例是谁"由内层的调用栈回答（run_inst / cur_vm），不再挂在 VM 上。
//
//   ⚠ 块名一律是**池下标**（运行时零文本）：run_event_user 的事件名也只 intern 一次，
//     之后查表（0..15 够了；超了就是明确的错误，不是偷偷拼字符串）。
// ============================================================
#include "outer.h"
#include <cstdio>       // snprintf（只在初始化时拼一次动态事件名）

// ── 跑"某个对象的某个块" ──
//   ⚠ 查块之前先问"这个对象归谁管"：那个对象没装载 → 直接 false，
//     **不许**回落到调用方自己的同名块（obj_melon_shield 那条链会无限递归爆栈）
bool run_block_on(vm_bin& vm, VM_INT obj_sid, VM_INT inst, VM_INT block_sid) {
    (void)vm;
    if (has_hot_block_sid(obj_sid, block_sid))       // ① C++ 硬编码热块
        return run_hot_block_sid(*cur_vm(), obj_sid, block_sid);
    vm_bin* owner = vm_of_object_sid(obj_sid);       // ② 翻译出来的块
    if (!owner) return false;
    if (owner->blocks.find(block_sid) == owner->blocks.end()) return false;
    vm_run_event(*owner, block_sid, inst);
    return true;
}

// ── run_block(块名) ──
//   跑**本 VM**（正在跑这块码的那个 VM）的某个块；返回值在槽 0，调用方自己取走。
void run_block(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { vm_set_err(VM_E_BLOCK_NOT_FOUND, 0, 0); return; }
    Value n = arg_v(mem, args, 0);
    if (n.t != MEM_STRING || !str_ok(n.v)) { vm_set_err(VM_E_BLOCK_NOT_FOUND, n.v, 0); return; }
    vm_bin* vm = cur_vm();
    if (!vm || vm->mem != mem) { vm_set_err(VM_E_BLOCK_NOT_FOUND, n.v, 0); return; }   // callbin 帧里没有自己的 VM
    if (vm->blocks.find(n.v) == vm->blocks.end()) { vm_set_err(VM_E_BLOCK_NOT_FOUND, n.v, 0); return; }
    vm_run_event(*vm, n.v, run_inst());
    wr_v(mem, dst, mem[0]);                          // 被调块把返回值写在槽 0
}

// ── run_event_user(实例, 事件号) ──
//   with (别人) { event_user(k) } 走这条：对**那一格实例**发事件。
//   事件号 → 块名 "_OBJECT_USER_EVENT_k"（和翻译器命名一致）：只 intern 一次，之后查表
static const int USER_EVENT_MAX = 16;
static VM_INT    g_user_event_sid[USER_EVENT_MAX];
static bool      g_user_event_ready = false;

static void user_event_init() {
    if (g_user_event_ready) return;
    g_user_event_ready = true;
    // 事件号 → 块名 "_OBJECT_USER_EVENT_k"
    //   ⚠ 这是**允许的动态字符串**：k 是运行时给的（0..USER_EVENT_MAX-1），
    //     块名的形状（前缀 "_OBJECT_USER_EVENT_" + 十进制数字）定不了，
    //     没法预先收进 const_str.h 的名单（名单里只有固定的 _OBJECT_USER_EVENT_0/1/2）。
    //     前缀是定死的文本，但这里跟数字一起拼 —— 全项目就这一处这种拼法。
    //   只在**初始化**时拼一次并 intern（之后查表），运行时零文本
    for (int k = 0; k < USER_EVENT_MAX; k++) {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "_OBJECT_USER_EVENT_%d", k);
        g_user_event_sid[k] = intern_str(std::string(buf, (size_t)n));
    }
}

void run_event_user(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) return;
    VM_INT inst = arg_i(mem, args, 0);
    VM_INT ev   = arg_i(mem, args, 1);
    user_event_init();
    if (ev < 0 || ev >= USER_EVENT_MAX) { vm_set_err(VM_E_BLOCK_NOT_FOUND, 0, ev); return; }
    Instance* in = inst_get(inst);
    if (!in) { vm_set_err(VM_E_BLOCK_NOT_FOUND, 0, inst); return; }
    // 事件带继承：硬编码那条链先试（父对象的 UserEvent），没有再走解释器
    if (!run_hot_event_inherited(*cur_vm(), inst, g_user_event_sid[ev]))
        run_block_on(*cur_vm(), in->obj, inst, g_user_event_sid[ev]);
    wr_i(mem, dst, 0);
}

// ── VM_Execute ──
//   老代码里就是个空壳（不知道平台想让它干什么）→ **留出来等定**
void VM_Execute(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    vm_set_err(VM_E_NOT_IMPL, S_VM_Execute, 0);
    wr_v(mem, dst, mk_undef());
}
