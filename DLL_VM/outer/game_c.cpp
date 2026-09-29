// ============================================================
// game_c.cpp — 游戏 API（批次 C：开关 / 横幅，13 条）
//
//   A 为什么有这些文件：game_api.cpp 只生成 150 条空壳（名字/顺序是契约），
//   实现写成 impl_XXX 放在这儿，再由 patch_game_fns_c() 挂进 kGameFns
//   （fn_table.cpp 的 patch_game_fns() 会按顺序调这三个钩子）。
//
//   本批要什么：要 host 表 + 少量全局开关
//
//   写法（照 game_self.cpp 的样子）：
//     static void impl_XXX(Value* mem, const uint8_t* args, int argc, int dst) { … }   // static：本文件私有
//     void patch_game_fns_c() { kGameFns[S_VM_XXX - SID_GAME_BASE] = impl_XXX; … }
//
//   ⚠ 只改本文件！批次之间、和 outer.h / fn_table.cpp / 表名单都别动（会互相踩）。
//   ⚠ 实参就是**槽位**（不是内存地址）：arg_v(mem, args, k) 取值，argslot(args, k) 只取槽号。
//   ⚠ 语义真源是 GML 参考：scripts/scr_command_VM/scr_command_VM.gml（那 150 条的正版实现），
//     老 C++ 有实现的在 transformGML2txt/vm_api.cpp。两条不一致时以 GML 参考为准。
//
//   本批名单（13 条；实现一条对一条）：
//
//   ⚠ 老 C++（transformGML2txt）这一批**一条都没实现**（vm.h 里只有名字），
//     所以全部照 scripts/scr_command_VM/scr_command_VM.gml 写；其中几条还对着
//     scripts/scr_command_VM_opt_execute/scr_command_VM_opt_execute.gml（优化执行器，
//     按 argc 实现，比基准 GML 更准）——不一致的地方在下面各条里都注明了。
//
//   ⚠ 本文件**不用 OUTER_TMP_SLOT**：OP_CALL 也会从 callbin 的帧里发出来，
//     而 callbin 的帧只有 cb.value 个槽（几十个），往 SCRATCH_BASE+32 写会越出帧外。
//     所以这里不借临时槽去调别的函数，需要文本/数值时就在 C++ 里就地算。
// ============================================================
#include "outer.h"
#include <cmath>        // floor：整数/浮点分开存（和 game_self.cpp 一个口径）
#include <cstdio>       // snprintf：把数字拼进文本（不是往控制台打）
#include <map>
#include <string>
#include <vector>

#define GAME_C_LIST(X) \
    X(VM_SetMaxSlots) X(VM_ShellPrint) X(VM_ShowNotice) X(VM_SetPlatformParams) \
    X(VM_SetDrawSlot) X(VM_SetDrawSlot_front) X(VM_BanGem) X(VM_ShowNoticeDur) \
    X(VM_SetEventEnabled) X(VM_RefreshPlatformSnapshots) X(VM_SetNoticeStyle) X(VM_SetDrawSlotEx) \
    X(VM_SetDrawSlotEx_front) \

namespace {

// ============================================================
// 小工具
// ============================================================
// 名字 → 池下标：字符串清单（const_str.h）里**没有**的名字在这儿懒加载一次
//   （第一次用到时 intern，之后就是固定下标；不改 const_str.h、也不动池的既有顺序）
VM_INT lazy_sid(const char* text, VM_INT& cache) {
    if (cache < 0) cache = intern_str(text);
    return cache;
}

// 数字结果：整数存 INT、否则 FLOAT（和 game_self.cpp / builtin.cpp 一个规矩）
Value mk_num_v(VM_FLOAT x) {
    if (std::floor(x) == x && x >= (VM_FLOAT)-2147483647.0 && x <= (VM_FLOAT)2147483647.0)
        return mk_int((VM_INT)x);
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}

// 第 k 个实参当整数（浮点截断；老代码 gnum_at 就是这个口径）
VM_INT num_arg(Value* mem, const uint8_t* args, int k) {
    return (VM_INT)arg_num(mem, args, k);
}

// 槽里那个值当数看（不是数 → dflt）
VM_FLOAT num_of(const Value& v, VM_FLOAT dflt) {
    if (v.t == MEM_FLOAT) return f_of(v.v);
    if (v.t == MEM_INT)   return (VM_FLOAT)v.v;
    return dflt;
}
VM_FLOAT val_num(Value* p, VM_FLOAT dflt) { return p ? num_of(*p, dflt) : dflt; }

// 第 k 个实参当数（没给 / undefined / -1 → dflt）
//   照 GML：`if (is_undefined(_x) || _x == -1) _x = 默认`
VM_FLOAT num_or(Value* mem, const uint8_t* args, int argc, int k, VM_FLOAT dflt) {
    if (k >= argc) return dflt;
    Value v = arg_v(mem, args, k);
    if (v.t == MEM_UNDEFINED) return dflt;
    VM_FLOAT f = num_of(v, dflt);
    return (f == (VM_FLOAT)-1) ? dflt : f;
}

// 值相等：GML 的 `==`（int / float 混着比也算相等；字符串比池下标 —— 同一个文本永远同一个下标）
//   ⚠ 和 container.cpp 的 array_contains（严格"类型 + 值"）故意不一样：那条照的是老代码的口径，
//     这里照 GML 的 `==`（VM_BanGem 用的 array_get_index 在 GML 里就是 `==`）
bool val_eq(const Value& a, const Value& b) {
    if (a.t == b.t) return a.v == b.v;
    bool an = (a.t == MEM_INT || a.t == MEM_FLOAT);
    bool bn = (b.t == MEM_INT || b.t == MEM_FLOAT);
    if (!an || !bn) return false;
    return num_of(a, 0) == num_of(b, 0);
}

// GML 的 string(x)：一个值 → 文本
//   ⚠ 格式化不在这：唯一实现在内层 value_to_text（core/pool.cpp，和 builtin.cpp 的
//     string() / 解释器的 OP_ADD 拼串共用同一份）。这里只要文本、不要槽 + dst，
//     所以直接转，不再自己拼一遍格式（本文件不用临时槽，理由见表头）。
std::string to_text(const Value& v) {
    return value_to_text(v);
}

// 把前 n 个实参拼成一条文本 → 池下标（GML：`_msg += string(vm_read_mem(...))`）
//   undefined 的段跳过（GML 的 is_undefined 判断；没传的实参在 GML 里也是 undefined）
//   GML 只看 16 段（a…p），多的不看
VM_INT build_msg(Value* mem, const uint8_t* args, int n) {
    if (n > 16) n = 16;
    std::string msg;
    for (int i = 0; i < n; i++) {
        Value v = arg_v(mem, args, i);
        if (v.t == MEM_UNDEFINED) continue;
        msg += to_text(v);
    }
    return intern_str(msg);
}

// 调一个**游戏侧脚本函数**（callbin）：GML 里 show_notice / shell_print 就是普通的全局函数，
//   新架构里它们编成了 kernel/function/*.callbin → 这儿直接走 callbin_call
//   （就是 native 转发最终走的那个地方；本文件不借临时槽，所以不绕 VM_CallFunc 那条路）
//   没装这个 callbin → 内层置 VM_E_FN_NOT_FOUND（**响亮**，不静默）
void call_script(VM_INT fn_sid, Value* av, int n) {
    Value r = callbin_call(fn_sid, av, n);
    release(r);                 // 返回值转交给我们这一份，没人接就放掉（非容器空操作）
}

// ============================================================
// 绘制槽（VM_SetDrawSlot / *_front / VM_SetDrawSlotEx / *_front 共 6 条）
//
//   GML 侧的数据模型（objects/obj_battle/Create_0.gml）：
//     global.map_draw_slots / map_draw_slots_front = 64 个 struct，
//     字段 sprite / x / y / alpha / image_angle / image_xscale / image_yscale，
//     由 obj_battle / obj_flame_manager 的 Draw 事件逐格 draw_sprite_ext 画出来。
//   这里照这个数据模型写进 **VM 自己的** global（数组 + struct，同一套字段名）。
//
//   ⚠ 宿主表（g_host_draw）里**没有**"往引擎的绘制槽里写一格"的原语，也还没有
//     "VM 的 global ↔ 引擎的 global" 那条桥 —— 所以引擎那边现在**看不见**这些槽，
//     要等宿主桥接（见文件尾的说明）。数据本身按 GML 原样落下，桥接时不用再翻格式。
//   ⚠ sprite 字段：GML 走 get_load_sprite(名字) 把贴图名解析成精灵句柄，那要宿主的贴图
//     缓存（_VM_sprite_temp_cache / _sprite_cache / sprite_add）。这里**原样存**：
//     int 本来就是句柄，string 就是名字，解析留给宿主那一侧 —— 免得在"设一格绘制槽"
//     里递归进 callbin / 置出错误码。
// ============================================================

// struct（MEM_MAP）里写一格：引用计数的账照 slot_write 的规矩（先 retain 新的、再 release 旧的）
void map_put(Value map_h, VM_INT key_sid, Value v) {
    if (map_h.t != MEM_MAP || !container_alive(map_h)) return;
    std::map<Value, Value, ValueLess>& m = g_map_pool[(size_t)map_h.v];
    Value k = mk_str_sid(key_sid);
    std::map<Value, Value, ValueLess>::iterator it = m.find(k);
    if (it == m.end()) { retain(v); m[k] = v; return; }
    if (it->second.t == v.t && it->second.v == v.v) return;
    retain(v);                              // 先拿新的
    release(it->second);                    // 再放旧的
    it->second = v;
}

// 清单里没有、6 条绘制槽共用的字段名
VM_INT S_alpha() { static VM_INT s = -1; return lazy_sid("alpha", s); }

// 绘制槽表（数组 + 64 个 struct）：返回**数组池下标**（不是指针 —— 建 struct 会让
//   g_arr_pool 重分配，指针当场就悬了）；没有 / 不是数组 → create 时照 Create_0 建一份
int draw_slots(VM_INT slots_sid, bool create) {
    Value* g = global_var_sid(slots_sid);
    if (g && g->t == MEM_ARRAY && container_alive(*g)) return (int)g->v;
    if (!create) return -1;

    Value arr{MEM_ARRAY, arr_new()};                 // 造出来就有主（refs = 1）
    g_arr_pool[(size_t)arr.v].reserve(64);
    for (int i = 0; i < 64; i++) {
        Value m{MEM_MAP, map_new()};                 // 同上：造出来 refs = 1
        map_put(m, S_sprite, mk_int(NOONE));
        map_put(m, S_x, mk_int(0));
        map_put(m, S_y, mk_int(0));
        map_put(m, S_alpha(), mk_num_v(1));
        map_put(m, S_image_angle,  mk_num_v(0));
        map_put(m, S_image_xscale, mk_num_v(1));
        map_put(m, S_image_yscale, mk_num_v(1));
        retain(m);                                   // 数组里存一份
        g_arr_pool[(size_t)arr.v].push_back(m);
        release(m);                                  // 创造者那份交出去
    }
    global_var_set_sid(slots_sid, arr);              // 全局表持一份（它自己 retain）
    release(arr);                                    // 创造者那份交出去
    return (int)arr.v;
}

// 清槽的判断（GML：`sprite == "" || sprite == -1 || sprite == noone`）
bool sprite_is_clear(const Value& v) {
    if (v.t == MEM_STRING) return str_ok(v.v) && str_at(v.v).empty();
    if (v.t == MEM_INT || v.t == MEM_FLOAT) {
        VM_FLOAT n = num_of(v, 0);
        return n == (VM_FLOAT)-1 || n == (VM_FLOAT)NOONE;
    }
    return false;
}

// 写一格绘制槽（照 GML 的 _vm_set_draw_slot）
//   x / y / alpha 取数（undefined → 0 / 0 / 1；GML 是把 undefined 原样存进去，画的时候才炸）
void draw_slot_set(VM_INT slots_sid, Value slot_v, Value sprite,
                   VM_FLOAT x, VM_FLOAT y, VM_FLOAT alpha,
                   VM_FLOAT angle, VM_FLOAT xscale, VM_FLOAT yscale) {
    VM_INT slot = (VM_INT)num_of(slot_v, -1);
    if (slot < 0 || slot >= 64) return;              // 槽位只认 0~63（GML 同样直接 return）

    int ai = draw_slots(slots_sid, true);
    if (ai < 0) return;
    std::vector<Value>& arr = g_arr_pool[(size_t)ai];
    if (slot >= (VM_INT)arr.size()) return;
    Value mh = arr[(size_t)slot];                    // 先抄一份句柄，后面不再碰 arr
    if (mh.t != MEM_MAP || !container_alive(mh)) return;

    if (sprite_is_clear(sprite)) {                   // 清槽：贴图 noone，其余复位成默认
        map_put(mh, S_sprite, mk_int(NOONE));
        map_put(mh, S_x, mk_int(0));
        map_put(mh, S_y, mk_int(0));
        map_put(mh, S_alpha(), mk_num_v(1));
        map_put(mh, S_image_angle,  mk_num_v(0));
        map_put(mh, S_image_xscale, mk_num_v(1));
        map_put(mh, S_image_yscale, mk_num_v(1));
        return;
    }
    map_put(mh, S_sprite, sprite);                   // int = 句柄 / string = 名字（表头有说明）
    map_put(mh, S_x, mk_num_v(x));
    map_put(mh, S_y, mk_num_v(y));
    map_put(mh, S_alpha(), mk_num_v(alpha));
    map_put(mh, S_image_angle,  mk_num_v(angle));
    map_put(mh, S_image_xscale, mk_num_v(xscale));
    map_put(mh, S_image_yscale, mk_num_v(yscale));
}

// 4 条 SetDrawSlot* 的公共实现：front 换槽表，ex 多读 3 个（角度 / 缩放）
void draw_slot_call(Value* mem, const uint8_t* args, int argc, bool front, bool ex) {
    if (argc < 5) return;                            // 参数不齐 → 空操作（GML 会报参数错）
    static VM_INT s_back = -1;                       // global.map_draw_slots
    static VM_INT s_front = -1;                      // global.map_draw_slots_front
    VM_INT slots_sid = front ? lazy_sid("map_draw_slots_front", s_front)
                             : lazy_sid("map_draw_slots",       s_back);
    VM_FLOAT angle = 0, xscale = 1, yscale = 1;
    if (ex) {
        angle  = num_or(mem, args, argc, 5, 0);
        xscale = num_or(mem, args, argc, 6, 1);
        yscale = num_or(mem, args, argc, 7, 1);
    }
    draw_slot_set(slots_sid, arg_v(mem, args, 0), arg_v(mem, args, 1),
                  num_of(arg_v(mem, args, 2), 0), num_of(arg_v(mem, args, 3), 0),
                  num_of(arg_v(mem, args, 4), 1), angle, xscale, yscale);
}

// 负数包装 id → 真实实例 id：global._VM_id_to_real（正数 VM id → 真实 id）
//   表不在 / 不是 ds_map / 没这一项 → -1（GML 那边 is_undefined → 直接 return）
VM_INT wrap_to_real(VM_INT key) {
    Value* g = global_var_sid(S__VM_id_to_real);
    if (!g || g->t != MEM_MAP || !container_alive(*g)) return -1;
    std::map<Value, Value, ValueLess>& m = g_map_pool[(size_t)g->v];
    std::map<Value, Value, ValueLess>::iterator it = m.find(mk_int(key));
    if (it == m.end()) return -1;
    return (VM_INT)num_of(it->second, -1);
}

}  // namespace

// ============================================================
// 1 VM_SetMaxSlots(n)：global._VM_max_slots = n（-1 = 不限制，引擎那半边读它）
//   GML: global._VM_max_slots = vm_read_mem(global.__vm, n_addr);
// ============================================================
static void impl_VM_SetMaxSlots(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    static VM_INT sid = -1;
    global_var_set_sid(lazy_sid("_VM_max_slots", sid), arg_v(mem, args, 0));
}

// ============================================================
// 2 VM_ShellPrint(a…p)：最多 16 段拼一条，交给游戏侧的 shell_print（写 mod/shell.log + 控制台）
//   GML: shell_print(_msg)
//   shell_print 在 native 表里**没有**（它是 kernel/function/shell_print.gml）→ 走 callbin
// ============================================================
static void impl_VM_ShellPrint(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    static VM_INT sid = -1;
    Value av[1];
    av[0] = mk_str_sid(build_msg(mem, args, argc));
    call_script(lazy_sid("shell_print", sid), av, 1);
}

// ============================================================
// 3 VM_ShowNotice(a…p)：最多 16 段拼一条，show_notice(_msg, 120)
//   GML: show_notice(_msg, 120)
//   show_notice 是 native 名（转发那条路最终也是打到同名 callbin），这里直接调它那条
// ============================================================
static void impl_VM_ShowNotice(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    Value av[2];
    av[0] = mk_str_sid(build_msg(mem, args, argc));
    av[1] = mk_int(120);
    call_script(S_show_notice, av, 2);
}

// ============================================================
// 4 VM_SetPlatformParams(plat_id, axis, distance, idle, direction)
//   以当前位置为新起点，重新设定平台的移动参数（GML 见 scr_command_VM.gml）
//   ⚠ GML 里还有两道**联网**的闸：vm_inst_write_ok(_plat)（客户端不改）和
//     vm_inst_broadcast_prop(...)（服务端广播）。那要引擎的网络那一半，C++ 这边没有
//     对应信息 → 跳过（网络语义由宿主补，语义差异见表尾）。
// ============================================================
static void impl_VM_SetPlatformParams(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 5) return;                       // 5 个参数缺一不可（GML 也是全读）
    VM_INT id = num_arg(mem, args, 0);
    if (id < 0) {                               // 客户端包装 id（负数）→ 换真实实例 id
        id = wrap_to_real(-id);
        if (id < 0) return;                     // 表里没有 → GML 也是直接 return
    }
    Instance* in = inst_get(id);
    if (!in) return;                            // !instance_exists(_plat)
    if (in->obj != S_obj_platform) return;      // _plat.object_index != obj_platform

    // 以当前位置为新起点：运动轴上的 current_offset 结算进 start_col / start_row，然后归零
    //   ⚠ inst_var_sid 的指针别跨 inst_var_set_sid 用（写会插进 unordered_map、指针会失效），
    //     下面每个指针都是"取出来立刻用掉"
    bool is_x = false;
    {
        Value* axis_v = inst_var_sid(id, S_move_axis);
        is_x = (axis_v && axis_v->t == MEM_STRING && axis_v->v == S_x);
    }
    VM_FLOAT off = val_num(inst_var_sid(id, S_current_offset), 0);
    if (is_x) {
        VM_FLOAT c = val_num(inst_var_sid(id, S_start_col), 0) + off;
        inst_var_set_sid(id, S_start_col, mk_num_v(c));
    } else {
        VM_FLOAT r = val_num(inst_var_sid(id, S_start_row), 0) + off;
        inst_var_set_sid(id, S_start_row, mk_num_v(r));
    }
    inst_var_set_sid(id, S_current_offset, mk_int(0));

    // 新参数：axis 0 = 上下（"y"）、其余 = 左右（"x"）；distance / idle / direction 原样落
    static VM_INT s_dist = -1, s_idle = -1, s_dir = -1;
    VM_INT axis = num_arg(mem, args, 1);
    inst_var_set_sid(id, S_move_axis, mk_str_sid(axis == 0 ? S_y : S_x));
    inst_var_set_sid(id, lazy_sid("move_distance",          s_dist), arg_v(mem, args, 2));
    inst_var_set_sid(id, lazy_sid("boundary_idle_duration", s_idle), arg_v(mem, args, 3));
    inst_var_set_sid(id, lazy_sid("move_direction",         s_dir),  arg_v(mem, args, 4));
}

// ============================================================
// 5 VM_SetDrawSlot(slot, sprite, x, y, alpha)：旋转/缩放用默认值
//   GML: _vm_set_draw_slot(global.map_draw_slots, slot, sprite, x, y, alpha, 0, 1, 1)
// ============================================================
static void impl_VM_SetDrawSlot(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    draw_slot_call(mem, args, argc, false, false);
}

// ============================================================
// 6 VM_SetDrawSlot_front(slot, sprite, x, y, alpha)：同上，写前层槽表
//   GML: _vm_set_draw_slot(global.map_draw_slots_front, …)
// ============================================================
static void impl_VM_SetDrawSlot_front(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    draw_slot_call(mem, args, argc, true, false);
}

// ============================================================
// 7 VM_BanGem(gem_name)：global.banned_gems_online 里没有就追加
//   GML: if (array_get_index(global.banned_gems_online, gem_name) == -1) array_push(...)
//   建宝石的地方会跳过这张表里的宝石（联机禁用）
// ============================================================
static void impl_VM_BanGem(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    Value gem = arg_v(mem, args, 0);

    // 全局必须是"活着的数组"；没有就建一个（GML 里这张表开局就建好了，这里兜一下）
    Value* g = global_var_sid(S_banned_gems_online);
    if (!g || g->t != MEM_ARRAY || !container_alive(*g)) {
        Value nv{MEM_ARRAY, arr_new()};
        global_var_set_sid(S_banned_gems_online, nv);   // 全局表持一份
        release(nv);                                    // 创造者那份交出去
        g = global_var_sid(S_banned_gems_online);
        if (!g) return;
    }
    std::vector<Value>& arr = g_arr_pool[(size_t)g->v];  // 后面不再建容器，引用不会失效
    for (size_t i = 0; i < arr.size(); i++)
        if (val_eq(arr[i], gem)) return;                 // array_get_index != -1 → 不重复 ban
    retain(gem);                                         // 数组存一份
    arr.push_back(gem);
}

// ============================================================
// 8 VM_ShowNoticeDur(a…p)：前面的段拼正文、**最后一段是时长**（帧）
//   ⚠ 基准 GML 写的是"只有第 16 段（下标 15）才是时长"，优化执行器
//     （scr_command_VM_opt_execute.gml 的 run_VM_ShowNoticeDur）改成"给的最后一段就是时长"。
//     编译器按实际实参个数发参数，所以照**优化执行器**：给 1 段时没时长 → 120。
// ============================================================
static void impl_VM_ShowNoticeDur(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int nmsg = argc;
    VM_INT dur = 120;
    if (argc >= 2) { nmsg = argc - 1; dur = num_arg(mem, args, argc - 1); }
    Value av[2];
    av[0] = mk_str_sid(build_msg(mem, args, nmsg));
    av[1] = mk_int(dur);
    call_script(S_show_notice, av, 2);
}

// ============================================================
// 9 VM_SetEventEnabled(val)：global._VM_event_enabled = (val != 0)
//   ⚠ 没给实参时参数在 GML 里是 undefined，而 `undefined != 0` 为真 → 当"开"（写 1）
// ============================================================
static void impl_VM_SetEventEnabled(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    VM_INT on = 1;
    if (argc > 0) {
        Value v = arg_v(mem, args, 0);
        if (v.t == MEM_INT)        on = (v.v != 0) ? 1 : 0;
        else if (v.t == MEM_FLOAT) on = (f_of(v.v) != (VM_FLOAT)0) ? 1 : 0;
        // 其它类型（string / undefined …）：GML 里 `!= 0` 都是真
    }
    static VM_INT sid = -1;
    global_var_set_sid(lazy_sid("_VM_event_enabled", sid), mk_int(on));
}

// ============================================================
// 10 VM_RefreshPlatformSnapshots()：平台快照机制已经改成引用计数，GML 里现在就是 `return 0`
// ============================================================
static void impl_VM_RefreshPlatformSnapshots(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, 0);
}

// ============================================================
// 11 VM_SetNoticeStyle(scale, r, g, b)：4 个全局（-1 = 用默认；公告是在 GML 那半边画的）
//   GML: global._VM_notice_scale / _VM_notice_color_r / _g / _b = 四个实参
// ============================================================
static void impl_VM_SetNoticeStyle(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 4) return;
    static VM_INT s_scale = -1, s_r = -1, s_g = -1, s_b = -1;
    global_var_set_sid(lazy_sid("_VM_notice_scale",   s_scale), arg_v(mem, args, 0));
    global_var_set_sid(lazy_sid("_VM_notice_color_r", s_r),     arg_v(mem, args, 1));
    global_var_set_sid(lazy_sid("_VM_notice_color_g", s_g),     arg_v(mem, args, 2));
    global_var_set_sid(lazy_sid("_VM_notice_color_b", s_b),     arg_v(mem, args, 3));
}

// ============================================================
// 12 VM_SetDrawSlotEx(slot, sprite, x, y, alpha, angle, xscale, yscale)
//   GML: 后 3 个 undefined / -1 → 0 / 1 / 1，再 _vm_set_draw_slot(global.map_draw_slots, …)
// ============================================================
static void impl_VM_SetDrawSlotEx(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    draw_slot_call(mem, args, argc, false, true);
}

// ============================================================
// 13 VM_SetDrawSlotEx_front(…)：同上，写前层槽表
// ============================================================
static void impl_VM_SetDrawSlotEx_front(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    draw_slot_call(mem, args, argc, true, true);
}

// ============================================================
// 挂表（13 条一条不少；fid = 池下标 - game 段基址，顺序由 const_str.h 的名单定）
// ============================================================
void patch_game_fns_c() {
    kGameFns[S_VM_SetMaxSlots              - SID_GAME_BASE] = impl_VM_SetMaxSlots;
    kGameFns[S_VM_ShellPrint               - SID_GAME_BASE] = impl_VM_ShellPrint;
    kGameFns[S_VM_ShowNotice               - SID_GAME_BASE] = impl_VM_ShowNotice;
    kGameFns[S_VM_SetPlatformParams        - SID_GAME_BASE] = impl_VM_SetPlatformParams;
    kGameFns[S_VM_SetDrawSlot              - SID_GAME_BASE] = impl_VM_SetDrawSlot;
    kGameFns[S_VM_SetDrawSlot_front        - SID_GAME_BASE] = impl_VM_SetDrawSlot_front;
    kGameFns[S_VM_BanGem                   - SID_GAME_BASE] = impl_VM_BanGem;
    kGameFns[S_VM_ShowNoticeDur            - SID_GAME_BASE] = impl_VM_ShowNoticeDur;
    kGameFns[S_VM_SetEventEnabled          - SID_GAME_BASE] = impl_VM_SetEventEnabled;
    kGameFns[S_VM_RefreshPlatformSnapshots - SID_GAME_BASE] = impl_VM_RefreshPlatformSnapshots;
    kGameFns[S_VM_SetNoticeStyle           - SID_GAME_BASE] = impl_VM_SetNoticeStyle;
    kGameFns[S_VM_SetDrawSlotEx            - SID_GAME_BASE] = impl_VM_SetDrawSlotEx;
    kGameFns[S_VM_SetDrawSlotEx_front      - SID_GAME_BASE] = impl_VM_SetDrawSlotEx_front;
}

// ============================================================
// 语义差异 / 缺口（都写在代码里，这里汇总一遍）
//
//   · VM_ShellPrint / VM_ShowNotice / VM_ShowNoticeDur 的正文：GML 走 `string(...)` 转换，
//     这里用同格式的 to_text；**没给** / 显式 undefined 的段都跳过（同 GML 的 is_undefined）。
//   · VM_ShowNoticeDur 的时长口径照**优化执行器**（最后一段），不照基准 GML（第 16 段）。
//   · VM_SetPlatformParams 少了 vm_inst_write_ok（联网平台客户端不改）和
//     vm_inst_broadcast_prop（服务端广播）—— 引擎/网络那半边的东西。
//   · VM_SetDrawSlot* 写的是 VM 自己的 global.map_draw_slots(_front)：宿主表 g_host_draw
//     里没有"往引擎绘制槽写一格"的原语，也没有 VM global ↔ 引擎 global 的桥，
//     所以引擎的 obj_battle / obj_flame_manager 现在还看不到这些槽（等宿主桥接）。
//     sprite 的"名字 → 句柄"解析（GML 的 get_load_sprite）同样留在宿主那一侧。
//   · VM_SetMaxSlots / VM_SetNoticeStyle / VM_BanGem 只写 VM 的全局表（和 game_self.cpp 那
//     17 条一个口径）—— 引擎那半边要读同一份值，同样得靠宿主桥接。
// ============================================================
