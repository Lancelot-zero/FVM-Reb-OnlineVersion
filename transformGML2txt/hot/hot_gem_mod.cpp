// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"

// ============================================================
// obj_gem_mod —— 通用 mod 宝石（武器上镶嵌的宝石，7 个事件）
//   宝石 id 由放置逻辑在创建前写进 global._mod_pending_gem_id（和卡片/敌人同一套），
//   效果逻辑全在它自己的 .bin 里；本文件是那几个事件的 C++ 硬编码版。
//   "这个实例属于哪个 mod 的 VM" = 实例上的 gem_id 查注册表；平台那边是
//   global.mod_gem_vms，这边就是 g_mod_vms（mod_vm_of_inst_sid 查的就是它，
//   所以 GML 里 variable_global_exists("mod_gem_vms") && ds_map_exists(...) 那两句
//   在这里退化成"查得到 VM 就继续"）。
//   ⚠ obj_gem_mod 没有父对象（obj_gem_mod.yy 里 parentObjectId = null）：
//     Create/Step/Draw/Destroy 本来就没调 event_inherited()；
//     Mouse_4/10/11 调了，但 hot_parent_of 不认得 obj_gem_mod → 返回 NULL，是空操作。
//   常量：CARD_STATE.IDLE=0 / ATTACK=1、noone=-4、c_white=16777215、c_black=0、
//         c_yellow=16776960、fa_left=fa_top=0、fa_center=fa_middle=1、
//         HIT_NORMAL=1 HIT_AIR=2 HIT_DANCE=4 HIT_OBSTACLE=8 HIT_DIVER=16 HIT_UNDERGROUND=32、
//         ds_type_list=0、make_color_rgb(50,50,80) = 50 | 50<<8 | 80<<16（就是 #323250）
//
// TODO(宿主)：
//   · GML 在跑 mod 块前后还切了 global.__vm = _vm（块内 VM_Execute 用）。
//     这里和 hot_card_mod.cpp / hot_enemy_mod.cpp 一样只切了 mv->cur_instance
//     （mod_run_block 干的），__vm 没动 —— 等宿主给 mod_run_block 补上。
//   · get_gem_info / get_gem_level 还是空壳（见 hot_player_character.cpp 里的注释），
//     所以 gem_info / gem_level / cooldown 现在拿不到真值，结构照 GML 抄齐。
//   · bbox_*（宿主补的包围盒）、point_in_rectangle、string_width_ext / string_height_ext
//     也还是空壳 → 悬停判定恒 false、说明框尺寸恒 0。宿主补上后本文件自动生效。
// ============================================================


// Create_0.gml（86 行）
//   只写初始值和归属字段；登记实例 + 跑 .bin 的 _OBJECT_CREATE 挪到 Step 顶部
//   （Create 是在 instance_create_depth 里跑的，那时 mod_point_* 还没写）
bool hot_gemmod_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    // gem_id = variable_global_exists("_mod_pending_gem_id") ? global._mod_pending_gem_id : ""
    if (global_exists_sid(S__mod_pending_gem_id)) inst_var_set_sid(s, S_gem_id, gv_sid(S__mod_pending_gem_id));
    else                                          inst_var_set_sid(s, S_gem_id, mk_str(vm, ""));
    std::string gid = pool_text(inst_var_sid(s, S_gem_id));

    // gem_info = get_gem_info(gem_id)   ← 结构体（容器）：存实例变量要 retain → set_var_sid
    {
        Value a[1]; a[0] = *inst_var_sid(s, S_gem_id);
        set_var_sid(s, S_gem_info, call_ret_sid(vm, S_get_gem_info, a, 1));
    }

    // var _g_lv = (gem_id != "") ? get_gem_level(gem_id) : 0
    double g_lv = 0;
    if (!gid.empty()) {
        Value a[1]; a[0] = *inst_var_sid(s, S_gem_id);
        g_lv = num_of(&call_ret_sid(vm, S_get_gem_level, a, 1));
    }
    // 等级：默认取本机存档；联机远端创建时由广播带来写在 _mod_pending_gem_level（>=0 时优先）
    if (global_exists_sid(S__mod_pending_gem_level) && gnumv_sid(S__mod_pending_gem_level) >= 0)
        g_lv = gnumv_sid(S__mod_pending_gem_level);

    // ── 数值型初始值（顺序照 GML；内置字段不能落 vars，见下面单列的两个）──
    static const struct { int n;              /* 池下标 */ double v; } init[] = {
        { S_cooldown, 60 },
        { S_parent_player, -4 },                                    // noone
        // 发射方信息（插件宝石脚本靠这几个定位；创建宝石的地方会覆盖）
        { S_mod_point_parent_player, -4 },                          // noone
        { S_mod_point_grid_row, 0 }, { S_mod_point_grid_col, 0 }, { S_mod_point_gem_level, 0 },
        // 预留显示属性：插件在 .bin 里用 VM_SetProp 改
        { S_on_click, 0 }, { S_clickable, 0 }, { S_clicked, 0 },
        { S_cooldown_timer, 0 }, { S_gem_level, 0 },
        // 动画配置（与 obj_card_parent 同款）
        { S_timer, 0 }, { S_flash_speed, 5 }, { S_idle_anim, 0 }, { S_attack_anim, 0 },
        { S_state, 0 },                                             // CARD_STATE.IDLE
        // 进 VM 的入口条件 + 盯梢 + 区域检测（宝石没有 hp、不移动，所以没有 hp_change/索敌那套）
        { S_mod_step_enter_var, 0 },
        { S_mod_has_enemy, 0 }, { S_mod_has_card, 0 }, { S_mod_tick_cool, 0 },
        // 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
        { S_mod_countdown, 0 }, { S_mod_alpha_add, 0 },
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i++)
        in->vars[init[i].n] = mk_num(init[i].v);

    inst_f(*in, F_image_speed) = mk_num(0);                          // image_speed = 0（内置字段）

    // 字符串：直接落值（不是容器，不用 retain）
    inst_var_set_sid(s, S_tooltip_text, mk_str(vm, ""));
    inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, "")); // "" / "mod" / "wait" / "cool" / ...
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));         // 【只读】这一帧第一个变化的盯梢属性名

    // 数组：要 retain → set_var_sid（和 obj_card_mod / obj_enemy_mod 一个写法）
    set_var_sid(s, S_mod_watch_last,       call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_watch_changed,    call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_step_enemy_area,  call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_enemy_types,      call_ret_sid(vm, S_array_literal, NULL, 0));
    set_var_sid(s, S_mod_step_card_area,   call_ret_sid(vm, S_array_literal, NULL, 0));

    // gem_level = (gem_id != "") ? _g_lv : 0
    if (!gid.empty()) set_num_sid(s, S_gem_level, g_lv);

    // if (is_struct(gem_info)) { 图标 / 冷却取自宝石数据 }
    Value gem_info = *inst_var_sid(s, S_gem_info);
    if (gem_info.t == MEM_MAP) {                                    // 我们这边结构体 = MEM_MAP
        Value icon = m_get_sid(vm, gem_info, S_icon);
        if (icon.t != MEM_UNDEFINED && num_of(&icon) != -4)          // !is_undefined(...) && != noone
            inst_var_set_sid(s, S_sprite_index, icon);               // 内置字段：直接落
        // 冷却取自宝石数据（原版宝石都是 cooldown 数组，按等级取）
        Value cd = m_get_sid(vm, gem_info, S_cooldown);
        if (cd.t == MEM_ARRAY) {
            int cn = container_size(cd);
            double idx = (g_lv < (double)(cn - 1)) ? g_lv : (double)(cn - 1);   // min(_lv, array_length(...) - 1)
            inst_var_set_sid(s, S_cooldown, array_v(vm, cd, (int)idx));
        } else {
            Value fc = m_get_sid(vm, gem_info, S_first_cooldown);
            if (fc.t != MEM_UNDEFINED) inst_var_set_sid(s, S_cooldown, fc);
        }
    }

    set_num_sid(s, S__mod_initialized, 0);                           // _mod_initialized = false
    return true;
}


// Step 里的区域索敌（跟 hot_card_mod.cpp 的 cardmod_enemy_search 同一口径，只是锚点由调用方给）
//   区域 = mod_step_enemy_area 每 4 个值一组 [上,下,左,右]；类型 = mod_enemy_types（空 = 任意）
//   实现靠 global.has_enemy_* 的每行前缀和（obj_battle 每帧重建）
static void gemmod_enemy_area(Vm& vm, int s, Vm* mv, double arow, double acol) {
    set_num_sid(s, S_mod_has_enemy, 0);
    if (!global_exists_sid(S_has_enemy_normal) || arow < 0 || acol < 0) return;

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
        // 一个都没认出来（写错了）→ 按"任意类型"兜底，别把宝石永远卡死
        if (!w_n && !w_o && !w_d && !w_a && !w_c && !w_u) w_n = w_o = w_d = w_a = w_c = w_u = true;
    }

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

        double r1 = arow - up;    if (r1 < 0) r1 = 0;
        double r2 = arow + down;  if (r2 > rows_m1) r2 = rows_m1;
        double c1raw = acol - left;
        double c1 = c1raw;        if (c1 < 0) c1 = 0;
        double c2 = acol + right; if (c2 > cols_p1) c2 = cols_p1;
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


// Step 里的区域找卡片（玩家底座 / 空位不算）：全局网格 global.grid_plants 里每格一个 ds_list
static void gemmod_card_area(Vm& vm, int s, Vm* mv, double arow, double acol) {
    (void)mv;
    set_num_sid(s, S_mod_has_card, 0);
    if (!global_exists_sid(S_grid_plants)) return;

    Value carea = *inst_var_sid(s, S_mod_step_card_area);
    int cgroups = 0;
    if (carea.t == MEM_ARRAY) cgroups = (int)std::floor(container_size(carea) / 4.0);
    if (cgroups < 1) {                                              // 空 → 默认 [1,1,0,99]
        carea = call_ret_sid(vm, S_array_literal, NULL, 0);
        double dflt[4] = { 1, 1, 0, 99 };
        for (int i = 0; i < 4; i++) array_setn(vm, carea, i, dflt[i]);
        cgroups = 1;
    }

    Value gp = gv_sid(S_grid_plants);
    double rows_m1 = gnumv_sid(S_grid_rows) - 1;
    double cols_m1 = gnumv_sid(S_grid_cols) - 1;                     // 卡片区域右界到 grid_cols-1（敌人那边是 +1）

    for (int cg = 0; cg < cgroups && get_num_sid(s, S_mod_has_card) == 0; cg++) {
        int ci = cg * 4;
        double cr1 = arow - array_n(vm, carea, ci);     if (cr1 < 0) cr1 = 0;
        double cr2 = arow + array_n(vm, carea, ci + 1); if (cr2 > rows_m1) cr2 = rows_m1;
        double cc1 = acol - array_n(vm, carea, ci + 2); if (cc1 < 0) cc1 = 0;
        double cc2 = acol + array_n(vm, carea, ci + 3); if (cc2 > cols_m1) cc2 = cols_m1;

        for (double cr = cr1; cr <= cr2 && get_num_sid(s, S_mod_has_card) == 0; cr++) {
            for (double ccx = cc1; ccx <= cc2; ccx++) {
                Value lst = v_grid(vm, gp, mk_num(ccx), mk_num(cr));       // ds_grid_get(global.grid_plants, _ccx, _cr)
                if (!is_numv(&lst)) continue;                               // !is_real(_lst)
                {
                    Value da[2]; da[0] = lst; da[1] = mk_num(0);            // ds_type_list = 0
                    if (!truthy(&call_ret_sid(vm, S_ds_exists, da, 2))) continue;
                }
                int lnum = container_size(lst);                             // ds_list_size(_lst)
                for (int ck = 0; ck < lnum; ck++) {
                    int cd = (int)num_of(&container_at(lst, ck));           // ds_list_find_value(_lst, _ck)
                    if (!inst_get(cd)) continue;
                    Value ea[2]; ea[0] = mk_num(cd); ea[1] = mk_str_sid(S_plant_id);
                    if (!truthy(&call_ret_sid(vm, S_variable_instance_exists, ea, 2))) continue;
                    std::string pid = pool_text(inst_var_sid(cd, S_plant_id));
                    if (pid.empty() || pid == str_at(S_player)) continue;   // 空位 / 玩家底座不算
                    set_num_sid(s, S_mod_has_card, 1);
                    break;
                }
                if (get_num_sid(s, S_mod_has_card) == 1) break;
            }
        }
    }
}


// Step_0.gml（297 行）
//   基础 tick → 动画 → 盯梢 / 区域检测 / 闸门 → 跑该宝石 .bin 的步进块
bool hot_gemmod_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;                         // if global.is_paused { exit }

    // ── 登记实例 + 跑该宝石 .bin 的 _OBJECT_CREATE（原本在 Create 里，挪到这儿）──
    //   创建宝石的地方是「建实例 → 写 mod_point_* → 调一次步」，所以归属字段这时才齐；
    //   GML 那边的 variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)
    //   在这里就是"查得到这个 id 的 VM"。
    Value* gidp = inst_var_sid(s, S_gem_id);
    std::string gid = pool_text(gidp);
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_gem_id);
    if (!truthy(inst_var_sid(s, S__mod_initialized)) && !gid.empty() && mv) {
        inst_var_set_sid(s, S__mod_initialized, mk_num(1));
        mod_inst_add_v(mv, s, true);                                 // mod_inst_add(_vm, id)
        // if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE"))：块不存在时 mod_run_block 自己返回 false
        Value bak_last = gv_sid(S__VM_last_created_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_created_card, mk_num(s));
        global_var_set_sid(S__VM_cur_card, mk_num(s));
        mod_run_block_sid(mv, s, S__OBJECT_CREATE, NULL);
        global_var_set_sid(S__VM_last_created_card, bak_last);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        if (!inst_get(s)) return true;                               // 块里把自己销毁了
    }

    // ── 基础属性：倒计时 + 每帧透明度变化（mod_base_tick(id)）──
    mod_base_tick_v(s);

    // 预留属性 cooldown_timer：每帧自减（点击时由 Mouse_4 设回 cooldown）
    if (get_num_sid(s, S_cooldown_timer) > 0) add_num_sid(s, S_cooldown_timer, -1);

    // ── 动画自动播放（与 obj_card_parent 同款）──
    if (get_num_sid(s, S_timer) < get_num_sid(s, S_flash_speed) - 1) {
        add_num_sid(s, S_timer, 1);
    } else {
        double st = get_num_sid(s, S_state);
        if (st == 0) {                                               // CARD_STATE.IDLE
            if (get_num_sid(s, S_image_index) < get_num_sid(s, S_idle_anim)) add_num_sid(s, S_image_index, 1);
            else set_num_sid(s, S_image_index, 0);
        } else if (st == 1) {                                        // CARD_STATE.ATTACK
            double idle = get_num_sid(s, S_idle_anim), atk = get_num_sid(s, S_attack_anim);
            double ii = get_num_sid(s, S_image_index);
            if (ii >= idle + 1 && ii <= idle + atk) add_num_sid(s, S_image_index, 1);
            else set_num_sid(s, S_image_index, idle + 1);
        }
        set_num_sid(s, S_timer, 0);
    }

    // ── 进 VM 的入口闸门 ──
    bool enter = true;
    const Value* condp = inst_var_sid(s, S_mod_step_enter_condition);   // _cond = mod_step_enter_condition

    // ── 盯梢 mod_watch：声明的属性里哪个值变了，就记进 mod_watch_changed ──
    inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
    Value watch_changed = *inst_var_sid(s, S_mod_watch_changed);
    { Value a[2]; a[0] = watch_changed; a[1] = mk_num(0);
      call_fn_sid(vm, S_array_resize, a, 2, -1); }                   // array_resize(mod_watch_changed, 0)
    if (!gid.empty() && mv) {
        std::unordered_map<int, std::vector<Value> >::iterator wit = mv->named_arrays.find(S_mod_watch);
        if (wit != mv->named_arrays.end()) {
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
                    else if (global_exists(nm)) cur = gv(vm, nm.c_str());      // 名字不在实例上 → 当全局读
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
                        call_fn_sid(vm, S_array_push, pa, 2, -1);          // 每个变化的属性各进一次 Step
                        array_setv(vm, last, i, cur);
                    }
                }
            }
        }
    }

    double cdv = 0;
    bool is_enemy_area = str_is_any_sid(vm, mv, condp, S_enemy_area)
                      || str_is_any_sid(vm, mv, condp, S_enemy_area_mod);
    bool is_card_area  = str_is_any_sid(vm, mv, condp, S_card_area)
                      || str_is_any_sid(vm, mv, condp, S_card_area_mod);

    // ── 区域检测（只有 "enemy_area*" / "card_area*" 会做，其它条件零开销）──
    if (is_enemy_area || is_card_area) {
        // 锚点格子：优先跟着"放置它的角色"走（mod_point_parent_player），
        //           角色不在了就用创建时记下的 mod_point_grid_row / col
        double arow = get_num_sid(s, S_mod_point_grid_row);
        double acol = get_num_sid(s, S_mod_point_grid_col);
        double pp = get_num_sid(s, S_mod_point_parent_player);
        if (pp != -4 && inst_get((int)pp)) {                         // != noone && instance_exists(...)
            arow = get_num_sid((int)pp, S_grid_row);
            acol = get_num_sid((int)pp, S_grid_col);
        }
        if (is_enemy_area) gemmod_enemy_area(vm, s, mv, arow, acol);
        if (is_card_area)  gemmod_card_area(vm, s, mv, arow, acol);
    }

    // ── 闸门 ──
    Value* sevp = inst_var_sid(s, S_mod_step_enter_var);
    if (str_is_any_sid(vm, mv, condp, S_mod)) {                      // 按全场帧号对齐
        if (is_numv(sevp) && num_of(sevp) > 0 && inst_count_sid(S_obj_battle) > 0) {
            int bat = inst_at_sid(S_obj_battle, 0);
            enter = (std::fmod(get_num_sid(bat, S_battle_time), num_of(sevp)) == 0);
        }
    } else if (str_is_any_sid(vm, mv, condp, S_wait)) {              // 倒数
        if (is_numv(sevp) && num_of(sevp) > 0) {
            set_num_sid(s, S_mod_step_enter_var, num_of(sevp) - 1);
            enter = (num_of(sevp) - 1 <= 0);
        } else {
            inst_var_set_sid(s, S_mod_step_enter_condition, mk_str(vm, ""));    // 不合法：进一次 + 清空
        }
    } else if (str_is_any_sid(vm, mv, condp, S_cool)) {              // 冷却结束才进（主动宝石用这个）
        enter = (get_num_sid(s, S_cooldown_timer) <= 0);
    } else if (is_enemy_area) {
        // 区域内有敌人就进；"_mod" 版进一次后要等 mod_step_enter_var 帧才能再进
        if (str_is_any_sid(vm, mv, condp, S_enemy_area)) {
            enter = (get_num_sid(s, S_mod_has_enemy) == 1);
        } else {
            cdv = num_of(sevp);
            if (!is_numv(sevp) || cdv < 0) cdv = 0;
            if (get_num_sid(s, S_mod_tick_cool) > 0) {
                add_num_sid(s, S_mod_tick_cool, -1);
                enter = false;
            } else if (get_num_sid(s, S_mod_has_enemy) == 1) {
                enter = true;
                set_num_sid(s, S_mod_tick_cool, cdv);
            } else {
                enter = false;
            }
        }
    } else if (is_card_area) {
        // 区域内有卡片就进；"_mod" 版同上带冷却
        if (str_is_any_sid(vm, mv, condp, S_card_area)) {
            enter = (get_num_sid(s, S_mod_has_card) == 1);
        } else {
            cdv = num_of(sevp);
            if (!is_numv(sevp) || cdv < 0) cdv = 0;
            if (get_num_sid(s, S_mod_tick_cool) > 0) {
                add_num_sid(s, S_mod_tick_cool, -1);
                enter = false;
            } else if (get_num_sid(s, S_mod_has_card) == 1) {
                enter = true;
                set_num_sid(s, S_mod_tick_cool, cdv);
            } else {
                enter = false;
            }
        }
    }

    // ── 这一帧进几次：盯梢每个变化的属性各 1 次，闸门放行再 1 次 ──
    int run_total = container_size(watch_changed) + (enter ? 1 : 0);
    for (int k = 0; k < run_total; k++) {
        if (!inst_get(s)) break;
        if (k < container_size(watch_changed)) {
            Value nm = array_v(vm, watch_changed, k);
            inst_var_set_sid(s, S_mod_changed_prop, nm);
        } else {
            inst_var_set_sid(s, S_mod_changed_prop, mk_str(vm, ""));
        }
        if (!gid.empty() && mv) {
            Value bak_cur = gv_sid(S__VM_cur_card);
            global_var_set_sid(S__VM_cur_card, mk_num(s));
            mod_run_block_sid(mv, s, S__OBJECT_STEP, NULL);
            global_var_set_sid(S__VM_cur_card, bak_cur);
        }
    }
    return true;
}


// Draw_0.gml（62 行）：宝石绘制块（没有就 draw_self）→ 冷却遮罩 → 说明框 → 等级星星
bool hot_gemmod_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    // 该宝石虚拟机里的绘制块；没有则默认画 sprite_index
    bool drawn = false;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_gem_id);
    if (mv) drawn = mod_run_block_sid(mv, s, S__OBJECT_DRAW, str_at(S__VM_cur_card).c_str());
    if (!drawn) call_fn_int_sid(vm, S_draw_self, NULL, 0, -1);       // if (!_drawn) draw_self()

    // 冷却遮罩 / 悬停判定用贴图的实际可见外框
    //   ⚠ bbox_* 是宿主补的（现在拿不到就是 0，遮罩/悬停会退化）
    double bl = get_num_sid(s, S_bbox_left);
    double bt = get_num_sid(s, S_bbox_top);
    double br = get_num_sid(s, S_bbox_right);
    double bb = get_num_sid(s, S_bbox_bottom);

    // 预留属性 cooldown_timer：冷却遮罩 + 剩余秒数（照原版宝石）
    if (get_num_sid(s, S_cooldown_timer) > 0) {
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }   // fa_middle = 1
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }   // fa_center = 1
        { Value a[1]; a[0] = mk_str_sid(S_font_hei); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0.5); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_colour, a, 1, -1); }   // c_black = 0
        { Value r[5];
          r[0] = mk_num(bl); r[1] = mk_num(bt); r[2] = mk_num(br - 3); r[3] = mk_num(bb - 3);
          r[4] = mk_num(0); call_fn_sid(vm, S_draw_roundrect, r, 5, -1); }
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(16777215); call_fn_sid(vm, S_draw_set_colour, a, 1, -1); }  // c_white
        {   // string(floor(cooldown_timer / 60))
            Value a[1]; a[0] = mk_num(std::floor(get_num_sid(s, S_cooldown_timer) / 60.0));
            Value txt = call_ret_sid(vm, S_string, a, 1);
            Value t[3]; t[0] = mk_num(get_num_sid(s, S_x)); t[1] = mk_num(get_num_sid(s, S_y));
            t[2] = txt; call_fn_sid(vm, S_draw_text, t, 3, -1);
        }
    }

    // 说明框：鼠标悬停，或插件把 on_click 置 true
    Value gem_info = iv_or_sid(s, S_gem_info, 0);
    bool hovered = false;
    if (gem_info.t == MEM_MAP) {                                     // is_struct(gem_info)
        Value r[6];                                                  // point_in_rectangle(mx,my,bl,bt,br,bb)
        r[0] = gv_sid(S_mouse_x); r[1] = gv_sid(S_mouse_y);
        r[2] = mk_num(bl); r[3] = mk_num(bt); r[4] = mk_num(br); r[5] = mk_num(bb);
        hovered = truthy(&call_ret_sid(vm, S_point_in_rectangle, r, 6));
    }
    if (gem_info.t == MEM_MAP && (truthy(inst_var_sid(s, S_on_click)) || hovered)) {
        Value da[1]; da[0] = m_get_sid(vm, gem_info, S_description);
        std::string desc = pool_text(&call_ret_sid(vm, S_string, da, 1));    // string(gem_info.description)
        if (get_num_sid(s, S_cooldown_timer) > 0) desc += "\n正在冷却中";
        if (!pool_text(inst_var_sid(s, S_tooltip_text)).empty())
            desc += "\n" + pool_text(inst_var_sid(s, S_tooltip_text));       // 插件追加的自定义文本
        Value display = mk_str(vm, desc.c_str());

        { Value a[1]; a[0] = mk_str_sid(S_font_pixel); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }   // fa_left = 0
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }   // fa_top = 0

        Value wa[3]; wa[0] = display; wa[1] = mk_num(0.65); wa[2] = mk_num(180);
        double tw = num_of(&call_ret_sid(vm, S_string_width_ext, wa, 3));
        double th = num_of(&call_ret_sid(vm, S_string_height_ext, wa, 3));
        // ⚠ string_width_ext / string_height_ext 要字体系统，宿主没给就恒 0（框会退化成一条线）
        double bx = get_num_sid(s, S_x) + 40;
        double by = get_num_sid(s, S_y) - (th + 27) / 2;

        Value bg; bg.t = MEM_INT; bg.v = 50 | (50 << 8) | (80 << 16);     // draw_set_color(#323250)
        { Value a[1]; a[0] = bg; call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(bx); r[1] = mk_num(by);
          r[2] = mk_num(bx + tw + 20); r[3] = mk_num(by + th + 27);
          r[4] = mk_num(1); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        Value mc[3]; mc[0] = mk_num(16776960); mc[1] = mk_num(16777215); mc[2] = mk_num(0.3);
        { Value a[1]; a[0] = call_ret_sid(vm, S_merge_color, mc, 3);      // merge_color(c_yellow, c_white, 0.3)
          call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(bx); r[1] = mk_num(by);
          r[2] = mk_num(bx + tw + 20); r[3] = mk_num(by + th + 27);
          r[4] = mk_num(0); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }    // c_black
        { Value t[3]; t[0] = mk_num(bx + 10); t[1] = mk_num(by + 5); t[2] = display;
          call_fn_sid(vm, S_draw_text, t, 3, -1); }

        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
    }

    // 预留属性 gem_level：等级星星
    if (get_num_sid(s, S_gem_level) > 0) {
        Value d[9];
        d[0] = mk_str_sid(S_spr_star_slot); d[1] = mk_num(get_num_sid(s, S_gem_level) - 1);
        d[2] = mk_num(get_num_sid(s, S_x) - 23); d[3] = mk_num(get_num_sid(s, S_y) - 23);
        d[4] = mk_num(0.75); d[5] = mk_num(0.75); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
    }
    return true;
}


// Destroy_0.gml（19 行）：跑该宝石的销毁块 + 把实例移出它的实例容器
bool hot_gemmod_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Vm* mv = mod_vm_of_inst_sid(vm, s, S_gem_id);
    if (mv) {
        Value bak_last = gv_sid(S__VM_last_destroyed_card);
        Value bak_cur  = gv_sid(S__VM_cur_card);
        global_var_set_sid(S__VM_last_destroyed_card, mk_num(s));   // 块内 VM_GetLastDestroyedCard() 取到本实例
        global_var_set_sid(S__VM_cur_card, mk_num(s));              // 块内 VM_GetCurCard() 取到本实例
        mod_run_block_sid(mv, s, S__OBJECT_DESTROY, NULL);
        global_var_set_sid(S__VM_last_destroyed_card, bak_last);
        global_var_set_sid(S__VM_cur_card, bak_cur);
        mod_inst_del_v(mv, s, true);                                // 计数 -1 + 移出 instances 列表
    }
    return true;
}


// Mouse_4 / Mouse_10 / Mouse_11：先做各事件自己的置位，再跑 .bin 的对应块，最后 event_inherited()
//   三个事件的壳一模一样，只有置位不同 → 共用一个 helper（和 hot_cardmod_mouse 同一套路）
bool hot_gemmod_mouse(Vm& vm, const char* block) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    int bsid = str_find(block);

    if (bsid == S__OBJECT_CLICK) {
        // 主动宝石（插件在 _OBJECT_CREATE 里把 clickable 置 1）：
        //   不在冷却时点一下 → 置 clicked 给插件放技能，并自动重新走冷却
        if (truthy(inst_var_sid(s, S_clickable)) && get_num_sid(s, S_cooldown_timer) <= 0) {
            set_num_sid(s, S_clicked, 1);
            set_num_sid(s, S_cooldown_timer, get_num_sid(s, S_cooldown));
        }
    } else if (bsid == S__OBJECT_MOUSE_ENTER) {
        set_num_sid(s, S_on_click, 1);                              // 鼠标进入：显示说明框
    } else if (bsid == S__OBJECT_MOUSE_LEAVE) {
        set_num_sid(s, S_on_click, 0);                              // 鼠标离开
    }

    Vm* mv = mod_vm_of_inst_sid(vm, s, S_gem_id);
    if (mv) mod_run_block_sid(mv, s, bsid, str_at(S__VM_cur_card).c_str());
    // event_inherited()：obj_gem_mod 没有父对象 → hot_parent_of 返回 NULL，空操作
    hot_inherited_sid(vm, S_obj_gem_mod, bsid);
    return true;
}

bool hot_gemmod_click(Vm& vm)       { return hot_gemmod_mouse(vm, str_at(S__OBJECT_CLICK).c_str()); }
bool hot_gemmod_mouse_enter(Vm& vm) { return hot_gemmod_mouse(vm, str_at(S__OBJECT_MOUSE_ENTER).c_str()); }
bool hot_gemmod_mouse_leave(Vm& vm) { return hot_gemmod_mouse(vm, str_at(S__OBJECT_MOUSE_LEAVE).c_str()); }
