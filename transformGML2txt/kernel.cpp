// ============================================================
// kernel.cpp — 内核：和"块"打交道的内部函数
//   新写实的内部函数放这儿，别堆进 vm_native.cpp（那个文件只放空壳）
//   已实现的内部函数要从 vm.h 的 NATIVE_FN_LIST 里拿掉，
//   改挂到 vm_native.cpp 表尾的显式列表（这样 id 还是连续唯一的）
// ============================================================

#include "vm.h"

// ── run_block(块名) ──
// 跑**本 Vm 那个对象**的某个块（一个 bin = 一个 Vm = 一个对象）。
// 调用约定：参数就一个字符串槽（块名）；返回值由被调块写进 RET_SLOT（0 号槽），
// 调用方（OP_NATIVE 的 dst）自己把它取走。
// 会递归进来（块里再调块），同一 Vm 上递归跑解释器。
void run_block(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 1) return;
    int slot = argslot(args, 0);
    int ty   = rd_t(vm, slot);
    if (ty != MEM_STRING) {
        printf("[vm] run_block: argument is not a string\n");
        return;
    }
    int si = rd_i(vm, slot);
    if (si < 0 || si >= (int)vm.strings.size()) {
        printf("[vm] run_block: bad string index %d\n", si);
        return;
    }
    vm.run_named(vm.strings[(size_t)si]);
}

// ── 跑"某个实例的某个块" ──
//   ⚠ 一个 Vm 一个对象，所以查块之前**必须先确认"这个 Vm 就是那个对象"**：
//     否则"对象没装载"会被误当成"就在本 Vm 里"，把调用方自己的同名块再跑一遍
//     （obj_melon_shield 的 Create 建 obj_melon_shield_inner，一单对象没装载就无限递归爆栈）。
//   cur_instance 是派发上下文，进来看一眼原来是什么，跑完换回去（事件里再发事件会嵌套进来）。
bool run_block_on(Vm& vm, const std::string& obj, int inst, const std::string& block) {
    // ① 硬编码块（父对象 / 宿主对象那批，它们没有自己的 Vm）
    if (has_hot_block(obj, block)) {
        int old_ins = vm.cur_instance;
        vm.cur_instance = inst;
        bool ok = run_hot_block(vm, obj, block);
        vm.cur_instance = old_ins;
        return ok;
    }
    // ② 翻译出来的块：得有装着这个对象的 Vm，而且那个 Vm 里真有这个块
    Vm* owner = vm_of_object(obj);
    if (!owner || owner->obj != obj || !owner->find_block(block)) return false;
    int old_ins = owner->cur_instance;
    owner->cur_instance = inst;
    bool ok = owner->run_named(block);
    owner->cur_instance = old_ins;
    return ok;
}

// ── run_event_user(实例, 事件号) ──
//   翻译器里 with (别人) { event_user(k) } 走这个：对**那一格实例**发事件。
//   事件号 → 块名："_OBJECT_USER_EVENT_%d"（跟翻译器的命名一致）
void run_event_user(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 2) return;
    int inst = rd_i(vm, argslot(args, 0));
    int ev   = rd_i(vm, argslot(args, 1));
    Instance* in = inst_get(inst);
    if (!in) {
        printf("[vm] run_event_user: instance %d does not exist\n", inst);
        return;
    }
    char name[64];
    snprintf(name, sizeof(name), "_OBJECT_USER_EVENT_%d", ev);
    // ⚠ 事件要**带继承**：obj_enemy_mod 自己没有 UserEvent 0，实际跑的是 obj_enemy_parent 的
    //   （damage_enemy → event_user(0) 这条链就靠它；不走继承的话不掉血、不闪白）
    if (!run_hot_event_inherited(vm, inst, name))
        run_block_on(vm, in->obj, inst, name);          // 硬编码里没有 → 走解释器那条路
    wr_i(vm, dst, 0);
}
