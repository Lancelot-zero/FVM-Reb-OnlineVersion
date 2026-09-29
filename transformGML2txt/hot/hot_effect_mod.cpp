// ============================================================
// hot_effect_mod.cpp —— obj_effect_mod（通用 mod 特效）7 个块
//
//   由插件用 VM_CreateInstance("obj_effect_mod", x, y) 创建，再用 VM_SetProp 设 mod_type。
//   和 obj_bullet_mod 是一个套路，但更简单：**没有父对象**、没有 hp、也不跟网格走
//   （所以没有 hp_change / cell / 索敌 那几类闸门条件）。
//
//   ⚠ GML 把 mod 的 VM 存在实例变量 _mod_vm 上（结构体句柄）。我们这边值只有 8 字节、
//     塞不下 Vm*，所以每次用 mod_type 去 g_mod_vms 查（eff_vm）。
//     GML 那句 `variable_global_exists("mod_effect_vms") && ds_map_exists(global.mod_effect_vms, mod_type)`
//     的对应物就是 eff_vm(...) != NULL（mod VM 注册表按 mod id 存，见 vm_instance.cpp）。
//   ⚠ _mod_initialized 的语义照抄：初始化成功就置 1；本例的 mod_inst_add(_mod_vm, id)
//     用的是**默认 _with_list = true**（子弹那边才是 false），见 src_mod.gml:2052。
//   ⚠ NEEDED_NAME（要往 names.h 的 REST_NAME_LIST 段尾加的几条，本文件已在用）：
//        _mod_pending_effect_id   Create_0.gml:5
//        mod_follow_instance      Create_0.gml:44 / Step_0.gml:12
//        mod_prestep_dx           Create_0.gml:38 / Step_0.gml:125
//        mod_prestep_dy           Create_0.gml:39 / Step_0.gml:126
// ============================================================

#include "hot_common.h"
#include "hot_objects.h"

// 这个特效的 mod VM（GML 的 _mod_vm）
static Vm* eff_vm(Vm& vm, int s) { return mod_vm_of_inst_sid(vm, s, S_mod_type); }
static bool eff_inited(int s) { return truthy(inst_var_sid(s, S__mod_initialized)); }

// 延迟/首次初始化：登记 + 跑特效自己的 _OBJECT_CREATE
//   （Create_0.gml:47-63、Step_0.gml:18-35、Draw_0.gml:2-19 是**一模一样**的三段）
static void eff_init(Vm& vm, int s, Vm* mv, bool with_list) {
    set_num_sid(s, S__mod_initialized, 1);
    if (with_list) mod_inst_add_v(mv, s, true);                 // mod_inst_add(_mod_vm, id)
    Value bak_last = gv_sid(S__VM_last_created_card);
    Value bak_cur  = gv_sid(S__VM_cur_card);
    global_var_set_sid(S__VM_last_created_card, mk_num(s));
    global_var_set_sid(S__VM_cur_card, mk_num(s));
    mod_run_block_sid(mv, s, S__OBJECT_CREATE, NULL);           // 块不存在 → 里面自己跳过
    global_var_set_sid(S__VM_last_created_card, bak_last);
    global_var_set_sid(S__VM_cur_card, bak_cur);
}

// mod_follow_instance 指向的实例 id（undefined / 已销毁 → -1）
//   负数 = VM 给的包装 id，先过 global._VM_id_to_real 换成真实 id（跟 cardmod_batch_set 一个口径）
static int eff_follow_target(Vm& vm, int s) {
    Value* p = inst_var_sid(s, S_mod_follow_instance);
    if (!p || p->t == MEM_UNDEFINED) return -1;
    if (p->t != MEM_INT && p->t != MEM_FLOAT) return -1;        // 不是实例 id（字符串之类）→ 不跟随
    double id = num_of(p);
    if (id < 0) {
        Value m = gv_sid(S__VM_id_to_real);
        if (m.t == MEM_MAP) {
            Value real = v_map(vm, m, mk_num(-id));
            if (real.t != MEM_UNDEFINED) id = num_of(&real);
        }
    }
    return (int)id;
}


// Create_0.gml（64 行）
bool hot_effectmod_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    // mod_type = variable_global_exists("_mod_pending_effect_id") ? global._mod_pending_effect_id : ""
    if (global_exists_sid(S__mod_pending_effect_id)) inst_var_set_sid(s, S_mod_type, gv_sid(S__mod_pending_effect_id));
    else                                             inst_var_set_sid(s, S_mod_type, mk_str(vm, ""));

    inst_f(*in, F_image_speed) = mk_num(0);                    // image_speed = 0（内置字段）

    static const struct { int n;              /* 池下标 */ double v; } init[] = {
        { S_destroy_timer, -1 }, { S__mod_initialized, 0 },
        { S_mod_step_enter_var, 0 },
        { S_mod_countdown, 0 }, { S_mod_alpha_add, 0 },        // 倒计时 + 每帧透明度变化
        { S_mod_prestep_dx, 0 }, { S_mod_prestep_dy, 0 },      // 每帧位移量
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        in->vars[init[i].n] = mk_num(init[i].v);

    // _mod_vm = undefined（值槽塞不下 Vm*，本项目不存，每次按 mod_type 查）
    { Value u; u.t = MEM_UNDEFINED; u.v = 0;
      in->vars[S_mod_follow_instance] = u; }                   // mod_follow_instance = undefined

    // 闸门 / 盯梢（跟子弹同一套，只是条件少几种）
    inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    set_var_sid(s, S_mod_watch_last,    call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_watch_changed, call_ret_sid(vm, S_array_literal, NULL, 0));

    // 创建前就指定了类型 → 立即初始化（这里 mod_inst_add 用默认的 with_list = true）
    std::string mt = pool_text(inst_var_sid(s, S_mod_type));
    if (!mt.empty()) {
        Vm* mv = eff_vm(vm, s);
        if (mv) eff_init(vm, s, mv, true);
    }
    return true;
}


// Step_0.gml（126 行）
bool hot_effectmod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;                   // if global.is_paused { exit }

    mod_base_tick_v(s);                                        // 倒计时 + 每帧透明度变化

    // ── 跟随目标：有设且实例还在，就把 x/y 贴到它身上（写在最前面，后面的块读到的是跟随后的位置）──
    {
        int ft = eff_follow_target(vm, s);
        if (inst_get(ft)) {
            set_num_sid(s, S_x, get_num_sid(ft, S_x));
            set_num_sid(s, S_y, get_num_sid(ft, S_y));
        }
    }

    // ── 延迟初始化（允许创建后再 VM_SetProp 设 mod_type）──
    Vm* mv = eff_vm(vm, s);
    if (!eff_inited(s) && mv) eff_init(vm, s, mv, true);

    bool enter = true;                                         // var _enter = true
    const Value* condp = inst_var_sid(s, S_mod_step_enter_condition);

    // ── 盯梢 mod_watch：声明里哪个属性值变了，就记进 mod_watch_changed ──
    //    （每个变化各进一次 _OBJECT_STEP，闸门放行再进一次）
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    Value watch_changed = *inst_var_sid(s, S_mod_watch_changed);
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0);
      call_fn_sid(vm, S_array_resize, a, 2, -1); }
    if (eff_inited(s) && mv) {
        std::unordered_map<int, std::vector<Value> >::iterator wit = mv->named_arrays.find(S_mod_watch);
        if (wit != mv->named_arrays.end()) {
            // mod_watch 里存的是**属性名字符串**，下标属于全局池（见 hot_common.cpp 的说明）
            const std::vector<Value>& wlist = wit->second;
            Value last = *inst_var_sid(s, S_mod_watch_last);
            int wn = (int)wlist.size();
            if (container_size(last) != wn) {
                // 列表长度变了（刚声明 / 改了）：这一帧只对齐快照，不报变化
                Value a[2]; a[0] = last; a[1] = mk_num(wn);
                call_fn_sid(vm, S_array_resize, a, 2, -1);
                for (int i = 0; i < wn; i++) {
                    std::string nm = pool_text(&wlist[(size_t)i]);
                    Value cur; cur.t = MEM_UNDEFINED; cur.v = 0;
                    Value* ip = inst_var(s, nm);
                    if (ip) cur = *ip;
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());   // 实例上没有 → 当全局读
                    array_setv(vm, last, i, cur);
                }
            } else {
                for (int i = 0; i < wn; i++) {
                    std::string nm = pool_text(&wlist[(size_t)i]);
                    Value cur; cur.t = MEM_UNDEFINED; cur.v = 0;
                    Value* ip = inst_var(s, nm);
                    if (ip) cur = *ip;
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());
                    Value old = array_v(vm, last, i);
                    if (cur.t != old.t || cur.v != old.v) {
                        Value pa[2]; pa[0] = watch_changed; pa[1] = wlist[(size_t)i];
                        call_fn_sid(vm, S_array_push, pa, 2, -1);       // 每个变化的属性各进一次
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    // ── 进 VM 的入口闸门："" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 ──
    Value* sev = inst_var_sid(s, S_mod_step_enter_var);
    if (str_is_any_sid(vm, mv, condp, S_mod)) {
        if (is_numv(sev) && num_of(sev) > 0 && inst_count_sid(S_obj_battle) > 0) {
            int bat = inst_at_sid(S_obj_battle, 0);
            enter = (std::fmod(get_num_sid(bat, S_battle_time), num_of(sev)) == 0);
        }
    } else if (str_is_any_sid(vm, mv, condp, S_wait)) {
        if (is_numv(sev) && num_of(sev) > 0) {
            set_num_sid(s, S_mod_step_enter_var, num_of(sev) - 1);
            enter = (num_of(sev) - 1 <= 0);
        } else {
            inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));   // 不合法：进一次 + 清空
        }
    }

    // ── 跑特效自己的 _OBJECT_STEP：watch 每个变化各 1 次 + 闸门放行 1 次 ──
    int run_total = container_size(watch_changed) + (enter ? 1 : 0);
    for (int k = 0; k < run_total; k++) {
        if (!inst_get(s)) break;
        if (k < container_size(watch_changed)) {
            Value nm = array_v(vm, watch_changed, k);
            inst_var_set_sid(s, S_mod_changed_prop, nm);
        } else {
            inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
        }
        if (eff_inited(s) && mv) {
            Value bak_cur = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_STEP, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
        }
    }

    if (!inst_get(s)) return true;                             // if (!instance_exists(id)) exit

    // ── 自动销毁 ──
    if (get_num_sid(s, S_destroy_timer) == 0) {
        call_fn_int_sid(vm, S_instance_destroy, NULL, 0, -1);
        return true;
    }
    if (get_num_sid(s, S_destroy_timer) > 0) {
        add_num_sid(s, S_destroy_timer, -1);
        if (get_num_sid(s, S_destroy_timer) <= 0) call_fn_int_sid(vm, S_instance_destroy, NULL, 0, -1);
    }
    // ⚠ 我们的 instance_destroy 是当场生效的（GML 是延迟到帧末），
    //   实例已经没了就别再写它的 x/y
    if (!inst_get(s)) return true;

    // ── 偏移：每帧结束时把这两个量加到 x/y 上（特效没人重置 x/y，所以是"每帧位移"）──
    add_num_sid(s, S_x, get_num_sid(s, S_mod_prestep_dx));
    add_num_sid(s, S_y, get_num_sid(s, S_mod_prestep_dy));
    return true;
}


// Draw_0.gml（32 行）：延迟初始化 → 有 mod 的绘制块就画它（并 exit），否则画本体
bool hot_effectmod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    Vm* mv = eff_vm(vm, s);
    if (!eff_inited(s) && mv) eff_init(vm, s, mv, true);       // 延迟初始化

    if (eff_inited(s) && mv) {
        Value bak_cur = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        bool ran = mod_run_block_sid(mv, s, S__OBJECT_DRAW, NULL);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        if (ran) return true;                                  // GML 的 exit：不再画本体
    }
    call_fn_int_sid(vm, S_draw_self, NULL, 0, -1);
    return true;
}


// Destroy_0.gml（17 行）：跑特效自己的 _OBJECT_DESTROY → 移出 VM 实例计数
bool hot_effectmod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (eff_inited(s)) {
        Vm* mv = eff_vm(vm, s);
        if (mv) {
            Value bak_last = gv_sid(S__VM_last_destroyed_card);
            Value bak_cur  = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_last_destroyed_card, mk_num(s));
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_DESTROY, NULL);
            global_var_set_sid(S__VM_last_destroyed_card, bak_last);
            global_var_set_sid(S__VM_cur_card, bak_cur);
            mod_inst_del_v(mv, s, true);                       // mod_inst_del(_mod_vm, id)（默认 with_list）
        }
    }
    return true;
}


// Mouse_4 / Mouse_10 / Mouse_11：先跑特效 VM 里对应的块，再 event_inherited()
//   （obj_effect_mod 的 parentObjectId 是 null，event_inherited 在 GML 里就是空操作）
bool hot_effectmod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = eff_vm(vm, s);
    if (eff_inited(s) && mv) mod_run_block(mv, s, block, str_at(S__VM_cur_card).c_str());
    hot_inherited_sid(vm, S_obj_effect_mod, str_find(block));  // 没有父对象 → 直接 false
    return true;
}

bool hot_effectmod_click(Vm& vm)       { return hot_effectmod_mouse(vm, str_at(S__OBJECT_CLICK).c_str()); }
bool hot_effectmod_mouse_enter(Vm& vm) { return hot_effectmod_mouse(vm, str_at(S__OBJECT_MOUSE_ENTER).c_str()); }
bool hot_effectmod_mouse_leave(Vm& vm) { return hot_effectmod_mouse(vm, str_at(S__OBJECT_MOUSE_LEAVE).c_str()); }
