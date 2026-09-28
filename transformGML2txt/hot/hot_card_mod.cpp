// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// variable_instance_set，但属性名可能是 mod VM 池里的字符串（mod_set_prop 那些）
void viset_any(Vm& vm, Vm* mv, int tgt, const Value& name, const Value& val) {
    std::string nm = text_pref_mod(vm, mv, name);
    if (nm.empty() || name.t != MEM_STRING) return;
    Value a[3];
    a[0] = mk_num(tgt);
    a[1] = mk_str(vm, nm.c_str());        // 池是全局的：文本已在池里，拿回的就是同一下标
    a[2] = val;
    call_fn_sid(vm, S_variable_instance_set, a, 3, -1);
}


// Create_0.gml（125 行）
bool hot_cardmod_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;
    hot_inherited_sid(vm, S_obj_card_mod, S__OBJECT_CREATE);           // event_inherited()

    // plant_id = variable_global_exists("_mod_pending_card_id") ? global._mod_pending_card_id : ""
    if (global_exists_sid(S__mod_pending_card_id)) inst_var_set_sid(s, S_plant_id, gv_sid(S__mod_pending_card_id));
    else                                       inst_var_set_sid(s, S_plant_id, mk_str(vm, ""));

    // 计时 / 索敌 / 进 VM 的条件（默认值必须**先**写完，event_user(0) 可能会抛错中断）
    static const struct { int n;              /* 池下标 */ double v; } init[] = {
        { S_mod_tick_cycle, 0 }, { S_mod_tick_max, 0 }, { S_mod_tick_enemy, 0 },
        { S_mod_has_enemy, 0 }, { S_mod_enemy_check, 0 }, { S_mod_step_enter_index, -1 },
        { S_mod_hp_last, 0 }, { S_mod_tick_cool, 0 }, { S_mod_countdown, 0 },
        { S_mod_alpha_add, 0 }, { S_mod_hover_mask, 0 }, { S_mod_hover_alpha, 0.35 },
        { S_mod_set_on, 0 }, { S_mod_set2_on, 0 }, { S_mod_step_enter_var, 0 },
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        in->vars[init[i].n] = mk_num(init[i].v);
    in->vars[S_mod_changed_prop] = mk_str(vm, "");          // 字符串：直接落值
    in->vars[S_mod_step_enter_condition] = mk_str(vm, "");
    { Value z[1]; z[0] = mk_num(0); (void)z;
      set_var_sid(s, S_mod_step_enemy_area, call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_enemy_types,     call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_watch_last,      call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_watch_changed,   call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_step_enter_arr,  call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set_id,          call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set_prop,        call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set_val,         call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set2_id,         call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set2_prop,       call_ret_sid(vm, S_array_literal, NULL, 0));
      set_var_sid(s, S_mod_set2_val,        call_ret_sid(vm, S_array_literal, NULL, 0));
    }

    inst_event_inherited_sid(vm, s, S__OBJECT_USER_EVENT_0);            // event_user(0)

    in->vars[S_mod_hp_last]  = mk_num(get_num_sid(s, S_hp));            // 血量基线 = 出生血量
    in->vars[S_mod_tick_cool] = mk_num(0);

    // 装进该卡的实例容器 + 跑它 VM 里的创建块
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_plant_id);
    if (mv) {
        if (mv->mod_card_data.t == MEM_MAP) {                       // _vm[$ "card_data"]
            Value pt = m_get_sid(vm, mv->mod_card_data, S_plant_type);
            if (pt.t != MEM_UNDEFINED) in->vars[S_plant_type] = pt;   // 字符串，直接落
        }
        mod_inst_add_v(mv, s, true);
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


// Step 里的索敌（mod_enemy_check==1 或条件 "norm_attack" 才走）
//   区域 = mod_step_enemy_area 每 4 个值一组 [上,下,左,右]；类型 = mod_enemy_types（空 = 任意）
//   实现靠 global.has_enemy_* 的每行前缀和（obj_battle 每帧重建）
void cardmod_enemy_search(Vm& vm, int s, Vm* mv) {
    Value area = *inst_var_sid(s, S_mod_step_enemy_area);
    int groups = 0;
    if (area.t == MEM_ARRAY) groups = (int)std::floor(container_size(area) / 4.0);
    if (groups < 1) {                                               // 凑不满一组 → 默认 [1,1,0,99]
        area = call_ret_sid(vm, S_array_literal, NULL, 0);
        double dflt[4] = { 1, 1, 0, 99 };
        for (int i = 0; i < 4; i++) array_setn(vm, area, i, dflt[i]);
        groups = 1;
    }

    // 要看的类型（空 = 任意）
    Value ty = *inst_var_sid(s, S_mod_enemy_types);
    bool any = (ty.t != MEM_ARRAY || container_size(ty) == 0);
    bool w_n = any, w_o = any, w_d = any, w_a = any, w_c = any, w_u = any;
    if (!any) {
        w_n = w_o = w_d = w_a = w_c = w_u = false;
        int tn = container_size(ty);
        for (int k = 0; k < tn; k++) {
            std::string t = text_pref_mod(vm, mv, array_v(vm, ty, k));
            if (t.empty() || t == str_at(S_all)) { w_n = w_o = w_d = w_a = w_c = w_u = true; }
            else if (t == str_at(S_normal))      w_n = true;
            else if (t == str_at(S_obstacle))    w_o = true;
            else if (t == str_at(S_diver))       w_d = true;
            else if (t == str_at(S_air))         w_a = true;
            else if (t == str_at(S_dance))       w_c = true;
            else if (t == str_at(S_underground)) w_u = true;
        }
        // 一个都没认出来（写错了）→ 按"任意类型"兜底，别把卡永远卡死
        if (!w_n && !w_o && !w_d && !w_a && !w_c && !w_u) w_n = w_o = w_d = w_a = w_c = w_u = true;
    }

    set_num_sid(s, S_mod_has_enemy, 0);
    if (!global_exists_sid(S_has_enemy_normal)) return;
    double gr = get_num_sid(s, S_grid_row), gc = get_num_sid(s, S_grid_col);
    if (gr < 0 || gc < 0) return;

    Value hn = gv_sid(S_has_enemy_normal), ho = gv_sid(S_has_enemy_obstacle);
    Value hd = gv_sid(S_has_enemy_diver),  ha = gv_sid(S_has_enemy_air);
    Value hc = gv_sid(S_has_enemy_dance),  hu = gv_sid(S_has_enemy_underground);
    double stride = gnumv_sid(S_grid_cols) + 2;
    double rows_m1 = gnumv_sid(S_grid_rows) - 1;
    double cols_p1 = gnumv_sid(S_grid_cols) + 1;

    for (int g = 0; g < groups; g++) {
        int gi = g * 4;
        double up = array_n(vm, area, gi), down = array_n(vm, area, gi + 1);
        double left = array_n(vm, area, gi + 2), right = array_n(vm, area, gi + 3);

        double r1 = gr - up;    if (r1 < 0) r1 = 0;
        double r2 = gr + down;  if (r2 > rows_m1) r2 = rows_m1;
        double c1raw = gc - left;
        double c1 = c1raw;      if (c1 < 0) c1 = 0;
        double c2 = gc + right; if (c2 > cols_p1) c2 = cols_p1;
        double before = c1 - 1;                                  // -1 = 左边界外，前缀和按 0 算

        if ((double)container_size(hn) > r2 * stride + c2) {
            for (double r = r1; r <= r2; r++) {
                double b = r * stride;
                double n = array_n(vm, hn, (int)(b + c2)) - (before >= 0 ? array_n(vm, hn, (int)(b + before)) : 0);
                double o = array_n(vm, ho, (int)(b + c2)) - (before >= 0 ? array_n(vm, ho, (int)(b + before)) : 0);
                double d = array_n(vm, hd, (int)(b + c2)) - (before >= 0 ? array_n(vm, hd, (int)(b + before)) : 0);
                double a = array_n(vm, ha, (int)(b + c2)) - (before >= 0 ? array_n(vm, ha, (int)(b + before)) : 0);
                double c = array_n(vm, hc, (int)(b + c2)) - (before >= 0 ? array_n(vm, hc, (int)(b + before)) : 0);
                double u = array_n(vm, hu, (int)(b + c2)) - (before >= 0 ? array_n(vm, hu, (int)(b + before)) : 0);
                if ((w_n && n > 0) || (w_o && o > 0) || (w_d && d > 0)
                        || (w_a && a > 0) || (w_c && c > 0) || (w_u && u > 0)) {
                    set_num_sid(s, S_mod_has_enemy, 1);
                    break;
                }
            }
        }
        // 场外左列（-1 / -2 …）：has_enemy_* 只统计 grid_col >= 0，负列敌人在 enemy_array_left 里
        if (get_num_sid(s, S_mod_has_enemy) == 0 && c1raw < 0 && global_exists_sid(S_enemy_array_left)) {
            int wmask = 0;
            if (w_n) wmask |= 1;
            if (w_o) wmask |= 8;
            if (w_d) wmask |= 16;
            if (w_a) wmask |= 2;
            if (w_c) wmask |= 4;
            if (w_u) wmask |= 32;
            Value left_arr = gv_sid(S_enemy_array_left);
            for (double lr = r1; lr <= r2; lr++) {
                if (lr >= container_size(left_arr)) break;
                Value llst = array_v(vm, left_arr, (int)lr);
                if (llst.t != MEM_ARRAY) continue;
                int ln = container_size(llst);
                for (int lk = 0; lk < ln; lk++) {
                    int le = (int)num_of(&array_v(vm, llst, lk));
                    if (!inst_get(le) || get_num_sid(le, S_hp) <= 0) continue;
                    if (get_num_sid(le, S_grid_col) < c1raw) continue;      // 比窗口左界还远的不算
                    if (((int)get_num_sid(le, S_tbit) & wmask) == 0) continue;
                    set_num_sid(s, S_mod_has_enemy, 1);
                    break;
                }
                if (get_num_sid(s, S_mod_has_enemy) == 1) break;
            }
        }
        if (get_num_sid(s, S_mod_has_enemy) == 1) break;
    }
}


// Step 末尾的"批量改属性"（两套一样，只有变量名不同）
void cardmod_batch_set(Vm& vm, Vm* mv, int s, double on,
                              const char* aid, const char* aprop, const char* aval) {
    if (on != 1) return;
    // ⚠ 跑完 mod 的块之后，实例可能已经被销毁了（块里 instance_destroy）——
    //   这里必须先确认它还在，否则 inst_var 返回 NULL，解引用就是崩溃
    if (!inst_get(s)) return;
    Value ids = iv_or(s, aid, 0), props = iv_or(s, aprop, 0), vals = iv_or(s, aval, 0);
    if (ids.t != MEM_ARRAY || props.t != MEM_ARRAY || vals.t != MEM_ARRAY) return;
    int n = container_size(ids);
    if (container_size(props) < n) n = container_size(props);
    if (container_size(vals) < n)  n = container_size(vals);
    for (int i = 0; i < n; i++) {
        Value tgtv = array_v(vm, ids, i);
        double tgt = num_of(&tgtv);
        if ((tgtv.t == MEM_INT || tgtv.t == MEM_FLOAT) && tgt < 0) {
            // VM 的包装 id（负数）→ 真实实例 id
            Value m = gv_sid(S__VM_id_to_real);
            if (m.t == MEM_MAP) {
                Value real = v_map(vm, m, mk_num(-tgt));
                if (real.t != MEM_UNDEFINED) tgt = num_of(&real);
            }
        }
        if (inst_get((int)tgt))
            viset_any(vm, mv, (int)tgt, array_v(vm, props, i), array_v(vm, vals, i));
    }
}


// Step_0.gml（328 行）
bool hot_cardmod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;

    hot_inherited_sid(vm, S_obj_card_mod, S__OBJECT_STEP);             // event_inherited()

    // 父类 Step 里 hp<=0 会销毁本实例；实例没了就别再往下做
    if (!inst_get(s)) return true;
    if (get_num_sid(s, S_hp) <= 0) return true;
    if (truthy(inst_var_sid(s, S_is_frozen))) return true;             // 冻住：计时不动，也不进 VM

    // ── 1) 一轮计时 ──
    double tmax = get_num_sid(s, S_mod_tick_max);
    if (tmax <= 0) tmax = get_num_sid(s, S_cycle);
    if (tmax <= 0) tmax = 1;
    add_num_sid(s, S_mod_tick_cycle, 1);
    if (get_num_sid(s, S_mod_tick_cycle) >= tmax) set_num_sid(s, S_mod_tick_cycle, 0);

    mod_base_tick_v(s);                                            // 1.5) 倒计时 + 透明度

    const Value* condp = inst_var_sid(s, S_mod_step_enter_condition);
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_plant_id);
    bool cond_norm_attack = str_is_any_sid(vm, mv, condp, S_norm_attack);

    // ── 2) 索敌（按需）──
    if (get_num_sid(s, S_mod_enemy_check) == 1 || cond_norm_attack) {
        cardmod_enemy_search(vm, s, mv);
        // 没敌人 → 顺手把攻击动画收回待机（只碰 IDLE/ATTACK，不碰睡眠这类自定义状态）
        double st = get_num_sid(s, S_state);
        if (get_num_sid(s, S_mod_has_enemy) == 0 && (st == 0 || st == 1)) set_num_sid(s, S_state, 0);
    }

    // ── 敌人计时器：有敌人才走，没敌人清 0 ──
    if (truthy(inst_var_sid(s, S_mod_has_enemy))) {
        add_num_sid(s, S_mod_tick_enemy, 1);
        if (get_num_sid(s, S_mod_tick_enemy) >= tmax) set_num_sid(s, S_mod_tick_enemy, 0);
    } else {
        set_num_sid(s, S_mod_tick_enemy, 0);
    }

    // ── 盯梢 mod_watch ──
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    Value watch_changed = *inst_var_sid(s, S_mod_watch_changed);
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0);
      call_fn_sid(vm, S_array_resize, a, 2, -1); }
    if (mv) {
        std::unordered_map<int, std::vector<Value> >::iterator wit = mv->named_arrays.find(S_mod_watch);
        if (wit != mv->named_arrays.end()) {
            const std::vector<Value>& wlist = wit->second;
            Value last = *inst_var_sid(s, S_mod_watch_last);
            int wn = (int)wlist.size();
            if (container_size(last) != wn) {                       // 长度变了：只对齐快照
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
                        call_fn_sid(vm, S_array_push, pa, 2, -1);
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    // ── 3) 这一帧要不要进 VM ──
    bool enter = true;
    set_num_sid(s, S_mod_step_enter_index, -1);
    Value* sev = inst_var_sid(s, S_mod_step_enter_var);

    if (str_is_any_sid(vm, mv, condp, S_mod)) {
        if (is_numv(sev) && num_of(sev) > 0 && inst_count(str_at(S_obj_battle)) > 0) {
            int bat = inst_at(str_at(S_obj_battle), 0);
            enter = (std::fmod(get_num_sid(bat, S_battle_time), num_of(sev)) == 0);
        }
    } else if (str_is_any_sid(vm, mv, condp, S_norm) || cond_norm_attack) {
        if (cond_norm_attack && !truthy(inst_var_sid(s, S_mod_has_enemy))) {
            enter = false;                                          // 没敌人：不进
        } else {
            Value arr = *inst_var_sid(s, S_mod_step_enter_arr);
            if (arr.t == MEM_ARRAY && container_size(arr) > 0) {
                enter = false;
                int an = container_size(arr);
                for (int i = 0; i < an; i++) {
                    Value v = array_v(vm, arr, i);
                    if (v.t == MEM_STRING) { enter = true; break; }  // 表里写了字符串（写错）→ 兜底每帧进
                    double vv = num_of(&v);
                    if (vv < 0) vv = tmax + vv;                      // 负数 = 从一轮末尾倒数
                    if (get_num_sid(s, S_mod_tick_cycle) == vv) {
                        enter = true;
                        set_num_sid(s, S_mod_step_enter_index, i);       // 告诉卡：第几个窗口
                        break;
                    }
                }
            }
        }
    } else if (str_is_any_sid(vm, mv, condp, S_wait)) {
        if (is_numv(sev) && num_of(sev) > 0) {
            set_num_sid(s, S_mod_step_enter_var, num_of(sev) - 1);
            enter = (num_of(sev) - 1 <= 0);
        } else {
            inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));
        }
    } else if (str_is_any_sid(vm, mv, condp, S_hp_change)) {
        enter = (get_num_sid(s, S_hp) != get_num_sid(s, S_mod_hp_last));
        set_num_sid(s, S_mod_hp_last, get_num_sid(s, S_hp));                // 基线每帧无条件跟上
    } else if (str_is_any_sid(vm, mv, condp, S_hp_change_mod)) {
        double cdv = num_of(sev);
        if (!is_numv(sev) || cdv < 0) cdv = 0;
        if (get_num_sid(s, S_mod_tick_cool) > 0) {
            add_num_sid(s, S_mod_tick_cool, -1);
            enter = false;
        } else if (get_num_sid(s, S_hp) != get_num_sid(s, S_mod_hp_last)) {
            enter = true;
            set_num_sid(s, S_mod_tick_cool, cdv);                       // 触发即上冷却
        } else {
            enter = false;
        }
        set_num_sid(s, S_mod_hp_last, get_num_sid(s, S_hp));
    }

    // 命中开火窗口 + 确实有敌人 → 进攻击动画
    {
        double st = get_num_sid(s, S_state);
        if (get_num_sid(s, S_mod_step_enter_index) >= 0 && truthy(inst_var_sid(s, S_mod_has_enemy))
                && (st == 0 || st == 1))
            set_num_sid(s, S_state, 1);
    }

    // ── 4) 跑卡的 _OBJECT_STEP ──
    int run_total = container_size(watch_changed) + (enter ? 1 : 0);
    for (int k = 0; k < run_total; k++) {
        if (!inst_get(s)) break;
        if (k < container_size(watch_changed)) {
            Value nm = array_v(vm, watch_changed, k);
            inst_var_set_sid(s, S_mod_changed_prop, nm);
        } else {
            inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
        }
        if (mv) {
            Value bak_cur = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_STEP, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
        }
    }

    // ── 批量改属性（卡这一帧写好的数组/开关当帧生效）──
    cardmod_batch_set(vm, mv, s, get_num_sid(s, S_mod_set_on),  str_at(S_mod_set_id).c_str(),  str_at(S_mod_set_prop).c_str(),  str_at(S_mod_set_val).c_str());
    cardmod_batch_set(vm, mv, s, get_num_sid(s, S_mod_set2_on), str_at(S_mod_set2_id).c_str(), str_at(S_mod_set2_prop).c_str(), str_at(S_mod_set2_val).c_str());
    return true;
}


// Draw_0.gml（33 行）：父类绘制 → 卡的绘制块 → 鼠标移入遮罩
bool hot_cardmod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    hot_inherited_sid(vm, S_obj_card_mod, S__OBJECT_DRAW);
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_plant_id);
    if (mv) mod_run_block_sid(mv, s, S__OBJECT_DRAW, str_at(S__VM_cur_card).c_str());

    if (get_num_sid(s, S_mod_hover_mask) == 1) {
        Value a[3];
        a[0] = gv_sid(S_mouse_x); a[1] = gv_sid(S_mouse_y); a[2] = mk_num(s);
        bool hovered = truthy(&call_ret_sid(vm, S_position_meeting, a, 3));
        if (!hovered) {
            Value r[5];
            r[0] = gv_sid(S_mouse_x); r[1] = gv_sid(S_mouse_y);
            r[2] = iv_or_sid(s, S_bbox_left, 0); r[3] = iv_or_sid(s, S_bbox_top, 0);
            r[4] = iv_or_sid(s, S_bbox_right, 0);
            hovered = truthy(&call_ret_sid(vm, S_point_in_rectangle, r, 5));
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
            d[7] = mk_num(16777215);                                // c_white
            d[8] = mk_num(get_num_sid(s, S_mod_hover_alpha));
            call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
            call_fn_int_sid(vm, S_shader_reset, NULL, 0, -1);
        }
    }
    return true;
}


// Destroy_0.gml（18 行）
bool hot_cardmod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    hot_inherited_sid(vm, S_obj_card_mod, S__OBJECT_DESTROY);
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_plant_id);
    if (mv) {
        Value bak_last = gv_sid(S__VM_last_destroyed_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_destroyed_card, mk_num(s));
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_DESTROY, NULL);
        global_var_set_sid(S__VM_last_destroyed_card, bak_last);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        mod_inst_del_v(mv, s, true);
    }
    return true;
}


// Mouse_4 / Mouse_10 / Mouse_11：先跑卡的块，再 event_inherited()
bool hot_cardmod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_plant_id);
    if (mv) mod_run_block(mv, s, block, str_at(S__VM_cur_card).c_str());
    hot_inherited_sid(vm, S_obj_card_mod, str_find(block));
    return true;
}

bool hot_cardmod_click(Vm& vm)        { return hot_cardmod_mouse(vm, str_at(S__OBJECT_CLICK).c_str()); }
bool hot_cardmod_mouse_enter(Vm& vm)  { return hot_cardmod_mouse(vm, str_at(S__OBJECT_MOUSE_ENTER).c_str()); }
bool hot_cardmod_mouse_leave(Vm& vm)  { return hot_cardmod_mouse(vm, str_at(S__OBJECT_MOUSE_LEAVE).c_str()); }

// ============================================================
// 注册表
// ============================================================
