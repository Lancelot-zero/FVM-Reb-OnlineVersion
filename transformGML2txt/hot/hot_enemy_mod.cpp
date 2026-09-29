// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"

// ============================================================
// obj_enemy_mod —— 通用 mod 敌人（Create 这块；其余事件在本文件下面）
//   继承 obj_enemy_parent（移动/攻击/死亡全复用父类），特殊逻辑在**这个敌人的 VM**里。
//   敌人 id 由生成逻辑在创建前写进 global._mod_pending_enemy_id。
// ============================================================

// Create_0.gml（109 行）
//   初始化直接落地：内置字段写字段（这里没有），其余写 vars；数组走 set_var（要 retain）
bool hot_enemymod_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;
    hot_inherited_sid(vm, S_obj_enemy_mod, S__OBJECT_CREATE);        // event_inherited()

    // enemy_id = variable_global_exists(...) ? global._mod_pending_enemy_id : ""
    if (global_exists_sid(S__mod_pending_enemy_id)) in->vars[S_enemy_id] = gv_sid(S__mod_pending_enemy_id);
    else                                        in->vars[S_enemy_id] = mk_str(vm, "");
    in->vars[S_mod_enemy_inited_id] = mk_str(vm, "");

    in->vars[S_mod_step_enter_condition] = mk_str(vm, "");
    in->vars[S_mod_changed_prop]         = mk_str(vm, "");
    set_var_sid(s, S_mod_watch_last,    call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_watch_changed, call_ret_sid(vm, S_array_literal, NULL, 0));

    static const struct { int n;              /* 池下标 */ double v; } init[] = {
        { S_mod_step_enter_var, 0 }, { S_mod_tick_cool, 0 }, { S_mod_has_card, 0 },
        { S_mod_countdown, 0 }, { S_mod_alpha_add, 0 },
        { S_mod_hover_mask, 0 }, { S_mod_hover_alpha, 0.35 },
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        in->vars[init[i].n] = mk_num(init[i].v);

    in->vars[S_prev_grid_col] = mk_num(std::floor((get_num_sid(s, S_x) - gnumv_sid(S_grid_offset_x)) / gnumv_sid(S_grid_cell_size_x)));
    in->vars[S_prev_grid_row] = mk_num(std::floor((get_num_sid(s, S_y) - gnumv_sid(S_grid_offset_y)) / gnumv_sid(S_grid_cell_size_y)));

    if (!pool_text(inst_var_sid(s, S_enemy_id)).empty())
        run_hot_block_sid(vm, S_obj_enemy_mod, S__FUNC_mod_enemy_init);
    return true;
}



// _FUNC_mod_enemy_init（Create 里定义的本地函数，联机时 id 晚到由 Step 补跑）
bool hot_enemymod_func_init(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Value* eidp = inst_var_sid(s, S_enemy_id);
    int eid_sid = (eidp && eidp->t == MEM_STRING && str_ok(eidp->v)) ? eidp->v : -1;   // 全程带下标
    if (eid_sid < 0) return true;
    Value* inited = inst_var_sid(s, S_mod_enemy_inited_id);
    if (inited && inited->t == MEM_STRING && inited->v == eidp->v) return true;   // 已经按这个 id 初始化过

    // 本机注册过这个 enemy_id 吗
    bool known = false;
    Value emap = gv_sid(S_enemy_map);
    if (global_exists_sid(S_enemy_map) && emap.t == MEM_MAP) {
        Value a[2]; a[0] = emap; a[1] = *eidp;
        known = truthy(&call_ret_sid(vm, S_ds_map_exists, a, 2));
    }
    Vm* mv = mod_vm_sid(eid_sid);                                   // 零哈希（原来是 pool_text + str_find）my_id]
    if (!known && mv) known = true;

    if (!known) {
        // 非法 mod 敌人 → 换成普通小老鼠（铁锅鼠）替身，本实例销毁
        Value m[1];
        m[0] = mk_str(vm, ("[mod 敌人] 未注册的 enemy_id，换成普通老鼠: " + str_at(eid_sid)).c_str());
        call_fn_sid(vm, S_show_debug_message, m, 1, -1);
        // _nid = ds_map_exists(global.network.map_instance_id_net_id, id) ? ...[? id] : -1
        double nid = -1;
        Value net = gv_sid(S_network);
        Value map; map.t = MEM_UNDEFINED; map.v = 0;
        if (net.t == MEM_MAP) map = v_struct_sid(vm, net, S_map_instance_id_net_id);
        if (map.t == MEM_MAP) {
            Value a[2]; a[0] = map; a[1] = mk_num(s);
            if (truthy(&call_ret_sid(vm, S_ds_map_exists, a, 2))) nid = num_of(&v_map(vm, map, mk_num(s)));
        }
        Value ca[4];
        ca[0] = mk_num(get_num_sid(s, S_x)); ca[1] = mk_num(get_num_sid(s, S_y));
        ca[2] = mk_num(get_num_sid(s, S_depth));
        ca[3] = mk_str_sid(S_obj_iron_pan_mouse);
        Value rep = call_ret_sid(vm, S_instance_create_depth, ca, 4);
        int rid = rep.t == MEM_INT ? rep.v : -1;
        if (inst_get(rid)) {
            if (nid != -1) {                                        // set_net_id(_rep.id, _nid)
                Value sa[2]; sa[0] = mk_num(rid); sa[1] = mk_num(nid);
                call_fn_sid(vm, S_set_net_id, sa, 2, -1);
            }
            inst_var_set_sid(rid, S_grid_row, *inst_var_sid(s, S_grid_row));
            inst_var_set_sid(rid, S_grid_col, *inst_var_sid(s, S_grid_col));
        }
        call_fn_int_sid(vm, S_instance_destroy, NULL, 0, -1);               // instance_destroy(id)
        return true;
    }

    inst_var_set_sid(s, S_mod_enemy_inited_id, *eidp);

    // 套用注册表数值（原版敌人是自己 Create 里写死的，mod 敌人统一从 enemy_map 取）
    if (global_exists_sid(S_enemy_map) && emap.t == MEM_MAP) {
        Value info = v_map(vm, emap, *eidp);
        if (info.t == MEM_MAP) {
            Value spr = m_get_sid(vm, info, S_spr);
            if (spr.t != MEM_UNDEFINED && num_of(&spr) != -4) inst_var_set_sid(s, S_sprite_index, spr);   // != noone
            set_num_sid(s, S_hp, m_or_num_sid(vm, info, S_hp, get_num_sid(s, S_hp)));
            set_num_sid(s, S_maxhp, get_num_sid(s, S_hp));
            set_num_sid(s, S_shield_hp, m_or_num_sid(vm, info, S_shield, 0));
            set_num_sid(s, S_shield_max_hp, get_num_sid(s, S_shield_hp));
            set_num_sid(s, S_move_speed,   m_or_num(vm, info, "speed", get_num_sid(s, S_move_speed)));
            set_num_sid(s, S_atk,          m_or_num_sid(vm, info, S_atk,   get_num_sid(s, S_atk)));
            set_num_sid(s, S_atk_cycle,    m_or_num_sid(vm, info, S_cycle, get_num_sid(s, S_atk_cycle)));
            set_num_sid(s, S_attack_range, m_or_num_sid(vm, info, S_range, get_num_sid(s, S_attack_range)));
            set_num_sid(s, S_immune_to_ash, m_or_num_sid(vm, info, S_ash_proof, 0));
            Value ft = m_get_sid(vm, info, S_feature);
            inst_var_set_sid(s, S_feature, ft.t == MEM_UNDEFINED ? mk_str_sid(S_land) : ft);
        }
    }

    // 加进该 mod 的实例容器，并跑它 VM 里的创建块
    if (mv) {
        mod_inst_add_v(mv, s, true);                                // mod_inst_add(_vm, id)
        Value bak_last = gv_sid(S__VM_last_created_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_created_card, mk_num(s));
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_CREATE, NULL);
        global_var_set_sid(S__VM_last_created_card, bak_last);
        global_var_set_sid(S__VM_cur_card, bak_cur);
    }
    return true;
}


// Step_0.gml（133 行）：基础 tick → 父类 Step → 进 VM 的闸门 → 跑 mod 的步进块
bool hot_enemymod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;

    mod_base_tick_v(s);                                            // mod_base_tick(id)

    set_num_sid(s, S_prev_grid_col, get_num_sid(s, S_grid_col));            // 父类 Step 会重算 grid_col/row
    set_num_sid(s, S_prev_grid_row, get_num_sid(s, S_grid_row));

    Value* eidp = inst_var_sid(s, S_enemy_id);
    int eid_sid = (eidp && eidp->t == MEM_STRING && str_ok(eidp->v)) ? eidp->v : -1;
    Value* inited = inst_var_sid(s, S_mod_enemy_inited_id);
    if (eid_sid >= 0 && !(inited && inited->t == MEM_STRING && inited->v == eidp->v)) {
        run_hot_block_sid(vm, S_obj_enemy_mod, S__FUNC_mod_enemy_init);
        if (!inst_get(s)) return true;                              // 非法敌人已被换掉 → exit
        eidp = inst_var_sid(s, S_enemy_id);
        eid_sid = (eidp && eidp->t == MEM_STRING && str_ok(eidp->v)) ? eidp->v : -1;
    }

    hot_inherited_sid(vm, S_obj_enemy_mod, S__OBJECT_STEP);             // event_inherited()

    bool enter = true;
    const Value* condp = inst_var_sid(s, S_mod_step_enter_condition);
    Vm* mv = mod_vm_sid(eid_sid);

    // ── 盯梢 mod_watch ──
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    Value watch_changed = *inst_var_sid(s, S_mod_watch_changed);
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0);
      call_fn_sid(vm, S_array_resize, a, 2, -1); }
    if (eid_sid >= 0 && mv) {
        std::unordered_map<int, std::vector<Value> >::iterator wit = mv->named_arrays.find(S_mod_watch);
        if (wit != mv->named_arrays.end()) {
            // mod_watch 里存的是**属性名字符串**，下标属于 mod VM 的池
            const std::vector<Value>& wlist = wit->second;
            Value last = *inst_var_sid(s, S_mod_watch_last);
            int wn = (int)wlist.size();
            if (container_size(last) != wn) {
                // 列表长度变了：这一帧只对齐快照，不报变化
                Value a[2]; a[0] = last; a[1] = mk_num(wn);
                call_fn_sid(vm, S_array_resize, a, 2, -1);
                for (int i = 0; i < wn; i++) {
                    std::string nm = pool_text(&wlist[(size_t)i]);
                    Value cur; cur.t = MEM_UNDEFINED; cur.v = 0;
                    Value* ip = inst_var(s, nm);
                    if (ip) cur = *ip;
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());
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
                        call_fn_sid(vm, S_array_push, pa, 2, -1);      // 每个变化的属性各进一次 Step
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    // ── 闸门 ──
    double cdv = 0;
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
    } else if (str_is_any_sid(vm, mv, condp, S_cell)) {
        enter = (get_num_sid(s, S_grid_col) != get_num_sid(s, S_prev_grid_col)
              || get_num_sid(s, S_grid_row) != get_num_sid(s, S_prev_grid_row));
    } else if (str_is_any_sid(vm, mv, condp, S_hp_change)) {
        enter = (get_num_sid(s, S_pre_hp) != get_num_sid(s, S_hp));            // 父类的 pre_hp 在 End Step 才更新
    } else if (str_is_any_sid(vm, mv, condp, S_hp_change_mod)) {
        cdv = num_of(sev);
        if (!is_numv(sev) || cdv < 0) cdv = 0;
        if (get_num_sid(s, S_mod_tick_cool) > 0) {
            add_num_sid(s, S_mod_tick_cool, -1);
            enter = false;
        } else if (get_num_sid(s, S_pre_hp) != get_num_sid(s, S_hp)) {
            enter = true;
            set_num_sid(s, S_mod_tick_cool, cdv);
        } else {
            enter = false;
        }
    } else if (str_is_any_sid(vm, mv, condp, S_card) || str_is_any_sid(vm, mv, condp, S_card_mod)) {
        bool has_card = inst_get((int)get_num_sid(s, S_target_plant)) ? true : false;   // instance_exists(target_plant)
        set_num_sid(s, S_mod_has_card, has_card ? 1 : 0);
        if (str_is_any_sid(vm, mv, condp, S_card)) {
            enter = has_card;
        } else {
            cdv = num_of(sev);
            if (!is_numv(sev) || cdv < 0) cdv = 0;
            if (get_num_sid(s, S_mod_tick_cool) > 0) {
                add_num_sid(s, S_mod_tick_cool, -1);
                enter = false;
            } else if (has_card) {
                enter = true;
                set_num_sid(s, S_mod_tick_cool, cdv);
            } else {
                enter = false;
            }
        }
    }

    // ── 这一帧进几次：盯梢每个变化的属性各 1 次，闸门放行再 1 次 ──
    //   （GML 在循环条件里每次重读 array_length，这里也一样：块里可能改这个数组）
    int run_total = container_size(watch_changed) + (enter ? 1 : 0);
    for (int k = 0; k < run_total; k++) {
        if (!inst_get(s)) break;
        if (k < container_size(watch_changed)) {
            Value nm = array_v(vm, watch_changed, k);
            inst_var_set_sid(s, S_mod_changed_prop, nm);
        } else {
            inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
        }
        if (eid_sid >= 0 && mv) {
            Value bak_cur = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_STEP, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
        }
    }
    return true;
}


// Draw_0.gml（34 行）：父类绘制 → mod 绘制块 → 鼠标移入遮罩
bool hot_enemymod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    hot_inherited_sid(vm, S_obj_enemy_mod, S__OBJECT_DRAW);             // event_inherited()

    Vm* mv = mod_vm_of_inst_sid(vm, s, S_enemy_id);
    if (mv) mod_run_block_sid(mv, s, S__OBJECT_DRAW, str_at(S__VM_cur_card).c_str());

    if (get_num_sid(s, S_mod_hover_mask) == 1) {
        // position_meeting 靠碰撞遮罩，贴图没遮罩时用 bbox 兜底
        //   ⚠ 碰撞/精灵几何还没建模：这两条现在恒为 false（宿主实现后自动生效）
        Value a[3];
        a[0] = gv_sid(S_mouse_x); a[1] = gv_sid(S_mouse_y); a[2] = mk_num(s);
        bool hovered = truthy(&call_ret_sid(vm, S_position_meeting, a, 3));
        if (!hovered) {
            Value r[5];
            r[0] = gv_sid(S_mouse_x); r[1] = gv_sid(S_mouse_y);
            r[2] = iv_or_sid(s, S_bbox_left, 0);   r[3] = iv_or_sid(s, S_bbox_top, 0);
            r[4] = iv_or_sid(s, S_bbox_right, 0);
            hovered = truthy(&call_ret_sid(vm, S_point_in_rectangle, r, 5));
            // （GML 里还有 bbox_bottom，point_in_rectangle 只收 4 个点 + 这里第 5 个是右下角）
        }
        if (hovered) {
            Value sa[1]; sa[0] = iv_or_sid(s, S_hit_effect_2, -1);
            call_fn_sid(vm, S_shader_set, sa, 1, -1);
            Value d[9];
            d[0] = *inst_var_sid(s, S_sprite_index);
            d[1] = *inst_var_sid(s, S_image_index);
            d[2] = mk_num(get_num_sid(s, S_x)); d[3] = mk_num(get_num_sid(s, S_y));
            d[4] = *inst_var_sid(s, S_image_xscale); d[5] = *inst_var_sid(s, S_image_yscale);
            d[6] = *inst_var_sid(s, S_image_angle);
            d[7] = mk_num(16777215);                                   // c_white
            d[8] = mk_num(get_num_sid(s, S_mod_hover_alpha));
            call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
            call_fn_int_sid(vm, S_shader_reset, NULL, 0, -1);
        }
    }
    return true;
}


// Destroy_0.gml（18 行）：父类销毁 → mod 销毁块 → 移出实例容器
bool hot_enemymod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    hot_inherited_sid(vm, S_obj_enemy_mod, S__OBJECT_DESTROY);
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_enemy_id);
    if (mv) {
        Value bak_last = gv_sid(S__VM_last_destroyed_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_destroyed_card, mk_num(s));
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_DESTROY, NULL);
        global_var_set_sid(S__VM_last_destroyed_card, bak_last);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        mod_inst_del_v(mv, s, true);                                // mod_inst_del(_vm, id)
    }
    return true;
}


// Mouse_4 / Mouse_10 / Mouse_11：先跑 mod VM 里的对应块，再 event_inherited()
bool hot_enemymod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_enemy_id);
    if (mv) mod_run_block(mv, s, block, str_at(S__VM_cur_card).c_str());
    hot_inherited_sid(vm, S_obj_enemy_mod, str_find(block));                      // 父对象没有这些块 → 直接 false
    return true;
}

bool hot_enemymod_click(Vm& vm)      { return hot_enemymod_mouse_sid(vm, S__OBJECT_CLICK); }
bool hot_enemymod_mouse_enter(Vm& vm) { return hot_enemymod_mouse_sid(vm, S__OBJECT_MOUSE_ENTER); }
bool hot_enemymod_mouse_leave(Vm& vm) { return hot_enemymod_mouse_sid(vm, S__OBJECT_MOUSE_LEAVE); }

// ============================================================
// obj_player_character —— 玩家角色卡（7 个事件，12.4 KB）
//   父对象是 obj_card_parent，但**一个事件都没调 event_inherited()**（全自己重写），
//   所以这里不接父类，照 GML 抄。
//   常量：CARD_STATE.IDLE=0 / ATTACK=1、noone=-4、c_white=16777215、MSG_GAME_OVER=10
//   ⚠ 它的血量变量叫 hp / max_hp（不是卡片父对象的 hp / maxhp）
// ============================================================

// 发一个事件给某个实例（GML 的 with (inst) event_perform(ev_step, ev_step_normal)）
//   → 就是"跑那个实例的 _OBJECT_STEP"，对象没装载/没硬编码就静默跳过
bool inst_event(Vm& vm, int inst, const char* block) {
    Instance* in = inst_get(inst);
    if (!in) return false;
    return run_block_on(vm, in->obj, inst, intern_str(block));   // 块名 → 池下标
}
