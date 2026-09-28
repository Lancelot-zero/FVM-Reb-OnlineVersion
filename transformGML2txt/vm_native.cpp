// ============================================================
// vm_native.cpp — 内部函数（OP_NATIVE 调用）+ 函数表
//   这个文件只放空壳；写实的实现挂在表尾，实体在
//   vm_container.cpp（容器/数组）、vm_instance.cpp（实例）、
//   vm_builtin.cpp（数学/类型/字符串/全局/坐标）、kernel.cpp（和块打交道的）
// ============================================================

#include "vm.h"


// ============================================================
// 未实现的 native 统一兜底：转成 VM_CallFunc("同名", 实参…)
//   名字用池下标直接给出（零哈希），实参照搬，返回值走同一个 dst。
//   目的：**不留静默空操作** —— 调用要么被脚本 / callbin / 宿主接住，
//         要么在 VM_CallFunc 里明确报"没有这个函数"。
//   ⚠ 临时槽从 SCRATCH_BASE+32 起：hot 的 call_fn_sid 用的是 0..8，
//     call_ret 用 8，不能撞。
// ============================================================
static void native_forward(Vm& vm, const uint8_t* args, int argc, int dst, int name_sid) {
    int n = (argc > 8) ? 8 : argc;
    int slots[9];
    slots[0] = SCRATCH_BASE + 32;
    Value nm; nm.t = MEM_STRING; nm.v = name_sid;
    wr_v(vm, slots[0], nm);
    for (int i = 0; i < n; i++) {
        slots[i + 1] = SCRATCH_BASE + 33 + i;
        wr_v(vm, slots[i + 1], rd_v(vm, argslot(args, i)));
    }
    int gi = S_VM_CallFunc - SID_GAME_BASE;
    Fn fn = (gi >= 0 && gi < GAME_FN_COUNT) ? kGameFns[gi] : NULL;
    if (!fn) {
        if (dst >= 0) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); }
        return;
    }
    fn(vm, (const uint8_t*)slots, n + 1, dst);
}

#define NATIVE_STUB(NAME) \
    static void NAME(Vm& vm, const uint8_t* args, int argc, int dst) { native_forward(vm, args, argc, dst, S_##NAME); }
NATIVE_FN_LIST(NATIVE_STUB)
#undef NATIVE_STUB

// 占位接口：先什么都不做（实现以后挂到表尾，顺序不能变）
#define PLACEHOLDER_STUB(NAME) \
    static void NAME(Vm& vm, const uint8_t* args, int argc, int dst) { native_forward(vm, args, argc, dst, S_##NAME); }
PLACEHOLDER_FN_LIST(PLACEHOLDER_STUB)
#undef PLACEHOLDER_STUB

const Fn kNativeFns[] = {
#define NATIVE_ENTRY(NAME) NAME,
    NATIVE_FN_LIST(NATIVE_ENTRY)
#undef NATIVE_ENTRY
#define PLACEHOLDER_ENTRY(NAME) NAME,
    PLACEHOLDER_FN_LIST(PLACEHOLDER_ENTRY)
#undef PLACEHOLDER_ENTRY
#define NATIVE_EXTRA_ENTRY(NAME) NAME,
    NATIVE_EXTRA_LIST(NATIVE_EXTRA_ENTRY)
#undef NATIVE_EXTRA_ENTRY
    run_block,
    ds_list_create,
    ds_list_destroy,
    ds_list_clear,
    ds_list_add,
    ds_list_delete,
    ds_list_size,
    ds_list_find_value,
    ds_list_find_index,
    array_literal,
    array_get,
    array_set,
    array_pop,
    array_clear,
    array_contains,
    ds_map_create,
    ds_map_set,
    ds_map_delete,
    ds_map_size,
    ds_map_clear,
    ds_map_keys_to_array,
    ds_map_destroy,
    struct_get,
    struct_set,
    struct_exists,
    array_push,
    ds_map_exists,
    ds_map_find_value,
    VM_Execute,
    ds_grid_create,
    ds_grid_set,
    ds_grid_get,
    merge_colour,
    ds_grid_width,
    ds_grid_height,
    ds_grid_clear,
    ds_grid_destroy,
    // ── 数组 / ds_* 判定（vm_container.cpp）──
    array_length,
    array_get_index,
    array_delete,
    array_resize,
    array_create,
    array_shuffle,
    ds_exists,
    // ── 实例（vm_instance.cpp）──
    instance_exists,
    instance_number,
    instance_create_depth,
    instance_destroy,
    variable_instance_exists,
    variable_instance_get,
    variable_instance_set,
    // ── 常规内置（vm_builtin.cpp）──
    floor,
    round,
    abs,
    max,
    min,
    clamp,
    lerp,
    random,
    random_range,
    irandom_range,
    sin,
    cos,
    degtorad,
    lengthdir_x,
    lengthdir_y,
    point_direction,
    point_distance,
    is_real,
    is_string,
    is_array,
    is_struct,
    is_undefined,
    string,
    json_stringify,
    variable_global_exists,
    variable_global_get,
    merge_color,
    damage_enemy,
    calculate_plant_depth,
    get_grid_position_from_world,
    get_world_position_from_grid,
    audio_play_sound,
    send_message,
    show_debug_message,
    // ── 块（kernel.cpp）──
    run_event_user,
    // ── 宿主负责画/算的（Player / Effects 那 59 个对象要的）：**先只登记签名** ──
    //    追加在表尾：id 一个都不后移 → 老的 361 个 bin 不用重转
    ceil,
    make_color_rgb,
    draw_set_color,
    draw_set_alpha,
    draw_set_font,
    draw_set_halign,
    draw_set_valign,
    draw_rectangle,
    draw_rectangle_color,
    draw_roundrect,
    string_width_ext,
    string_height_ext,
    network_broadcast_active_skill,
};
// 条数定义在 vm.h（预置段要用它算下标基址）；这里卡住"表长度没变"
static_assert(sizeof(kNativeFns) / sizeof(kNativeFns[0]) == NATIVE_FN_COUNT,
              "kNativeFns 的长度和 vm.h 的 NATIVE_FN_COUNT 对不上");
// 名字表不进源码了：全项目的字符串都在 names.cpp（由 gen_names.py 生成）


// 名字 → native id：走全局字符串池（预置段里函数名占 1..N），一次哈希就够
int native_id(const char* name) {
    int sid = str_find(name);
    if (sid < 0) return -1;
    if (sid < SID_NATIVE_BASE || sid >= SID_NATIVE_BASE + NATIVE_FN_COUNT) return -1;
    return sid - SID_NATIVE_BASE;
}

// ── 预置段的两个查表口 ──
//   池下标落在 native 区间 → kNativeFns，落在 game 区间 → kGameFns；
//   别的（空串、脚本自己的字符串、属性名…）→ NULL。
Fn fn_of_sid(int sid) {
    if (sid >= SID_NATIVE_BASE && sid < SID_NATIVE_BASE + NATIVE_FN_COUNT)
        return kNativeFns[sid - SID_NATIVE_BASE];
    if (sid >= SID_GAME_BASE && sid < SID_GAME_BASE + GAME_FN_COUNT)
        return kGameFns[sid - SID_GAME_BASE];
    return NULL;
}

// 名字 → 函数：一次哈希 + 区间判断（hot/ 里按名字调内置走这条）
Fn fn_by_name(const char* name) {
    int sid = str_find(name);                  // 一次哈希；池下标自己就说明它在哪张表
    return (sid < 0) ? NULL : fn_of_sid(sid);
}
