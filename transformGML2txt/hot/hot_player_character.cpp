// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// 结构体多级取（global.save_data.equipped_items.main_weapon.id 这种）
//   中间任意一级不是 struct → undefined（跟 GML 一样不报错）
Value m_chain(Vm& vm, Value root, const char* const* keys, int n) {
    Value cur = root;
    for (int i = 0; i < n; i++) {
        if (cur.t != MEM_MAP) { Value u; u.t = MEM_UNDEFINED; u.v = 0; return u; }
        cur = m_get(vm, cur, keys[i]);
    }
    return cur;
}


// 给一把武器建它的宝石对象（Mouse_53 里主武器/超级武器/副武器各一段，形状一样）
//   ⚠ get_gem_info / get_gem_level / get_gem_index 还是空壳 → 现在整段不会真的建东西，
//     但结构照 GML 抄齐，宿主把那几个 API 补上就自动生效
double make_weapon_gems(Vm& vm, int s, const Value& gem_list, double gem_index_in) {
    double gem_index = gem_index_in;
    if (gem_list.t != MEM_ARRAY) return gem_index;
    int n = container_size(gem_list);
    for (int i = 0; i < n; i++) {
        Value gem_id = array_v(vm, gem_list, i);
        Value banned = gv(vm, "banned_gems_online");
        Value ai[2]; ai[0] = banned; ai[1] = gem_id;
        if (num_of(&call_ret(vm, "array_get_index", ai, 2)) != -1) continue;   // 被 ban 的跳过
        Value ga[1]; ga[0] = gem_id;
        Value gem_info = call_ret_held(vm, "get_gem_info", ga, 1);
        if (gem_info.t != MEM_MAP) continue;                                   // is_struct(gem_info)
        Value objv = m_get(vm, gem_info, "obj");
        release(gem_info);
        if (num_of(&objv) == -4) continue;                                     // obj == noone
        global_var_set("_mod_pending_gem_id", gem_id);
        Value ca[4];
        ca[0] = mk_num(390); ca[1] = mk_num(213 + gem_index * 80); ca[2] = mk_num(-500);
        ca[3] = objv;
        Value inst = call_ret(vm, "instance_create_depth", ca, 4);
        int gid = inst.t == MEM_INT ? inst.v : -1;
        if (inst_get(gid)) {
            // 发射方信息：插件宝石靠这几个变量定位（别用 parent_player，会和原版语义打架）
            inst_var_set(gid, "mod_point_parent_player", mk_num(s));
            inst_var_set(gid, "mod_point_grid_row", *inst_var(s, "grid_row"));
            inst_var_set(gid, "mod_point_grid_col", *inst_var(s, "grid_col"));
            Value lv[1]; lv[0] = gem_id;
            inst_var_set(gid, "mod_point_gem_level", call_ret(vm, "get_gem_level", lv, 1));
            // mod 宝石：归属写完了再调一次它的步（初始化在它的 Step 顶部）
            if (mod_vm(pool_text(vm.strings, &gem_id))) inst_event(vm, gid, "_OBJECT_STEP");
        }
        gem_index++;
    }
    return gem_index;
}


// Create_0.gml（33 行）
bool hot_player_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    // 初始化直接落地（内置字段：image_xscale / image_yscale / image_speed / depth）
    in->image_xscale = mk_num(1.6);
    in->image_yscale = mk_num(1.6);
    in->image_speed  = mk_num(0);
    in->depth        = mk_num(-5000);

    struct NV { const char* n; double v; };
    static const NV nums[] = {
        { "hp", 600 }, { "grid_row", -1 }, { "grid_col", -1 }, { "is_placed", 0 },
        { "timer", 0 }, { "attack_anim", 6 }, { "flash_speed", 6 }, { "flash_value", 0 },
        { "state", 0 },                     // CARD_STATE.IDLE
        { "depth_value", 0 }, { "depth_offset", 0 },
        { "can_shovel_remove", 0 }, { "invincible", 0 }, { "hurt_time", 0 },
        { "cycle", 78 }, { "attack_timer", 0 },
        { "awake_buff_timer", 0 }, { "pre_hp", -1 },
    };
    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) in->vars[nums[i].n] = mk_num(nums[i].v);
    in->vars["max_hp"] = in->vars["hp"];                     // max_hp = hp

    struct SV { const char* n; const char* v; };
    static const SV strs[] = {
        { "plant_type", "normal" }, { "feature_type", "normal" },
        { "target_card", "none" },  { "plant_id", "player" },
    };
    for (size_t i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
        in->vars[strs[i].n] = mk_str(vm, strs[i].v);

    // if card_equipped_attire_id("player") != -1 { sprite_index = get_attire_info(...).spr }
    {
        Value a[1]; a[0] = mk_str(vm, "player");
        double aid = num_of(&call_ret(vm, "card_equipped_attire_id", a, 1));
        if (aid != -1) {
            Value a2[1]; a2[0] = mk_num(aid);
            Value info = call_ret_held(vm, "get_attire_info", a2, 1);
            if (info.t == MEM_MAP) in->sprite_index = m_get(vm, info, "spr");   // 内置：直接落
            release(info);
        }
    }
    return true;
}


// Step_0.gml（97 行）
bool hot_player_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue(vm, "is_paused")) return true;

    // 动画计时器（只在已放置时走；这里的 7 是写死的帧边界，不是 idle_anim）
    if (truthy(inst_var(s, "is_placed"))) {
        if (get_num(s, "timer") < get_num(s, "flash_speed") - 1) {
            add_num(s, "timer", 1);
        } else {
            if (get_num(s, "state") == 0) {                       // CARD_STATE.IDLE
                if (get_num(s, "image_index") < 7) add_num(s, "image_index", 1);
                else set_num(s, "image_index", 0);
            } else if (get_num(s, "state") == 1) {                // CARD_STATE.ATTACK
                double ii = get_num(s, "image_index");
                if (ii >= 8 && ii <= 8 + get_num(s, "attack_anim")) add_num(s, "image_index", 1);
                else set_num(s, "image_index", 8);
            }
            set_num(s, "timer", 0);
        }
    }

    // 死了 → 游戏结束（跟敌人 Step 里那套一样）
    if (get_num(s, "hp") <= 0) {
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
                msg[0] = item; msg[1] = mk_num(10);               // MSG_GAME_OVER
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
    if (get_num(s, "flash_value") > 0) add_num(s, "flash_value", -10);

    // 深度：放置了才有真实深度；有平台实例时不改（平台自己管）
    if (truthy(inst_var(s, "is_placed"))) {
        bool have_platform = false;
        for (size_t k = 1; k < g_instances.size(); k++) {          // with obj_platform { have_platform = true; break }
            if (!g_instances[k].alive) continue;
            if (!inst_is((int)k, "obj_platform")) continue;
            have_platform = true;
            break;
        }
        if (!have_platform) {
            Value da[3];
            da[0] = mk_num(get_num(s, "grid_col")); da[1] = mk_num(get_num(s, "grid_row"));
            da[2] = mk_str(vm, "normal");
            set_num(s, "depth", num_of(&call_ret(vm, "calculate_plant_depth", da, 3)));
        }
    } else {
        set_num(s, "depth", -5000);
    }

    if (get_num(s, "hurt_time") > 0) add_num(s, "hurt_time", -1);
    if (get_num(s, "hurt_time") <= 0 && get_num(s, "hp") < get_num(s, "max_hp")) add_num(s, "hp", 1);

    // 右方有敌人吗（with 里 self 是敌人，other 是本角色）
    bool has_enemy = false;
    for (size_t k = 1; k < g_instances.size(); k++) {
        if (!g_instances[k].alive) continue;
        if (!inst_is((int)k, "obj_enemy_parent")) continue;
        int e = (int)k;
        if (get_num(e, "grid_row") == get_num(s, "grid_row")
                && get_num(e, "grid_col") >= get_num(s, "grid_col")
                && get_num(e, "grid_col") <= gnumv(vm, "grid_cols") + 1
                && get_num(e, "hp") > 0) {
            has_enemy = true;
            break;
        }
    }
    if (has_enemy) {
        if (get_num(s, "attack_timer") <= get_num(s, "cycle") - get_num(s, "attack_anim") * get_num(s, "flash_speed")) {
            add_num(s, "attack_timer", 1);
        } else if (get_num(s, "attack_timer") <= get_num(s, "cycle")) {
            add_num(s, "attack_timer", 1);
            set_num(s, "state", 1);                               // CARD_STATE.ATTACK
        } else {
            // event_user(1)（射子弹）在 GML 里是注释掉的
            set_num(s, "attack_timer", 0);
            set_num(s, "state", 0);
        }
    } else {
        set_num(s, "attack_timer", 0);
        set_num(s, "state", 0);
    }
    return true;
}


// Step_2.gml（14 行，End Step）：没放置就跟鼠标；掉血钩子
bool hot_player_step_end(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (!truthy(inst_var(s, "is_placed"))) {
        set_num(s, "x", gnumv(vm, "mouse_x"));
        set_num(s, "y", gnumv(vm, "mouse_y"));
    }
    if (get_num(s, "pre_hp") > get_num(s, "hp")) {
        global_var_set("_VM_last_damaged_player", mk_num(s));
        Value* buf = global_var("_VM_PLAYER_DAMAGED");
        if (buf) {
            Value b[1]; b[0] = *buf;
            Value bexists = call_ret(vm, "buffer_exists", b, 1);
            if (truthy(&bexists)) {
                Value a[3];
                a[0] = global_var("__vm") ? *global_var("__vm") : mk_num(0);
                a[1] = *buf;
                a[2] = mk_str(vm, "_VM_PLAYER_DAMAGED");
                call_fn_v(vm, "VM_Execute", a, 3, -1);
            }
        }
        Value h[1]; h[0] = mk_str(vm, "_VM_PLAYER_DAMAGED");
        call_fn_v(vm, "vm_hook_run", h, 1, -1);
    }
    set_num(s, "pre_hp", get_num(s, "hp"));
    return true;
}


// Draw_0.gml（21 行）
bool hot_player_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    if (!truthy(inst_var(s, "is_placed"))) {
        // 放置预览：can_place_at_position 还是空壳 → 现在恒不画（宿主补上就生效）
        Value ca[4];
        ca[0] = gv(vm, "mouse_x"); ca[1] = gv(vm, "mouse_y");
        ca[2] = mk_str(vm, "normal"); ca[3] = mk_str(vm, "amphi");
        bool can_plant = truthy(&call_ret(vm, "can_place_at_position", ca, 4));
        if (can_plant) {
            Value ga[2]; ga[0] = gv(vm, "mouse_x"); ga[1] = gv(vm, "mouse_y");
            Value gp = call_ret_held(vm, "get_grid_position_from_world", ga, 2);
            Value d[9];
            d[0] = *inst_var(s, "sprite_index");
            d[1] = mk_num(0);
            d[2] = m_get(vm, gp, "x"); d[3] = m_get(vm, gp, "y");
            d[4] = mk_num(1.6); d[5] = mk_num(1.6); d[6] = mk_num(0);
            d[7] = mk_num(16777215);                                  // c_white
            d[8] = mk_num(0.5);
            call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
        }
    }
    if (get_num(s, "flash_value") > 0) {
        call_fn(vm, "draw_self", NULL, 0, -1);
        Value sa[1]; sa[0] = iv_or(s, "hit_effect_2", -1);
        call_fn_v(vm, "shader_set", sa, 1, -1);
        Value d[9];
        d[0] = *inst_var(s, "sprite_index");
        d[1] = *inst_var(s, "image_index");
        d[2] = mk_num(get_num(s, "x")); d[3] = mk_num(get_num(s, "y"));
        d[4] = *inst_var(s, "image_xscale"); d[5] = *inst_var(s, "image_yscale");
        d[6] = *inst_var(s, "image_angle");
        d[7] = mk_num(16777215);
        d[8] = mk_num(get_num(s, "flash_value") / 200.0);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
        call_fn(vm, "shader_reset", NULL, 0, -1);
    } else {
        set_num(s, "image_blend", 16777215);                          // c_white
        call_fn(vm, "draw_self", NULL, 0, -1);
    }
    return true;
}


// Other_11（UserEvent 1）：射一发小龙包子弹（GML 里 Step 里那行 event_user(1) 是注释掉的，
//   所以现在没人调它，但事件本身要留着）
bool hot_player_user_event_1(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Value ca[4];
    ca[0] = mk_num(get_num(s, "x") + 40);
    ca[1] = mk_num(get_num(s, "y") - 75);
    ca[2] = mk_num(get_num(s, "depth") - 500);
    ca[3] = mk_str(vm, "obj_xiaolongbao_bullet");
    Value inst = call_ret(vm, "instance_create_depth", ca, 4);
    int bid = inst.t == MEM_INT ? inst.v : -1;
    if (inst_get(bid)) {
        set_num(bid, "damage", 10);
        set_num(bid, "move_speed", 8);
    }
    return true;
}


// Other_12（UserEvent 2）：受击表现
bool hot_player_user_event_2(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    set_num(s, "flash_value", 150);
    set_num(s, "hurt_time", 600);
    return true;
}


// Mouse_53（181 行）：放置角色 —— 摆格子、建武器/宝石/盾、联机交给服务端
//   ⚠ can_place_at_position / instance_position / get_weapon_info / get_gem_* 都是空壳，
//     所以现在 can_plant 恒为 false，整段不会真的执行；结构照 GML 抄齐
bool hot_player_mouse_53(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (truthy(inst_var(s, "is_placed"))) return true;

    double logical_x = gnumv(vm, "mouse_x");
    double logical_y = gnumv(vm, "mouse_y");
    double shift_x = 0, shift_y = 0;

    // 鼠标底下的平台：用它自己的视觉偏移把"逻辑坐标"还原
    Value pa[3];
    pa[0] = mk_num(logical_x); pa[1] = mk_num(logical_y); pa[2] = mk_str(vm, "obj_platform");
    Value plat = call_ret(vm, "instance_position", pa, 3);
    int pid = plat.t == MEM_INT ? plat.v : -1;
    if (inst_get(pid)) {
        shift_x = get_num(pid, "visual_x_shift");
        shift_y = get_num(pid, "visual_y_shift");
        logical_x -= shift_x;
        logical_y -= shift_y;
    }

    Value ca2[5];
    ca2[0] = mk_num(logical_x); ca2[1] = mk_num(logical_y);
    ca2[2] = mk_str(vm, "normal"); ca2[3] = mk_str(vm, "amphi"); ca2[4] = mk_str(vm, "none");
    if (!truthy(&call_ret(vm, "can_place_at_position", ca2, 5))) return true;   // 放不下 → 什么都不做

    set_num(s, "is_placed", 1);
    global_var_set("is_paused", mk_num(0));
    {
        Value ga[2]; ga[0] = mk_num(logical_x); ga[1] = mk_num(logical_y);
        Value gp = call_ret_held(vm, "get_grid_position_from_world", ga, 2);
        double gx = num_of(&m_get(vm, gp, "x")), gy = num_of(&m_get(vm, gp, "y"));
        set_num(s, "x", gx + shift_x);
        set_num(s, "y", gy + 10 + shift_y);
        set_num(s, "grid_row", num_of(&m_get(vm, gp, "row")));
        set_num(s, "grid_col", num_of(&m_get(vm, gp, "col")));
    }
    {   // card_created(id, grid_col, grid_row)
        Value a[3];
        a[0] = mk_num(s);
        a[1] = mk_num(get_num(s, "grid_col"));
        a[2] = mk_num(get_num(s, "grid_row"));
        call_fn_v(vm, "card_created", a, 3, -1);
    }

    Value net = gv(vm, "network");
    Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
    if (net.t == MEM_MAP) mode = v_struct(vm, net, "mode");

    // 联机：客户端只做本地预测（角色本体 + 宝石），本体随后销毁，等服务器广播回来重建
    if (str_is(vm, &mode, "client")) {
        static const char* kMain[] = { "equipped_items", "main_weapon" };
        Value save = gv(vm, "save_data");
        Value mw_id = m_chain(vm, save, kMain, 2);
        mw_id = m_get(vm, mw_id, "id");
        if (!pool_text(vm.strings, &mw_id).empty()) {
            Value mw = m_chain(vm, save, kMain, 2);
            make_weapon_gems(vm, s, m_get(vm, mw, "gems"), 0);
        }
        call_fn(vm, "instance_destroy", NULL, 0, -1);               // instance_destroy(id)
        return true;                                                // exit
    }

    {   // 放置音效 + 特效
        Value sa[3]; sa[0] = mk_str(vm, "snd_place1"); sa[1] = mk_num(0); sa[2] = mk_num(0);
        call_fn_v(vm, "audio_play_sound", sa, 3, -1);
        Value ea[4];
        ea[0] = mk_num(get_num(s, "x")); ea[1] = mk_num(get_num(s, "y"));
        ea[2] = mk_num(-2); ea[3] = mk_str(vm, "obj_place_effect");
        call_fn_v(vm, "instance_create_depth", ea, 4, -1);
    }
    // 水面 → 先垫一块木板
    {
        Value terrains = gv(vm, "grid_terrains");
        Value rowv[2]; rowv[0] = terrains; rowv[1] = mk_num(get_num(s, "grid_row"));
        Value rowarr = call_ret(vm, "array_get", rowv, 2);
        Value colv[2]; colv[0] = rowarr; colv[1] = mk_num(get_num(s, "grid_col"));
        Value cell = call_ret(vm, "array_get", colv, 2);
        Value ctype = (cell.t == MEM_MAP) ? m_get(vm, cell, "type") : Value{MEM_UNDEFINED, 0};
        if (str_is(vm, &ctype, "water")) {
            Value wa[4];
            wa[0] = mk_num(get_num(s, "x")); wa[1] = mk_num(get_num(s, "y") - 10);
            wa[2] = mk_num(get_num(s, "depth") + 1); wa[3] = mk_str(vm, "obj_wooden_plate");
            Value wood = call_ret(vm, "instance_create_depth", wa, 4);
            int wid2 = wood.t == MEM_INT ? wood.v : -1;
            if (inst_get(wid2)) {
                Value a[3];
                a[0] = mk_num(wid2);
                a[1] = mk_num(get_num(s, "grid_col"));
                a[2] = mk_num(get_num(s, "grid_row"));
                call_fn_v(vm, "card_created", a, 3, -1);
            }
        }
    }

    Value save = gv(vm, "save_data");
    double gem_index = 0;

    // ── 主武器 ──
    static const char* kMain[] = { "equipped_items", "main_weapon" };
    {
        Value mw = m_chain(vm, save, kMain, 2);
        Value mw_id = m_get(vm, mw, "id");
        std::string mid = pool_text(vm.strings, &mw_id);
        if (!mid.empty() && !gtrue(vm, "_VM_ban_weapon")) {
            Value ga[1]; ga[0] = mw_id;
            Value info = call_ret_held(vm, "get_weapon_info", ga, 1);
            global_var_set("_mod_pending_weapon_id", mw_id);
            Value ca[4];
            ca[0] = mk_num(get_num(s, "x") - 10); ca[1] = mk_num(get_num(s, "y") - 100);
            ca[2] = mk_num(get_num(s, "depth") - 1);
            ca[3] = m_get(vm, info, "obj");
            Value wi = call_ret(vm, "instance_create_depth", ca, 4);
            global_var_set("_mod_pending_weapon_id", mk_str(vm, ""));
            int wid2 = wi.t == MEM_INT ? wi.v : -1;
            if (inst_get(wid2)) {
                inst_var_set(wid2, "parent_player", mk_num(s));
                if (mod_vm(mid)) {                                   // mod_weapon_vms 里有它 → 补跑一次步
                    inst_var_set(wid2, "weapon_id", mw_id);
                    inst_event(vm, wid2, "_OBJECT_STEP");
                }
                inst_var_set(wid2, "grid_row", *inst_var(s, "grid_row"));
                inst_var_set(wid2, "grid_col", *inst_var(s, "grid_col"));
            }
            set_num(s, "cycle", num_of(&m_get(vm, info, "cycle")));
            gem_index = make_weapon_gems(vm, s, m_get(vm, mw, "gems"), gem_index);
        }
    }

    // ── 副武器（盾）──
    static const char* kSec[] = { "equipped_items", "secondary_weapon" };
    {
        Value sw = m_chain(vm, save, kSec, 2);
        Value sw_id = m_get(vm, sw, "id");
        std::string sid = pool_text(vm.strings, &sw_id);
        if (!sid.empty() && !gtrue(vm, "_VM_ban_shield")) {
            Value sa[4];
            sa[0] = mk_num(get_num(s, "x")); sa[1] = mk_num(get_num(s, "y"));
            sa[2] = mk_num(get_num(s, "depth")); sa[3] = mk_str(vm, "obj_player_shield");
            Value sInst = call_ret(vm, "instance_create_depth", sa, 4);
            int shid = sInst.t == MEM_INT ? sInst.v : -1;
            if (inst_get(shid)) {
                inst_var_set(shid, "parent_player", mk_num(s));
                inst_var_set(shid, "grid_row", *inst_var(s, "grid_row"));
                inst_var_set(shid, "grid_col", *inst_var(s, "grid_col"));
            }
            Value ga[1]; ga[0] = sw_id;
            Value info = call_ret_held(vm, "get_weapon_info", ga, 1);
            // hp_increase 是条件字段（JSON 里不写就没有），不能无条件读
            if (m_get(vm, info, "hp_increase").t != MEM_UNDEFINED) {
                double inc = num_of(&m_get(vm, info, "hp_increase"));
                add_num(s, "hp", inc);
                add_num(s, "max_hp", inc);
            }
            Value hg[1]; hg[0] = mk_str(vm, "health_gem");
            if (num_of(&call_ret(vm, "get_gem_index", hg, 1)) != -1) {
                Value hgi = call_ret_held(vm, "get_gem_info", hg, 1);
                Value hgl = call_ret(vm, "get_gem_level", hg, 1);
                double inc = num_of(&m_get(vm, hgi, "hp_increase")) * (num_of(&hgl) + 1);
                add_num(s, "hp", inc);
                add_num(s, "max_hp", inc);
            }
            // 副武器再挂一个 obj_weapon_mod：只有真加载了 mod 盾 .bin 才挂
            if (mod_vm(sid)) {
                global_var_set("_mod_pending_weapon_id", sw_id);
                Value wa[4];
                wa[0] = mk_num(get_num(s, "x") - 10); wa[1] = mk_num(get_num(s, "y") - 100);
                wa[2] = mk_num(get_num(s, "depth") - 1);
                wa[3] = mk_str(vm, "obj_weapon_mod");
                Value ws = call_ret(vm, "instance_create_depth", wa, 4);
                global_var_set("_mod_pending_weapon_id", mk_str(vm, ""));
                int wsid = ws.t == MEM_INT ? ws.v : -1;
                if (inst_get(wsid)) {
                    inst_var_set(wsid, "parent_player", mk_num(s));
                    inst_var_set(wsid, "weapon_id", sw_id);
                    inst_event(vm, wsid, "_OBJECT_STEP");
                    inst_var_set(wsid, "grid_row", *inst_var(s, "grid_row"));
                    inst_var_set(wsid, "grid_col", *inst_var(s, "grid_col"));
                }
            }
            gem_index = make_weapon_gems(vm, s, m_get(vm, sw, "gems"), gem_index);
        }
    }

    // ── 超级武器 ──
    static const char* kSup[] = { "equipped_items", "super_weapon" };
    {
        Value uw = m_chain(vm, save, kSup, 2);
        Value uw_id = m_get(vm, uw, "id");
        std::string uid = pool_text(vm.strings, &uw_id);
        if (!uid.empty() && !gtrue(vm, "_VM_ban_super_weapon")) {
            Value ga[1]; ga[0] = uw_id;
            Value info = call_ret_held(vm, "get_weapon_info", ga, 1);
            global_var_set("_mod_pending_weapon_id", uw_id);
            Value ca[4];
            ca[0] = mk_num(get_num(s, "x") - 10); ca[1] = mk_num(get_num(s, "y") - 100);
            ca[2] = mk_num(get_num(s, "depth") - 1);
            ca[3] = m_get(vm, info, "obj");
            Value wi = call_ret(vm, "instance_create_depth", ca, 4);
            global_var_set("_mod_pending_weapon_id", mk_str(vm, ""));
            int wid2 = wi.t == MEM_INT ? wi.v : -1;
            if (inst_get(wid2)) {
                inst_var_set(wid2, "parent_player", mk_num(s));
                if (mod_vm(uid)) {
                    inst_var_set(wid2, "weapon_id", uw_id);
                    inst_event(vm, wid2, "_OBJECT_STEP");
                }
                inst_var_set(wid2, "grid_row", *inst_var(s, "grid_row"));
                inst_var_set(wid2, "grid_col", *inst_var(s, "grid_col"));
            }
            gem_index = make_weapon_gems(vm, s, m_get(vm, uw, "gems"), gem_index);
        }
    }
    return true;
}
