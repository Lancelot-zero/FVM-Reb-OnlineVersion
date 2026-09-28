// ============================================================
// names.h — 全项目唯一的字符串清单（**手写**，加字符串就往下面加一行）
//
//   这里有且只有两种加字符串的方式：
//     ① 脚本/bin 里的字符串 → 装载时 intern 进去（不用碰这个文件）
//     ② C++ 代码里要引用的名字 → 在这个文件的对应段**尾部**加一行 X(名字)
//
//   枚举值 = 字符串在全局池里的下标，由 ★同一份清单★ 展开两次得到：
//     一次展开成 enum Str（S_xxx）
//     一次展开成 names.cpp 里的字符串表
//   所以枚举和表**不可能对不上**（原来是生成器保证的，现在 CPP 自己保证）。
//
//   ⚠ 只能往各段尾部加：中间插/删会让后面所有 S_* 错位。
//   ⚠ 段顺序 = 池顺序：空串 | native | game | 内置变量名 | 其余
//     前四段的顺序必须和 kNativeFns / kGameFns / INNER_FIELD_LIST 一致。
// ============================================================
#ifndef FVM_NAMES_H
#define FVM_NAMES_H

// ── native 表尾（宏段之外手工实现的那批；顺序 = vm_native.cpp 的 kNativeFns 表尾）──
#define NATIVE_TAIL_LIST(X) \
    /* run_* 系列 */ \
    X(run_block) \
    /* 列表（ds_list_*） */ \
    X(ds_list_create) \
    X(ds_list_destroy) \
    X(ds_list_clear) \
    X(ds_list_add) \
    X(ds_list_delete) \
    X(ds_list_size) \
    X(ds_list_find_value) \
    X(ds_list_find_index) \
    /* 数组 */ \
    X(array_literal) \
    X(array_get) \
    X(array_set) \
    X(array_pop) \
    X(array_clear) \
    X(array_contains) \
    /* 表/字典（ds_map_*） */ \
    X(ds_map_create) \
    X(ds_map_set) \
    X(ds_map_delete) \
    X(ds_map_size) \
    X(ds_map_clear) \
    X(ds_map_keys_to_array) \
    X(ds_map_destroy) \
    /* struct_* 系列 */ \
    X(struct_get) \
    X(struct_set) \
    X(struct_exists) \
    /* 数组 */ \
    X(array_push) \
    /* 表/字典（ds_map_*） */ \
    X(ds_map_exists) \
    X(ds_map_find_value) \
    X(VM_Execute) \
    /* 网格（ds_grid_*） */ \
    X(ds_grid_create) \
    X(ds_grid_set) \
    X(ds_grid_get) \
    /* merge_* 系列 */ \
    X(merge_colour) \
    /* 网格（ds_grid_*） */ \
    X(ds_grid_width) \
    X(ds_grid_height) \
    X(ds_grid_clear) \
    X(ds_grid_destroy) \
    /* 数组 */ \
    X(array_length) \
    X(array_get_index) \
    X(array_delete) \
    X(array_resize) \
    X(array_create) \
    X(array_shuffle) \
    /* ds_* 系列 */ \
    X(ds_exists) \
    /* 实例 */ \
    X(instance_exists) \
    X(instance_number) \
    X(instance_create_depth) \
    X(instance_destroy) \
    /* 变量 */ \
    X(variable_instance_exists) \
    X(variable_instance_get) \
    X(variable_instance_set) \
    X(floor) \
    X(round) \
    X(abs) \
    X(max) \
    X(min) \
    X(clamp) \
    X(lerp) \
    X(random) \
    /* random_* 系列 */ \
    X(random_range) \
    /* irandom_* 系列 */ \
    X(irandom_range) \
    X(sin) \
    X(cos) \
    X(degtorad) \
    /* lengthdir_* 系列 */ \
    X(lengthdir_x) \
    X(lengthdir_y) \
    /* point_* 系列 */ \
    X(point_direction) \
    X(point_distance) \
    /* is_* 系列 */ \
    X(is_real) \
    X(is_string) \
    X(is_array) \
    X(is_struct) \
    X(is_undefined) \
    X(string) \
    /* json_* 系列 */ \
    X(json_stringify) \
    /* 变量 */ \
    X(variable_global_exists) \
    X(variable_global_get) \
    /* merge_* 系列 */ \
    X(merge_color) \
    /* damage_* 系列 */ \
    X(damage_enemy) \
    /* calculate_* 系列 */ \
    X(calculate_plant_depth) \
    /* get_* 系列 */ \
    X(get_grid_position_from_world) \
    X(get_world_position_from_grid) \
    /* 音频 */ \
    X(audio_play_sound) \
    /* send_* 系列 */ \
    X(send_message) \
    /* show_* 系列 */ \
    X(show_debug_message) \
    /* run_* 系列 */ \
    X(run_event_user) \
    X(ceil) \
    /* make_* 系列 */ \
    X(make_color_rgb) \
    /* 绘制 */ \
    X(draw_set_color) \
    X(draw_set_alpha) \
    X(draw_set_font) \
    X(draw_set_halign) \
    X(draw_set_valign) \
    X(draw_rectangle) \
    X(draw_rectangle_color) \
    X(draw_roundrect) \
    /* 字符串 */ \
    X(string_width_ext) \
    X(string_height_ext) \
    /* 网络 */ \
    X(network_broadcast_active_skill)

// ── 其余（hot/ 用到的字符串，以及核心里那几个）── 新名字加这里
#define REST_NAME_LIST(X) \
    /* 本地函数块名 */ \
    X(_FUNC_mod_enemy_init) \
    X(_FUNC_select_slot) \
    X(_FUNC_try_place_once) \
    /* 对象事件块名 */ \
    X(_OBJECT_CLICK) \
    X(_OBJECT_COLLISION_obj_enemy_parent) \
    X(_OBJECT_CREATE) \
    X(_OBJECT_DESTROY) \
    X(_OBJECT_DRAW) \
    X(_OBJECT_DRAW_64) \
    X(_OBJECT_DRAW_73) \
    X(_OBJECT_MOUSE_53) \
    X(_OBJECT_MOUSE_ENTER) \
    X(_OBJECT_MOUSE_LEAVE) \
    X(_OBJECT_STEP) \
    X(_OBJECT_STEP_END) \
    X(_OBJECT_USER_EVENT_0) \
    X(_OBJECT_USER_EVENT_1) \
    X(_OBJECT_USER_EVENT_2) \
    /* VM/脚本约定的全局变量名 */ \
    X(_VM_CARD_DAMAGED) \
    X(_VM_CARD_PREVIEW_PICKED) \
    X(_VM_ENEMY_DAMAGED) \
    X(_VM_PLAYER_DAMAGED) \
    X(_VM_ban_shield) \
    X(_VM_ban_super_weapon) \
    X(_VM_ban_weapon) \
    X(_VM_card_level_cap) \
    X(_VM_card_shape_cap) \
    X(_VM_card_skill_cap) \
    X(_VM_conveyor_belt) \
    X(_VM_conveyor_belt_arr) \
    X(_VM_cur_card) \
    X(_VM_id_to_real) \
    X(_VM_last_created_card) \
    X(_VM_last_damaged_card) \
    X(_VM_last_damaged_enemy) \
    X(_VM_last_damaged_player) \
    X(_VM_last_destroyed_card) \
    X(__vm) \
    /* 吃/收集 */ \
    X(_eat_scratch) \
    /* mod 挂起态 */ \
    X(_mod_initialized) \
    X(_mod_nomask_spr) \
    X(_mod_pending_bullet_id) \
    X(_mod_pending_card_id) \
    X(_mod_pending_enemy_id) \
    X(_mod_pending_gem_id) \
    X(_mod_pending_weapon_id) \
    /* 联网 */ \
    X(_net_before_plant_current_level) \
    X(_net_before_plant_shape) \
    X(_net_before_plant_skill) \
    X(_net_card_equipped_attire_id) \
    /* 统计 */ \
    X(_stat_add) \
    X(_stat_drop) \
    X(_stat_frame) \
    X(_stat_merge) \
    X(_state_prev) \
    /* add_* 系列 */ \
    X(add_n) \
    X(air) \
    X(all) \
    X(amount) \
    X(amphi) \
    /* ash_* 系列 */ \
    X(ash_proof) \
    X(atk) \
    /* atk_* 系列 */ \
    X(atk_cycle) \
    X(attack) \
    /* 攻击 */ \
    X(attack_anim) \
    X(attack_cell) \
    X(attack_collision) \
    X(attack_cycle) \
    X(attack_range) \
    X(attack_timer) \
    X(attack_type) \
    /* awake_* 系列 */ \
    X(awake_anim) \
    X(awake_buff_timer) \
    /* banding_* 系列 */ \
    X(banding_card_obj) \
    X(banding_sleep_obj) \
    X(banding_star_obj) \
    X(banding_water_obj) \
    /* banned_* 系列 */ \
    X(banned_gems_online) \
    /* battle_* 系列 */ \
    X(battle_time) \
    /* bbox_* 系列 */ \
    X(bbox_left) \
    X(bbox_right) \
    X(bbox_top) \
    /* block_* 系列 */ \
    X(block_mouse_id_list) \
    /* bullet_* 系列 */ \
    X(bullet_add_dmg) \
    X(bullet_angle_add) \
    X(bullet_charge_dmg) \
    X(bullet_charge_self) \
    X(bullet_collide) \
    X(bullet_damage) \
    X(bullet_damage_type) \
    X(bullet_destroy) \
    X(bullet_flag) \
    X(bullet_flip_x) \
    X(bullet_flip_y) \
    X(bullet_freeze_add) \
    X(bullet_freeze_chance) \
    X(bullet_freeze_frame) \
    X(bullet_freeze_mul) \
    X(bullet_hits) \
    X(bullet_hits_add) \
    X(bullet_hits_init) \
    X(bullet_hits_left) \
    X(bullet_max) \
    X(bullet_mul_dmg) \
    X(bullet_para_dist) \
    X(bullet_para_t) \
    X(bullet_para_x0) \
    X(bullet_para_y0) \
    X(bullet_parabola) \
    X(bullet_parabola_h) \
    X(bullet_parabola_time) \
    X(bullet_pass_count) \
    X(bullet_path_i) \
    X(bullet_scale) \
    X(bullet_slow) \
    X(bullet_slow_frame) \
    X(bullet_stun_chance) \
    X(bullet_stun_frame) \
    X(bullet_target) \
    X(bullet_target_x) \
    X(bullet_target_y) \
    X(bun) \
    /* can_* 系列 */ \
    X(can_dropped) \
    X(can_shovel_remove) \
    X(card) \
    /* card_* 系列 */ \
    X(card_data) \
    X(card_hpbar) \
    X(card_id) \
    X(card_loss) \
    X(card_mod) \
    X(card_obj) \
    X(card_slot_icon) \
    X(card_spr) \
    X(cell) \
    /* 格位 */ \
    X(cell_flag) \
    X(cell_row) \
    X(cell_target_col) \
    X(cell_target_row) \
    X(cell_terrain_flag) \
    /* chomp_* 系列 */ \
    X(chomp_sound_list) \
    X(clevel) \
    X(client) \
    /* client_* 系列 */ \
    X(client_able) \
    X(coffee) \
    X(col) \
    /* connected_* 系列 */ \
    X(connected_clients) \
    X(cooldown) \
    /* 冷却 */ \
    X(cooldown_ignore_list) \
    X(cooldown_timer) \
    /* cooling_* 系列 */ \
    X(cooling_alpha) \
    /* 花费 */ \
    X(cost) \
    X(count) \
    /* count_* 系列 */ \
    X(count_max) \
    X(cshape) \
    X(cskill) \
    /* current_* 系列 */ \
    X(current_cost) \
    X(current_frozen) \
    X(current_level) \
    X(current_offset) \
    X(cycle) \
    X(damage) \
    /* damage_* 系列 */ \
    X(damage_amount) \
    X(damage_type) \
    X(dance) \
    /* death_* 系列 */ \
    X(death_anim) \
    X(debug) \
    /* debug_* 系列 */ \
    X(debug_merge) \
    /* depth_* 系列 */ \
    X(depth_group) \
    X(depth_offset) \
    X(depth_value) \
    X(description) \
    /* deselect_* 系列 */ \
    X(deselect_shovel) \
    /* destroy_* 系列 */ \
    X(destroy_timer) \
    /* device_* 系列 */ \
    X(device_mouse_x_to_gui) \
    X(device_mouse_y_to_gui) \
    X(difficulty) \
    X(diver) \
    /* 绘制 */ \
    X(draw_text_ext_transformed) \
    /* drop_* 系列 */ \
    X(drop_n) \
    X(dwarf) \
    /* dx_* 系列 */ \
    X(dx_arr) \
    /* dy_* 系列 */ \
    X(dy_arr) \
    /* eat_* 系列 */ \
    X(eat_order) \
    /* enemy_* 系列 */ \
    X(enemy_array) \
    X(enemy_array_left) \
    X(enemy_hpbar) \
    X(enemy_id) \
    X(enemy_map) \
    /* equipped_* 系列 */ \
    X(equipped_cookbook) \
    X(equipped_items) \
    X(feature) \
    /* feature_* 系列 */ \
    X(feature_type) \
    /* first_* 系列 */ \
    X(first_produce) \
    X(first_produce_delay) \
    X(flame) \
    /* flame_* 系列 */ \
    X(flame_produce) \
    /* flash_* 系列 */ \
    X(flash_color) \
    X(flash_speed) \
    X(flash_value) \
    /* 字体 */ \
    X(font_number) \
    X(font_pixel) \
    X(font_song) \
    X(font_yuan) \
    /* frame_* 系列 */ \
    X(frame_count) \
    /* frozen_* 系列 */ \
    X(frozen_timer) \
    /* game_* 系列 */ \
    X(game_over) \
    X(gems) \
    /* get_* 系列 */ \
    X(get_card_info) \
    X(get_cookbook_data) \
    X(get_plant_data) \
    /* giant_* 系列 */ \
    X(giant_type) \
    /* 格子 */ \
    X(grid_cell_size_x) \
    X(grid_cell_size_y) \
    X(grid_col) \
    X(grid_cols) \
    X(grid_offset_x) \
    X(grid_offset_y) \
    X(grid_plants) \
    X(grid_row) \
    X(grid_rows) \
    X(grid_terrains) \
    /* has_* 系列 */ \
    X(has_enemy_air) \
    X(has_enemy_dance) \
    X(has_enemy_diver) \
    X(has_enemy_normal) \
    X(has_enemy_obstacle) \
    X(has_enemy_underground) \
    /* health_* 系列 */ \
    X(health_gem) \
    /* helmet_* 系列 */ \
    X(helmet_hp) \
    X(helmet_max_hp) \
    /* hit_* 系列 */ \
    X(hit_effect_2) \
    X(hit_sound) \
    /* hover_* 系列 */ \
    X(hover_alpha) \
    X(hp) \
    /* hp_* 系列 */ \
    X(hp_change) \
    X(hp_change_mod) \
    X(hp_increase) \
    X(hp_modified) \
    X(hp_modify) \
    /* hurt_* 系列 */ \
    X(hurt_rate) \
    X(hurt_time) \
    /* 冰冻 */ \
    X(ice_cream) \
    X(ice_ok) \
    X(ice_sprite) \
    X(ice_timer) \
    X(id) \
    /* idle_* 系列 */ \
    X(idle_anim) \
    /* immune_* 系列 */ \
    X(immune_to_ash) \
    /* info_* 系列 */ \
    X(info_got) \
    X(invincible) \
    /* is_* 系列 */ \
    X(is_boss) \
    X(is_frozen) \
    X(is_gold) \
    X(is_paused) \
    X(is_placed) \
    X(is_ready) \
    X(is_reversed) \
    X(is_scare) \
    X(is_selected) \
    X(is_slowdown) \
    X(is_stun) \
    /* jump_* 系列 */ \
    X(jump_times) \
    /* keybind_* 系列 */ \
    X(keybind_map) \
    /* 键盘 */ \
    X(keyboard_check_pressed) \
    X(keyboard_get_string) \
    /* king_* 系列 */ \
    X(king_bun) \
    X(king_tbun) \
    /* laboretory_* 系列 */ \
    X(laboretory_room) \
    X(land) \
    /* left_* 系列 */ \
    X(left_move_flashs) \
    X(length) \
    X(level) \
    /* level_* 系列 */ \
    X(level_file) \
    X(lilypad) \
    /* magic_* 系列 */ \
    X(magic_chicken) \
    /* main_* 系列 */ \
    X(main_weapon) \
    /* map_* 系列 */ \
    X(map_id) \
    X(map_instance_id_net_id) \
    /* mask_* 系列 */ \
    X(mask_index) \
    /* max_* 系列 */ \
    X(max_bullets) \
    X(max_hp) \
    X(maxhp) \
    X(merge1_n) \
    X(merge2_n) \
    /* merge_* 系列 */ \
    X(merge_enable) \
    X(merge_lv1) \
    X(merge_lv2) \
    X(merge_n) \
    X(mod) \
    /* mod 系统字段名 */ \
    X(mod_alpha_add) \
    X(mod_changed_prop) \
    X(mod_collision_enemies) \
    X(mod_collision_enemy) \
    X(mod_collision_hit) \
    X(mod_countdown) \
    X(mod_enemy_check) \
    X(mod_enemy_inited_id) \
    X(mod_enemy_types) \
    X(mod_has_card) \
    X(mod_has_enemy) \
    X(mod_hover_alpha) \
    X(mod_hover_mask) \
    X(mod_hp_last) \
    X(mod_point_gem_level) \
    X(mod_point_grid_col) \
    X(mod_point_grid_row) \
    X(mod_point_parent_player) \
    X(mod_set2_id) \
    X(mod_set2_on) \
    X(mod_set2_prop) \
    X(mod_set2_val) \
    X(mod_set_id) \
    X(mod_set_on) \
    X(mod_set_prop) \
    X(mod_set_val) \
    X(mod_step_enemy_area) \
    X(mod_step_enter_arr) \
    X(mod_step_enter_condition) \
    X(mod_step_enter_index) \
    X(mod_step_enter_var) \
    X(mod_tick_cool) \
    X(mod_tick_cycle) \
    X(mod_tick_enemy) \
    X(mod_tick_max) \
    X(mod_type) \
    X(mod_watch) \
    X(mod_watch_changed) \
    X(mod_watch_last) \
    X(mode) \
    X(modif) \
    /* modif_* 系列 */ \
    X(modif_calc) \
    X(modif_type) \
    /* 鼠标 */ \
    X(mouse_check_button_pressed) \
    X(mouse_id) \
    X(mouse_train_1) \
    X(mouse_x) \
    X(mouse_y) \
    /* 移动 */ \
    X(move_anim) \
    X(move_axis) \
    X(move_speed) \
    X(move_speed_modify) \
    X(move_time) \
    X(moving) \
    X(multiply) \
    X(name) \
    /* net_* 系列 */ \
    X(net_player_id) \
    X(network) \
    X(night) \
    X(none) \
    X(norm) \
    /* norm_* 系列 */ \
    X(norm_attack) \
    X(normal) \
    X(obj) \
    /* 对象名 */ \
    X(obj_) \
    X(obj_Bullet_Screen_Management) \
    X(obj_Homing_Bullet_Management) \
    X(obj_battle) \
    X(obj_bullet_mod) \
    X(obj_card_mod) \
    X(obj_card_parent) \
    X(obj_card_preview) \
    X(obj_card_slot) \
    X(obj_cat) \
    X(obj_coin) \
    X(obj_enemy_mod) \
    X(obj_enemy_parent) \
    X(obj_fire_bullet_effect) \
    X(obj_game_over) \
    X(obj_in_water_effect) \
    X(obj_iron_pan_mouse) \
    X(obj_place_effect) \
    X(obj_platform) \
    X(obj_player_character) \
    X(obj_player_shield) \
    X(obj_shovel_slot) \
    X(obj_sleep_effect) \
    X(obj_small_fire) \
    X(obj_stars) \
    X(obj_task_manager) \
    X(obj_unfreeze_effect) \
    X(obj_weapon_mod) \
    X(obj_wooden_plate) \
    X(obj_xiaolongbao_bullet) \
    X(obstacle) \
    X(offline) \
    /* origin_* 系列 */ \
    X(origin_cycle) \
    /* parent_* 系列 */ \
    X(parent_card) \
    X(parent_player) \
    X(parent_slot) \
    X(pierce) \
    /* place_* 系列 */ \
    X(place_preview) \
    /* plant_* 系列 */ \
    X(plant_id) \
    X(plant_type) \
    /* platform_* 系列 */ \
    X(platform_grid_lock) \
    X(player) \
    X(plus) \
    /* plus_* 系列 */ \
    X(plus_card_map) \
    /* pre_* 系列 */ \
    X(pre_hp) \
    X(pre_state) \
    /* prev_* 系列 */ \
    X(prev_grid_col) \
    X(prev_grid_row) \
    X(prev_place_id) \
    X(prev_place_id_shape) \
    /* preview_* 系列 */ \
    X(preview_alpha) \
    X(preview_sprite) \
    /* quick_* 系列 */ \
    X(quick_placement) \
    /* random_* 系列 */ \
    X(random_count) \
    X(random_seed) \
    X(range) \
    /* replace_* 系列 */ \
    X(replace_placement) \
    /* 房间 */ \
    X(room_height) \
    X(room_width) \
    X(row) \
    /* save_* 系列 */ \
    X(save_data) \
    /* scare_* 系列 */ \
    X(scare_timer) \
    /* secondary_* 系列 */ \
    X(secondary_weapon) \
    /* selected_* 系列 */ \
    X(selected_preview) \
    X(selected_slot) \
    X(server) \
    /* 着色器 */ \
    X(shader_hit) \
    X(shape) \
    X(shield) \
    /* 护盾 */ \
    X(shield_hp) \
    X(shield_max_hp) \
    X(shield_outer) \
    X(skill) \
    /* skill_* 系列 */ \
    X(skill_change_style) \
    X(skill_choose) \
    X(skill_move_type) \
    X(skill_timer) \
    /* slot_* 系列 */ \
    X(slot_index) \
    /* 音效资源名 */ \
    X(snd_bullet_burnt) \
    X(snd_card_lift) \
    X(snd_enter_water) \
    X(snd_hit1) \
    X(snd_lose) \
    X(snd_mouse_unfreeze) \
    X(snd_place1) \
    /* special_* 系列 */ \
    X(special_ash) \
    X(spr) \
    /* 精灵资源名 */ \
    X(spr_enter_water_effect) \
    X(spr_fire_bullet) \
    X(spr_fire_bullet_effect) \
    X(spr_flame) \
    X(spr_hotkey_prompt) \
    X(spr_mouse_frozen) \
    X(spr_mouse_scared) \
    X(spr_mouse_stun) \
    X(spr_slot) \
    X(spr_slot_1) \
    X(spr_small_fire) \
    X(spr_star_slot) \
    X(spr_xiaolongbao_bullet) \
    X(sprite) \
    /* 精灵 */ \
    X(sprite_height) \
    X(sprite_width) \
    /* start_* 系列 */ \
    X(start_col) \
    X(start_row) \
    X(state) \
    /* 眩晕 */ \
    X(stun_sprite) \
    X(stun_timer) \
    /* super_* 系列 */ \
    X(super_weapon) \
    /* 目标 */ \
    X(target_card) \
    X(target_coord) \
    X(target_plant) \
    X(target_pos) \
    X(target_type) \
    X(tbit) \
    X(tbun) \
    X(timer) \
    /* tower_* 系列 */ \
    X(tower_cake) \
    X(type) \
    /* UI 绘制参数 */ \
    X(u_flashColor) \
    X(u_progress) \
    X(underground) \
    X(upgrade) \
    /* vector_* 系列 */ \
    X(vector_x) \
    X(version) \
    /* visual_* 系列 */ \
    X(visual_x_shift) \
    X(visual_y_shift) \
    /* 卡片 VM 字段 */ \
    X(vm_card_spr) \
    X(vm_card_spr_addx) \
    X(vm_card_spr_addy) \
    X(vm_current_cost) \
    X(vm_place_preview) \
    X(vx) \
    X(vy) \
    X(wait) \
    X(water) \
    /* 水域 */ \
    X(water_effect_timer) \
    /* weapon_* 系列 */ \
    X(weapon_id) \
    X(width) \
    /* x_* 系列 */ \
    X(x_move_speed) \
    /* y_* 系列 */ \
    X(y_move) \
    X(y_move_speed) \
    /* VM/脚本约定的全局变量名 */ \
    X(_VM_CONST_INIT) \
    X(_CALL_ENTRY) \
    X(array) \
    X(float) \
    X(grid) \
    X(int) \
    X(list) \
    X(null) \
    X(rb) \
    /* 护盾 */ \
    X(shield_inner) \
    X(undefined) \
    X(unknown) \
    /* map_* 系列 */ \
    X(map_struct)

// ── 全清单：段顺序 = 池顺序 ──
#define NAME_LIST(X) \
    X(empty) \
    NATIVE_FN_LIST(X) \
    NATIVE_TAIL_LIST(X) \
    GAME_FN_LIST(X) \
    INNER_FIELD_LIST(X) \
    REST_NAME_LIST(X)

enum Str {
#define NAME_ENUM(n) S_##n,
    NAME_LIST(NAME_ENUM)
#undef NAME_ENUM
    S_LAST_
};

// 把清单按同样顺序灌进全局字符串池（pool_seed 里调一次）
void seed_names();

#endif
