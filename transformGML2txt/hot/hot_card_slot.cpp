// ============================================================
// hot_card_slot.cpp —— obj_card_slot（卡槽 UI，6 个事件 + 2 个本地函数）
//
//   这是**游戏 UI**，不是 mod 宿主：卡片图标、价格、冷却、悬停提示、点选/放置。
//   ⚠ 它一大半依赖宿主侧的东西，现在还是占位：
//     · mouse_check_button_pressed / keyboard_check_pressed → 恒 0（假）
//     · device_mouse_x_to_gui / keyboard_get_string / string_width_ext … → 恒 0
//     · get_load_sprite / get_card_info* / can_place_at_position → 空壳
//     所以"放置"这条线现在走不到（can_plant 恒 false），绘制要宿主接上才看得见。
//
//   ⚠ 放置逻辑在 GML 里写了两份：Create 里的 try_place_once()、Step 里内联的一大段。
//     两份**不完全一样**（差别都在下面 cs_place_body 的 from_step 分支里，逐条注明）。
//     这里合成一个函数 + 一个 from_step 标志，而不是抄两遍 170 行 —— 抄两遍更容易走形。
// ============================================================

#include "hot_common.h"
#include "hot_objects.h"

// global.grid_terrains[row][col].type（拿不到 → undefined）
static Value cs_terrain_type(Vm& vm, double row, double col) {
    Value rows = gv_sid(S_grid_terrains);
    Value rowv; rowv.t = MEM_UNDEFINED; rowv.v = 0;
    if (rows.t == MEM_ARRAY && row >= 0 && row < container_size(rows)) {
        rowv = container_at(rows, (int)row);
        if (rowv.t == MEM_ARRAY && col >= 0 && col < container_size(rowv))
            rowv = container_at(rowv, (int)col);
        else rowv.t = MEM_UNDEFINED;
    }
    return (rowv.t == MEM_MAP) ? m_get_sid(vm, rowv, S_type) : Value{MEM_UNDEFINED, 0};
}

// GML 里到处写的那个条件：客户端 && !client_able → 本地跳过销毁（等服务端广播统一销毁）
static bool cs_client_skip(Vm& vm) {
    Value net = gv_sid(S_network);
    if (net.t != MEM_MAP) return false;
    Value mode = v_struct_sid(vm, net, S_mode);
    if (!str_is_sid(&mode, S_client)) return false;
    Value able = v_struct_sid(vm, net, S_client_able);
    return !truthy(&able);
}

// Create_0.gml（218 行）：字段初始化（本地函数见下面的 _FUNC_* 两个热块）
//   内置字段：image_xscale / image_yscale / image_speed
bool hot_cardslot_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    inst_f(*in, F_image_xscale) = mk_num(0.33);
    inst_f(*in, F_image_yscale) = mk_num(0.33);
    inst_f(*in, F_image_speed)  = mk_num(0);

    static const struct { int n;              /* 池下标 */ double v; } nums[] = {
        { S_cooldown, 420 },  // 7 * 60
        { S_cost, 50 },
        { S_current_cost, 0 },
        { S_vm_current_cost, -1 },  // -1 = 不覆盖当前卡片价格
        { S_night, 0 },  // false
        { S_cooldown_timer, 30000 },
        { S_slot_index, 0 },
        { S_clevel, 0 },
        { S_cshape, 0 },
        { S_cskill, 0 },
        { S_info_got, 0 },  // false
        { S_vm_card_spr, -1 },
        { S_vm_place_preview, -1 },
        { S_vm_card_spr_addx, 0 },
        { S_vm_card_spr_addy, 0 },
        { S_is_ready, 1 },  // true：卡槽可用
        { S_is_selected, 0 },  // false
        { S_hover_alpha, 0 },
        { S_cooling_alpha, 0 },
        { S_selected_preview, -4 },  // noone
        { S_preview_alpha, 0.5 },
    };
    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++)
        in->vars[nums[i].n] = mk_num(nums[i].v);

    in->vars[S_card_obj]    = mk_str_sid(S_obj_small_fire);
    in->vars[S_card_id]     = mk_str(vm, "");
    in->vars[S_description] = mk_str(vm, "小火炉:生产火苗");
    {   // card_spr / place_preview = get_load_sprite("spr_small_fire")
        Value a[1]; a[0] = mk_str_sid(S_spr_small_fire);
        in->vars[S_card_spr]      = call_ret_sid(vm, S_get_load_sprite, a, 1);
        in->vars[S_place_preview] = call_ret_sid(vm, S_get_load_sprite, a, 1);
    }
    {   // cooldown_ignore_list = ["ice_cream","magic_chicken"]
        Value a[2];
        a[0] = mk_str_sid(S_ice_cream); a[1] = mk_str_sid(S_magic_chicken);
        set_var_sid(s, S_cooldown_ignore_list, call_ret_sid(vm, S_array_literal, a, 2));
    }
    return true;
}

// _FUNC_select_slot（Create_0.gml:41）：选中本卡槽（顺便取消别处的选中）
bool hot_cardslot_func_select_slot(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    set_num_sid(s, S_is_selected, 1);

    // GML: instance_find(obj_shovel_slot, 0) —— 我们这边 inst_at 就是"第 0 个活实例"
    int shovel = inst_at(str_at(S_obj_shovel_slot), 0);
    if (inst_get(shovel) && truthy(inst_var_sid(shovel, S_is_selected)))
        call_fn_int_sid(vm, S_deselect_shovel, NULL, 0, -1);          // 项目脚本（宿主实现）

    Value* sel = global_var_sid(S_selected_slot);
    double sel_id = sel ? num_of(sel) : -4;
    if (sel_id != -4 && sel_id != s && inst_get((int)sel_id)) {
        int o = (int)sel_id;
        set_num_sid(o, S_is_selected, 0);
        int sp = (int)get_num_sid(o, S_selected_preview);
        if (inst_get(sp)) call_fn_int_sid(vm, S_instance_destroy, &sp, 1, -1);
        set_num_sid(o, S_selected_preview, -4);
    }
    { Value a[3]; a[0] = mk_str_sid(S_snd_card_lift); a[1] = mk_num(0); a[2] = mk_num(0);
      call_fn_sid(vm, S_audio_play_sound, a, 3, -1); }
    global_var_set_sid(S_selected_slot, mk_num(s));
    return true;
}

// ── 放置主体（GML 里两份的合并；from_step = 走 Step 里那份）──
//   Step 那份多出来的（都在下面用 from_step 门控）：
//     · 客户端预测：新建的植物立刻销毁（炸弹类跳过 Destroy 事件）
//     · 平台移动中放置 → new_plant.platform_grid_lock = true
//     · 传送带卡槽：从 global._VM_conveyor_belt_arr 里删掉并销毁自己
//     · global.prev_place_id_shape = new_plant 的 sprite_index
//     · 放置音效按联机模式门控（客户端不放）
//   参数 belt_index = 这一帧算出来的传送带下标（-1 = 不在传送带上）
//   返回 true = 真的放了（调用方用它决定后续）
static bool cs_place_body(Vm& vm, int s, const Value& cid, const Value& card_data,
                          double mx, double my, bool from_step, int belt_index) {
    // 联机模式（下面好几处要用）：client / server / offline
    Value net = gv_sid(S_network);
    Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
    if (net.t == MEM_MAP) mode = v_struct_sid(vm, net, S_mode);
    bool is_client = str_is_sid(&mode, S_client);

    double found_plat = -4, shift_x = 0, shift_y = 0;
    double logical_col = -1, logical_row = -1;
    bool direct_in_platform = false;

    {   // 鼠标视觉格（不传 raw → 会回绕）
        Value a[2]; a[0] = mk_num(mx); a[1] = mk_num(my);
        Value gpv = call_ret_sid(vm, S_get_grid_position_from_world, a, 2);
        double vis_col = num_of(&m_get_sid(vm, gpv, S_col));
        double vis_row = num_of(&m_get_sid(vm, gpv, S_row));

        // with (obj_platform)：找鼠标落在哪个平台的可放置范围里
        for (std::unordered_map<int, int>::iterator it = g_inst_alive.begin(); it != g_inst_alive.end(); ++it) {
            if (!inst_is_sid(it->first, S_obj_platform)) continue;
            int p = it->first;
            bool axis_x = inst_var_sid(p, S_move_axis) && str_is(vm, inst_var_sid(p, S_move_axis), "x");
            double sx = axis_x ? get_num_sid(p, S_visual_x_shift) : 0;
            double sy = axis_x ? 0 : get_num_sid(p, S_visual_y_shift);
            Value a2[2]; a2[0] = mk_num(mx - sx); a2[1] = mk_num(my - sy);
            Value gpa = call_ret_sid(vm, S_get_grid_position_from_world, a2, 2);
            double adj_col = num_of(&m_get_sid(vm, gpa, S_col));
            double adj_row = num_of(&m_get_sid(vm, gpa, S_row));

            double c_off = axis_x ? get_num_sid(p, S_current_offset) : 0;
            double r_off = axis_x ? 0 : get_num_sid(p, S_current_offset);
            double sc = get_num_sid(p, S_start_col) + c_off;
            double sr = get_num_sid(p, S_start_row) + r_off;
            double pw = get_num_sid(p, S_width), pl = get_num_sid(p, S_length);

            if (adj_col >= sc && adj_col < sc + pw && adj_row >= sr && adj_row < sr + pl) {
                found_plat = p; logical_col = adj_col; logical_row = adj_row;
                shift_x = sx; shift_y = sy;
                break;
            }
            if (!direct_in_platform
                    && vis_col >= sc && vis_col < sc + pw && vis_row >= sr && vis_row < sr + pl)
                direct_in_platform = true;
        }
        if (found_plat == -4) {
            if (direct_in_platform) { logical_col = -1; logical_row = -1; }   // 平台外一格：禁止
            else                    { logical_col = vis_col; logical_row = vis_row; }
        }
    }

    Value lw;
    { Value a[2]; a[0] = mk_num(logical_col); a[1] = mk_num(logical_row);
      lw = call_ret_sid(vm, S_get_world_position_from_grid, a, 2); }
    double lwx = num_of(&m_get(vm, lw, "x")), lwy = num_of(&m_get(vm, lw, "y"));

    Value pt = m_get_sid(vm, card_data, S_plant_type);
    Value ft = m_get_sid(vm, card_data, S_feature_type);
    Value tc = m_get_sid(vm, card_data, S_target_card);

    bool can_plant;
    { Value a[5]; a[0] = mk_num(lwx); a[1] = mk_num(lwy); a[2] = pt; a[3] = ft; a[4] = tc;
      can_plant = truthy(&call_ret_sid(vm, S_can_place_at_position, a, 5)); }

    if (!can_plant || gnumv_sid(S_flame) < get_num_sid(s, S_current_cost)) {
        if (truthy(global_var_sid(S_quick_placement))) {                    // 快速放置失败 → 取消选中
            set_num_sid(s, S_is_selected, 0);
            int sp = (int)get_num_sid(s, S_selected_preview);
            if (sp != -4 && inst_get(sp)) call_fn_int_sid(vm, S_instance_destroy, &sp, 1, -1);
            set_num_sid(s, S_selected_preview, -4);
            global_var_set_sid(S_selected_slot, mk_num(-4));
        }
        return false;
    }

    // ── 真的放了 ──
    Value plants;
    { Value a[3]; a[0] = gv_sid(S_grid_plants); a[1] = mk_num(logical_col); a[2] = mk_num(logical_row);
      plants = call_ret_sid(vm, S_ds_grid_get, a, 3); }

    // ① target_card：替换底座卡片（feature_type == "upgrade"）
    if (str_is_sid(&ft, S_upgrade) && tc.t != MEM_UNDEFINED && !str_is_sid(&tc, S_none)) {
        int n = 0;
        { Value a[1]; a[0] = plants; n = (int)num_of(&call_ret_sid(vm, S_ds_list_size, a, 1)); }
        for (int i = 0; i < n; i++) {
            int plant = -1;
            { Value a[2]; a[0] = plants; a[1] = mk_num(i);
              plant = (int)num_of(&call_ret_sid(vm, S_ds_list_find_value, a, 2)); }
            if (!inst_get(plant)) continue;
            Value* pid = inst_var_sid(plant, S_plant_id);
            if (!pid || !str_eq(vm, pid, &tc)) continue;
            if (!cs_client_skip(vm)) {
                call_fn_int_sid(vm, S_card_destroyed, &plant, 1, -1);
                call_fn_int_sid(vm, S_instance_destroy, &plant, 1, -1);
            }
            break;
        }
    }
    // ② 通用替换（同 plant_type 的植物，或开了 replace_placement）
    if (truthy(global_var_sid(S_replace_placement))) {
        int n = 0;
        { Value a[1]; a[0] = plants; n = (int)num_of(&call_ret_sid(vm, S_ds_list_size, a, 1)); }
        for (int i = 0; i < n; i++) {
            int plant = -1;
            { Value a[2]; a[0] = plants; a[1] = mk_num(i);
              plant = (int)num_of(&call_ret_sid(vm, S_ds_list_find_value, a, 2)); }
            if (!inst_get(plant)) continue;
            Value* ppt = inst_var_sid(plant, S_plant_type);
            if (!ppt || !str_eq(vm, ppt, &pt)) continue;                       // 同类型才替换
            if (str_is_sid(inst_var_sid(plant, S_plant_id), S_player)) continue;
            if (str_is_sid(ppt, S_coffee)) continue;
            Value* pft = inst_var_sid(plant, S_feature_type);
            bool king   = (str_is_sid(&ft, S_bun) && str_is_sid(pft, S_king_bun))
                       || (str_is_sid(&ft, S_king_bun) && str_is_sid(pft, S_king_bun));
            bool kingtb = (str_is_sid(&ft, S_tbun) && str_is_sid(pft, S_king_tbun))
                       || (str_is_sid(&ft, S_king_tbun) && str_is_sid(pft, S_king_tbun));
            if (king || kingtb) continue;
            if (!cs_client_skip(vm)) {
                call_fn_int_sid(vm, S_card_destroyed, &plant, 1, -1);
                call_fn_int_sid(vm, S_instance_destroy, &plant, 1, -1);
            }
        }
    }

    // ③ 建植物：mod 卡的身份要在创建前写进 pending 全局
    if (str_is_sid(inst_var_sid(s, S_card_obj), S_obj_card_mod))
        global_var_set_sid(S__mod_pending_card_id, cid);
    int plant = -1;
    {
        Value a[4];
        a[0] = mk_num(lwx + shift_x); a[1] = mk_num(lwy + shift_y);
        a[2] = mk_num(0); a[3] = *inst_var_sid(s, S_card_obj);
        Value r = call_ret_sid(vm, S_instance_create_depth, a, 4);
        plant = r.t == MEM_INT ? r.v : -1;
    }
    global_var_set_sid(S__mod_pending_card_id, mk_str(vm, ""));

    if (inst_get(plant)) {
        double depth_value = 0;
        { Value a[3]; a[0] = mk_num(logical_col); a[1] = mk_num(logical_row);
          a[2] = *inst_var_sid(plant, S_plant_type);
          depth_value = num_of(&call_ret_sid(vm, S_calculate_plant_depth, a, 3)); }
        { Value a[3]; a[0] = mk_num(plant); a[1] = mk_num(logical_col); a[2] = mk_num(logical_row);
          call_fn_sid(vm, S_card_created, a, 3, -1); }
        set_num_sid(plant, S_depth, depth_value);

        double new_spr = get_num_sid(plant, S_sprite_index);     // Step 那份要存 prev_place_id_shape

        // ④ 客户端本地建完就销毁（等服务端广播重建）
        if (from_step && is_client) {
            // ⚠ GML 里有第二个参数 false（不跑 Destroy 事件，给炸弹类用）；
            //   我们的 instance_destroy 目前只认第一个参数，第二个忽略 —— 待宿主/VM 补
            Value a[1]; a[0] = mk_num(plant);
            call_fn_sid(vm, S_instance_destroy, a, 1, -1);
        }
        // ⑤ 平台移动中放置 → 锁住逻辑格
        if (from_step && !is_client) {
            if (found_plat != -4 && inst_var_sid((int)found_plat, S_state)
                    && str_is_sid(inst_var_sid((int)found_plat, S_state), S_moving))
                set_num_sid(plant, S_platform_grid_lock, 1);
        }

        // ⑥ 放置特效（按地形）
        Value ctype = cs_terrain_type(vm, logical_row, logical_col);
        if (str_is_sid(&ctype, S_normal)) {
            Value a[4];
            a[0] = mk_num(lwx + shift_x); a[1] = mk_num(lwy + shift_y);
            a[2] = mk_num(-2); a[3] = mk_str_sid(S_obj_place_effect);
            call_fn_sid(vm, S_instance_create_depth, a, 4, -1);
        } else if (str_is_sid(&ctype, S_water)) {
            Value a[4];
            a[0] = mk_num(lwx + shift_x); a[1] = mk_num(lwy + shift_y + 20);
            a[2] = mk_num(-2500); a[3] = mk_str_sid(S_obj_place_effect);
            Value fx = call_ret_sid(vm, S_instance_create_depth, a, 4);
            int fid = fx.t == MEM_INT ? fx.v : -1;
            if (inst_get(fid)) inst_var_set_sid(fid, S_sprite_index, mk_str_sid(S_spr_enter_water_effect));
        }

        // ⑦ 扣火苗
        global_var_set_sid(S_flame, mk_num(gnumv_sid(S_flame) - get_num_sid(s, S_current_cost)));

        // ⑧ 传送带（只在 Step 那份里）
        if (from_step && gtrue_sid(S__VM_conveyor_belt) && belt_index != -1) {
            Value* belt = global_var_sid(S__VM_conveyor_belt_arr);
            if (belt && belt->t == MEM_ARRAY) {
                Value a[3]; a[0] = *belt; a[1] = mk_num(belt_index); a[2] = mk_num(1);
                call_fn_sid(vm, S_array_delete, a, 3, -1);
            }
            call_fn_int_sid(vm, S_instance_destroy, NULL, 0, -1);
        }

        // ⑨ 冷却重置 + prev_place_id
        set_num_sid(s, S_cooldown_timer, 0);
        set_num_sid(s, S_is_ready, 0);
        {
            Value* ignore = inst_var_sid(s, S_cooldown_ignore_list);
            Value a[2]; a[0] = ignore ? *ignore : Value{MEM_INT, 0}; a[1] = cid;
            if (num_of(&call_ret_sid(vm, S_array_get_index, a, 2)) == -1) {
                global_var_set_sid(S_prev_place_id, cid);
                if (from_step) global_var_set_sid(S_prev_place_id_shape, mk_num(new_spr));
            }
        }

        // ⑩ 放置音效（Step 那份在客户端不放）
        if (!from_step || !is_client) {
            int snd = str_is_sid(&ctype, S_normal) ? S_snd_place1
                    : (str_is_sid(&ctype, S_water) ? S_snd_enter_water : -1);
            if (snd >= 0) {
                Value a[3]; a[0] = mk_str_sid(snd); a[1] = mk_num(0); a[2] = mk_num(0);
                call_fn_sid(vm, S_audio_play_sound, a, 3, -1);
            }
        }
    }

    // ⑪ 取消选择
    set_num_sid(s, S_is_selected, 0);
    {
        int sp = (int)get_num_sid(s, S_selected_preview);
        if (sp != -4 && inst_get(sp)) call_fn_int_sid(vm, S_instance_destroy, &sp, 1, -1);
    }
    set_num_sid(s, S_selected_preview, -4);
    global_var_set_sid(S_selected_slot, mk_num(-4));
    return true;
}

// _FUNC_try_place_once（Create_0.gml:61）：快速放置走这条（形状没做 _VM_card_shape_cap 裁剪）
bool hot_cardslot_func_try_place_once(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    Value cid = *inst_var_sid(s, S_card_id);
    Value card_data;
    {   // card_shape = get_card_info_simple(card_id).shape → card_data = deck_get_card_data(card_id, shape)
        Value a[1]; a[0] = cid;
        Value cs = call_ret_sid(vm, S_get_card_info_simple, a, 1);
        double shape = num_of(&m_get_sid(vm, cs, S_shape));
        Value a2[2]; a2[0] = cid; a2[1] = mk_num(shape);
        card_data = call_ret_sid(vm, S_deck_get_card_data, a2, 2);

        if (str_is_sid(&cid, S_magic_chicken)) {              // 魔法鸡：用"上一次放的卡"
            Value ppid = gv_sid(S_prev_place_id);
            std::string pidtxt = pool_text(&ppid);
            if (!pidtxt.empty()) {
                Value a3[1]; a3[0] = ppid;
                Value cs2 = call_ret_sid(vm, S_get_card_info_simple, a3, 1);
                double shape2 = num_of(&m_get_sid(vm, cs2, S_shape));
                Value a4[2]; a4[0] = ppid; a4[1] = mk_num(shape2);
                card_data = call_ret_sid(vm, S_deck_get_card_data, a4, 2);
            }
        }
    }
    cs_place_body(vm, s, cid, card_data, gnumv_sid(S_mouse_x), gnumv_sid(S_mouse_y), false, -1);
    return true;
}

// 建"放置预览"对象（GML 在鼠标和键盘两支里写了两遍，一模一样 —— 这里合成一个）
//   selected_preview = instance_create_depth(mouse_x, mouse_y, depth-2, obj_card_preview)
//   然后设 preview_sprite / parent_slot / card_id，再跑 _VM_CARD_PREVIEW_PICKED 钩子
static void cs_make_preview(Vm& vm, int s, const Value& cid) {
    Value ca[4];
    ca[0] = gv_sid(S_mouse_x); ca[1] = gv_sid(S_mouse_y);
    ca[2] = mk_num(get_num_sid(s, S_depth) - 2); ca[3] = mk_str_sid(S_obj_card_preview);
    Value pv = call_ret_sid(vm, S_instance_create_depth, ca, 4);
    int sp = pv.t == MEM_INT ? pv.v : -1;
    if (!inst_get(sp)) return;
    set_var_sid(sp, S_preview_sprite, *inst_var_sid(s, S_card_spr));
    if (inst_var_sid(s, S_place_preview))                        // != undefined
        set_var_sid(sp, S_preview_sprite, *inst_var_sid(s, S_place_preview));
    set_num_sid(sp, S_parent_slot, s);
    inst_var_set_sid(sp, S_card_id, cid);
    Value* buf = global_var_sid(S__VM_CARD_PREVIEW_PICKED);
    if (buf && truthy(&call_ret_sid(vm, S_buffer_exists, buf, 1))) {
        Value ea[3];
        ea[0] = global_var_sid(S___vm) ? *global_var_sid(S___vm) : mk_num(0);
        ea[1] = *buf; ea[2] = mk_str_sid(S__VM_CARD_PREVIEW_PICKED);
        call_fn_sid(vm, S_VM_Execute, ea, 3, -1);
    }
    Value h[1]; h[0] = mk_str_sid(S__VM_CARD_PREVIEW_PICKED);
    call_fn_sid(vm, S_vm_hook_run, h, 1, -1);
    set_num_sid(s, S_selected_preview, sp);
}

// 食谱加成（Other_10 和 Step_2 里那套三层循环，一模一样两处）：按 modif_type 改自己的属性
static void cs_cookbook_modifs(Vm& vm, int s, const Value& card_id) {
    Value save = gv_sid(S_save_data);
    Value book = (save.t == MEM_MAP) ? m_get_sid(vm, save, S_equipped_cookbook) : Value{MEM_UNDEFINED, 0};
    if (book.t != MEM_ARRAY) return;
    for (int i = 0; i < container_size(book); i++) {
        Value row = container_at(book, i);
        if (row.t != MEM_ARRAY) continue;
        for (int j = 0; j < container_size(row); j++) {
            Value item = container_at(row, j);
            Value a1[1]; a1[0] = item;
            Value cook = call_ret_sid(vm, S_get_cookbook_data, a1, 1);
            Value modifs = m_get_sid(vm, cook, S_modif);
            if (modifs.t != MEM_ARRAY) continue;
            for (int k = 0; k < container_size(modifs); k++) {
                Value mod = container_at(modifs, k);
                if (mod.t != MEM_MAP) continue;
                Value ids = m_get_sid(vm, mod, S_card_id);
                Value a2[2]; a2[0] = ids; a2[1] = card_id;
                if (num_of(&call_ret_sid(vm, S_array_get_index, a2, 2)) == -1) continue;
                Value mtype = m_get_sid(vm, mod, S_modif_type);
                std::string tname = pool_text(&mtype);
                if (tname.empty()) continue;
                Value* cur = inst_var(s, tname);                 // variable_instance_exists(self, modif_type)
                if (!cur) continue;
                double v = num_of(cur);
                double amount = num_of(&m_get_sid(vm, mod, S_amount));
                Value calc = m_get_sid(vm, mod, S_modif_calc);
                if (str_is_sid(&calc, S_plus))          v += amount;
                else if (str_is_sid(&calc, S_multiply)) v *= amount;
                else continue;
                Value va[3]; va[0] = mk_num(s); va[1] = mtype; va[2] = mk_num(v);
                call_fn_sid(vm, S_variable_instance_set, va, 3, -1);
            }
        }
    }
}

// Step_0.gml（331 行）：传送带 / 价格 / 冷却 / 悬停 / 点选 / 快捷键 / 放置（内联的那份）
bool hot_cardslot_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;

    // ── 传送带模式：位置对齐 + 免费 + 无冷却 ──
    int belt_index = -1;
    if (gtrue_sid(S__VM_conveyor_belt)) {
        Value* belt = global_var_sid(S__VM_conveyor_belt_arr);
        int len = (belt && belt->t == MEM_ARRAY) ? container_size(*belt) : 0;
        set_num_sid(s, S_cooldown_timer, get_num_sid(s, S_cooldown));
        set_num_sid(s, S_cost, 0);
        for (int i = 0; i < len; i++) {
            if ((int)num_of(&container_at(*belt, i)) != s) continue;
            belt_index = i;
            double target_x = 535 + i * 90;                  // start_x(535) + i * x_spacing(90)
            if (get_num_sid(s, S_x) > target_x) add_num_sid(s, S_x, -1);
        }
    }

    Value cid = *inst_var_sid(s, S_card_id);

    // ── 价格：基础 cost + 加号卡 +50；vm_current_cost 覆盖 ──
    if (!str_is_sid(&cid, S_magic_chicken)) {
        set_num_sid(s, S_current_cost, get_num_sid(s, S_cost));
        Value plus_map = gv_sid(S_plus_card_map);
        Value plus_info; plus_info.t = MEM_UNDEFINED; plus_info.v = 0;
        if (plus_map.t == MEM_MAP) plus_info = v_map(vm, plus_map, cid);
        if (plus_info.t != MEM_UNDEFINED) {
            // with plus_info[0] { if shape < plus_info[1] other.current_cost += 50 }
            int owner = (int)num_of(&container_at(plus_info, 0));
            double limit = num_of(&container_at(plus_info, 1));
            if (inst_get(owner) && get_num_sid(owner, S_shape) < limit) add_num_sid(s, S_current_cost, 50);
        }
        if (get_num_sid(s, S_vm_current_cost) != -1) set_num_sid(s, S_current_cost, get_num_sid(s, S_vm_current_cost));
    }
    if (gtrue_sid(S_debug)) set_num_sid(s, S_cooldown_timer, get_num_sid(s, S_cooldown));

    // ── 冷却 ──
    if (get_num_sid(s, S_cooldown_timer) < get_num_sid(s, S_cooldown)) {
        add_num_sid(s, S_cooldown_timer, 1);
        set_num_sid(s, S_is_ready, 0);
        set_num_sid(s, S_cooling_alpha, std::fmin(get_num_sid(s, S_cooling_alpha) + 0.05, 0.7));
    } else {
        set_num_sid(s, S_cooling_alpha, std::fmax(get_num_sid(s, S_cooling_alpha) - 0.05, 0));
        set_num_sid(s, S_is_ready, gnumv_sid(S_flame) >= get_num_sid(s, S_current_cost) ? 1 : 0);
    }

    // ── 悬停（GUI 坐标）──
    {
        Value a[1]; a[0] = mk_num(0);
        double mx = num_of(&call_ret_sid(vm, S_device_mouse_x_to_gui, a, 1));
        double my = num_of(&call_ret_sid(vm, S_device_mouse_y_to_gui, a, 1));
        Value r[5];
        r[0] = mk_num(mx); r[1] = mk_num(my);
        r[2] = mk_num(get_num_sid(s, S_x) - 42); r[3] = mk_num(get_num_sid(s, S_y) - 55);
        r[4] = mk_num(get_num_sid(s, S_x) + 42);
        bool hovered = truthy(&call_ret_sid(vm, S_point_in_rectangle, r, 5));
        set_num_sid(s, S_hover_alpha, hovered ? std::fmin(get_num_sid(s, S_hover_alpha) + 0.1, 1) : 0);
    }

    // ── 鼠标左键点卡槽 → 选中 + 建预览（下面键盘那支是同一段，GML 也写了两遍）──
    {
        Value a[1]; a[0] = mk_num(1);                        // mb_left
        if (truthy(inst_var_sid(s, S_is_ready)) && truthy(&call_ret_sid(vm, S_mouse_check_button_pressed, a, 1))) {
            Value r[5];
            r[0] = gv_sid(S_mouse_x); r[1] = gv_sid(S_mouse_y);
            r[2] = mk_num(get_num_sid(s, S_x) - 50); r[3] = mk_num(get_num_sid(s, S_y) - 70);
            r[4] = mk_num(get_num_sid(s, S_x) + 50);
            if (truthy(&call_ret_sid(vm, S_point_in_rectangle, r, 5))) {
                run_hot_block_sid(vm, S_obj_card_slot, S__FUNC_select_slot);
                if (get_num_sid(s, S_selected_preview) == -4) cs_make_preview(vm, s, cid);
            }
        }
    }

    // ── 快捷键（keybind_map["卡槽" + slot_index]）──
    {
        std::string keyname = "卡槽" + std::to_string((int)get_num_sid(s, S_slot_index));
        Value km = gv_sid(S_keybind_map);
        Value slot_key; slot_key.t = MEM_UNDEFINED; slot_key.v = 0;
        if (km.t == MEM_MAP) slot_key = v_map(vm, km, mk_str(vm, keyname.c_str()));
        Value ka[1]; ka[0] = slot_key;
        if (truthy(&call_ret_sid(vm, S_keyboard_check_pressed, ka, 1)) && truthy(inst_var_sid(s, S_is_ready))) {
            if (!truthy(inst_var_sid(s, S_is_selected))) {
                run_hot_block_sid(vm, S_obj_card_slot, S__FUNC_select_slot);
                if (gtrue_sid(S_quick_placement)) run_hot_block_sid(vm, S_obj_card_slot, S__FUNC_try_place_once);
                else if (get_num_sid(s, S_selected_preview) == -4) cs_make_preview(vm, s, cid);
            } else {                                             // 再按一次 = 取消选中
                set_num_sid(s, S_is_selected, 0);
                int sp = (int)get_num_sid(s, S_selected_preview);
                if (sp != -4 && inst_get(sp)) call_fn_int_sid(vm, S_instance_destroy, &sp, 1, -1);
                set_num_sid(s, S_selected_preview, -4);
                global_var_set_sid(S_selected_slot, mk_num(-4));
            }
        }
    }

    // ── 选中状态下：预览跟鼠标 / 右键或 ESC 取消 / 左键放置 ──
    if (truthy(inst_var_sid(s, S_is_selected))) {
        int sp = (int)get_num_sid(s, S_selected_preview);
        if (sp != -4 && inst_get(sp)) {
            set_num_sid(sp, S_x, gnumv_sid(S_mouse_x));
            set_num_sid(sp, S_y, gnumv_sid(S_mouse_y));
        }
        Value ra[1]; ra[0] = mk_num(2);                      // mb_right
        Value ea[1]; ea[0] = mk_num(27);                     // vk_escape
        if (truthy(&call_ret_sid(vm, S_mouse_check_button_pressed, ra, 1))
                || truthy(&call_ret_sid(vm, S_keyboard_check_pressed, ea, 1))) {
            set_num_sid(s, S_is_selected, 0);
            if (sp != -4 && inst_get(sp)) call_fn_int_sid(vm, S_instance_destroy, &sp, 1, -1);
            set_num_sid(s, S_selected_preview, -4);
            global_var_set_sid(S_selected_slot, mk_num(-4));
        }
        Value la[1]; la[0] = mk_num(1);                      // mb_left
        if (truthy(&call_ret_sid(vm, S_mouse_check_button_pressed, la, 1))) {
            // 取卡数据（Step 这份多了 _VM_card_shape_cap 的裁剪）
            Value card_data;
            {
                Value a1[1]; a1[0] = cid;
                Value cs = call_ret_sid(vm, S_get_card_info_simple, a1, 1);
                double shape = num_of(&m_get_sid(vm, cs, S_shape));
                double cap = gnumv_sid(S__VM_card_shape_cap);
                if (cap >= 0 && cap < shape) shape = cap;
                Value a2[2]; a2[0] = cid; a2[1] = mk_num(shape);
                card_data = call_ret_sid(vm, S_deck_get_card_data, a2, 2);

                if (str_is_sid(&cid, S_magic_chicken)) {
                    Value ppid = gv_sid(S_prev_place_id);
                    if (!pool_text(&ppid).empty()) {
                        Value a3[1]; a3[0] = ppid;
                        Value cs2 = call_ret_sid(vm, S_get_card_info_simple, a3, 1);
                        double shape2 = num_of(&m_get_sid(vm, cs2, S_shape));
                        if (cap >= 0 && cap < shape2) shape2 = cap;
                        Value a4[2]; a4[0] = ppid; a4[1] = mk_num(shape2);
                        card_data = call_ret_sid(vm, S_deck_get_card_data, a4, 2);
                    }
                }
            }
            cs_place_body(vm, s, cid, card_data, gnumv_sid(S_mouse_x), gnumv_sid(S_mouse_y),
                          true, belt_index);
        }
    }

    set_num_sid(s, S_depth, get_num_sid(s, S_slot_index) * -1 - 1000);
    if (!truthy(inst_var_sid(s, S_info_got)))
        inst_event_inherited_sid(vm, s, S__OBJECT_USER_EVENT_0);     // event_user(0)
    return true;
}

// Step_2.gml（57 行，End Step）：魔法鸡的价格/冷却跟着"上一张放的卡"走
bool hot_cardslot_step_end(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Value cid = *inst_var_sid(s, S_card_id);
    if (!str_is_sid(&cid, S_magic_chicken)) return true;
    Value ppid = gv_sid(S_prev_place_id);
    if (pool_text(&ppid).empty()) return true;

    Value a1[1]; a1[0] = ppid;
    Value csd = call_ret_sid(vm, S_get_card_info_simple, a1, 1);
    if (eq_num(csd, 0)) return true;                             // != false

    Value shape = m_get_sid(vm, csd, S_shape);
    Value level = m_get_sid(vm, csd, S_level);
    Value skill = m_get_sid(vm, csd, S_skill);
    Value a[4]; a[0] = ppid; a[1] = shape; a[2] = level; a[3] = skill;
    Value pci = call_ret_sid(vm, S_get_plant_data_with_skill, a, 4);
    set_num_sid(s, S_cost, num_of(&m_get_sid(vm, pci, S_cost)));

    if (get_num_sid(s, S_cooldown_timer) >= get_num_sid(s, S_cooldown)) {
        set_num_sid(s, S_cooldown, num_of(&m_get_sid(vm, pci, S_cooldown)));
        cs_cookbook_modifs(vm, s, ppid);
        if (get_num_sid(s, S_cooldown) < 13.5 * 60) set_num_sid(s, S_cooldown, 13.5 * 60);
        {   // 联机魔塔：冷却 +50%
            Value net = gv_sid(S_network);
            Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
            if (net.t == MEM_MAP) mode = v_struct_sid(vm, net, S_mode);
            if (!str_is_sid(&mode, S_offline) && str_is_sid(&gv_sid(S_map_id), S_tower_cake))
                set_num_sid(s, S_cooldown, std::ceil(get_num_sid(s, S_cooldown) * 1.5));
        }
        set_num_sid(s, S_cooldown_timer, get_num_sid(s, S_cooldown));
    }

    {   // card_spr = deck_get_card_data(prev_place_id, shape).sprite
        Value a2[2]; a2[0] = ppid; a2[1] = m_get_sid(vm, csd, S_shape);
        Value cslot = call_ret_sid(vm, S_deck_get_card_data, a2, 2);
        inst_var_set_sid(s, S_card_spr, m_get_sid(vm, cslot, S_sprite));
    }
    set_num_sid(s, S_current_cost, get_num_sid(s, S_cost));
    {   // 加号卡 +50
        Value plus_map = gv_sid(S_plus_card_map);
        Value plus_info; plus_info.t = MEM_UNDEFINED; plus_info.v = 0;
        if (plus_map.t == MEM_MAP) plus_info = v_map(vm, plus_map, ppid);
        if (plus_info.t != MEM_UNDEFINED) {
            int owner = (int)num_of(&container_at(plus_info, 0));
            double limit = num_of(&container_at(plus_info, 1));
            Value shapev = m_get_sid(vm, csd, S_shape);
            if (inst_get(owner) && num_of(&shapev) < limit) add_num_sid(s, S_current_cost, 50);
        }
    }
    if (get_num_sid(s, S_vm_current_cost) != -1) set_num_sid(s, S_current_cost, get_num_sid(s, S_vm_current_cost));
    return true;
}

// Draw_0.gml（111 行）：卡槽本体 + 价格 + 图标 + 星级 + 冷却遮罩/进度/倒计时 + 快捷键
//   ⚠ sprite_width / sprite_height 是宿主的精灵尺寸，拿不到就是 0（遮罩会退化成 0 面积）
bool hot_cardslot_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;

    call_fn_int_sid(vm, S_draw_self, NULL, 0, -1);

    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }   // fa_left = 0
    { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }   // fa_middle = 1
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }    // c_black
    { Value a[1]; a[0] = mk_str_sid(S_font_number); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
    {   // 价格
        Value a[1]; a[0] = mk_num(get_num_sid(s, S_current_cost));
        Value txt = call_ret_sid(vm, S_string, a, 1);
        Value t[8];
        t[0] = mk_num(get_num_sid(s, S_x) - 12); t[1] = mk_num(get_num_sid(s, S_y) + 44); t[2] = txt;
        t[3] = mk_num(25); t[4] = mk_num(1800); t[5] = mk_num(1); t[6] = mk_num(1); t[7] = mk_num(0);
        call_fn_sid(vm, S_draw_text_ext_transformed, t, 8, -1);
    }
    {   // 火苗图标
        Value d[9];
        d[0] = mk_str_sid(S_spr_flame); d[1] = mk_num(0);
        d[2] = mk_num(get_num_sid(s, S_x) - 24); d[3] = mk_num(get_num_sid(s, S_y) + 43);
        d[4] = mk_num(0.3); d[5] = mk_num(0.3); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
    }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }   // fa_top = 0

    Value cid = *inst_var_sid(s, S_card_id);

    // 时装（attire）：图标 / 预览 换成时装那套
    {
        Value a[1]; a[0] = cid;
        double aid = num_of(&call_ret_sid(vm, S_card_equipped_attire_id, a, 1));
        if (aid != -1) {
            Value a2[1]; a2[0] = mk_num(aid);
            Value info = call_ret_held_sid(vm, S_get_attire_info, a2, 1);
            Value a3[1]; a3[0] = cid;
            Value csi = call_ret_sid(vm, S_get_card_info_simple, a3, 1);
            int si = (int)num_of(&m_get_sid(vm, csi, S_shape));
            Value icon = m_get_sid(vm, info, S_card_slot_icon);
            if (icon.t == MEM_ARRAY && si >= 0 && si < container_size(icon))
                inst_var_set_sid(s, S_card_spr, container_at(icon, si));
            Value plist = m_get_sid(vm, info, S_spr);
            if (plist.t == MEM_ARRAY && si >= 0 && si < container_size(plist))
                inst_var_set_sid(s, S_place_preview, container_at(plist, si));
            release(info);
        }
    }
    if (get_num_sid(s, S_vm_card_spr) != -1) {                       // VM 覆盖贴图
        Value a[1]; a[0] = *inst_var_sid(s, S_vm_card_spr);
        set_var_sid(s, S_card_spr, call_ret_sid(vm, S_get_load_sprite, a, 1));
    }
    if (get_num_sid(s, S_vm_place_preview) != -1) {
        Value a[1]; a[0] = *inst_var_sid(s, S_vm_place_preview);
        set_var_sid(s, S_place_preview, call_ret_sid(vm, S_get_load_sprite, a, 1));
    }
    {   // 植物图标
        Value d[9];
        d[0] = *inst_var_sid(s, S_card_spr); d[1] = mk_num(0);
        d[2] = mk_num(get_num_sid(s, S_x) + get_num_sid(s, S_vm_card_spr_addx));
        d[3] = mk_num(get_num_sid(s, S_y) + 18 + get_num_sid(s, S_vm_card_spr_addy));
        d[4] = mk_num(0.9); d[5] = mk_num(0.9); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
    }
    if (get_num_sid(s, S_clevel) > 0) {                              // 星级
        Value d[9];
        d[0] = mk_str_sid(S_spr_star_slot); d[1] = mk_num(get_num_sid(s, S_clevel) - 1);
        d[2] = mk_num(get_num_sid(s, S_x) - 30); d[3] = mk_num(get_num_sid(s, S_y) - 43);
        d[4] = mk_num(0.8); d[5] = mk_num(0.8); d[6] = mk_num(0);
        d[7] = mk_num(16777215); d[8] = mk_num(1);
        call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1);
    }

    // 冷却遮罩 + 进度条
    double slot_width  = get_num_sid(s, S_sprite_width);
    double slot_height = get_num_sid(s, S_sprite_height);
    double slot_x1 = get_num_sid(s, S_x) - slot_width / 2;
    double slot_y1 = get_num_sid(s, S_y) - slot_height / 2;
    bool cool = get_num_sid(s, S_cooldown_timer) < get_num_sid(s, S_cooldown);

    if ((!truthy(inst_var_sid(s, S_is_ready)) || truthy(inst_var_sid(s, S_is_selected))) && !cool) {
        { Value a[1]; a[0] = mk_num(0.6); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0);   call_fn_sid(vm, S_draw_set_color, a, 1, -1); }   // make_color_rgb(0,0,0)
        Value r[5];
        r[0] = mk_num(slot_x1); r[1] = mk_num(slot_y1);
        r[2] = mk_num(slot_x1 + slot_width); r[3] = mk_num(slot_y1 + slot_height);
        r[4] = mk_num(0);
        call_fn_sid(vm, S_draw_rectangle, r, 5, -1);
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
    }
    if (cool) {
        double progress = 1.0 - (get_num_sid(s, S_cooldown_timer) / get_num_sid(s, S_cooldown));
        double ph = slot_height * progress;
        { Value a[1]; a[0] = mk_num(0.8); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0);   call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        Value r[5];
        r[0] = mk_num(slot_x1); r[1] = mk_num(slot_y1 + slot_height - ph);
        r[2] = mk_num(slot_x1 + slot_width); r[3] = mk_num(slot_y1 + slot_height);
        r[4] = mk_num(0);
        call_fn_sid(vm, S_draw_rectangle, r, 5, -1);
        { Value a[1]; a[0] = mk_num(0.6); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
        Value r2[5];
        r2[0] = mk_num(slot_x1); r2[1] = mk_num(slot_y1);
        r2[2] = mk_num(slot_x1 + slot_width); r2[3] = mk_num(slot_y1 + slot_height - ph);
        r2[4] = mk_num(0);
        call_fn_sid(vm, S_draw_rectangle, r2, 5, -1);
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
    }
    if (cool) {                                                  // 剩余秒数（<2 秒显示 1 位小数）
        double time_left = (get_num_sid(s, S_cooldown) - get_num_sid(s, S_cooldown_timer)) / 60.0;
        char buf[32];
        if (time_left < 2) snprintf(buf, sizeof(buf), "%.1f", time_left);
        else               snprintf(buf, sizeof(buf), "%d", (int)std::floor(time_left));
        { Value a[1]; a[0] = mk_num(16777215); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value a[1]; a[0] = mk_str_sid(S_font_number); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }   // fa_center = 1
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
        { Value t[3]; t[0] = mk_num(get_num_sid(s, S_x)); t[1] = mk_num(get_num_sid(s, S_y));
          t[2] = mk_str(vm, buf); call_fn_sid(vm, S_draw_text, t, 3, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
    }
    {   // 快捷键提示
        std::string keyname = "卡槽" + std::to_string((int)get_num_sid(s, S_slot_index));
        Value km = gv_sid(S_keybind_map);
        Value slot_key; slot_key.t = MEM_UNDEFINED; slot_key.v = 0;
        if (km.t == MEM_MAP) slot_key = v_map(vm, km, mk_str(vm, keyname.c_str()));
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value a[1]; a[0] = mk_str_sid(S_font_song); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
        { Value d[9];
          d[0] = mk_str_sid(S_spr_hotkey_prompt); d[1] = mk_num(0);
          d[2] = mk_num(get_num_sid(s, S_x) + 33); d[3] = mk_num(get_num_sid(s, S_y) - 46);
          d[4] = mk_num(1); d[5] = mk_num(1); d[6] = mk_num(0);
          d[7] = mk_num(16777215); d[8] = mk_num(1);
          call_fn_sid(vm, S_draw_sprite_ext, d, 9, -1); }
        { Value a[1]; a[0] = slot_key;
          Value ks = call_ret_sid(vm, S_keyboard_get_string, a, 1);
          Value t[3]; t[0] = mk_num(get_num_sid(s, S_x) + 33); t[1] = mk_num(get_num_sid(s, S_y) - 48);
          t[2] = ks; call_fn_sid(vm, S_draw_text, t, 3, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
    }
    return true;
}

// Draw_73.gml（60 行）：悬停提示框（描述 + "没有足够的火苗" / "正在冷却中"）
bool hot_cardslot_draw_73(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue_sid(S_is_paused)) return true;
    if (!(get_num_sid(s, S_hover_alpha) > 0)) return true;

    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
    { Value a[1]; a[0] = mk_str_sid(S_font_pixel); call_fn_sid(vm, S_draw_set_font, a, 1, -1); }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }

    Value desc = *inst_var_sid(s, S_description);
    Value wa[3]; wa[0] = desc; wa[1] = mk_num(0.65); wa[2] = mk_num(180);
    double text_width  = num_of(&call_ret_sid(vm, S_string_width_ext, wa, 3));
    double text_height = num_of(&call_ret_sid(vm, S_string_height_ext, wa, 3));
    double box_width = text_width + 20, box_height = text_height + 15;
    Value display = desc;
    bool cooling = get_num_sid(s, S_cooldown_timer) < get_num_sid(s, S_cooldown);
    if (!truthy(inst_var_sid(s, S_is_ready)) && gnumv_sid(S_flame) < get_num_sid(s, S_current_cost) && !cooling) {
        box_height += text_height;
        display = mk_str(vm, (pool_text(&desc) + "\n没有足够的火苗").c_str());
    } else if (!truthy(inst_var_sid(s, S_is_ready)) && cooling) {
        box_height += text_height;
        display = mk_str(vm, (pool_text(&desc) + "\n正在冷却中").c_str());
    }

    // 框底色 make_color_rgb(50,50,80) = 50 | 50<<8 | 80<<16；描边 merge_color(c_yellow,c_white,0.3)
    Value mc[3]; mc[0] = mk_num(16776960); mc[1] = mk_num(16777215); mc[2] = mk_num(0.3);
    Value border = call_ret_sid(vm, S_merge_color, mc, 3);
    Value bg; bg.t = MEM_INT; bg.v = 50 | (50 << 8) | (80 << 16);

    if (get_num_sid(s, S_slot_index) <= 14) {
        double box_x = get_num_sid(s, S_x), box_y = get_num_sid(s, S_y) + 60;
        { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
        { Value a[1]; a[0] = bg; call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(box_x - box_width / 2); r[1] = mk_num(box_y);
          r[2] = mk_num(box_x + box_width / 2); r[3] = mk_num(box_y + box_height);
          r[4] = mk_num(1); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        { Value a[1]; a[0] = border; call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(box_x - box_width / 2); r[1] = mk_num(box_y);
          r[2] = mk_num(box_x + box_width / 2); r[3] = mk_num(box_y + box_height);
          r[4] = mk_num(0); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value t[3]; t[0] = mk_num(box_x); t[1] = mk_num(box_y + 7); t[2] = display;
          call_fn_sid(vm, S_draw_text, t, 3, -1); }
    } else {
        double box_x = get_num_sid(s, S_x) - box_width - 47, box_y = get_num_sid(s, S_y) + 25;
        { Value a[1]; a[0] = bg; call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(box_x); r[1] = mk_num(box_y);
          r[2] = mk_num(box_x + box_width); r[3] = mk_num(box_y + box_height);
          r[4] = mk_num(1); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        { Value a[1]; a[0] = border; call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value r[5];
          r[0] = mk_num(box_x); r[1] = mk_num(box_y);
          r[2] = mk_num(box_x + box_width); r[3] = mk_num(box_y + box_height);
          r[4] = mk_num(0); call_fn_sid(vm, S_draw_rectangle, r, 5, -1); }
        { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_color, a, 1, -1); }
        { Value t[3]; t[0] = mk_num(box_x + 10); t[1] = mk_num(box_y + 7); t[2] = display;
          call_fn_sid(vm, S_draw_text, t, 3, -1); }
    }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_halign, a, 1, -1); }
    { Value a[1]; a[0] = mk_num(0); call_fn_sid(vm, S_draw_set_valign, a, 1, -1); }
    { Value a[1]; a[0] = mk_num(1); call_fn_sid(vm, S_draw_set_alpha, a, 1, -1); }
    return true;
}

// Other_10.gml（51 行，UserEvent 0）：取卡数据 → 套基础属性 → 食谱加成 → 金色卡槽贴图
bool hot_cardslot_user_event_0(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    Value cid = *inst_var_sid(s, S_card_id);

    Value a0[1]; a0[0] = cid;
    Value cs = call_ret_sid(vm, S_get_card_info_simple, a0, 1);
    set_num_sid(s, S_clevel, num_of(&m_get_sid(vm, cs, S_level)));
    set_num_sid(s, S_cshape, num_of(&m_get_sid(vm, cs, S_shape)));

    Value a1[1]; a1[0] = cid;
    Value pd = call_ret_sid(vm, S_get_plant_data, a1, 1);
    if (pd.t != MEM_UNDEFINED) {
        Value a2[1]; a2[0] = cid;
        Value csd = call_ret_sid(vm, S_get_card_info, a2, 1);
        Value shape = m_get_sid(vm, csd, S_shape);
        Value level = m_get_sid(vm, csd, S_level);
        Value skill = m_get_sid(vm, csd, S_skill);
        set_num_sid(s, S_clevel, num_of(&level));
        set_num_sid(s, S_cshape, num_of(&shape));
        set_num_sid(s, S_cskill, num_of(&skill));

        Value a[4]; a[0] = cid; a[1] = shape; a[2] = level; a[3] = skill;
        Value up = call_ret_sid(vm, S_get_plant_data_with_skill, a, 4);
        if (up.t != MEM_UNDEFINED) {
            set_num_sid(s, S_cost, num_of(&m_get_sid(vm, up, S_cost)));
            set_num_sid(s, S_cooldown, num_of(&m_get_sid(vm, up, S_cooldown)));
            Value net = gv_sid(S_network);
            Value mode; mode.t = MEM_UNDEFINED; mode.v = 0;
            if (net.t == MEM_MAP) mode = v_struct_sid(vm, net, S_mode);
            if (!str_is_sid(&mode, S_offline) && str_is_sid(&gv_sid(S_map_id), S_tower_cake))
                set_num_sid(s, S_cooldown, std::ceil(get_num_sid(s, S_cooldown) * 1.5));
        }
        cs_cookbook_modifs(vm, s, cid);
    }

    {   // 金色卡槽：deck_get_card_data(card_id, cshape) 里 is_gold == 1
        Value a[2]; a[0] = cid; a[1] = mk_num(get_num_sid(s, S_cshape));
        Value cd = call_ret_sid(vm, S_deck_get_card_data, a, 2);
        Value ea[2]; ea[0] = cd; ea[1] = mk_num(1);               // ds_type_map = 1
        bool exists = cd.t == MEM_MAP && truthy(&call_ret_sid(vm, S_ds_exists, ea, 2));
        double gold = exists ? num_of(&m_get_sid(vm, cd, S_is_gold)) : 0;
        inst_var_set_sid(s, S_sprite_index, mk_str_sid(gold == 1 ? S_spr_slot_1 : S_spr_slot));
    }
    set_num_sid(s, S_info_got, 1);
    return true;
}
