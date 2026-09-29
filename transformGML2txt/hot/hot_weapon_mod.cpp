// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// ============================================================
// obj_weapon_mod —— 通用 mod 武器（7 个事件）
//   和 obj_card_mod 同一个 mod 宿主框架，差别只有四点：
//     · 武器没有 hp，也没有"冻住"：闸门少 hp_change / hp_change_mod 两条
//     · id 变量是 weapon_id，注册表是 global.mod_weapon_vms（就是 mod_vm() 那张表）
//     · Create 里**不登记 VM、也不跑 _OBJECT_CREATE**：放置逻辑要先写 parent_player /
//       摆位置，所以整段挪到了 Step 顶部，用 _mod_initialized 只放行一次
//     · 没有父对象（obj_weapon_mod.yy 的 parentObjectId = null）→ Create/Step/Draw/Destroy
//       都没有 event_inherited()；只有 Mouse_4/10/11 末尾调了一次（没父对象 → 直接 false）
//   常量：CARD_STATE.IDLE=0 / ATTACK=1、noone=-4、
//         HIT_NORMAL=1 HIT_OBSTACLE=8 HIT_DIVER=16 HIT_AIR=2 HIT_DANCE=4 HIT_UNDERGROUND=32
//
//   ⚠ 索敌那段（Step 的 2)）和 obj_card_mod 的**逐字相同** → 直接复用 cardmod_enemy_search()
//     （它只写 mod_has_enemy；"没敌人收回待机"由调用方做，和两边的 GML 一致）
//   ⚠ NEEDED_NAME（要往 names.h 的 REST_NAME_LIST 段尾加的几条，本文件已在用）：
//        weapon_info        Create_0.gml:7（weapon_info = get_weapon_info(weapon_id)）
//        atk_impact         Create_0.gml:30-31
//        attack_gem         Create_0.gml:30-31
//        （mod_prestep_dx / mod_prestep_dy 已经加过了 —— obj_effect_mod 那批一起加的）
// ============================================================

// 该 mod VM 里有没有这个块（对 GML 的 ds_map_exists(_vm.blocks, blk)）
//   判据和 mod_run_block() 里那条完全一致：解释器块 或 硬编码块 都算"存在"
//   （自己写一份是因为"块存在"和"块跑成功"是两回事：Draw 要靠它决定走不走 draw_self）
static bool weapon_mod_has_block(Vm* mv, const char* block) {
    if (!mv) return false;
    int obj_sid = mv->obj;                  // 池下标
    if (obj_sid < 0) return false;
    int block_sid = intern_str(block);
    if (has_hot_block_sid(obj_sid, block_sid)) return true;
    return mv->find_block_sid(block_sid) != NULL;
}

static bool weapon_mod_has_block_sid(Vm* mv, int sid) {
    return weapon_mod_has_block(mv, str_at(sid).c_str());
}


// Create_0.gml（74 行）
//   只做"数值 / 贴图"初始化；登记实例 + 跑 .bin 的 _OBJECT_CREATE 在 Step 顶部（见下）
bool hot_weaponmod_create(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    // weapon_id = variable_global_exists("_mod_pending_weapon_id") ? global._mod_pending_weapon_id : ""
    if (global_exists_sid(S__mod_pending_weapon_id)) set_var_sid(s, S_weapon_id, gv_sid(S__mod_pending_weapon_id));
    else                                             set_var_sid(s, S_weapon_id, mk_str(vm, ""));

    // weapon_info = get_weapon_info(weapon_id)
    //   结构体句柄要跨若干次 call_ret 用 → 用 held 版留住，存进实例变量后把 held 那份还回去
    Value wa[1]; wa[0] = *inst_var_sid(s, S_weapon_id);
    Value winfo = call_ret_held_sid(vm, S_get_weapon_info, wa, 1);
    set_var_sid(s, S_weapon_info, winfo);

    // 数值默认值。内置字段（image_xscale 这类）不能直接写 in->vars：
    //   set_num_sid → inst_var_set_sid 自己分派"内置字段 / vars"，所以这里一律走它
    static const struct { int n;              /* 池下标 */ double v; } init[] = {
        { S_atk, 0 }, { S_cycle, 60 },
        { S_image_xscale, 1.6 }, { S_image_yscale, 1.6 }, { S_image_speed, 0 },
        { S_parent_player, -4 },                                  // noone
        { S_grid_col, 0 }, { S_grid_row, 0 },
        // 偏移：每帧 Step 末加到 x/y 上（默认 0 = 不偏移；.bin 用 VM_SetProp 改）
        { S_mod_prestep_dx, 0 }, { S_mod_prestep_dy, 0 },
        // 动画配置（与 obj_card_parent 同款），.bin 可在 _OBJECT_CREATE 覆盖
        { S_timer, 0 }, { S_flash_speed, 5 }, { S_idle_anim, 0 }, { S_attack_anim, 0 },
        { S_state, 0 },                                           // CARD_STATE.IDLE
        // 进 VM 的入口条件（和 obj_card_mod 同款，去掉武器用不上的 hp_change）
        { S_mod_tick_cycle, 0 }, { S_mod_tick_max, 0 }, { S_mod_tick_enemy, 0 },
        { S_mod_has_enemy, 0 }, { S_mod_enemy_check, 0 },
        { S_mod_step_enter_var, 0 }, { S_mod_step_enter_index, -1 },
        // 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
        { S_mod_countdown, 0 }, { S_mod_alpha_add, 0 },
        { S__mod_initialized, 0 },                                // false：Step 顶部才登记
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        set_num_sid(s, init[i].n, init[i].v);

    // 字符串：标量直接落值；数组是容器句柄，走 set_var_sid（要 retain）
    set_var_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));   // "" / "mod" / "norm" / "norm_attack" / "wait"
    set_var_sid(s, S_mod_changed_prop, mk_str(vm, ""));           // 【只读】这一帧变化的盯梢属性名
    set_var_sid(s, S_mod_step_enemy_area, call_ret_sid(vm, S_array_literal, NULL, 0));  // [上,下,左,右]；空 = [1,1,0,99]
    set_var_sid(s, S_mod_enemy_types,     call_ret_sid(vm, S_array_literal, NULL, 0));  // 空 = 任意类型
    set_var_sid(s, S_mod_watch_last,      call_ret_sid(vm, S_array_literal, NULL, 0));  // 【内部】上一帧各盯梢值
    set_var_sid(s, S_mod_watch_changed,   call_ret_sid(vm, S_array_literal, NULL, 0));  // 【内部】这一帧变化的属性名
    set_var_sid(s, S_mod_step_enter_arr,  call_ret_sid(vm, S_array_literal, NULL, 0));  // 开火窗口表（负数 = 从末尾倒数）

    // ── weapon_info 覆盖（拿不到结构体就保持上面的默认值）──
    if (truthy(&call_ret_sid(vm, S_is_struct, &winfo, 1))) {
        inst_var_set_sid(s, S_atk,          m_get_sid(vm, winfo, S_atk));
        inst_var_set_sid(s, S_cycle,        m_get_sid(vm, winfo, S_cycle));
        inst_var_set_sid(s, S_sprite_index, m_get_sid(vm, winfo, S_sprite));
        Value ga[1]; ga[0] = mk_str_sid(S_attack_gem);
        if (num_of(&call_ret_sid(vm, S_get_gem_index, ga, 1)) != -1) {
            Value va[2]; va[0] = winfo; va[1] = mk_str_sid(S_atk_impact);
            if (truthy(&call_ret_sid(vm, S_variable_struct_exists, va, 2))) {
                Value lv[1]; lv[0] = mk_str_sid(S_attack_gem);
                Value gl  = call_ret_sid(vm, S_get_gem_level, lv, 1);
                Value arr = m_get_sid(vm, winfo, S_atk_impact);
                Value got = array_v(vm, arr, (int)num_of(&gl));
                inst_var_set_sid(s, S_atk, got);                  // atk = weapon_info.atk_impact[get_gem_level("attack_gem")]
            }
        }
    }
    release(winfo);                                               // call_ret_held 那份还回去（实例变量那份留着）
    return true;
}


// Step_0.gml（296 行）
//   ① 登记实例 + 跑 .bin 的 _OBJECT_CREATE（只一次）② 基础 tick ③ 跟随玩家 ④ 动画
//   ⑤ 一轮计时 ⑥ 索敌 ⑦ 盯梢 + 闸门 ⑧ 跑武器 VM 的 _OBJECT_STEP ⑨ x/y 偏移
bool hot_weaponmod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;                          // if global.is_paused { exit }

    // ── 0) 登记实例 + 跑该武器 .bin 的 _OBJECT_CREATE ──
    //   为什么不在 Create 里：Create 跑在 instance_create_depth 内部，那时放置逻辑
    //   还没执行 parent_player = 角色；插件 _OBJECT_CREATE 里清 parent_player / 摆位置
    //   会被随后那行赋值盖掉。weapon_id == "" 时 mod_vm_of_inst_sid 返回 NULL（= 表里没有）
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_weapon_id);
    if (!truthy(inst_var_sid(s, S__mod_initialized)) && mv) {
        set_num_sid(s, S__mod_initialized, 1);                        // 先置位，块里再进 Step 不会重入
        mod_inst_add_v(mv, s, true);                                  // mod_inst_add(_vm, id)
        Value bak_last = gv_sid(S__VM_last_created_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_created_card, mk_num(s));
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_CREATE, NULL);             // 块不存在就不跑
        global_var_set_sid(S__VM_cur_card, bak_cur);
        global_var_set_sid(S__VM_last_created_card, bak_last);
        if (!inst_get(s)) return true;                                // 创建块里可能把自己销毁了
    }

    // ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用）──
    mod_base_tick_v(s);                                               // mod_base_tick(id)

    // ── 跟随放置它的玩家 ──
    //   ⚠ is_string / is_undefined 拦一下：插件可能把 parent_player 写成字符串 "",
    //     instance_exists 收到字符串会直接报 "incorrect type (string) expecting a Number"
    {
        Value* pp = inst_var_sid(s, S_parent_player);
        if (pp && (pp->t == MEM_INT || pp->t == MEM_FLOAT)) {
            int pid = (int)num_of(pp);
            if (inst_get(pid)) {
                set_num_sid(s, S_depth,    get_num_sid(pid, S_depth) - 1);
                set_num_sid(s, S_x,        get_num_sid(pid, S_x) - 10);
                set_num_sid(s, S_y,        get_num_sid(pid, S_y) - 100);
                set_num_sid(s, S_grid_row, get_num_sid(pid, S_grid_row));
                set_num_sid(s, S_grid_col, get_num_sid(pid, S_grid_col));
            }
        }
    }

    // ── 动画自动播放：与 obj_card_parent 同款，只认 IDLE / ATTACK 两个状态 ──
    {
        double st = get_num_sid(s, S_state);
        if (get_num_sid(s, S_timer) < get_num_sid(s, S_flash_speed) - 1) {
            add_num_sid(s, S_timer, 1);
        } else {
            double idle = get_num_sid(s, S_idle_anim);
            double ii   = get_num_sid(s, S_image_index);
            if (st == 0) {                                            // CARD_STATE.IDLE
                if (ii < idle) set_num_sid(s, S_image_index, ii + 1);
                else           set_num_sid(s, S_image_index, 0);
            } else if (st == 1) {                                     // CARD_STATE.ATTACK
                if (ii >= idle + 1 && ii <= idle + get_num_sid(s, S_attack_anim))
                    set_num_sid(s, S_image_index, ii + 1);
                else
                    set_num_sid(s, S_image_index, idle + 1);
            }
            set_num_sid(s, S_timer, 0);
        }
    }

    // ── 1) 一轮计时：每帧 +1，到一轮长度归零（一轮长度 = mod_tick_max，不设就用这把武器的 cycle）──
    double tmax = get_num_sid(s, S_mod_tick_max);
    if (tmax <= 0) tmax = get_num_sid(s, S_cycle);
    if (tmax <= 0) tmax = 1;
    add_num_sid(s, S_mod_tick_cycle, 1);
    if (get_num_sid(s, S_mod_tick_cycle) >= tmax) set_num_sid(s, S_mod_tick_cycle, 0);

    // ── 2) 索敌（按需）：条件 "norm_attack" 或武器写了 mod_enemy_check = 1 才算 ──
    //   区域前缀和那段和 obj_card_mod 完全同款 → 直接复用 cardmod_enemy_search()
    bool cond_norm_attack = str_is_any_sid(vm, mv, inst_var_sid(s, S_mod_step_enter_condition), S_norm_attack);
    if (get_num_sid(s, S_mod_enemy_check) == 1 || cond_norm_attack) {
        cardmod_enemy_search(vm, s, mv);
        // 没敌人 → 顺手把攻击动画收回待机（只在自己索敌的武器上做，且只在 IDLE/ATTACK 之间切）
        double st = get_num_sid(s, S_state);
        if (get_num_sid(s, S_mod_has_enemy) == 0 && (st == 0 || st == 1)) set_num_sid(s, S_state, 0);
    }

    // ── 敌人计时器：有敌人才走，没敌人清 0（节奏重数）──
    if (truthy(inst_var_sid(s, S_mod_has_enemy))) {
        add_num_sid(s, S_mod_tick_enemy, 1);
        if (get_num_sid(s, S_mod_tick_enemy) >= tmax) set_num_sid(s, S_mod_tick_enemy, 0);
    } else {
        set_num_sid(s, S_mod_tick_enemy, 0);
    }

    // ── 3) 盯梢 mod_watch：声明的属性里哪个值变了，就记进 mod_watch_changed ──
    //   每有一个变化就各进一次 _OBJECT_STEP（mod_changed_prop = 那个属性名），闸门放行再进一次
    //   名字先当**本实例属性**读；实例上没有、但有同名**全局变量**时，就当全局读
    const Value* condp = inst_var_sid(s, S_mod_step_enter_condition);
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    Value watch_changed = *inst_var_sid(s, S_mod_watch_changed);
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0);
      call_fn_sid(vm, S_array_resize, a, 2, -1); }
    if (mv) {
        std::unordered_map<int, std::vector<Value> >::iterator wit = mv->named_arrays.find(S_mod_watch);
        if (wit != mv->named_arrays.end()) {
            const std::vector<Value>& wlist = wit->second;            // 存的是**属性名字符串**（池下标）
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
                        call_fn_sid(vm, S_array_push, pa, 2, -1);     // 每个变化的属性各进一次 Step
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    // ── 这一帧要不要进 VM ──
    bool enter = true;
    set_num_sid(s, S_mod_step_enter_index, -1);
    Value* sev = inst_var_sid(s, S_mod_step_enter_var);

    if (str_is_any_sid(vm, mv, condp, S_mod)) {
        if (is_numv(sev) && num_of(sev) > 0 && inst_count_sid(S_obj_battle) > 0) {
            int bat = inst_at_sid(S_obj_battle, 0);
            enter = (std::fmod(get_num_sid(bat, S_battle_time), num_of(sev)) == 0);
        }
    } else if (str_is_any_sid(vm, mv, condp, S_norm) || cond_norm_attack) {
        if (cond_norm_attack && !truthy(inst_var_sid(s, S_mod_has_enemy))) {
            enter = false;                                            // 没敌人：不进
        } else {
            Value arr = *inst_var_sid(s, S_mod_step_enter_arr);
            if (arr.t == MEM_ARRAY && container_size(arr) > 0) {
                enter = false;
                int an = container_size(arr);
                for (int i = 0; i < an; i++) {
                    Value v = array_v(vm, arr, i);
                    if (v.t == MEM_STRING) { enter = true; break; }   // 表里写了字符串（写错）→ 兜底每帧进
                    double vv = num_of(&v);
                    if (vv < 0) vv = tmax + vv;                       // 负数 = 从一轮末尾倒数
                    if (get_num_sid(s, S_mod_tick_cycle) == vv) {
                        enter = true;
                        set_num_sid(s, S_mod_step_enter_index, i);    // 告诉武器：这是第几个窗口
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
            inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));   // 不合法：进一次 + 清空
        }
    }

    // 命中开火窗口 + 确实有敌人 → 进攻击动画
    {
        double st = get_num_sid(s, S_state);
        if (get_num_sid(s, S_mod_step_enter_index) >= 0 && truthy(inst_var_sid(s, S_mod_has_enemy))
                && (st == 0 || st == 1))
            set_num_sid(s, S_state, 1);
    }

    // ── 4) 跑武器 VM 的 _OBJECT_STEP ──
    //   这一帧进几次：watch 每个变化的属性各 1 次，闸门放行再 1 次
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
        if (mv) {
            Value bak_cur = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_STEP, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
        }
    }

    // ── 5) 偏移：加在最后 ──
    //   上面"跟随 parent_player"每帧都会把 x/y 重置成 player.x-10 / player.y-100，
    //   所以偏移必须加在它后面；加在 VM 之后 = 只影响绘制和别的对象读到的位置
    if (inst_get(s)) {
        add_num_sid(s, S_x, get_num_sid(s, S_mod_prestep_dx));
        add_num_sid(s, S_y, get_num_sid(s, S_mod_prestep_dy));
    }
    return true;
}


// Draw_0.gml（15 行）：有武器 VM 的绘制块 → 跑它并 exit；没有 → draw_self()
bool hot_weaponmod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_weapon_id);
    if (weapon_mod_has_block_sid(mv, S__OBJECT_DRAW)) {
        Value bak_cur = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_DRAW, NULL);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        return true;                                              // GML 的 exit：有绘制块就别再 draw_self()
    }
    call_fn_sid(vm, S_draw_self, NULL, 0, -1);                     // draw_self()
    return true;
}


// Destroy_0.gml（19 行）：跑武器 VM 的销毁块 + 把实例移出该武器的实例容器
bool hot_weaponmod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_weapon_id);
    if (mv) {
        if (weapon_mod_has_block_sid(mv, S__OBJECT_DESTROY)) {
            Value bak_last = gv_sid(S__VM_last_destroyed_card);
            Value bak_cur  = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_last_destroyed_card, mk_num(s));   // 块内 VM_GetLastDestroyedCard()
            global_var_set_sid(S__VM_cur_card, mk_num(s));              // 块内 VM_GetCurCard()
            mod_run_block_sid(mv, s, S__OBJECT_DESTROY, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
            global_var_set_sid(S__VM_last_destroyed_card, bak_last);
        }
        mod_inst_del_v(mv, s, true);                               // 计数 -1 + 移出 instances 列表
    }
    return true;
}


// Mouse_4 / Mouse_10 / Mouse_11：先跑武器 VM 里的对应块，再 event_inherited()
//   块存在才跑（对 GML 的 ds_map_exists(_vm.blocks, blk)），执行期间切 _VM_cur_card
//   （obj_weapon_mod 没有父对象 → hot_inherited 直接 false，等价于什么都不做）
bool hot_weaponmod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_weapon_id);
    if (weapon_mod_has_block(mv, block)) {
        Value bak_cur = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block(mv, s, block, NULL);
        global_var_set_sid(S__VM_cur_card, bak_cur);
    }
    hot_inherited_sid(vm, S_obj_weapon_mod, str_find(block));
    return true;
}

bool hot_weaponmod_click(Vm& vm)       { return hot_weaponmod_mouse(vm, str_at(S__OBJECT_CLICK).c_str()); }
bool hot_weaponmod_mouse_enter(Vm& vm) { return hot_weaponmod_mouse(vm, str_at(S__OBJECT_MOUSE_ENTER).c_str()); }
bool hot_weaponmod_mouse_leave(Vm& vm) { return hot_weaponmod_mouse(vm, str_at(S__OBJECT_MOUSE_LEAVE).c_str()); }

// ============================================================
// 注册表（由宿主加进 hot_table.cpp 的 kHotBlocks）：
//   { S_obj_weapon_mod, S__OBJECT_CREATE,       hot_weaponmod_create      },
//   { S_obj_weapon_mod, S__OBJECT_STEP,         hot_weaponmod_step        },
//   { S_obj_weapon_mod, S__OBJECT_DRAW,         hot_weaponmod_draw        },
//   { S_obj_weapon_mod, S__OBJECT_DESTROY,      hot_weaponmod_destroy     },
//   { S_obj_weapon_mod, S__OBJECT_CLICK,        hot_weaponmod_click       },
//   { S_obj_weapon_mod, S__OBJECT_MOUSE_ENTER,  hot_weaponmod_mouse_enter },
//   { S_obj_weapon_mod, S__OBJECT_MOUSE_LEAVE,  hot_weaponmod_mouse_leave },
// ============================================================
