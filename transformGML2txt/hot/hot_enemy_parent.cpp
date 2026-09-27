// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// ============================================================
// obj_enemy_parent
// ============================================================

// Create_0.gml（80 行）—— 纯初始化 + 两个函数调用
//   常量：ENEMY_STATE.NORMAL = 2、noone = -4、#FFFFFF = 16777215
//   精灵/声音名（spr_mouse_frozen / spr_mouse_stun / snd_hit1）在这边当字符串传，宿主解析
//   （跟对象名一个规矩；GML 里它们是 64 位资产句柄，我们统一用名字）
bool hot_enemy_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    // 初始化直接落地：内置字段写字段（这里就这三个），其余写 vars
    in->image_xscale = mk_num(1.8);
    in->image_yscale = mk_num(1.8);
    in->image_speed  = mk_num(0);

    struct NV { const char* n; double v; };
    static const NV nums[] = {
        { "atk",               10   },
        { "atk_cycle",         36   },   // 0.6 * 60
        { "move_speed",        0.32 },
        { "move_speed_modify", 1    },
        { "hp",                100  },
        { "maxhp",             100  },
        { "hurt_rate",         0.5  },
        { "state",             2    },   // ENEMY_STATE.NORMAL
        { "flash_speed",       6    },
        { "attack_anim",       4    },
        { "move_anim",         8    },
        { "death_anim",        13   },
        { "flash_value",       0    },
        { "timer",             0    },
        { "attack_timer",      0    },
        { "target_plant",     -4    },   // noone
        { "attack_range",      90   },
        { "immune_to_ash",     0    },
        { "ice_timer",         0    },
        { "is_slowdown",       0    },
        { "frozen_timer",      0    },
        { "is_frozen",         0    },
        { "helmet_hp",         0    },
        { "helmet_max_hp",     0    },
        { "shield_hp",         0    },
        { "shield_max_hp",     0    },
        { "special_ash",       0    },
        { "current_frozen",    0    },
        { "is_stun",           0    },
        { "stun_timer",        0    },
        { "giant_type",        0    },
        { "is_boss",           0    },
        { "can_dropped",       0    },
        { "is_scare",          0    },
        { "scare_timer",       0    },
        { "y_move",            0    },
        { "left_move_flashs",  0    },
        { "flash_color",       16777215 },   // #FFFFFF
        { "grid_col",          99   },
        { "grid_row",          99   },
        { "damage_amount",     0    },
        { "water_effect_timer",0    },
        { "hp_modified",       0    },
        { "frame_count",       0    },
        { "random_count",      0    },
        { "pre_state",        -1    },
        { "pre_hp",           -1    },
        { "ice_ok",            1    },
    };
    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++)
        in->vars[nums[i].n] = mk_num(nums[i].v);

    struct SV { const char* n; const char* v; };
    static const SV strs[] = {
        { "ice_sprite",  "spr_mouse_frozen" },
        { "stun_sprite", "spr_mouse_stun"   },
        { "mouse_id",    ""                 },
        { "target_type", "normal"           },
        { "damage_type", ""                 },
        { "hit_sound",   "snd_hit1"         },
    };
    for (size_t i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
        in->vars[strs[i].n] = mk_str(vm, strs[i].v);

    set_num(s, "shader_hit", get_num(s, "hit_effect_2"));    // shader_hit = hit_effect_2
    {
        Value a[2];
        a[0] = *inst_var(s, "shader_hit"); a[1] = mk_str(vm, "u_progress");
        set_var(s, "u_progress",   call_ret(vm, "shader_get_uniform", a, 2));
        a[1] = mk_str(vm, "u_flashColor");
        set_var(s, "u_flashColor", call_ret(vm, "shader_get_uniform", a, 2));
    }
    // block_mouse_id_list = ["mouse_train_1"]
    {
        Value e[1]; e[0] = mk_str(vm, "mouse_train_1");
        set_var(s, "block_mouse_id_list", call_ret(vm, "array_literal", e, 1));
    }
    // random_seed = irandom_range(0, 4096)
    {
        Value a[2]; a[0] = mk_num(0); a[1] = mk_num(4096);
        set_num(s, "random_seed", num_of(&call_ret(vm, "irandom_range", a, 2)));
    }
    set_num(s, "_state_prev", get_num(s, "state"));                       // _state_prev = state
    set_num(s, "tbit", enemy_tbit_of(vm, inst_var(s, "target_type")));    // tbit = enemy_tbit_of(...)
    return true;
}


// Draw_0.gml（43 行）—— 绘制
//   常量：c_white = 16777215、c_blue = 16711680
bool hot_enemy_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    call_fn(vm, "draw_self", NULL, 0, -1);

    if (get_num(s, "flash_value") > 0) {
        Value sa[1]; sa[0] = *inst_var(s, "shader_hit");
        call_fn_v(vm, "shader_set", sa, 1, -1);
        Value d[9];
        d[0] = *inst_var(s, "sprite_index");
        d[1] = *inst_var(s, "image_index");
        d[2] = mk_num(get_num(s, "x"));
        d[3] = mk_num(get_num(s, "y"));
        d[4] = *inst_var(s, "image_xscale");
        d[5] = *inst_var(s, "image_yscale");
        d[6] = *inst_var(s, "image_angle");
        d[7] = *inst_var(s, "flash_color");
        d[8] = mk_num(get_num(s, "flash_value") / 200.0);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
        call_fn(vm, "shader_reset", NULL, 0, -1);
    }
    if (get_num(s, "is_slowdown") != 0) {
        Value m[3]; m[0] = mk_num(16777215); m[1] = mk_num(16711680); m[2] = mk_num(0.5);
        inst_var_set(s, "image_blend", call_ret(vm, "merge_color", m, 3));
    } else {
        set_num(s, "image_blend", 16777215);
    }
    if (get_num(s, "is_frozen") != 0) {
        Value d[9];
        d[0] = *inst_var(s, "ice_sprite");
        d[1] = mk_num(0);
        d[2] = mk_num(get_num(s, "x"));
        d[3] = mk_num(get_num(s, "y") + 50);
        d[4] = mk_num(1.8); d[5] = mk_num(1.8); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    if (get_num(s, "is_scare") != 0) {
        Value d[9];
        d[0] = mk_str(vm, "spr_mouse_scared");
        d[1] = mk_num(0);
        d[2] = mk_num(get_num(s, "x") - 45);
        d[3] = mk_num(get_num(s, "y") - 125);
        d[4] = mk_num(1.8); d[5] = mk_num(1.8); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    if (get_num(s, "is_stun") != 0) {
        Value spr = *inst_var(s, "stun_sprite");
        Value na[1]; na[0] = spr;
        double n = num_of(&call_ret(vm, "sprite_get_number", na, 1));
        Value fa[1]; fa[0] = mk_num(get_num(s, "stun_timer") / 5);
        double fl = num_of(&call_ret(vm, "floor", fa, 1));
        double idx = n - fmod(fl, n);                       // (floor(timer/5) mod n)
        Value d[9];
        d[0] = spr;
        d[1] = mk_num(idx);
        d[2] = mk_num(get_num(s, "x") - 20);
        d[3] = mk_num(get_num(s, "y") - 150);
        d[4] = mk_num(1.8); d[5] = mk_num(1.8); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    return true;
}


// Draw_64.gml（49 行）—— 敌人血条 + 护盾条
//   常量：c_white=16777215、c_black=0、c_purple=8388736、c_aqua=16776960
bool hot_enemy_draw_64(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (truthy(global_var("is_paused"))) return true;
    if (!(truthy(global_var("enemy_hpbar")) && get_num(s, "hp") > 0)) return true;

    Value a1[1]; a1[0] = mk_num(0.5);
    call_fn_v(vm, "draw_set_alpha", a1, 1, -1);

    double bar_x = get_num(s, "x");
    double bar_y = get_num(s, "y") - 30;
    double bar_w = 80, bar_h = 20;

    { Value c[1]; c[0] = mk_num(16777215); call_fn_v(vm, "draw_set_color", c, 1, -1); }
    {
        Value r[5];
        r[0] = mk_num(bar_x - bar_w / 2); r[1] = mk_num(bar_y);
        r[2] = mk_num(bar_x + bar_w / 2); r[3] = mk_num(bar_y + bar_h);
        r[4] = mk_num(1);
        call_fn_v(vm, "draw_rectangle", r, 5, -1);
    }
    double hp_ratio = get_num(s, "hp") / get_num(s, "maxhp");
    hp_ratio = (get_num(s, "hp") * 100) / get_num(s, "maxhp");
    hp_ratio = hp_ratio * 0.01;
    {
        Value cl[3]; cl[0] = mk_num(hp_ratio); cl[1] = mk_num(0); cl[2] = mk_num(1);
        hp_ratio = num_of(&call_ret(vm, "clamp", cl, 3));
    }
    { Value c[1]; c[0] = mk_num(8388736); call_fn_v(vm, "draw_set_color", c, 1, -1); }   // c_purple
    {
        Value r[5];
        r[0] = mk_num(bar_x - bar_w / 2); r[1] = mk_num(bar_y);
        r[2] = mk_num(bar_x - bar_w / 2 + bar_w * hp_ratio); r[3] = mk_num(bar_y + bar_h);
        r[4] = mk_num(0);
        call_fn_v(vm, "draw_rectangle", r, 5, -1);
    }
    if (get_num(s, "shield_hp") > 0 && get_num(s, "shield_max_hp") != 0) {
        Value cl[3];
        cl[0] = mk_num(get_num(s, "shield_hp") / get_num(s, "shield_max_hp"));
        cl[1] = mk_num(0); cl[2] = mk_num(1);
        double sr = num_of(&call_ret(vm, "clamp", cl, 3));
        Value c[1]; c[0] = mk_num(16776960); call_fn_v(vm, "draw_set_color", c, 1, -1);  // c_aqua
        Value r[5];
        r[0] = mk_num(bar_x - bar_w / 2); r[1] = mk_num(bar_y);
        r[2] = mk_num(bar_x - bar_w / 2 + bar_w * sr); r[3] = mk_num(bar_y + bar_h);
        r[4] = mk_num(0);
        call_fn_v(vm, "draw_rectangle", r, 5, -1);
    }
    { Value h[1]; h[0] = mk_num(1); call_fn_v(vm, "draw_set_halign", h, 1, -1); }        // fa_center
    { Value v[1]; v[0] = mk_num(1); call_fn_v(vm, "draw_set_valign", v, 1, -1); }        // fa_middle
    { Value f[1]; f[0] = mk_str(vm, "font_yuan"); call_fn_v(vm, "draw_set_font", f, 1, -1); }
    { Value c[1]; c[0] = mk_num(16777215); call_fn_v(vm, "draw_set_color", c, 1, -1); }
    if (get_num(s, "shield_hp") > 0) {
        Value c[1]; c[0] = mk_num(0);                                                    // c_black
        call_fn_v(vm, "draw_set_colour", c, 1, -1);
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


// Destroy_0.gml：
//   if !global.laboretory_room {
//       var is_drop = random_range(0,100)
//       if is_drop < 10 { instance_create_depth(x, y-50, depth-200, obj_coin) }
//   }
bool hot_enemy_destroy(Vm& vm) {
    if (truthy(global_var("laboretory_room"))) return true;   // 实验房里不掉
    int self = vm.cur_instance;
    if (inst_get(self)) {
        Value a2[2]; a2[0] = mk_num(0); a2[1] = mk_num(100);
        double is_drop = num_of(&call_ret(vm, "random_range", a2, 2));
        if (is_drop < 10) {
            Value args[4];
            args[0] = mk_num(get_num(self, "x"));
            args[1] = mk_num(get_num(self, "y") - 50);
            args[2] = mk_num(get_num(self, "depth") - 200);
            args[3] = mk_str(vm, "obj_coin");                 // 对象名在这边就是字符串
            call_fn_v(vm, "instance_create_depth", args, 4, -1);
        }
    }
    return true;
}


// Other_10.gml（UserEvent 0）：
//   flash_value = 150
//   if shield_hp > 0 {
//       normal → shield_hp -= damage_amount
//       pierce → shield_hp -= damage_amount; hp -= damage_amount
//       其它   → hp -= damage_amount
//   } else { hp -= damage_amount }
bool hot_enemy_user_event_0(Vm& vm) {
    int self = vm.cur_instance;
    if (!inst_get(self)) return true;
    set_num(self, "flash_value", 150);
    double shield = get_num(self, "shield_hp");
    double dmg    = get_num(self, "damage_amount");
    const Value* dtype = inst_var(self, "damage_type");
    if (shield > 0) {
        if (str_is(vm, dtype, "normal")) {
            set_num(self, "shield_hp", shield - dmg);
        } else if (str_is(vm, dtype, "pierce")) {
            set_num(self, "shield_hp", shield - dmg);
            add_num(self, "hp", -dmg);
        } else {
            add_num(self, "hp", -dmg);
        }
    } else {
        add_num(self, "hp", -dmg);
    }
    return true;
}


// ============================================================
// obj_enemy_parent：剩下的两个 Step
// ============================================================
// 跑 mod VM 里的一个块 —— 平台那边是：
double world_y_of_grid(Vm& vm, double col, double row) {
    Value a[2]; a[0] = mk_num(col); a[1] = mk_num(row);
    Value s = call_ret(vm, "get_world_position_from_grid", a, 2);
    return num_of(&v_struct(vm, s, "y"));
}


// 敌人动画帧：伤害越深往下走一个"受伤档"，有护盾另有一套档位
//   照抄 Step_0 里那两段四层嵌套（护盾满/破盾 × 重伤/轻伤 = 4 档）：
//     · 移动：档位偏移 = 0 / anim / 2*anim / 3*anim
//     · 攻击：同样的档位，再加一层 move_anim*4（有盾）或 move_anim*2（没盾）
//   attack=false 时 anim = move_anim；attack=true 时 anim = attack_anim
double enemy_anim_frame(Vm& vm, int s, bool attack) {
    double anim = attack ? get_num(s, "attack_anim") : get_num(s, "move_anim");
    double ma   = get_num(s, "move_anim");
    double f    = std::floor(get_num(s, "timer") / get_num(s, "flash_speed"));
    double base = std::fmod(f, anim);                       // GML 的 mod
    double hp = get_num(s, "hp"), maxhp = get_num(s, "maxhp"), helmet = get_num(s, "helmet_hp");
    double hurt = get_num(s, "hurt_rate");
    bool heavy;                                             // 重伤档（血量比例 <= hurt_rate）
    if (helmet > 0 && hp > maxhp - helmet) heavy = !((hp + helmet - maxhp) / maxhp > hurt);
    else                                   heavy = !(hp / (maxhp - helmet) > hurt);
    bool shielded  = get_num(s, "shield_max_hp") > 0 && get_num(s, "shield_hp") > 0;
    bool shield_ok = get_num(s, "shield_hp") > hurt * get_num(s, "shield_max_hp");
    double off;
    if (shielded) {
        if (shield_ok) off = heavy ? 0 : anim * 2;
        else           off = heavy ? anim : anim * 3;
    } else {
        off = heavy ? 0 : anim;
    }
    double idx = base + off;
    if (attack) idx += shielded ? ma * 4 : ma * 2;
    return idx;
}


// "找前方植物"这段（Step_0 的 NORMAL / ATTACK 两个 case 里一模一样）
//   plant_order_list = global._eat_scratch（4 格数组），按 global.eat_order 的优先级填；
//   返回找到的植物实例 id，没找到 → noone(-4)
//   ⚠ 注释掉的那段 with (obj_card_parent) 碰撞版在 GML 里就是注释，别翻出来
int enemy_find_plant(Vm& vm, int s) {
    const double NOONE = -4;

    Value scratch = gv(vm, "_eat_scratch");
    for (int i = 0; i < 4; i++) {                            // 先全清成 noone
        Value a[3]; a[0] = scratch; a[1] = mk_num(i); a[2] = mk_num(NOONE);
        call_fn_v(vm, "array_set", a, 3, -1);
    }
    if (scratch.t != MEM_ARRAY) return (int)NOONE;           // 没有暂存数组就找不到东西

    if (!global_exists("grid_plants")) return (int)NOONE;
    Value grid = gv(vm, "grid_plants");
    Value da[2]; da[0] = grid; da[1] = mk_num(3);            // ds_type_grid
    if (!truthy(&call_ret(vm, "ds_exists", da, 2))) return (int)NOONE;

    double cols = gnumv(vm, "grid_cols"), rows = gnumv(vm, "grid_rows");
    double gc   = get_num(s, "grid_col"), gr = get_num(s, "grid_row");
    double start = std::fmax(gc - 2, -1);
    double end   = std::fmin(gc + 2, cols - 1);

    for (double c = start; c <= end; c++) {
        if (gr < 0 || gr >= rows) continue;
        double wrapped = std::fmod(c + cols + 64, cols + 64);          // (_c+cols+64)%(cols+64)
        Value ga[3]; ga[0] = grid; ga[1] = mk_num(wrapped); ga[2] = mk_num(gr);
        Value list = call_ret(vm, "ds_grid_get", ga, 3);               // 借来的句柄：grid 里存着它，安全
        Value la[1]; la[0] = list;
        int ln = (int)num_of(&call_ret(vm, "ds_list_size", la, 1));
        for (int k = 0; k < ln; k++) {
            Value ga2[2]; ga2[0] = list; ga2[1] = mk_num(k);
            Value plant = call_ret(vm, "ds_list_find_value", ga2, 2);
            int pid = plant.t == MEM_INT ? plant.v : (int)num_of(&plant);
            if (!inst_get(pid)) continue;                              // instance_exists
            double dx = get_num(pid, "x") - get_num(s, "x");
            double ar = get_num(s, "attack_range");
            bool in_front = (ar > 0) ? (dx < 0 && dx > -ar) : (dx > 0 && dx < -ar);
            bool type_ok  = !str_is(vm, inst_var(pid, "feature_type"), "dwarf")
                            || truthy(inst_var(s, "giant_type"));
            if (!in_front || get_num(pid, "grid_row") != get_num(s, "grid_row") || !type_ok) continue;
            // 按 eat_order（ds_list）的优先级填格
            Value order = gv(vm, "eat_order");
            Value oa[1]; oa[0] = order;
            int on = (order.t == MEM_LIST) ? (int)num_of(&call_ret(vm, "ds_list_size", oa, 1)) : 0;
            for (int i = 0; i < on; i++) {
                Value oa2[2]; oa2[0] = order; oa2[1] = mk_num(i);
                Value tar = call_ret(vm, "ds_list_find_value", oa2, 2);
                if (str_eq(vm, inst_var(pid, "plant_type"), &tar)) {
                    Value sa[3]; sa[0] = scratch; sa[1] = mk_num(i); sa[2] = mk_num(pid);
                    call_fn_v(vm, "array_set", sa, 3, -1);
                    break;
                }
            }
        }
    }
    for (int i = 0; i < 4; i++) {                            // 取第一个非 noone
        Value a[2]; a[0] = scratch; a[1] = mk_num(i);
        Value v = call_ret(vm, "array_get", a, 2);
        double n = num_of(&v);
        if (n != NOONE) return (int)n;
    }
    return (int)NOONE;
}


// Step_2.gml（End Step，110 行）—— tbit（命中位掩码）的唯一写点 + 难度修正 +
//   地形减速 + 服务端 Boss 属性广播 + 掉血钩子
//   常量：ENEMY_STATE.DEAD = 3、MSG_MODIFY_PROP = 25
bool hot_enemy_step_2(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    set_num(s, "tbit", enemy_tbit_of(vm, inst_var(s, "target_type")));
    set_num(s, "ice_ok", inst_var(s, "ice_timer") ? 1 : 0);   // variable_instance_exists(id,"ice_timer")

    set_num(s, "damage_amount", 0);
    inst_var_set(s, "damage_type", mk_str(vm, ""));

    // ── 难度修正（只做一次）──
    if (!truthy(inst_var(s, "hp_modified"))) {
        static const char* hpv[] = { "maxhp", "hp", "helmet_hp", "helmet_max_hp", "shield_hp", "shield_max_hp" };
        double diff = gnumv(vm, "difficulty");
        if (diff == 0)
            for (int i = 0; i < 6; i++) set_num(s, hpv[i], get_num(s, hpv[i]) * 0.8);
        if (diff >= 3)
            // GML 里 map_id == "tower_cake" 与否**两个分支都是 1.2**，照抄，不是漏写
            for (int i = 0; i < 6; i++) set_num(s, hpv[i], get_num(s, hpv[i]) * 1.2);
        Value lf = gv(vm, "level_file");
        if (lf.t == MEM_MAP) {
            Value ver = v_struct(vm, lf, "version");
            if ((ver.t == MEM_INT || ver.t == MEM_FLOAT) && !truthy(inst_var(s, "is_boss"))) {
                double hm = num_of(&v_struct(vm, lf, "hp_modify"));
                for (int i = 0; i < 6; i++) set_num(s, hpv[i], get_num(s, hpv[i]) * hm);
            }
        }
        set_num(s, "hp_modified", 1);
    }

    // ── 地形位：障碍=bit0、黏液=bit1、岩浆=bit2、海水=bit3 ──
    double tc = gnumv(vm, "grid_cols"), tr = gnumv(vm, "grid_rows");
    Value flag = gv(vm, "cell_terrain_flag");
    bool has_terrain = global_exists("cell_terrain_flag") && tc > 0 && tr > 0
                       && (double)container_size(flag) == tc * tr;   // array_length(...) == tc*tr
    double tf = 0;
    if (has_terrain) {
        double gr = get_num(s, "grid_row"), gc = get_num(s, "grid_col");
        if (gr >= 0 && gr < tr && gc >= 0 && gc < tc)
            tf = num_of(&container_at(flag, (int)(gr * tc + gc)));
    }
    bool slow = false;
    const Value* tt = inst_var(s, "target_type");
    if (((int)tf & 4) != 0 && (str_is(vm, tt, "normal") || str_is(vm, tt, "dance")
                               || str_is(vm, tt, "air") || str_is(vm, tt, "obstacle"))) slow = true;
    if (((int)tf & 2) != 0 && (str_is(vm, tt, "normal") || str_is(vm, tt, "dance"))) slow = true;
    set_num(s, "move_speed_modify", slow ? 2 : 1);

    // ── Boss 状态切换到技能时广播给客户端 ──
    {
        Value net = gv(vm, "network");
        Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
        if (net.t == MEM_MAP) mode = v_struct(vm, net, "mode");
        if (str_is(vm, &mode, "server") && truthy(inst_var(s, "is_boss"))
                && get_num(s, "state") != get_num(s, "_state_prev")) {
            Value map = v_struct(vm, net, "map_instance_id_net_id");
            Value ea[2]; ea[0] = map; ea[1] = mk_num(s);
            if (truthy(&call_ret(vm, "ds_map_exists", ea, 2))) {
                Value net_id = v_map(vm, map, mk_num(s));
                // 先把要发的属性收进局部：下面建 map 之后就不方便再插 call_ret 了
                static const char* extra[] = {
                    "x_move_speed", "y_move_speed", "target_pos", "target_coord", "skill_timer",
                    "jump_times", "skill_move_type", "skill_choose", "skill_change_style",
                    "is_reversed", "image_angle", "image_xscale", "image_alpha", "visible",
                    "move_time", "grid_col", "grid_row",
                };
                const int NX = (int)(sizeof(extra) / sizeof(extra[0]));
                const char* kn[24]; Value kv[24]; int nk = 0;
                for (int i = 0; i < NX; i++) {
                    Value* p = inst_var(s, extra[i]);          // variable_instance_exists(id, ...)
                    if (p) { kn[nk] = extra[i]; kv[nk] = *p; nk++; }
                }
                Value st = call_ret_held(vm, "ds_map_create", NULL, 0);
                Value ka[3]; ka[0] = st;
                static const char* base[] = { "state", "timer", "x", "y", "random_count" };
                for (int i = 0; i < 5; i++) {
                    ka[1] = mk_str(vm, base[i]);
                    ka[2] = mk_num(get_num(s, base[i]));
                    call_fn_v(vm, "ds_map_set", ka, 3, -1);
                }
                for (int i = 0; i < nk; i++) {
                    ka[1] = mk_str(vm, kn[i]);
                    ka[2] = kv[i];
                    call_fn_v(vm, "ds_map_set", ka, 3, -1);
                }
                Value js = call_ret(vm, "json_stringify", &st, 1);
                release(st);                                         // call_ret_held 那份还回去
                Value list = v_struct(vm, net, "connected_clients");
                int n = 0;
                if (list.t == MEM_ARRAY) {
                    Value la[1]; la[0] = list;
                    n = (int)num_of(&call_ret(vm, "array_length", la, 1));
                }
                for (int i = 0; i < n; i++) {
                    Value item = v_array(vm, list, mk_num(i));
                    Value msg[4];
                    msg[0] = item;
                    msg[1] = mk_num(25);                          // MSG_MODIFY_PROP
                    msg[2] = net_id;
                    msg[3] = js;
                    call_fn_v(vm, "send_message", msg, 4, -1);
                }
            }
        }
    }
    set_num(s, "_state_prev", get_num(s, "state"));

    // ── 掉血钩子 ──
    if (get_num(s, "pre_hp") > get_num(s, "hp")) {
        global_var_set("_VM_last_damaged_enemy", mk_num(s));
        Value* buf = global_var("_VM_ENEMY_DAMAGED");
        if (buf) {
            Value b[1]; b[0] = *buf;
            Value bexists = call_ret(vm, "buffer_exists", b, 1);      // buffer 还没建模 → 恒为 0
            if (truthy(&bexists)) {
                Value a[3];
                a[0] = global_var("__vm") ? *global_var("__vm") : mk_num(0);
                a[1] = *buf;
                a[2] = mk_str(vm, "_VM_ENEMY_DAMAGED");
                call_fn_v(vm, "VM_Execute", a, 3, -1);
            }
        }
        Value h[1]; h[0] = mk_str(vm, "_VM_ENEMY_DAMAGED");
        call_fn_v(vm, "vm_hook_run", h, 1, -1);
    }
    set_num(s, "pre_hp", get_num(s, "hp"));
    return true;
}


// Step_0.gml（500 行）—— 敌人每帧主逻辑：计时器/状态机（IDLE/NORMAL/ACTING/ATTACK/DEAD）/
//   找前方植物/攻击/死亡动画/深度/游戏结束/破冰动画
//   常量：ENEMY_STATE.IDLE=0、ATTACK=1、NORMAL=2、DEAD=3、ACTING=4、noone=-4、
//         MSG_GAME_OVER=10
bool hot_enemy_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    if (gtrue(vm, "is_paused")) return true;

    set_num(s, "_state_prev", get_num(s, "state"));

    // 客户端：直接死亡，不等待服务端
    if (get_num(s, "hp") <= 0 && get_num(s, "state") != 3) {
        set_num(s, "timer", 0);
        set_num(s, "state", 3);                              // ENEMY_STATE.DEAD
        set_num(s, "target_plant", -4);                      // noone
    }

    // 网格位置（内联算，GML 那边是为了避免每帧建结构体）
    set_num(s, "grid_col", std::floor((get_num(s, "x") - gnumv(vm, "grid_offset_x")) / gnumv(vm, "grid_cell_size_x")));
    set_num(s, "grid_row", std::floor((get_num(s, "y") - gnumv(vm, "grid_offset_y")) / gnumv(vm, "grid_cell_size_y")));

    // 减速 / 冰冻 / 恐惧 / 眩晕
    if (get_num(s, "ice_timer") > 0)    { add_num(s, "ice_timer", -1);    set_num(s, "is_slowdown", 1); } else set_num(s, "is_slowdown", 0);
    if (get_num(s, "frozen_timer") > 0) { set_num(s, "current_frozen", 1); add_num(s, "frozen_timer", -1); set_num(s, "is_frozen", 1); } else set_num(s, "is_frozen", 0);
    if (get_num(s, "scare_timer") > 0)  { add_num(s, "scare_timer", -1);  set_num(s, "is_scare", 1); } else set_num(s, "is_scare", 0);
    if (get_num(s, "stun_timer") > 0)   { add_num(s, "stun_timer", -1);   set_num(s, "is_stun", 1); }
    else { set_num(s, "is_stun", 0); inst_var_set(s, "stun_sprite", mk_str(vm, "spr_mouse_stun")); }
    if (get_num(s, "flash_value") > 0) add_num(s, "flash_value", -10);
    if (get_num(s, "hp") <= 0) {                             // 死了就把控制类计时器清掉
        set_num(s, "frozen_timer", 0);
        set_num(s, "scare_timer", 0);
        set_num(s, "left_move_flashs", 0);
        set_num(s, "stun_timer", 0);
    }

    double cur_atk_cycle = 0, cur_move_speed = 0;
    if (truthy(inst_var(s, "is_slowdown"))) {
        set_num(s, "flash_speed", 12);
        cur_move_speed = get_num(s, "move_speed") / 2;
        cur_atk_cycle  = get_num(s, "atk_cycle") * 2;
    } else {
        set_num(s, "flash_speed", 6);
        cur_move_speed = get_num(s, "move_speed");
        cur_atk_cycle  = get_num(s, "atk_cycle");
    }

    // 上下抖动（左移闪避的那套）
    if (get_num(s, "left_move_flashs") > 0) {
        add_num(s, "y", get_num(s, "y_move"));
        add_num(s, "left_move_flashs", -1);
        double y = get_num(s, "y"), ym = get_num(s, "y_move");
        if ((y <= world_y_of_grid(vm, 0, 0) + 38 && ym < 0)
                || (y >= world_y_of_grid(vm, 0, gnumv(vm, "grid_rows") - 1) + 38 && ym > 0)) {
            set_num(s, "left_move_flashs", 0);
            set_num(s, "y_move", 0);
        }
    }
    if (truthy(inst_var(s, "is_frozen")) || truthy(inst_var(s, "is_scare")) || truthy(inst_var(s, "is_stun")))
        return true;                                         // GML 的 exit

    add_num(s, "timer", 1);

    // 目标植物已经没了 → 清掉目标
    {
        int tp = (int)get_num(s, "target_plant");
        if (inst_get(tp) && get_num(tp, "hp") <= 0) set_num(s, "target_plant", -4);
    }

    double state = get_num(s, "state");

    if (state == 2) {                                        // ── ENEMY_STATE.NORMAL ──
        add_num(s, "x", -(cur_move_speed * get_num(s, "move_speed_modify")));
        set_num(s, "image_index", enemy_anim_frame(vm, s, false));
        int plant = enemy_find_plant(vm, s);
        if (plant != -4) {
            set_num(s, "state", 1);                          // ENEMY_STATE.ATTACK
            set_num(s, "target_plant", plant);
            set_num(s, "attack_timer", 0);
            set_num(s, "timer", 0);
        }
    } else if (state == 1) {                                 // ── ENEMY_STATE.ATTACK ──
        set_num(s, "image_index", enemy_anim_frame(vm, s, true));
        int plant = enemy_find_plant(vm, s);
        if (plant != -4) {
            set_num(s, "target_plant", plant);
        } else {
            set_num(s, "state", 2);                          // 没目标了 → 回到 NORMAL
            set_num(s, "target_plant", -4);
            set_num(s, "attack_timer", 0);
            set_num(s, "timer", 0);
        }
        add_num(s, "attack_timer", 1);
        if (get_num(s, "attack_timer") >= cur_atk_cycle) {
            int tp = (int)get_num(s, "target_plant");
            if (inst_get(tp)) {
                // with (target_plant) { if !invincible hp -= other.atk; event_user(2) ... }
                if (!truthy(inst_var(tp, "invincible"))) add_num(tp, "hp", -get_num(s, "atk"));
                Value ea[2]; ea[0] = mk_num(tp); ea[1] = mk_num(2);
                call_fn_v(vm, "run_event_user", ea, 2, -1);  // 那一格的 UserEvent 2
            }
            // 播放音效：obj_battle.chomp_sound_list[a]（取 obj_battle 的第一个实例）
            Value ia[2]; ia[0] = mk_num(0); ia[1] = mk_num(2);
            int a = (int)num_of(&call_ret(vm, "irandom_range", ia, 2));
            int bat = inst_at("obj_battle", 0);
            Value* sndl = (bat > 0) ? inst_var(bat, "chomp_sound_list") : NULL;
            if (sndl && sndl->t == MEM_LIST) {
                Value la[2]; la[0] = *sndl; la[1] = mk_num(a);
                Value snd = call_ret(vm, "ds_list_find_value", la, 2);
                Value sa[3]; sa[0] = snd; sa[1] = mk_num(0); sa[2] = mk_num(0);
                call_fn_v(vm, "audio_play_sound", sa, 3, -1);
            }
            set_num(s, "attack_timer", 0);
        }
    } else if (state == 3) {                                 // ── ENEMY_STATE.DEAD ──
        set_num(s, "ice_timer", 0);
        set_num(s, "frozen_timer", 0);
        double ma = get_num(s, "move_anim"), aa = get_num(s, "attack_anim"), da = get_num(s, "death_anim");
        double layer = (get_num(s, "shield_max_hp") > 0 && get_num(s, "shield_hp") > 0) ? (ma * 4 + aa * 4)
                                                                                       : (ma * 2 + aa * 2);
        if (get_num(s, "image_index") >= da + layer - 1) add_num(s, "image_alpha", -0.08);
        else set_num(s, "image_index", std::fmod(std::floor(get_num(s, "timer") / get_num(s, "flash_speed")), da) + layer);
    }
    // （IDLE=0 / ACTING=4 两个 case 里本来就是空的）

    // 死亡处理
    if (get_num(s, "hp") <= 0 && get_num(s, "state") != 3) {
        set_num(s, "timer", 0);
        set_num(s, "state", 3);
        set_num(s, "target_plant", -4);
    }
    // 透明度处理
    if (get_num(s, "image_alpha") <= 0 && get_num(s, "state") == 3)
        call_fn(vm, "instance_destroy", NULL, 0, -1);

    // 网格位置对应的深度（僵尸比植物稍微靠后一点）
    set_num(s, "depth", -10 - (get_num(s, "grid_row") * 45) - (9 * 5));

    // 跑到最左边 → 游戏结束
    {
        Value pm[3];
        pm[0] = mk_num(get_num(s, "x")); pm[1] = mk_num(get_num(s, "y")); pm[2] = mk_str(vm, "obj_cat");
        Value hitcat = call_ret(vm, "place_meeting", pm, 3);
        Value ai[2]; ai[0] = *inst_var(s, "block_mouse_id_list"); ai[1] = *inst_var(s, "mouse_id");
        double mi = num_of(&call_ret(vm, "array_get_index", ai, 2));
        // ⚠ place_meeting 的碰撞还没建模（空壳 → 0），这里等于"永远没碰到猫"
        if (get_num(s, "x") < gnumv(vm, "grid_offset_x") - 150 && get_num(s, "hp") > 0
                && !truthy(&hitcat) && mi == -1) {
            Value net = gv(vm, "network");
            Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
            if (net.t == MEM_MAP) mode = v_struct(vm, net, "mode");
            if (str_is(vm, &mode, "server")) {
                Value list = v_struct(vm, net, "connected_clients");
                int n = 0;
                if (list.t == MEM_ARRAY) {
                    Value la[1]; la[0] = list;
                    n = (int)num_of(&call_ret(vm, "array_length", la, 1));
                }
                for (int i = 0; i < n; i++) {
                    Value item = v_array(vm, list, mk_num(i));
                    Value msg[3];
                    msg[0] = item; msg[1] = mk_num(10);      // MSG_GAME_OVER
                    msg[2] = mk_num(0);
                    call_fn_v(vm, "send_message", msg, 3, -1);
                }
            }
            if (!str_is(vm, &mode, "client")) {
                global_var_set("is_paused", mk_num(1));
                global_var_set("game_over", mk_num(1));
                Value ca[4];
                ca[0] = mk_num(gnumv(vm, "room_width") / 2);
                ca[1] = mk_num(gnumv(vm, "room_height") / 2);
                ca[2] = mk_num(-3001);
                ca[3] = mk_str(vm, "obj_game_over");
                call_fn_v(vm, "instance_create_depth", ca, 4, -1);
                Value sa[3]; sa[0] = mk_str(vm, "snd_lose"); sa[1] = mk_num(0); sa[2] = mk_num(0);
                call_fn_v(vm, "audio_play_sound", sa, 3, -1);
            }
        }
    }

    // 破冰动画
    if (truthy(inst_var(s, "current_frozen")) && !truthy(inst_var(s, "is_frozen"))) {
        Value sa[3]; sa[0] = mk_str(vm, "snd_mouse_unfreeze"); sa[1] = mk_num(0); sa[2] = mk_num(0);
        call_fn_v(vm, "audio_play_sound", sa, 3, -1);
        Value ca[4];
        ca[0] = mk_num(get_num(s, "x"));
        ca[1] = mk_num(get_num(s, "y") + 50);
        ca[2] = mk_num(get_num(s, "depth"));
        ca[3] = mk_str(vm, "obj_unfreeze_effect");
        Value fx = call_ret(vm, "instance_create_depth", ca, 4);
        int fid = fx.t == MEM_INT ? fx.v : -1;
        if (inst_get(fid)) inst_var_set(fid, "sprite_index", *inst_var(s, "ice_sprite"));
        inst_var_set(s, "ice_sprite", mk_str(vm, "spr_mouse_frozen"));
        set_num(s, "current_frozen", 0);
        add_num(s, "frame_count", 1);
        // ⚠ frame_count++ 在 GML 里就在这个 if 里面（缩进看着像在外面，但大括号在里面），照抄
    }
    return true;
}
