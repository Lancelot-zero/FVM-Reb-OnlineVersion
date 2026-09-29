// ============================================================
// outer_split.h — 外层函数按"写到哪个文件 / 要不要宿主"分组
//   ⚠ 这些分组只决定**写在哪个 .cpp**，不决定表里的顺序：
//     表的顺序永远由 core/const_str.h 的 NATIVE_FN_LIST / NATIVE_IMPL_LIST 说了算
//     每个名字必须**恰好**被定义一次（少一个 = 链接报错，多一个 = 重复符号）
// ============================================================
#pragma once

// 转发：没有 C++ 实现的 native → VM_CallFunc(名字池下标, 实参…)（79 条）
#define FORWARD_LIST(X) \
    X(_find_right_cell) X(alarm_get) X(armor_drop) X(array_deep_copy) \
    X(asset_get_index) X(audio_stop_sound) X(boss_array_shuffle) X(boss_random) \
    X(bresenham) X(buffer_exists) X(bullet_screen_add_Ex) X(bullet_screen_add_Exs) \
    X(Camera_Shock) X(can_hit) X(can_place_at_position) X(can_target_on) \
    X(card_created) X(card_equipped_attire_id) X(clear_train_body) X(connectOnesToTwos) \
    X(create_train_body) X(create_train_body_order) X(create_train_body_pos) X(deck_get_card_data) \
    X(draw_self) X(draw_set_colour) X(draw_sprite_ext) X(draw_text) \
    X(effect_create_above) X(event_perform) X(find_priority_enemy) X(get_attire_info) \
    X(get_card_info_simple) X(get_gem_index) X(get_gem_info) X(get_gem_level) \
    X(get_load_sprite) X(get_plant_data_with_skill) X(get_weapon_info) X(homing_bullet_add) \
    X(instance_position) X(into_act) X(is_cookbook_equipped) X(mod_base_tick) \
    X(mod_enemy_init) X(mod_inst_add) X(mod_inst_del) X(move_towards_point) \
    X(network_apply_plant_level) X(network_spawn_enemy) X(object_get_name) X(place_meeting) \
    X(point_in_rectangle) X(position_meeting) X(remove_train_body) X(screen_flash) \
    X(set_net_id) X(shader_get_uniform) X(shader_reset) X(shader_set) \
    X(show_notice) X(sprite_add) X(sprite_collision_mask) X(sprite_exists) \
    X(sprite_get_number) X(sprite_set_offset) X(variable_struct_exists) X(vm_hook_run) \
    X(card_destroyed) X(get_nearest_grid_position) X(collision_other) X(inst_array_read) \
    X(inst_array_assign) X(inst_array_index) X(inst_array_delete) X(inst_array_pop) \
    X(inst_array_resize) X(inst_array_shuffle) X(inst_array_create) \

// 容器：数组 / ds_list / ds_map / ds_grid / struct   [自举]（44 条）
#define CONTAINER_LIST(X) \
    X(ds_list_create) X(ds_list_destroy) X(ds_list_clear) X(ds_list_add) \
    X(ds_list_delete) X(ds_list_size) X(ds_list_find_value) X(ds_list_find_index) \
    X(array_literal) X(array_get) X(array_set) X(array_pop) \
    X(array_clear) X(array_contains) X(ds_map_create) X(ds_map_set) \
    X(ds_map_delete) X(ds_map_size) X(ds_map_clear) X(ds_map_keys_to_array) \
    X(ds_map_destroy) X(struct_get) X(struct_set) X(struct_exists) \
    X(array_push) X(ds_map_exists) X(ds_map_find_value) X(ds_grid_create) \
    X(ds_grid_set) X(ds_grid_get) X(ds_grid_width) X(ds_grid_height) \
    X(ds_grid_clear) X(ds_grid_destroy) X(array_length) X(array_get_index) \
    X(array_delete) X(array_resize) X(array_create) X(array_shuffle) \
    X(ds_exists) X(ds_map_add) X(array_copy) X(variable_struct_remove) \

// 实例：instance_* / variable_instance_*            [自举]（10 条）
#define INST_LIST(X) \
    X(instance_exists) X(instance_number) X(instance_create_depth) X(instance_destroy) \
    X(variable_instance_exists) X(variable_instance_get) X(variable_instance_set) X(instance_log_enable) \
    X(instance_log_disable) X(instance_find) \

// 派发：run_block / run_event_user / VM_Execute      [自举]（3 条）
#define DISPATCH_LIST(X) \
    X(run_block) X(VM_Execute) X(run_event_user) \

// 内置：数学 / 类型 / 字符串 / 全局 / 坐标 / 随机     [自举]（42 条）
#define BUILTIN_LIST(X) \
    X(merge_colour) X(floor) X(round) X(abs) \
    X(max) X(min) X(clamp) X(lerp) \
    X(random) X(random_range) X(irandom_range) X(sin) \
    X(cos) X(degtorad) X(lengthdir_x) X(lengthdir_y) \
    X(point_direction) X(point_distance) X(is_real) X(is_string) \
    X(is_array) X(is_struct) X(is_undefined) X(string) \
    X(json_stringify) X(variable_global_exists) X(variable_global_get) X(merge_color) \
    X(ceil) X(make_color_rgb) X(string_length) X(json_parse) \
    X(string_copy) X(string_replace_all) X(ord) X(irandom) \
    X(variable_global_set) X(real) X(chr) X(string_pos) \
    X(string_delete) X(string_starts_with) \

// 归业务层：网格数学 3 + 伤害 1（4 条）
#define BIZ_NATIVE_LIST(X) \
    X(damage_enemy) X(calculate_plant_depth) X(get_grid_position_from_world) X(get_world_position_from_grid) \

// 自己能算：文件 / 目录 / 缓冲区 / 日期 / ini        [已实现，见 host_real.cpp]（27 条）
#define HOST_REAL_LIST(X) \
    X(buffer_load) X(buffer_read) X(ini_open) X(buffer_delete) \
    X(ini_write_real) X(ini_close) X(file_exists) X(file_text_open_write) \
    X(file_text_write_string) X(file_text_close) X(directory_exists) X(directory_create) \
    X(buffer_create) X(buffer_write) X(file_text_open_append) X(file_text_open_read) \
    X(buffer_get_size) X(date_current_datetime) X(file_text_eof) X(buffer_seek) \
    X(date_datetime_string) X(file_text_read_string) X(buffer_tell) X(file_text_readln) \
    X(file_text_writeln) X(buffer_copy) X(buffer_poke) \

// 读 B1 状态区：B1 共享内存（契约 host/host_layout.h）  [已实现，见 host_state_in.cpp]（6 条）
#define HOST_STATE_LIST(X) \
    X(mouse_check_button_released) X(mouse_check_button) X(device_mouse_x_to_gui) X(keyboard_check_pressed) \
    X(device_mouse_y_to_gui) X(mouse_check_button_pressed) \

// **真引擎侧**：绘制 / 输入 / 音频 / 窗口 / 相机 / 房间 / 精灵 / 网络收发 / 系统（71 条）
#define HOST_ENGINE_LIST(X) \
    X(audio_play_sound) X(draw_set_color) X(draw_set_alpha) X(draw_set_font) \
    X(draw_set_halign) X(draw_set_valign) X(draw_rectangle) X(draw_rectangle_color) \
    X(draw_roundrect) X(string_width_ext) X(string_height_ext) X(network_broadcast_active_skill) \
    X(audio_group_set_gain) X(audio_is_paused) X(audio_is_playing) X(sprite_get_width) \
    X(string_width) X(surface_free) X(texture_prefetch) X(view_get_camera) \
    X(window_get_fullscreen) X(window_has_focus) X(window_set_cursor) X(audio_pause_all) \
    X(audio_resume_sound) X(audio_sound_gain) X(camera_set_view_pos) X(draw_notices) \
    X(draw_sprite) X(http_request) X(sprite_get_height) X(string_height) \
    X(surface_exists) X(window_set_fullscreen) X(audio_pause_sound) X(audio_resume_all) \
    X(surface_create) X(surface_set_target) X(audio_group_get_gain) X(draw_clear_alpha) \
    X(draw_line) X(room_restart) X(surface_reset_target) X(window_handle) \
    X(draw_surface) X(gpu_set_tex_filter) X(native_enable_ime) X(sprite_get_name) \
    X(audio_exists) X(draw_text_ext) X(native_disable_ime) X(show_question) \
    X(window_enable_borderless_fullscreen) X(native_stop_mouse_limit) X(url_open) X(native_start_mouse_limit) \
    X(keyboard_get_string) X(draw_text_ext_transformed) X(game_end) X(game_set_speed) \
    X(room_goto) X(object_is_ancestor) X(draw_enable_drawevent) X(camera_get_view_x) \
    X(camera_get_view_y) X(camera_get_view_width) X(camera_get_view_height) X(audio_create_stream) \
    X(room_exists) X(network_destroy) X(network_send_raw) \

// 项目业务：平台已有同名 callbin（或照 GML 搬）→ 转发（72 条）
#define HOST_BIZ_LIST(X) \
    X(send_message) X(show_debug_message) X(clear_deck) X(deck_slot_count) \
    X(get_card_attire_list) X(get_card_craft_rule) X(get_material_amount) X(get_task_data) \
    X(get_task_state) X(is_card_unlocked) X(is_level_completed) X(is_weapon_unlocked) \
    X(load_custom_deck) X(mod_card_vm_deck_refresh) X(refresh_cookbook_list) X(refresh_level_button) \
    X(refresh_task_list) X(save_file) X(save_to_custom_deck) X(shop_list_recharge) \
    X(update_notices) X(VM_FlushHooks) X(add_material_amount) X(add_task_progress) \
    X(edit_task_progress) X(edit_task_state) X(equip_cookbook) X(is_gem_unlocked) \
    X(upgrade_card_shape) X(equip_attire) X(get_task_progress) X(is_attire_unlocked) \
    X(is_task_unlocked) X(unequip_cookbook) X(upgrade_card) X(add_net_id) \
    X(deck_slot_is_empty) X(get_gem_max_level) X(refresh_reward_button) X(remove_task) \
    X(unequip_attire) X(unlock_card) X(unlock_task) X(deck_ensure_size) \
    X(get_gem_craft_rule) X(get_load_audio) X(get_material_info) X(get_plant_shape_data) \
    X(unlock_weapon) X(version_greater) X(deck_slot_max) X(edit_gem_max_level) \
    X(unlock_gem) X(add_to_deck) X(edit_gem_level) X(unlock_attire) \
    X(remove_from_deck) X(get_cookbook_data) X(select_shovel) X(deselect_shovel) \
    X(reset_task_state) X(sort_plants_in_grid) X(get_map_object_data) X(network_broadcast_shovel_remove) \
    X(gui_stack_pop) X(sh_disconnect) X(refresh_task_progress) X(complete_level) \
    X(gui_stack_to) X(mod_sync_props_tick) X(create_battle_slots) X(disconnect) \

// host_fn.cpp 里生成空壳的那批 = 真引擎侧 + 项目业务（143 条）；读 B1 的那 6 条有自己的实现文件，**不在**这里
#define HOST_LIST(X) \
    HOST_ENGINE_LIST(X) \
    HOST_BIZ_LIST(X)
