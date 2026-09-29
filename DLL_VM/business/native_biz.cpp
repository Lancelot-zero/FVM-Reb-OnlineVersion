// ============================================================
// business/native_biz.cpp — 归**业务层**的 native（BIZ_NATIVE_LIST，4 条）
//
//   damage_enemy / calculate_plant_depth / get_grid_position_from_world / get_world_position_from_grid
//
//   为什么单独一个文件、不留在 host_fn.cpp：
//     这 4 条不是"宿主平台原语"（不需要引擎 / 系统），而是**项目业务规则** ——
//     网格数学 3 条 + 伤害结算 1 条。它们要的是"这个游戏的地图多大、格子多宽、
//     植物怎么算深度、伤害往哪张表上记"，这些属于业务层，和 host_fn.cpp 那 149 条
//     真引擎 / 业务转发分开写（表里的顺序和条数不动，见 outer_split.h）。
//
//   语义真源：
//     · 网格 2 条      scripts/get_grid_position_from_world、scripts/get_world_position_from_grid
//     · 深度 1 条      scripts/card_depth
//     · 伤害 1 条      scripts/scr_command_VM/scr_command_VM.gml 的 damage_enemy
//     · A 批（outer/game_a.cpp）里已经有**等价的本地实现**：
//         grid_world(col,row,&x,&y) / grid_wrap_col / grid_wrap_row / plant_depth(col,row,type)
//       ——A 批是"自己内部要用，所以先local 了一份"，以后收编到这里来（那时 A 批改成调这 4 条，
//       免得同一套算式有两份，改了这边忘了那边）。
//
//   本次：**先建好定义**（空壳：置 VM_E_NOT_IMPL + 回 undefined），和 host_fn.cpp 一个样子。
//   这样链接不缺符号、表体检也照样把"还没实现的"数出来，而不是静默算错。
//
//   ⚠ 只允许写这一个文件（别把 host_fn.cpp / game_a.cpp 的名单搬过来）。
// ============================================================
#include "../outer/outer.h"

#define STUB(NAME) \
    void NAME(Value* mem, const uint8_t* args, int argc, int dst) { \
        (void)args; (void)argc; \
        vm_set_err(VM_E_NOT_IMPL, S_##NAME, 0); \
        wr_v(mem, dst, mk_undef()); \
    }
BIZ_NATIVE_LIST(STUB)
#undef STUB
