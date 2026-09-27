// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// ============================================================
// obj_card_parent
// ============================================================
// （下面这几个用的是"当前实例 = vm.cur_instance"这套）

// Create_0.gml —— 纯实例变量初始化（没有函数调用，照抄就行）
//   枚举值：ATTACK_TYPE.PRODUCER = 0、CARD_STATE.IDLE = 0、noone = -4
//   初始化这种"一次写几十个字段"的地方**直接落地**：内置字段写字段、其余写 vars，
//   不走"按名字派发"那一套（见 vm.h 的两条规矩）。这里只有 image_speed 是内置。
bool hot_card_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    in->image_speed = mk_num(0);          // 内置字段：直接写字段

    struct NV { const char* n; double v; };
    static const NV nums[] = {
        { "net_player_id",    0   },
        { "cost",             0   },
        { "cooldown",         0   },
        { "hp",               100 },
        { "max_hp",           100 },
        { "atk",              0   },
        { "range",            0   },
        { "attack_type",      0   },   // ATTACK_TYPE.PRODUCER
        { "shape",            0   },
        { "skill",            0   },
        { "invincible",       0   },   // false
        { "can_shovel_remove",1   },   // true
        { "is_slowdown",      0   },   // false
        { "state",            0   },   // CARD_STATE.IDLE
        { "timer",            0   },
        { "attack_timer",     0   },
        { "cycle",            0   },
        { "attack_cycle",    -1   },
        { "origin_cycle",     0   },
        { "first_produce",    0   },
        { "first_produce_delay", 0 },
        { "flame_produce",    0   },
        { "awake_buff_timer", 0   },
        { "bullet_charge_dmg",0   },
        { "bullet_pass_count",0   },
        { "grid_col",        -1   },
        { "grid_row",        -1   },
        { "depth_value",      0   },
        { "depth_group",      0   },
        { "flash_speed",      6   },
        { "attack_anim",      0   },
        { "banding_star_obj",  -4 },   // noone
        { "banding_water_obj", -4 },
        { "banding_sleep_obj", -4 },
        { "flash_value",      0   },
        { "idle_anim",        0   },
        { "ice_timer",        0   },
        { "frozen_timer",     0   },
        { "is_frozen",        0   },   // false
        { "awake_anim",       0   },
        { "current_level",    0   },
        { "pre_hp",          -1   },
    };
    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++)
        in->vars[nums[i].n] = mk_num(nums[i].v);

    struct SV { const char* n; const char* v; };
    static const SV strs[] = {
        { "name",         "未命名植物" },
        { "description",  ""           },
        { "plant_type",   ""           },
        { "feature_type", "normal"     },
        { "target_card",  "none"       },
        { "target_type",  "normal"     },
        { "plant_id",     ""           },
    };
    for (size_t i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
        in->vars[strs[i].n] = mk_str(vm, strs[i].v);
    return true;
}


// Destroy_0.gml：
//   if hp < max_hp && !invincible { obj_task_manager.card_loss ++ }
//   card_destroyed(id);
bool hot_card_destroy(Vm& vm) {
    int self = vm.cur_instance;
    if (!inst_get(self)) return true;                    // 没有当前实例：没什么可做
    if (iv_of(inst_var(self, "hp")) < iv_of(inst_var(self, "max_hp"))
            && !iv_of(inst_var(self, "invincible"))) {
        int tm = inst_at("obj_task_manager", 0);          // 找到那个唯一的管理器实例
        if (tm > 0) add_iv(tm, "card_loss", 1);
    }
    call_fn(vm, "card_destroyed", &self, 1, -1);
    return true;
}


// Other_12.gml：flash_value = 150
bool hot_card_user_event_2(Vm& vm) {
    set_iv(vm.cur_instance, "flash_value", 150);
    return true;
}


// Step_0.gml（160 行）—— 卡片每帧主逻辑
//   GML 里的 exit = 直接 return true（退出这个事件）
//   ⚠ 调任何"可能创建实例"的函数（instance_create_depth）之前，先把要用的值读进局部变量：
//     inst_var() 返回的是指针，而实例表 g_instances 是 vector，新增实例会扩容 → 指针悬空
bool hot_card_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    // if global.is_paused { exit }
    if (truthy(global_var("is_paused"))) return true;

    // 不在网格内 → visible = false; exit
    {
        double rows = num_of(global_var("grid_rows"));
        double cols = num_of(global_var("grid_cols"));
        double gr = get_num(s, "grid_row"), gc = get_num(s, "grid_col");
        if ((gr > rows + 2 && gr < rows + 62) || (gc > cols + 2 && gc < cols + 62)) {
            set_num(s, "visible", 0);
            return true;
        }
        set_num(s, "visible", 1);
    }

    // if (global.network.mode == "client" && hp <= 0) { exit }
    {
        // ⚠ 全局一律 gv() 直查全局表：v_getprop 走的是 VM_GetProp，那个还是空壳，
        //   绕它读回来恒为 0 —— 会把"联机模式"误判成非 offline，卡片 Step 每帧自杀
        Value net0 = gv(vm, "network");
        Value mode = (net0.t == MEM_MAP) ? v_struct(vm, net0, "mode") : Value{MEM_UNDEFINED, 0};
        if (str_is(vm, &mode, "client") && get_num(s, "hp") <= 0) return true;
    }

    // ice_timer / frozen_timer
    if (get_num(s, "ice_timer") > 0)    { add_num(s, "ice_timer", -1);    set_num(s, "is_slowdown", 1); }
    else                                { set_num(s, "is_slowdown", 0); }
    if (get_num(s, "frozen_timer") > 0) { add_num(s, "frozen_timer", -1); set_num(s, "is_frozen", 1); }
    else                                { set_num(s, "is_frozen", 0); }

    // if hp <= 0 { instance_destroy() }
    if (get_num(s, "hp") <= 0) call_fn(vm, "instance_destroy", NULL, 0, -1);

    // if flash_value > 0 { flash_value -= 10 }
    if (get_num(s, "flash_value") > 0) add_num(s, "flash_value", -10);

    // awake_buff_timer（睡醒）
    if (get_num(s, "awake_buff_timer") > 0) {
        add_num(s, "awake_buff_timer", -1);
        if (get_num(s, "state") == 4) {                                  // CARD_STATE.SLEEP
            set_num(s, "state", get_num(s, "awake_anim") > 0 ? 5 : 0);   // AWAKE : IDLE
            Value net  = gv(vm, "network");
            Value mode = v_struct(vm, net, "mode");
            if (str_is(vm, &mode, "server")) {
                Value mapid = v_struct(vm, net, "map_instance_id_net_id");
                Value a2[2]; a2[0] = mapid; a2[1] = mk_num(s);
                Value ex = call_ret(vm, "ds_map_exists", a2, 2);
                if (truthy(&ex)) {
                    Value net_id = v_map(vm, mapid, mk_num(s));
                    Value st = call_ret_held(vm, "ds_map_create", NULL, 0);   // {state: state}（要活到 json 之后）
                    Value kv[3];
                    kv[0] = st; kv[1] = mk_str(vm, "state"); kv[2] = mk_num(get_num(s, "state"));
                    call_fn_v(vm, "struct_set", kv, 3, -1);
                    Value js = call_ret(vm, "json_stringify", &st, 1);
                    release(st);
                    Value lst = v_struct(vm, net, "connected_clients");
                    Value lenv = call_ret(vm, "array_length", &lst, 1);
                    for (int j = 0; j < (int)num_of(&lenv); j++) {
                        Value item = v_array(vm, lst, mk_num(j));
                        Value msg[4];
                        msg[0] = item; msg[1] = mk_num(25);              // MSG_MODIFY_PROP
                        msg[2] = net_id; msg[3] = js;
                        call_fn_v(vm, "send_message", msg, 4, -1);
                    }
                }
            }
        }
    }

    // 睡眠特效：没有就建，有就跟位置
    {
        Value so = *inst_var(s, "banding_sleep_obj");
        Value a2[1]; a2[0] = so;
        Value exists = call_ret(vm, "instance_exists", a2, 1);
        if (get_num(s, "state") == 4 && !truthy(&exists) && get_num(s, "awake_anim") == 0) {
            Value a[4];
            a[0] = mk_num(get_num(s, "x") - 15);
            a[1] = mk_num(get_num(s, "y") - 20);
            a[2] = mk_num(get_num(s, "depth") - 1);
            a[3] = mk_str(vm, "obj_sleep_effect");
            Value r = call_ret(vm, "instance_create_depth", a, 4);
            inst_var_set(s, "banding_sleep_obj", r);
            if (r.t == MEM_INT && r.v > 0) inst_var_set(r.v, "banding_card_obj", mk_num(s));
        }
    }
    {
        Value so = *inst_var(s, "banding_sleep_obj");
        Value a2[1]; a2[0] = so;
        Value exists = call_ret(vm, "instance_exists", a2, 1);
        if (truthy(&exists) && so.t == MEM_INT && so.v > 0) {
            inst_var_set(so.v, "x",     mk_num(get_num(s, "x") - 15));
            inst_var_set(so.v, "y",     mk_num(get_num(s, "y") - 20));
            inst_var_set(so.v, "depth", mk_num(get_num(s, "depth") - 1));
        }
    }
    {
        Value so = *inst_var(s, "banding_sleep_obj");
        Value a2[1]; a2[0] = so;
        Value exists = call_ret(vm, "instance_exists", a2, 1);
        if (get_num(s, "state") != 4 && truthy(&exists) && so.t == MEM_INT && so.v > 0)
            call_fn(vm, "instance_destroy", &so.v, 1, -1);
    }

    // var grid_pos = get_grid_position_from_world(x, y)
    //   ⚠ 结构体是 call_ret 借来的，中间再插一个 call_ret 就会把它冲掉 —— 字段当场读出来
    double gp_col = 0, gp_row = 0;
    {
        Value a[2]; a[0] = mk_num(get_num(s, "x")); a[1] = mk_num(get_num(s, "y"));
        Value grid_pos = call_ret(vm, "get_grid_position_from_world", a, 2);
        gp_col = num_of(&v_struct(vm, grid_pos, "col"));
        gp_row = num_of(&v_struct(vm, grid_pos, "row"));
    }
    // if !(variable_instance_exists(id, "platform_grid_lock") && platform_grid_lock) { ... }
    {
        Value a[2]; a[0] = mk_num(s); a[1] = mk_str(vm, "platform_grid_lock");
        Value ex = call_ret(vm, "variable_instance_exists", a, 2);
        if (!(truthy(&ex) && get_num(s, "platform_grid_lock") != 0)) {
            set_num(s, "grid_col", gp_col);
            set_num(s, "grid_row", gp_row);
        }
    }
    // depth = calculate_plant_depth(...)
    {
        double col = get_num(s, "grid_col"), row = get_num(s, "grid_row");
        Value a[3];
        a[0] = mk_num(col > 50 ? col - 64 - num_of(global_var("grid_cols")) : col);
        a[1] = mk_num(row);
        a[2] = *inst_var(s, "plant_type");
        inst_var_set(s, "depth", call_ret(vm, "calculate_plant_depth", a, 3));
    }
    // 星星特效跟层
    {
        Value so = *inst_var(s, "banding_star_obj");
        Value a2[1]; a2[0] = so;
        Value exists = call_ret(vm, "instance_exists", a2, 1);
        if (truthy(&exists) && so.t == MEM_INT && so.v > 0)
            inst_var_set(so.v, "depth", mk_num(get_num(s, "depth") - 1));
    }
    // 水面判定：global.grid_terrains[clamp(grid_row,…)][clamp(grid_col,…)].type == "water"
    {
        Value a[3];
        a[0] = mk_num(get_num(s, "grid_col")); a[1] = mk_num(0);
        a[2] = mk_num(num_of(global_var("grid_cols")) - 1);
        double wy = num_of(&call_ret(vm, "clamp", a, 3));
        Value b[3];
        b[0] = mk_num(get_num(s, "grid_row")); b[1] = mk_num(0);
        b[2] = mk_num(num_of(global_var("grid_rows")) - 1);
        double wx = num_of(&call_ret(vm, "clamp", b, 3));
        Value cell  = v_array(vm, v_array(vm, gv(vm, "grid_terrains"), mk_num(wx)), mk_num(wy));
        Value ctype = v_struct(vm, cell, "type");
        Value wo = *inst_var(s, "banding_water_obj");
        Value wa[1]; wa[0] = wo;
        Value wexists = call_ret(vm, "instance_exists", wa, 1);
        if (!truthy(&wexists) && str_is(vm, &ctype, "water")) {
            if (str_is(vm, inst_var(s, "plant_type"), "lilypad")
                || (str_is(vm, inst_var(s, "feature_type"), "water")
                    && str_is(vm, inst_var(s, "plant_type"), "normal"))) {
                Value c[4];
                c[0] = mk_num(get_num(s, "x"));
                c[1] = mk_num(get_num(s, "y"));
                c[2] = mk_num(get_num(s, "depth") + 5);
                c[3] = mk_str(vm, "obj_in_water_effect");
                Value r = call_ret(vm, "instance_create_depth", c, 4);
                inst_var_set(s, "banding_water_obj", r);
                if (r.t == MEM_INT && r.v > 0) inst_var_set(r.v, "banding_card_obj", mk_num(s));
            }
        }
        if (truthy(&wexists) && !str_is(vm, &ctype, "water") && wo.t == MEM_INT && wo.v > 0)
            call_fn(vm, "instance_destroy", &wo.v, 1, -1);
    }

    // if is_frozen { exit }
    if (get_num(s, "is_frozen") != 0) return true;

    // 动画计时器
    double cur_flash = get_num(s, "flash_speed");
    if (get_num(s, "is_slowdown") != 0) cur_flash *= 2;
    if (get_num(s, "attack_cycle") == -1) set_num(s, "attack_cycle", get_num(s, "origin_cycle"));
    set_num(s, "cycle", get_num(s, "is_slowdown") != 0 ? get_num(s, "attack_cycle") * 2
                                                       : get_num(s, "attack_cycle"));
    if (get_num(s, "timer") < cur_flash - 1) {
        add_num(s, "timer", 1);
    } else {
        double st = get_num(s, "state");
        if (st == 0) {                                            // CARD_STATE.IDLE
            if (get_num(s, "image_index") < get_num(s, "idle_anim")) add_num(s, "image_index", 1);
            else set_num(s, "image_index", 0);
        } else if (st == 1) {                                     // CARD_STATE.ATTACK
            double ii = get_num(s, "image_index"), idle = get_num(s, "idle_anim");
            if (ii >= idle + 1 && ii <= idle + get_num(s, "attack_anim")) add_num(s, "image_index", 1);
            else set_num(s, "image_index", idle + 1);
        }
        set_num(s, "timer", 0);
    }

    // 联机：不在映射表里就销毁
    {
        Value net  = gv(vm, "network");
        Value mode = v_struct(vm, net, "mode");
        if (!str_is(vm, &mode, "offline")) {
            Value mapid = v_struct(vm, net, "map_instance_id_net_id");
            Value a[2]; a[0] = mapid; a[1] = mk_num(s);
            Value ex = call_ret(vm, "ds_map_exists", a, 2);
            if (!truthy(&ex)) {
                call_fn(vm, "instance_destroy", &s, 1, -1);
                Value m[1]; m[0] = mk_str(vm, "销毁卡片");
                call_fn_v(vm, "show_debug_message", m, 1, -1);
            }
        }
    }

    // 掉血钩子
    if (get_num(s, "pre_hp") > get_num(s, "hp")) {
        global_var_set("_VM_last_damaged_card", mk_num(s));
        Value* buf = global_var("_VM_CARD_DAMAGED");
        if (buf) {
            Value b[1]; b[0] = *buf;
            Value bexists = call_ret(vm, "buffer_exists", b, 1);
            if (truthy(&bexists)) {
                Value a[3];
                a[0] = global_var("__vm") ? *global_var("__vm") : mk_num(0);
                a[1] = *buf;
                a[2] = mk_str(vm, "_VM_CARD_DAMAGED");
                call_fn_v(vm, "VM_Execute", a, 3, -1);
            }
        }
        Value h[1]; h[0] = mk_str(vm, "_VM_CARD_DAMAGED");
        call_fn_v(vm, "vm_hook_run", h, 1, -1);
    }
    set_num(s, "pre_hp", get_num(s, "hp"));
    return true;
}


// Draw_0.gml（27 行）—— 绘制本体
//   常量：c_white = 16777215、c_blue = 16711680
//   精灵名（spr_mouse_frozen）在这边当字符串传，宿主去解析
bool hot_card_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    set_num(s, "image_xscale", 1.8);
    set_num(s, "image_yscale", 1.8);

    if (get_num(s, "flash_value") > 0) {
        call_fn(vm, "draw_self", NULL, 0, -1);
        Value sa[1]; sa[0] = iv_or(s, "hit_effect_2", -1);   // 宿主没给就 -1（shader_set(-1) 等于不设）
        call_fn_v(vm, "shader_set", sa, 1, -1);
        Value d[9];
        d[0] = *inst_var(s, "sprite_index");
        d[1] = *inst_var(s, "image_index");
        d[2] = mk_num(get_num(s, "x"));
        d[3] = mk_num(get_num(s, "y"));
        d[4] = *inst_var(s, "image_xscale");
        d[5] = *inst_var(s, "image_yscale");
        d[6] = *inst_var(s, "image_angle");
        d[7] = mk_num(16777215);                                  // c_white
        d[8] = mk_num(get_num(s, "flash_value") / 200.0);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
        call_fn(vm, "shader_reset", NULL, 0, -1);
    } else if (get_num(s, "is_slowdown") != 0) {
        Value m[3]; m[0] = mk_num(16777215); m[1] = mk_num(16711680); m[2] = mk_num(0.5);
        inst_var_set(s, "image_blend", call_ret(vm, "merge_color", m, 3));
        call_fn(vm, "draw_self", NULL, 0, -1);
    } else {
        set_num(s, "image_blend", 16777215);
        call_fn(vm, "draw_self", NULL, 0, -1);
    }

    {
        Value so = *inst_var(s, "banding_star_obj");
        Value a[1]; a[0] = so;
        Value ex = call_ret(vm, "instance_exists", a, 1);
        if (truthy(&ex) && so.t == MEM_INT && so.v > 0)
            inst_var_set(so.v, "depth", mk_num(get_num(s, "depth") - 1));
    }

    if (get_num(s, "is_frozen") != 0) {
        Value d[9];
        d[0] = mk_str(vm, "spr_mouse_frozen");
        d[1] = mk_num(0);
        d[2] = mk_num(get_num(s, "x"));
        d[3] = mk_num(get_num(s, "y") + 95);
        d[4] = mk_num(1.8);
        d[5] = mk_num(1.8);
        d[6] = mk_num(0);
        d[7] = mk_num(16777215);
        d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    return true;
}


// Draw_64.gml（47 行）—— 血条
//   常量：c_white=16777215、c_black=0、c_green=32768、c_lime=65280、c_yellow=16776960
//         fa_center=1、fa_middle=1；字体名 font_yuan 当字符串传
bool hot_card_draw_64(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (truthy(global_var("is_paused"))) return true;

    if (!(truthy(global_var("card_hpbar")) && get_num(s, "hp") > 0
          && !str_is(vm, inst_var(s, "plant_type"), "coffee")))
        return true;

    Value a1[1]; a1[0] = mk_num(0.5);
    call_fn_v(vm, "draw_set_alpha", a1, 1, -1);

    double bar_x = get_num(s, "x");
    double bar_y = get_num(s, "y") - 20;
    double bar_color = 32768;                                     // c_green
    if (str_is(vm, inst_var(s, "plant_type"), "lilypad")) {
        bar_y = get_num(s, "y") + 10;
        bar_color = 16776960;                                     // c_yellow
    } else if (str_is(vm, inst_var(s, "plant_type"), "shield_outer")) {
        bar_y = get_num(s, "y") - 50;
        bar_color = 65280;                                        // c_lime
    }
    double bar_w = 80, bar_h = 20;

    Value c1[1]; c1[0] = mk_num(16777215);                        // c_white
    call_fn_v(vm, "draw_set_color", c1, 1, -1);
    {
        Value r[5];
        r[0] = mk_num(bar_x - bar_w / 2); r[1] = mk_num(bar_y);
        r[2] = mk_num(bar_x + bar_w / 2); r[3] = mk_num(bar_y + bar_h);
        r[4] = mk_num(1);
        call_fn_v(vm, "draw_rectangle", r, 5, -1);
    }

    double hp_ratio = get_num(s, "hp") / get_num(s, "max_hp");
    hp_ratio = (get_num(s, "hp") * 100) / get_num(s, "max_hp");
    hp_ratio = hp_ratio * 0.01;
    {
        Value cl[3]; cl[0] = mk_num(hp_ratio); cl[1] = mk_num(0); cl[2] = mk_num(1);
        hp_ratio = num_of(&call_ret(vm, "clamp", cl, 3));
    }

    Value c2[1]; c2[0] = mk_num(bar_color);
    call_fn_v(vm, "draw_set_color", c2, 1, -1);
    {
        Value r[5];
        r[0] = mk_num(bar_x - bar_w / 2); r[1] = mk_num(bar_y);
        r[2] = mk_num(bar_x - bar_w / 2 + bar_w * hp_ratio); r[3] = mk_num(bar_y + bar_h);
        r[4] = mk_num(0);
        call_fn_v(vm, "draw_rectangle", r, 5, -1);
    }
    { Value h[1]; h[0] = mk_num(1); call_fn_v(vm, "draw_set_halign", h, 1, -1); }   // fa_center
    { Value v[1]; v[0] = mk_num(1); call_fn_v(vm, "draw_set_valign", v, 1, -1); }   // fa_middle
    { Value f[1]; f[0] = mk_str(vm, "font_yuan"); call_fn_v(vm, "draw_set_font", f, 1, -1); }
    { Value c3[1]; c3[0] = mk_num(16777215); call_fn_v(vm, "draw_set_color", c3, 1, -1); }
    if (bar_color != 32768) {
        Value c4[1]; c4[0] = mk_num(0);                           // c_black
        call_fn_v(vm, "draw_set_colour", c4, 1, -1);
    }
    {
        Value sa[1]; sa[0] = mk_num(get_num(s, "hp"));
        Value txt = call_ret(vm, "string", sa, 1);
        Value t[3];
        t[0] = mk_num(get_num(s, "x")); t[1] = mk_num(bar_y + 9); t[2] = txt;
        call_fn_v(vm, "draw_text", t, 3, -1);
    }
    Value a2[1]; a2[0] = mk_num(1);
    call_fn_v(vm, "draw_set_alpha", a2, 1, -1);
    return true;
}


// Other_10.gml（= UserEvent 0，155 行）—— 用注册数据初始化属性
//   常量：noone = -4
bool hot_card_user_event_0(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    Value pid[1]; pid[0] = *inst_var(s, "plant_id");
    Value plant_data = call_ret(vm, "get_plant_data", pid, 1);
    set_num(s, "current_level", 0);

    if (!is_undef(plant_data) && !eq_num(plant_data, -4)) {                 // 不是 undefined / noone
        inst_var_set(s, "name",        v_map(vm, plant_data, mk_str(vm, "name")));
        inst_var_set(s, "description", v_map(vm, plant_data, mk_str(vm, "description")));

        Value ci[1]; ci[0] = *inst_var(s, "plant_id");
        Value card_save = call_ret(vm, "get_card_info", ci, 1);
        if (!eq_num(card_save, 0)) {                                        // != false
            double cap = num_of(global_var("_VM_card_level_cap"));
            double lv  = num_of(&v_member(vm, card_save, "level"));
            set_num(s, "current_level", (cap >= 0 && lv > cap) ? cap : lv);
            double scap = num_of(global_var("_VM_card_shape_cap"));
            double sv   = num_of(&v_member(vm, card_save, "shape"));
            set_num(s, "shape", (scap >= 0 && sv > scap) ? scap : sv);
            double kcap = num_of(global_var("_VM_card_skill_cap"));
            double kv   = num_of(&v_member(vm, card_save, "skill"));
            set_num(s, "skill", (kcap >= 0 && kv > kcap) ? kcap : kv);
        }

        set_num(s, "_net_card_equipped_attire_id", -4);                     // noone
        struct G2V { const char* g; const char* v; };
        static const G2V net[] = {
            { "_net_before_plant_skill",         "skill"                     },
            { "_net_before_plant_shape",         "shape"                     },
            { "_net_before_plant_current_level", "current_level"             },
            { "_net_card_equipped_attire_id",    "_net_card_equipped_attire_id" },
        };
        for (size_t i = 0; i < sizeof(net) / sizeof(net[0]); i++) {
            Value* gv = global_var(net[i].g);
            if (gv && !eq_num(*gv, -4)) {                                   // != noone
                inst_var_set(s, net[i].v, *gv);
                global_var_set(net[i].g, mk_num(-4));
            }
        }

        Value ud[4];
        ud[0] = *inst_var(s, "plant_id");
        ud[1] = *inst_var(s, "shape");
        ud[2] = *inst_var(s, "current_level");
        ud[3] = *inst_var(s, "skill");
        Value upgrade = call_ret(vm, "get_plant_data_with_skill", ud, 4);
        if (!is_undef(upgrade) && !eq_num(upgrade, -4)) {
            inst_var_set(s, "hp", v_map(vm, upgrade, mk_str(vm, "hp")));
            set_num(s, "max_hp", get_num(s, "hp"));
            inst_var_set(s, "cost",     v_map(vm, upgrade, mk_str(vm, "cost")));
            inst_var_set(s, "atk",      v_map(vm, upgrade, mk_str(vm, "atk")));
            inst_var_set(s, "range",    v_map(vm, upgrade, mk_str(vm, "range")));
            inst_var_set(s, "cooldown", v_map(vm, upgrade, mk_str(vm, "cooldown")));
            inst_var_set(s, "cycle",    v_map(vm, upgrade, mk_str(vm, "cycle")));
            Value ka[2]; ka[0] = upgrade; ka[1] = mk_str(vm, "flame_produce");
            Value ex = call_ret(vm, "ds_map_exists", ka, 2);
            if (truthy(&ex))
                inst_var_set(s, "flame_produce", v_map(vm, upgrade, mk_str(vm, "flame_produce")));
        }
    }

    // current_level >= 4 → 绑定星级贴图
    if (get_num(s, "current_level") >= 4) {
        Value a[4];
        a[0] = mk_num(get_num(s, "x"));
        a[1] = mk_num(get_num(s, "y") - 5);
        a[2] = mk_num(get_num(s, "depth") - 1);
        a[3] = mk_str(vm, "obj_stars");
        Value inst = call_ret(vm, "instance_create_depth", a, 4);
        Value star_spr = mk_num(-1);
        double lv = get_num(s, "current_level");
        if (lv >= 4 && lv <= 16) {                                          // switch 4..16
            char buf[32];
            sprintf(buf, "spr_star_%d", (int)lv);                           // 精灵名当字符串传
            star_spr = mk_str(vm, buf);
        }
        if (inst.t == MEM_INT && inst.v > 0) {
            inst_var_set(inst.v, "sprite_index", star_spr);
            inst_var_set(inst.v, "parent_card",  mk_num(s));
        }
        set_num(s, "banding_star_obj", (inst.t == MEM_INT) ? inst.v : -4);  // inst.id
    }

    // 卡组表查不到 → 退回注册表
    Value sd[1]; sd[0] = *inst_var(s, "plant_id");
    Value shape_data = call_ret(vm, "get_card_info_simple", sd, 1);
    double card_shape = 0;
    if (!eq_num(shape_data, 0)) card_shape = num_of(&v_member(vm, shape_data, "shape"));
    Value dd[2]; dd[0] = *inst_var(s, "plant_id"); dd[1] = mk_num(card_shape);
    Value card_data = call_ret(vm, "deck_get_card_data", dd, 2);
    if (eq_num(card_data, -4) || is_undef(card_data)) {
        Value pd[1]; pd[0] = *inst_var(s, "plant_id");
        card_data = call_ret(vm, "get_plant_data", pd, 1);
    }
    if (!eq_num(card_data, -4) && !is_undef(card_data)) {
        static const char* keys[] = { "plant_type", "feature_type", "target_card" };
        for (size_t i = 0; i < 3; i++) {
            Value ka[2]; ka[0] = card_data; ka[1] = mk_str(vm, keys[i]);
            Value ex = call_ret(vm, "ds_map_exists", ka, 2);
            if (truthy(&ex)) inst_var_set(s, keys[i], v_map(vm, card_data, mk_str(vm, keys[i])));
        }
    }

    // 检查食谱：三层循环 + 按 modif_type 改属性
    Value cookbook = v_member(vm, gv(vm, "save_data"), "equipped_cookbook");
    {
        Value la[1]; la[0] = cookbook;
        int n = (int)num_of(&call_ret(vm, "array_length", la, 1));
        for (int i = 0; i < n; i++) {
            Value row = v_array(vm, cookbook, mk_num(i));
            Value lb[1]; lb[0] = row;
            int m = (int)num_of(&call_ret(vm, "array_length", lb, 1));
            for (int j = 0; j < m; j++) {
                Value item = v_array(vm, row, mk_num(j));
                Value ca[1]; ca[0] = item;
                Value cook = call_ret(vm, "get_cookbook_data", ca, 1);
                Value modifs = v_member(vm, cook, "modif");
                Value lc[1]; lc[0] = modifs;
                int p = (int)num_of(&call_ret(vm, "array_length", lc, 1));
                for (int k = 0; k < p; k++) {
                    Value mod = v_array(vm, modifs, mk_num(k));
                    Value ai[2]; ai[0] = v_member(vm, mod, "card_id"); ai[1] = *inst_var(s, "plant_id");
                    if (num_of(&call_ret(vm, "array_get_index", ai, 2)) == -1) continue;
                    Value mtype = v_member(vm, mod, "modif_type");
                    Value ve[2]; ve[0] = mk_num(s); ve[1] = mtype;
                    Value vex = call_ret(vm, "variable_instance_exists", ve, 2);
                    if (!truthy(&vex)) continue;
                    Value vg[2]; vg[0] = mk_num(s); vg[1] = mtype;
                    double cur = num_of(&call_ret(vm, "variable_instance_get", vg, 2));
                    Value calc = v_member(vm, mod, "modif_calc");
                    double amount = num_of(&v_member(vm, mod, "amount"));
                    if (str_is(vm, &calc, "plus"))          cur += amount;
                    else if (str_is(vm, &calc, "multiply")) cur *= amount;
                    else continue;
                    Value vs[3]; vs[0] = mk_num(s); vs[1] = mtype; vs[2] = mk_num(cur);
                    call_fn_v(vm, "variable_instance_set", vs, 3, -1);
                    set_num(s, "max_hp", get_num(s, "hp"));
                }
            }
        }
    }

    set_num(s, "origin_cycle", get_num(s, "cycle"));

    // var grid_pos = get_grid_position_from_world(x, y)（GML 里 col/row 只在被注释掉的段落用）
    {
        Value a[2]; a[0] = mk_num(get_num(s, "x")); a[1] = mk_num(get_num(s, "y"));
        Value gp = call_ret_held(vm, "get_grid_position_from_world", a, 2);
        (void)v_member(vm, gp, "col");
        (void)v_member(vm, gp, "row");
        release(gp);
    }

    // if global.network.mode == "client" { exit }
    Value mode = v_member(vm, gv(vm, "network"), "mode");
    if (str_is(vm, &mode, "client")) return true;
    return true;
}
