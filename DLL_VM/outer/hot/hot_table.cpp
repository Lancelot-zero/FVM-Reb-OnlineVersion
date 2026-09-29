// ============================================================
// hot_table.cpp — 热块的查找/派发（**空缝**，一个对象一个文件的整块事件还没开始）
//
//   设计：热点对象的事件用 C++ 整块重写（老代码 hot/ 那 14 个文件 6500 行）。
//   派发时的规矩：**先问有没有热块，有就走它，没有才把字节码块交给 VM_RUN**。
//
//   现在一律"没有" → 全部回落到字节码解释 ✓（功能不缺，只是慢）
//   以后加：一张 (对象名池下标, 块名池下标) → 函数的表，这里照着查。
// ============================================================
#include "../outer.h"

bool has_hot_block_sid(VM_INT obj_sid, VM_INT block_sid) {
    (void)obj_sid; (void)block_sid;
    return false;
}

bool run_hot_block_sid(vm_bin& vm, VM_INT obj_sid, VM_INT block_sid) {
    (void)vm; (void)obj_sid; (void)block_sid;
    return false;
}

// 事件带继承的热块（obj_enemy_mod 自己没有 UserEvent0 → 跑 obj_enemy_parent 的）
bool run_hot_event_inherited(vm_bin& vm, VM_INT inst_id, VM_INT event_sid) {
    (void)vm; (void)inst_id; (void)event_sid;
    return false;
}
