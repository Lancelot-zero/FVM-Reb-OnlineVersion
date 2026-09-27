// ============================================================
// hot_bullet_mod.cpp —— obj_bullet_mod（通用 mod 子弹）8 个块
//
//   由插件用 VM_CreateInstance("obj_bullet_mod", x, y) 创建，再用 VM_SetProp 设 mod_type。
//   闸门比卡片多几种（cell_row / cell_target_* / attack_cell / attack_collision），
//   还自带抛物线（bullet_parabola）和"路径数组"（dx_arr/dy_arr 命名数组）两套位移。
//
//   ⚠ GML 把 mod 的 VM 存在实例变量 _mod_vm 上（结构体句柄）。我们这边值只有 8 字节、
//     塞不下 Vm*，所以每次用 mod_type 去 g_mod_vms 查（bul_vm）。
//   ⚠ _mod_initialized 的语义照抄：Create 里初始化成功就置 1；
//     但**只有 Draw 的延迟初始化那支会 mod_inst_add(..., false)**（子弹不进 instances 列表）。
//     所以"创建时就带 _mod_pending_bullet_id"的子弹其实从没记过数，
//     Destroy 里的 mod_inst_del 会把计数减到 0 为止（mod_inst_del 自己夹住不会变负）。
//     这是 GML 原样，不改。
// ============================================================

#include "hot_common.h"
#include "hot_objects.h"

// 这颗子弹的 mod VM（GML 的 _mod_vm）
static Vm* bul_vm(Vm& vm, int s) { return mod_vm_of_inst(vm, s, "mod_type"); }
static bool bul_inited(int s) { return truthy(inst_var(s, "_mod_initialized")); }

// 延迟/首次初始化：登记 + 跑子弹自己的 _OBJECT_CREATE（Create 和 Step/Draw 里一模一样那段）
static void bul_init(Vm& vm, int s, Vm* mv, bool with_list) {
    set_num(s, "_mod_initialized", 1);
    if (with_list) mod_inst_add_v(mv, s, true);
    Value bak_last = gv(vm, "_VM_last_created_card");
    Value bak_cur  = gv(vm, "_VM_cur_card");
    global_var_set("_VM_last_created_card", mk_num(s));
    global_var_set("_VM_cur_card", mk_num(s));
    mod_run_block(mv, s, "_OBJECT_CREATE", NULL);
    global_var_set("_VM_last_created_card", bak_last);
    global_var_set("_VM_cur_card", bak_cur);
}

// 场上敌人个数（GML 的 instance_number(obj_enemy_parent)：**含子对象**）
static int bul_enemy_count() {
    int n = 0;
    for (size_t i = 1; i < g_instances.size(); i++)
        if (g_instances[i].alive && inst_is((int)i, "obj_enemy_parent")) n++;
    return n;
}

// Create_0.gml（156 行）
bool hot_bulletmod_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    // mod_type = variable_global_exists("_mod_pending_bullet_id") ? global._mod_pending_bullet_id : ""
    if (global_exists("_mod_pending_bullet_id")) inst_var_set(s, "mod_type", gv(vm, "_mod_pending_bullet_id"));
    else                                         inst_var_set(s, "mod_type", mk_str(vm, ""));

    in->image_speed = mk_num(0);              // 内置字段
    static const struct { const char* n; double v; } init[] = {
        { "destroy_timer", -1 }, { "_mod_initialized", 0 }, { "vector_x", 0 },
        { "vx", 0 }, { "vy", 0 },
        { "mod_step_enter_var", 0 }, { "mod_has_enemy", 0 },
        { "mod_collision_enemy", -1 }, { "mod_collision_hit", 0 },
            { "bullet_damage", 0 }, { "bullet_collide", 1 }, { "bullet_slow", 0 },
        { "bullet_slow_frame", 0 }, { "bullet_freeze_chance", 0 }, { "bullet_freeze_frame", 0 },
        { "bullet_stun_chance", 0 }, { "bullet_stun_frame", 0 },
        { "bullet_hits", -1 },                    // -1 = 不消耗（默认）
        { "bullet_hits_left", 0 }, { "bullet_hits_init", 0 },
            { "bullet_parabola", 0 }, { "bullet_target", -1 }, { "bullet_target_x", 0 },
        { "bullet_target_y", 0 }, { "bullet_parabola_time", 30 }, { "bullet_parabola_h", -1 },
        { "bullet_para_t", -1 }, { "bullet_para_x0", 0 }, { "bullet_para_y0", 0 },
        { "bullet_para_dist", 0 },
        { "bullet_path_i", 0 },                       { "mod_countdown", 0 }, { "mod_alpha_add", 0 },   // 倒计时 + 透明度
        { "grid_col", -1 }, { "grid_row", -1 }, { "prev_grid_col", -1 }, { "prev_grid_row", -1 },
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        in->vars[init[i].n] = mk_num(init[i].v);

    // 闸门 / 盯梢 / 碰撞记录
    inst_var_set(s, "mod_step_enter_condition", mk_str(vm, ""));
    inst_var_set(s, "mod_changed_prop", mk_str(vm, ""));
    set_var(s, "mod_watch_last",    call_ret(vm, "array_literal", NULL, 0));
    set_var(s, "mod_watch_changed", call_ret(vm, "array_literal", NULL, 0));
    set_var(s, "mod_collision_enemies", call_ret(vm, "array_literal", NULL, 0));

    // 碰撞结算参数（引擎自动做；默认全 0 = 不结算）
    inst_var_set(s, "bullet_damage_type", mk_str(vm, "normal"));

    // 格子坐标（raw = true：不回绕）
    {
        Value a[3];
        a[0] = mk_num(get_num(s, "x")); a[1] = mk_num(get_num(s, "y")); a[2] = mk_num(1);
        Value gp = call_ret(vm, "get_grid_position_from_world", a, 3);
        set_num(s, "grid_col", num_of(&m_get(vm, gp, "col")));
        set_num(s, "grid_row", num_of(&m_get(vm, gp, "row")));
        set_num(s, "prev_grid_col", get_num(s, "grid_col"));
        set_num(s, "prev_grid_row", get_num(s, "grid_row"));
    }

    // 创建前就指定了类型 → 立即初始化（注意：这里**不** mod_inst_add，跟 GML 一样）
    std::string mt = pool_text(vm.strings, inst_var(s, "mod_type"));
    if (!mt.empty()) {
        Vm* mv = bul_vm(vm, s);
        if (mv) bul_init(vm, s, mv, false);
    }
    return true;
}

// Step_0.gml（274 行）
bool hot_bulletmod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue(vm, "is_paused")) return true;

    mod_base_tick_v(s);                                        // 倒计时 + 透明度

    // bullet_collide = 0 → 换"空掩码"精灵，让引擎整个跳过碰撞（精灵是宿主的东西）
    if (get_num(s, "bullet_collide") == 0) {
        double nomask = gnumv(vm, "_mod_nomask_spr");
        if (!global_exists("_mod_nomask_spr") || nomask == -1) {
            // GML 这里用 try/catch 兜底（老版本没有 sprite_collision_mask）；
            // C++ 没法捕获，照直写，natives 是空壳时不会有事
            Value a[6];
            a[0] = mk_str(vm, "_ph_empty.png"); a[1] = mk_num(1); a[2] = mk_num(0);
            a[3] = mk_num(0); a[4] = mk_num(0); a[5] = mk_num(0);
            nomask = num_of(&call_ret(vm, "sprite_add", a, 6));
            Value s9[9];
            s9[0] = mk_num(nomask); s9[1] = mk_num(0); s9[2] = mk_num(2);
            s9[3] = mk_num(0); s9[4] = mk_num(0); s9[5] = mk_num(0);
            s9[6] = mk_num(0); s9[7] = mk_num(0); s9[8] = mk_num(0);
            call_fn_v(vm, "sprite_collision_mask", s9, 9, -1);
            global_var_set("_mod_nomask_spr", mk_num(nomask));
        }
        if (nomask != -1 && get_num(s, "mask_index") != nomask) set_num(s, "mask_index", nomask);
    } else if (global_exists("_mod_nomask_spr") && gnumv(vm, "_mod_nomask_spr") != -1
               && get_num(s, "mask_index") == gnumv(vm, "_mod_nomask_spr")) {
        set_num(s, "mask_index", -1);                          // 恢复成"用自家贴图的掩码"
    }

    // 当前帧 / 上一帧格子坐标
    set_num(s, "prev_grid_col", get_num(s, "grid_col"));
    set_num(s, "prev_grid_row", get_num(s, "grid_row"));
    {
        Value a[3];
        a[0] = mk_num(get_num(s, "x")); a[1] = mk_num(get_num(s, "y")); a[2] = mk_num(1);
        Value gp = call_ret(vm, "get_grid_position_from_world", a, 3);
        set_num(s, "grid_col", num_of(&m_get(vm, gp, "col")));
        set_num(s, "grid_row", num_of(&m_get(vm, gp, "row")));
    }

    // 延迟初始化（允许创建后再 VM_SetProp 设 mod_type）
    Vm* mv = bul_vm(vm, s);
    if (!bul_inited(s) && mv) bul_init(vm, s, mv, false);

    bool enter = true;
    const Value* condp = inst_var(s, "mod_step_enter_condition");

    // 盯梢 mod_watch
    inst_var_set(s, "mod_changed_prop", mk_str(vm, ""));
    Value watch_changed = *inst_var(s, "mod_watch_changed");
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0); call_fn_v(vm, "array_resize", a, 2, -1); }
    if (bul_inited(s) && mv) {
        std::map<std::string, std::vector<Value> >::iterator wit = mv->named_arrays.find("mod_watch");
        if (wit != mv->named_arrays.end()) {
            const std::vector<Value>& wlist = wit->second;
            Value last = *inst_var(s, "mod_watch_last");
            int wn = (int)wlist.size();
            if (container_size(last) != wn) {
                Value a[2]; a[0] = last; a[1] = mk_num(wn);
                call_fn_v(vm, "array_resize", a, 2, -1);
                for (int i = 0; i < wn; i++) {
                    std::string nm = pool_text(mv->strings, &wlist[(size_t)i]);
                    Value cur; cur.t = MEM_UNDEFINED; cur.v = 0;
                    Value* ip = inst_var(s, nm);
                    if (ip) cur = *ip;
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());
                    array_setv(vm, last, i, cur);
                }
            } else {
                for (int i = 0; i < wn; i++) {
                    std::string nm = pool_text(mv->strings, &wlist[(size_t)i]);
                    Value cur; cur.t = MEM_UNDEFINED; cur.v = 0;
                    Value* ip = inst_var(s, nm);
                    if (ip) cur = *ip;
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());
                    Value old = array_v(vm, last, i);
                    if (cur.t != old.t || cur.v != old.v) {
                        Value pa[2]; pa[0] = watch_changed; pa[1] = wlist[(size_t)i];
                        call_fn_v(vm, "array_push", pa, 2, -1);
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    Value* sev = inst_var(s, "mod_step_enter_var");
    if (str_is_any(vm, mv, condp, "mod")) {
        if (is_numv(sev) && num_of(sev) > 0 && inst_count("obj_battle") > 0) {
            int bat = inst_at("obj_battle", 0);
            enter = (std::fmod(get_num(bat, "battle_time"), num_of(sev)) == 0);
        }
    } else if (str_is_any(vm, mv, condp, "wait")) {
        if (is_numv(sev) && num_of(sev) > 0) {
            set_num(s, "mod_step_enter_var", num_of(sev) - 1);
            enter = (num_of(sev) - 1 <= 0);
        } else {
            inst_var_set(s, "mod_step_enter_condition", mk_str(vm, ""));
        }
    } else if (str_is_any(vm, mv, condp, "cell")) {
        enter = (get_num(s, "grid_col") != get_num(s, "prev_grid_col")
              || get_num(s, "grid_row") != get_num(s, "prev_grid_row"));
    } else if (str_is_any(vm, mv, condp, "cell_row")) {
        enter = (get_num(s, "grid_row") != get_num(s, "prev_grid_row"));
    } else if (str_is_any(vm, mv, condp, "cell_target_row") || str_is_any(vm, mv, condp, "cell_target_col")) {
        bool has_t = false;
        double trow = 0, tcol = 0;
        int tgt = (int)get_num(s, "bullet_target");
        if (tgt != -1 && inst_get(tgt)) {
            trow = get_num(tgt, "grid_row");
            tcol = get_num(tgt, "grid_col");
            has_t = true;
        } else if (get_num(s, "bullet_target_x") != 0 || get_num(s, "bullet_target_y") != 0) {
            Value a[3];
            a[0] = mk_num(get_num(s, "bullet_target_x")); a[1] = mk_num(get_num(s, "bullet_target_y"));
            a[2] = mk_num(1);
            Value tgp = call_ret(vm, "get_grid_position_from_world", a, 3);
            trow = num_of(&m_get(vm, tgp, "row"));
            tcol = num_of(&m_get(vm, tgp, "col"));
            has_t = true;
        }
        if (has_t) {
            if (str_is_any(vm, mv, condp, "cell_target_row")) enter = (get_num(s, "grid_row") == trow);
            else                                              enter = (get_num(s, "grid_col") == tcol);
        } else {
            enter = false;                                     // 目标没设 → 永不进
        }
    } else if (str_is_any(vm, mv, condp, "attack")) {
        set_num(s, "mod_has_enemy", bul_enemy_count() > 0 ? 1 : 0);
        enter = truthy(inst_var(s, "mod_has_enemy"));
    } else if (str_is_any(vm, mv, condp, "attack_collision")) {
        enter = (container_size(*inst_var(s, "mod_collision_enemies")) > 0);
    } else if (str_is_any(vm, mv, condp, "attack_cell")) {
        set_num(s, "mod_has_enemy", 0);
        double gr = get_num(s, "grid_row"), gc = get_num(s, "grid_col");
        if (global_exists("enemy_array") && global_exists("grid_cols")
                && gr >= 0 && gr < gnumv(vm, "grid_rows")) {
            double stride = gnumv(vm, "grid_cols") + 2;
            if (gc >= 0 && gc < stride) {
                Value ea = gv(vm, "enemy_array");
                int idx = (int)(gr * stride + gc);
                if (idx >= 0 && idx < container_size(ea)) {
                    Value cell = container_at(ea, idx);
                    if (cell.t == MEM_ARRAY) set_num(s, "mod_has_enemy", container_size(cell) > 0 ? 1 : 0);
                }
            } else if (gc < 0 && global_exists("enemy_array_left")) {
                Value left = gv(vm, "enemy_array_left");
                if (gr < container_size(left)) {
                    Value cell = container_at(left, (int)gr);
                    if (cell.t == MEM_ARRAY) set_num(s, "mod_has_enemy", container_size(cell) > 0 ? 1 : 0);
                }
            }
        }
        enter = (get_num(s, "mod_has_enemy") == 1);
    }

    // 跑子弹自己的 _OBJECT_STEP
    int run_total = container_size(watch_changed) + (enter ? 1 : 0);
    for (int k = 0; k < run_total; k++) {
        if (!inst_get(s)) break;
        if (k < container_size(watch_changed)) {
            Value nm = array_v(vm, watch_changed, k);
            inst_var_set(s, "mod_changed_prop", nm);
        } else {
            inst_var_set(s, "mod_changed_prop", mk_str(vm, ""));
        }
        if (bul_inited(s) && mv) {
            Value bak_cur = gv(vm, "_VM_cur_card");
            global_var_set("_VM_cur_card", mk_num(s));
            mod_run_block(mv, s, "_OBJECT_STEP", NULL);
            global_var_set("_VM_cur_card", bak_cur);
        }
    }
    if (!inst_get(s)) return true;

    // 碰撞列表用完就清（无条件清，不然不读它的子弹会一直涨）
    { Value a[2]; a[0] = *inst_var(s, "mod_collision_enemies"); a[1] = mk_num(0);
      call_fn_v(vm, "array_resize", a, 2, -1); }
    set_num(s, "mod_collision_hit", 0);

    // 自动销毁
    if (get_num(s, "destroy_timer") == 0) { call_fn(vm, "instance_destroy", NULL, 0, -1); return true; }
    if (get_num(s, "destroy_timer") > 0) {
        add_num(s, "destroy_timer", -1);
        if (get_num(s, "destroy_timer") <= 0) call_fn(vm, "instance_destroy", NULL, 0, -1);
    }

    add_num(s, "x", get_num(s, "vx"));
    add_num(s, "y", get_num(s, "vy"));

    // 抛物线（覆盖直线位移）
    if (get_num(s, "bullet_parabola") == 1) {
        if (get_num(s, "bullet_para_t") < 0) {
            set_num(s, "bullet_para_x0", get_num(s, "x"));
            set_num(s, "bullet_para_y0", get_num(s, "y"));
            int tgt = (int)get_num(s, "bullet_target");
            if (tgt != -1 && inst_get(tgt)) {
                set_num(s, "bullet_target_x", get_num(tgt, "x"));
                set_num(s, "bullet_target_y", get_num(tgt, "y"));
            }
            set_num(s, "bullet_para_t", 0);
            double dx = get_num(s, "bullet_target_x") - get_num(s, "bullet_para_x0");
            double dy = get_num(s, "bullet_target_y") - get_num(s, "bullet_para_y0");
            set_num(s, "bullet_para_dist", std::sqrt(dx * dx + dy * dy));   // point_distance
            if (get_num(s, "bullet_parabola_h") < 0)
                set_num(s, "bullet_parabola_h", get_num(s, "bullet_para_dist") * 0.25);
        }
        add_num(s, "bullet_para_t", 1);
        double pt = std::fmax(1, get_num(s, "bullet_parabola_time"));
        double pf = std::fmin(1, get_num(s, "bullet_para_t") / pt);
        double x0 = get_num(s, "bullet_para_x0"), y0 = get_num(s, "bullet_para_y0");
        double tx = get_num(s, "bullet_target_x"), ty = get_num(s, "bullet_target_y");
        set_num(s, "x", x0 + (tx - x0) * pf);                              // lerp
        set_num(s, "y", y0 + (ty - y0) * pf - get_num(s, "bullet_parabola_h") * std::sin(3.14159265358979323846 * pf));
        if (get_num(s, "bullet_para_t") >= pt) {
            set_num(s, "x", tx);
            set_num(s, "y", ty);
            set_num(s, "vx", 0);
            set_num(s, "vy", 0);
            set_num(s, "bullet_parabola", 0);
            set_num(s, "bullet_para_t", -1);
        }
    }

    // 路径数组：dx_arr / dy_arr 每帧位移（叠加，不覆盖 vx/vy 和抛物线）
    if (bul_inited(s) && mv) {
        std::map<std::string, std::vector<Value> >::iterator ix = mv->named_arrays.find("dx_arr");
        std::map<std::string, std::vector<Value> >::iterator iy = mv->named_arrays.find("dy_arr");
        if (ix != mv->named_arrays.end() && iy != mv->named_arrays.end()) {
            // 命名数组在我们这边是 vector<Value>，下标从 0 起，跟 GML 的数组一样
            int lx = (int)ix->second.size(), ly = (int)iy->second.size();
            int plen = lx < ly ? lx : ly;                                 // 按短的算
            if (plen > 0) {
                int pk = (int)std::fmod(get_num(s, "bullet_path_i"), plen);
                add_num(s, "x", num_of(&ix->second[(size_t)pk]));
                add_num(s, "y", num_of(&iy->second[(size_t)pk]));
                add_num(s, "bullet_path_i", 1);
            }
        }
    }

    if (get_num(s, "x") > 2200 || get_num(s, "y") > 1200
            || get_num(s, "x") < 0 || get_num(s, "y") < 0)
        call_fn(vm, "instance_destroy", NULL, 0, -1);
    return true;
}

// Draw_0.gml（32 行）：延迟初始化 → 有 mod 的绘制块就画它（并 exit），否则画本体
bool hot_bulletmod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    Vm* mv = bul_vm(vm, s);
    if (!bul_inited(s) && mv) bul_init(vm, s, mv, false);   // GML 在 Draw 里也做延迟初始化（且只在这儿 mod_inst_add？见文件头说明）

    if (bul_inited(s) && mv) {
        Value bak_cur = gv(vm, "_VM_cur_card");
        global_var_set("_VM_cur_card", mk_num(s));
        if (mod_run_block(mv, s, "_OBJECT_DRAW", NULL)) {
            global_var_set("_VM_cur_card", bak_cur);
            return true;                                   // GML 的 exit：不再画本体
        }
        global_var_set("_VM_cur_card", bak_cur);
    }
    call_fn(vm, "draw_self", NULL, 0, -1);
    return true;
}

// Destroy_0.gml（17 行）
bool hot_bulletmod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (bul_inited(s)) {
        Vm* mv = bul_vm(vm, s);
        if (mv) {
            Value bak_last = gv(vm, "_VM_last_destroyed_card");
            Value bak_cur  = gv(vm, "_VM_cur_card");
            global_var_set("_VM_last_destroyed_card", mk_num(s));
            global_var_set("_VM_cur_card", mk_num(s));
            mod_run_block(mv, s, "_OBJECT_DESTROY", NULL);
            global_var_set("_VM_last_destroyed_card", bak_last);
            global_var_set("_VM_cur_card", bak_cur);
            mod_inst_del_v(mv, s, false);                  // 子弹只减计数，不动 instances
        }
    }
    return true;
}

// Collision_obj_enemy_parent.gml（65 行）——记录 + 按字段自动结算
//   other = 宿主派发这个块之前设好的 vm.collision_other
bool hot_bulletmod_collision(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    int other = vm.collision_other;
    if (!inst_get(other)) return true;

    if (get_num(s, "bullet_collide") == 0) return true;    // 关掉碰撞：不结算不记录
    if (get_num(other, "hp") <= 0) return true;            // 已经打死的（播死亡动画的）不再重复结算

    set_num(s, "mod_collision_enemy", other);
    set_num(s, "mod_collision_hit", 1);
    { Value a[2]; a[0] = *inst_var(s, "mod_collision_enemies"); a[1] = mk_num(other);
      call_fn_v(vm, "array_push", a, 2, -1); }

    // 先把要用的值取出来（伤害结算可能把敌人打死）
    double dmg   = get_num(s, "bullet_damage");
    Value dtype  = *inst_var(s, "bullet_damage_type");
    double slow  = (get_num(s, "bullet_slow") == 1) ? get_num(s, "bullet_slow_frame") : 0;
    double frz = 0, stun = 0;
    if (get_num(s, "bullet_freeze_chance") > 0) {
        Value a[1]; a[0] = mk_num(1);
        if (num_of(&call_ret(vm, "random", a, 1)) < get_num(s, "bullet_freeze_chance"))
            frz = get_num(s, "bullet_freeze_frame");
    }
    if (get_num(s, "bullet_stun_chance") > 0) {
        Value a[1]; a[0] = mk_num(1);
        if (num_of(&call_ret(vm, "random", a, 1)) < get_num(s, "bullet_stun_chance"))
            stun = get_num(s, "bullet_stun_frame");
    }

    // ① 伤害：走敌人自己的受击事件（闪白/音效/护盾都对）
    if (dmg > 0) {
        Value a[3];
        a[0] = mk_num(other); a[1] = mk_num(dmg); a[2] = dtype;
        call_fn_v(vm, "damage_enemy", a, 3, -1);
    }

    // ② 状态：取"更长的那一个"，不会把敌人身上更长的状态缩短
    if (inst_get(other) && (slow > 0 || frz > 0 || stun > 0)) {
        if (slow > 0 && get_num(other, "ice_timer") < slow)     set_num(other, "ice_timer", slow);
        if (frz > 0 && get_num(other, "frozen_timer") < frz)    set_num(other, "frozen_timer", frz);
        if (stun > 0 && get_num(other, "stun_timer") < stun)    set_num(other, "stun_timer", stun);
    }

    // ③ 消耗次数：第一次撞到才按 bullet_hits 初始化；-1 = 穿透，永不消耗
    if (get_num(s, "bullet_hits_init") == 0) {
        set_num(s, "bullet_hits_left", get_num(s, "bullet_hits"));
        set_num(s, "bullet_hits_init", 1);
    }
    if (get_num(s, "bullet_hits") != -1) {
        add_num(s, "bullet_hits_left", -1);
        // ⚠ 不在这儿 instance_destroy（碰撞事件跑在 Step 之后）：
        //   置 destroy_timer = 0，让下一次 Step 先跑完 VM 再销毁
        if (get_num(s, "bullet_hits_left") <= 0) set_num(s, "destroy_timer", 0);
    }
    return true;
}

// Mouse_4 / Mouse_10 / Mouse_11：先跑子弹 VM 里对应的块，再 event_inherited()
//   （obj_bullet_mod 没有父对象，event_inherited 在 GML 里就是空操作）
bool hot_bulletmod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = bul_vm(vm, s);
    if (bul_inited(s) && mv) mod_run_block(mv, s, block, "_VM_cur_card");
    hot_inherited(vm, "obj_bullet_mod", block);            // 没有父对象 → 直接 false
    return true;
}
bool hot_bulletmod_click(Vm& vm)       { return hot_bulletmod_mouse(vm, "_OBJECT_CLICK"); }
bool hot_bulletmod_mouse_enter(Vm& vm) { return hot_bulletmod_mouse(vm, "_OBJECT_MOUSE_ENTER"); }
bool hot_bulletmod_mouse_leave(Vm& vm) { return hot_bulletmod_mouse(vm, "_OBJECT_MOUSE_LEAVE"); }
