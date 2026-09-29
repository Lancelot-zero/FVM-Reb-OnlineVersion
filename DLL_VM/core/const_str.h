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

// ── 名单本体（顺序 = 契约：池下标 = native / game 的表下标）──
//   真源是编译器 gm2bin.py（它编出的 bin 里烘的就是这些下标）：
//     native = NATIVE_STUBS + NATIVE_IMPL
//     game   = GAME_NAMES（= LabMapCompiler 的 FUNC_DEFS）
//   ⚠ 只能往各段**尾部**加；中间插 / 删 / 重排 → 已编译的 bin 会静默调错函数
#define INNER_FIELD_LIST(X) \
    X(x) X(y) X(sprite_index) X(image_index) \
    X(image_speed) X(image_xscale) X(image_yscale) X(image_alpha) \
    X(image_angle) X(image_blend) X(depth) X(speed) \
    X(direction) X(hspeed) X(vspeed) X(gravity) \
    X(gravity_direction) X(friction) X(visible) \

#define NATIVE_FN_LIST(X) \
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

// native 表尾（后加的都在这里）
#define NATIVE_IMPL_LIST(X) \
    X(run_block) X(ds_list_create) X(ds_list_destroy) X(ds_list_clear) \
    X(ds_list_add) X(ds_list_delete) X(ds_list_size) X(ds_list_find_value) \
    X(ds_list_find_index) X(array_literal) X(array_get) X(array_set) \
    X(array_pop) X(array_clear) X(array_contains) X(ds_map_create) \
    X(ds_map_set) X(ds_map_delete) X(ds_map_size) X(ds_map_clear) \
    X(ds_map_keys_to_array) X(ds_map_destroy) X(struct_get) X(struct_set) \
    X(struct_exists) X(array_push) X(ds_map_exists) X(ds_map_find_value) \
    X(VM_Execute) X(ds_grid_create) X(ds_grid_set) X(ds_grid_get) \
    X(merge_colour) X(ds_grid_width) X(ds_grid_height) X(ds_grid_clear) \
    X(ds_grid_destroy) X(array_length) X(array_get_index) X(array_delete) \
    X(array_resize) X(array_create) X(array_shuffle) X(ds_exists) \
    X(instance_exists) X(instance_number) X(instance_create_depth) X(instance_destroy) \
    X(variable_instance_exists) X(variable_instance_get) X(variable_instance_set) X(floor) \
    X(round) X(abs) X(max) X(min) \
    X(clamp) X(lerp) X(random) X(random_range) \
    X(irandom_range) X(sin) X(cos) X(degtorad) \
    X(lengthdir_x) X(lengthdir_y) X(point_direction) X(point_distance) \
    X(is_real) X(is_string) X(is_array) X(is_struct) \
    X(is_undefined) X(string) X(json_stringify) X(variable_global_exists) \
    X(variable_global_get) X(merge_color) X(damage_enemy) X(calculate_plant_depth) \
    X(get_grid_position_from_world) X(get_world_position_from_grid) X(audio_play_sound) X(send_message) \
    X(show_debug_message) X(run_event_user) X(ceil) X(make_color_rgb) \
    X(draw_set_color) X(draw_set_alpha) X(draw_set_font) X(draw_set_halign) \
    X(draw_set_valign) X(draw_rectangle) X(draw_rectangle_color) X(draw_roundrect) \
    X(string_width_ext) X(string_height_ext) X(network_broadcast_active_skill) X(audio_group_set_gain) \
    X(audio_is_paused) X(audio_is_playing) X(buffer_load) X(clear_deck) \
    X(deck_slot_count) X(ds_map_add) X(get_card_attire_list) X(get_card_craft_rule) \
    X(get_material_amount) X(get_task_data) X(get_task_state) X(is_card_unlocked) \
    X(is_level_completed) X(is_weapon_unlocked) X(load_custom_deck) X(mod_card_vm_deck_refresh) \
    X(refresh_cookbook_list) X(refresh_level_button) X(refresh_task_list) X(save_file) \
    X(save_to_custom_deck) X(shop_list_recharge) X(sprite_get_width) X(string_width) \
    X(surface_free) X(texture_prefetch) X(update_notices) X(view_get_camera) \
    X(window_get_fullscreen) X(window_has_focus) X(window_set_cursor) X(VM_FlushHooks) \
    X(add_material_amount) X(add_task_progress) X(array_copy) X(audio_pause_all) \
    X(audio_resume_sound) X(audio_sound_gain) X(buffer_read) X(camera_set_view_pos) \
    X(draw_notices) X(draw_sprite) X(edit_task_progress) X(edit_task_state) \
    X(equip_cookbook) X(http_request) X(ini_open) X(is_gem_unlocked) \
    X(sprite_get_height) X(string_height) X(string_length) X(surface_exists) \
    X(upgrade_card_shape) X(window_set_fullscreen) X(audio_pause_sound) X(audio_resume_all) \
    X(buffer_delete) X(equip_attire) X(get_task_progress) X(ini_write_real) \
    X(instance_log_enable) X(is_attire_unlocked) X(is_task_unlocked) X(json_parse) \
    X(mouse_check_button_released) X(string_copy) X(surface_create) X(surface_set_target) \
    X(unequip_cookbook) X(upgrade_card) X(add_net_id) X(audio_group_get_gain) \
    X(deck_slot_is_empty) X(draw_clear_alpha) X(draw_line) X(get_gem_max_level) \
    X(ini_close) X(refresh_reward_button) X(remove_task) X(room_restart) \
    X(string_replace_all) X(surface_reset_target) X(unequip_attire) X(unlock_card) \
    X(unlock_task) X(window_handle) X(deck_ensure_size) X(draw_surface) \
    X(get_gem_craft_rule) X(get_load_audio) X(get_material_info) X(get_plant_shape_data) \
    X(gpu_set_tex_filter) X(mouse_check_button) X(native_enable_ime) X(ord) \
    X(sprite_get_name) X(unlock_weapon) X(version_greater) X(audio_exists) \
    X(deck_slot_max) X(draw_text_ext) X(edit_gem_max_level) X(instance_log_disable) \
    X(native_disable_ime) X(show_question) X(unlock_gem) X(window_enable_borderless_fullscreen) \
    X(add_to_deck) X(edit_gem_level) X(native_stop_mouse_limit) X(unlock_attire) \
    X(url_open) X(native_start_mouse_limit) X(remove_from_deck) X(device_mouse_x_to_gui) \
    X(get_cookbook_data) X(keyboard_check_pressed) X(keyboard_get_string) X(device_mouse_y_to_gui) \
    X(draw_text_ext_transformed) X(mouse_check_button_pressed) X(irandom) X(select_shovel) \
    X(variable_global_set) X(deselect_shovel) X(reset_task_state) X(sort_plants_in_grid) \
    X(get_map_object_data) X(network_broadcast_shovel_remove) X(gui_stack_pop) X(instance_find) \
    X(sh_disconnect) X(game_end) X(game_set_speed) X(refresh_task_progress) \
    X(complete_level) X(gui_stack_to) X(real) X(room_goto) \
    X(mod_sync_props_tick) X(object_is_ancestor) X(create_battle_slots) X(draw_enable_drawevent) \
    X(chr) X(string_pos) X(file_exists) X(camera_get_view_x) \
    X(string_delete) X(file_text_open_write) X(file_text_write_string) X(file_text_close) \
    X(camera_get_view_y) X(camera_get_view_width) X(camera_get_view_height) X(audio_create_stream) \
    X(room_exists) X(directory_exists) X(directory_create) X(network_destroy) \
    X(variable_struct_remove) X(buffer_create) X(buffer_write) X(disconnect) \
    X(file_text_open_append) X(file_text_open_read) X(string_starts_with) X(buffer_get_size) \
    X(date_current_datetime) X(file_text_eof) X(buffer_seek) X(date_datetime_string) \
    X(file_text_read_string) X(buffer_tell) X(file_text_readln) X(file_text_writeln) \
    X(buffer_copy) X(buffer_poke) X(network_send_raw) \

#define GAME_FN_LIST(X) \
    X(VM_BanCard) X(VM_SetCardLevelCap) X(VM_SetMaxSlots) X(VM_ShellPrint) \
    X(VM_ShowNotice) X(VM_CreatePlatform) X(VM_SpawnPlant) X(VM_SpawnEnemy) \
    X(VM_SpawnBoss) X(VM_LoadSprite) X(VM_SpawnObject) X(VM_SetProp) \
    X(VM_GetWave) X(VM_GetSubwave) X(VM_GetProp) X(VM_GetLastBoss) \
    X(VM_GetLastCreatedEnemy) X(VM_GetLastKilledEnemy) X(VM_GetLastCreatedCard) X(VM_GetLastDestroyedCard) \
    X(VM_GetFlame) X(VM_SetFlame) X(VM_SetTerrain) X(VM_ClearPlants) \
    X(VM_Random) X(VM_GetLastIdlePlatform) X(VM_SetPlatformParams) X(VM_SetMapBackground) \
    X(VM_SetDrawSlot) X(VM_GetLoadedSpriteName) X(VM_GameWin) X(VM_GameLose) \
    X(VM_SetDrawSlot_front) X(VM_SpawnCats) X(VM_ClearMapObjects) X(VM_BanGem) \
    X(VM_SetRowFeature) X(VM_ClearPlantsByType) X(VM_WakePlants) X(VM_SetCardProp) \
    X(VM_SetEnemyProp) X(VM_ShowNoticeDur) X(VM_SetEventEnabled) X(VM_RefreshPlatformSnapshots) \
    X(VM_GetMouseX) X(VM_GetMouseY) X(VM_GetMouseCol) X(VM_GetMouseRow) \
    X(VM_GetTerrain) X(VM_GetMousePressed) X(VM_GetKeyDown) X(VM_GetKeyPressed) \
    X(VM_GetEnemyCount) X(VM_GetPlantCount) X(VM_GetPlantCountAt) X(VM_PlaySound) \
    X(VM_GetPlantAt) X(VM_SwapPlants) X(VM_SwapPlantRects) X(VM_CompactColumn) \
    X(VM_CompactRow) X(VM_CompactColumnRev) X(VM_CompactRowRev) X(VM_SpawnPlantsRandom) \
    X(VM_CreateButton) X(VM_GetLastClickedButton) X(VM_LoadSpriteFrames) X(VM_ApplyPlantLevel) \
    X(VM_LoadSpritePerm) X(VM_FreeSpritePerm) X(VM_SetCardSlotProp) X(VM_CalcCardSlotProp) \
    X(VM_GetCardSlotCount) X(VM_GetPreviewCard) X(VM_AliasSprite) X(VM_LoadSpriteFrames_Ex) \
    X(VM_GetLastBossStateChangeId) X(VM_GetLastBossOldState) X(VM_GetLastBossNewState) X(VM_ArrayGet) \
    X(VM_ArraySet) X(VM_ArrayDel) X(VM_ArrayADD) X(VM_ArraySize) \
    X(VM_ArrayClear) X(VM_ArrayClearAll) X(VM_SetNoticeStyle) X(VM_conveyor_belt_able) \
    X(VM_Slot_add) X(VM_BanAllCard) X(VM_CannelBanCard) X(VM_BanWeapon) \
    X(VM_BanSuperWeapon) X(VM_BanShield) X(VM_SetCardShapeCap) X(VM_SetCardSkillCap) \
    X(VM_GetKilledProp) X(VM_IsUndefined) X(VM_IsDestroyed) X(VM_LoadSound) \
    X(VM_Floor) X(VM_Ceil) X(VM_SpawnBatMouse) X(VM_GetTimeLimit) \
    X(VM_SetTimeLimit) X(VM_SetDrawSlotEx) X(VM_SetDrawSlotEx_front) X(VM_SetWaveAuto) \
    X(VM_SetWave) X(VM_GetCurCard) X(VM_EnemyInRange) X(VM_GetHomingTarget) \
    X(VM_GetInstancesInRange) X(VM_CreateInstance) X(VM_LoadSpritePerm_Ex) X(VM_SetShovelFlameRate) \
    X(VM_BulletScreenAdd) X(VM_DrawSpriteExt) X(VM_RunStep) X(VM_DestroyInstance) \
    X(VM_GetCardProp) X(VM_CanPlace) X(VM_CallFunc) X(VM_FuncExists) \
    X(VM_FuncDesc) X(VM_SpriteExists) X(VM_AliasSpritePerm) X(VM_ArrayExists) \
    X(VM_CellCount) X(VM_CellItem) X(VM_CellContains) X(VM_ArrayContains) \
    X(VM_InstArrayExists) X(VM_InstArraySize) X(VM_InstArrayItem) X(VM_InstArraySet) \
    X(VM_InstArrayAdd) X(VM_InstArrayDel) X(VM_InstArrayClear) X(VM_InstArrayContains) \
    X(VM_HomingBulletAdd) X(VM_DamageEnemy) X(VM_DamageEnemyAsh) X(VM_BulletScreenAdd_Ex) \
    X(VM_BulletScreenAdd_Exs) X(VM_GetInfo) X(VM_CatInRow) X(VM_MapObj) \
    X(VM_GetInstanceCount) X(VM_GetInstanceAt) \

// ── 其余（hot/ 用到的字符串，以及核心里那几个）── 新名字加这里
#define REST_NAME_LIST(X) \
    X(_FUNC_mod_enemy_init) \
    X(_FUNC_select_slot) \
    X(_FUNC_try_place_once) \
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
    X(_eat_scratch) \
    X(_mod_initialized) \
    X(_mod_nomask_spr) \
    X(_mod_pending_bullet_id) \
    X(_mod_pending_card_id) \
    X(_mod_pending_enemy_id) \
    X(_mod_pending_gem_id) \
    X(_mod_pending_weapon_id) \
    X(_net_before_plant_current_level) \
    X(_net_before_plant_shape) \
    X(_net_before_plant_skill) \
    X(_net_card_equipped_attire_id) \
    X(_stat_add) \
    X(_stat_drop) \
    X(_stat_frame) \
    X(_stat_merge) \
    X(_state_prev) \
    X(add_n) \
    X(air) \
    X(all) \
    X(amount) \
    X(amphi) \
    X(ash_proof) \
    X(atk) \
    X(atk_cycle) \
    X(attack) \
    X(attack_anim) \
    X(attack_cell) \
    X(attack_collision) \
    X(attack_cycle) \
    X(attack_range) \
    X(attack_timer) \
    X(attack_type) \
    X(awake_anim) \
    X(awake_buff_timer) \
    X(banding_card_obj) \
    X(banding_sleep_obj) \
    X(banding_star_obj) \
    X(banding_water_obj) \
    X(banned_gems_online) \
    X(battle_time) \
    X(bbox_left) \
    X(bbox_right) \
    X(bbox_top) \
    X(block_mouse_id_list) \
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
    X(can_dropped) \
    X(can_shovel_remove) \
    X(card) \
    X(card_data) \
    X(card_hpbar) \
    X(card_id) \
    X(card_loss) \
    X(card_mod) \
    X(card_obj) \
    X(card_slot_icon) \
    X(card_spr) \
    X(cell) \
    X(cell_flag) \
    X(cell_row) \
    X(cell_target_col) \
    X(cell_target_row) \
    X(cell_terrain_flag) \
    X(chomp_sound_list) \
    X(clevel) \
    X(client) \
    X(client_able) \
    X(coffee) \
    X(col) \
    X(connected_clients) \
    X(cooldown) \
    X(cooldown_ignore_list) \
    X(cooldown_timer) \
    X(cooling_alpha) \
    X(cost) \
    X(count) \
    X(count_max) \
    X(cshape) \
    X(cskill) \
    X(current_cost) \
    X(current_frozen) \
    X(current_level) \
    X(current_offset) \
    X(cycle) \
    X(damage) \
    X(damage_amount) \
    X(damage_type) \
    X(dance) \
    X(death_anim) \
    X(debug) \
    X(debug_merge) \
    X(depth_group) \
    X(depth_offset) \
    X(depth_value) \
    X(description) \
    X(destroy_timer) \
    X(difficulty) \
    X(diver) \
    X(drop_n) \
    X(dwarf) \
    X(dx_arr) \
    X(dy_arr) \
    X(eat_order) \
    X(enemy_array) \
    X(enemy_array_left) \
    X(enemy_hpbar) \
    X(enemy_id) \
    X(enemy_map) \
    X(equipped_cookbook) \
    X(equipped_items) \
    X(feature) \
    X(feature_type) \
    X(first_produce) \
    X(first_produce_delay) \
    X(flame) \
    X(flame_produce) \
    X(flash_color) \
    X(flash_speed) \
    X(flash_value) \
    X(font_number) \
    X(font_pixel) \
    X(font_song) \
    X(font_yuan) \
    X(frame_count) \
    X(frozen_timer) \
    X(game_over) \
    X(gems) \
    X(get_card_info) \
    X(get_plant_data) \
    X(giant_type) \
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
    X(has_enemy_air) \
    X(has_enemy_dance) \
    X(has_enemy_diver) \
    X(has_enemy_normal) \
    X(has_enemy_obstacle) \
    X(has_enemy_underground) \
    X(health_gem) \
    X(helmet_hp) \
    X(helmet_max_hp) \
    X(hit_effect_2) \
    X(hit_sound) \
    X(hover_alpha) \
    X(hp) \
    X(hp_change) \
    X(hp_change_mod) \
    X(hp_increase) \
    X(hp_modified) \
    X(hp_modify) \
    X(hurt_rate) \
    X(hurt_time) \
    X(ice_cream) \
    X(ice_ok) \
    X(ice_sprite) \
    X(ice_timer) \
    X(id) \
    X(idle_anim) \
    X(immune_to_ash) \
    X(info_got) \
    X(invincible) \
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
    X(jump_times) \
    X(keybind_map) \
    X(king_bun) \
    X(king_tbun) \
    X(laboretory_room) \
    X(land) \
    X(left_move_flashs) \
    X(length) \
    X(level) \
    X(level_file) \
    X(lilypad) \
    X(magic_chicken) \
    X(main_weapon) \
    X(map_id) \
    X(map_instance_id_net_id) \
    X(mask_index) \
    X(max_bullets) \
    X(max_hp) \
    X(maxhp) \
    X(merge1_n) \
    X(merge2_n) \
    X(merge_enable) \
    X(merge_lv1) \
    X(merge_lv2) \
    X(merge_n) \
    X(mod) \
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
    X(modif_calc) \
    X(modif_type) \
    X(mouse_id) \
    X(mouse_train_1) \
    X(mouse_x) \
    X(mouse_y) \
    X(move_anim) \
    X(move_axis) \
    X(move_speed) \
    X(move_speed_modify) \
    X(move_time) \
    X(moving) \
    X(multiply) \
    X(name) \
    X(net_player_id) \
    X(network) \
    X(night) \
    X(none) \
    X(norm) \
    X(norm_attack) \
    X(normal) \
    X(obj) \
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
    X(origin_cycle) \
    X(parent_card) \
    X(parent_player) \
    X(parent_slot) \
    X(pierce) \
    X(place_preview) \
    X(plant_id) \
    X(plant_type) \
    X(platform_grid_lock) \
    X(player) \
    X(plus) \
    X(plus_card_map) \
    X(pre_hp) \
    X(pre_state) \
    X(prev_grid_col) \
    X(prev_grid_row) \
    X(prev_place_id) \
    X(prev_place_id_shape) \
    X(preview_alpha) \
    X(preview_sprite) \
    X(quick_placement) \
    X(random_count) \
    X(random_seed) \
    X(range) \
    X(replace_placement) \
    X(room_height) \
    X(room_width) \
    X(row) \
    X(save_data) \
    X(scare_timer) \
    X(secondary_weapon) \
    X(selected_preview) \
    X(selected_slot) \
    X(server) \
    X(shader_hit) \
    X(shape) \
    X(shield) \
    X(shield_hp) \
    X(shield_max_hp) \
    X(shield_outer) \
    X(skill) \
    X(skill_change_style) \
    X(skill_choose) \
    X(skill_move_type) \
    X(skill_timer) \
    X(slot_index) \
    X(snd_bullet_burnt) \
    X(snd_card_lift) \
    X(snd_enter_water) \
    X(snd_hit1) \
    X(snd_lose) \
    X(snd_mouse_unfreeze) \
    X(snd_place1) \
    X(special_ash) \
    X(spr) \
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
    X(sprite_height) \
    X(sprite_width) \
    X(start_col) \
    X(start_row) \
    X(state) \
    X(stun_sprite) \
    X(stun_timer) \
    X(super_weapon) \
    X(target_card) \
    X(target_coord) \
    X(target_plant) \
    X(target_pos) \
    X(target_type) \
    X(tbit) \
    X(tbun) \
    X(timer) \
    X(tower_cake) \
    X(type) \
    X(u_flashColor) \
    X(u_progress) \
    X(underground) \
    X(upgrade) \
    X(vector_x) \
    X(version) \
    X(visual_x_shift) \
    X(visual_y_shift) \
    X(vm_card_spr) \
    X(vm_card_spr_addx) \
    X(vm_card_spr_addy) \
    X(vm_current_cost) \
    X(vm_place_preview) \
    X(vx) \
    X(vy) \
    X(wait) \
    X(water) \
    X(water_effect_timer) \
    X(weapon_id) \
    X(width) \
    X(x_move_speed) \
    X(y_move) \
    X(y_move_speed) \
    X(_VM_CONST_INIT) \
    X(_CALL_ENTRY) \
    X(array) \
    X(float) \
    X(grid) \
    X(int) \
    X(list) \
    X(null) \
    X(rb) \
    X(shield_inner) \
    X(undefined) \
    X(unknown) \
    X(map_struct) \
    X(obj_gem_mod) \
    X(obj_effect_mod) \
    X(_mod_pending_effect_id) \
    X(mod_follow_instance) \
    X(mod_prestep_dx) \
    X(mod_prestep_dy) \
    X(weapon_info) \
    X(atk_impact) \
    X(attack_gem) \
    X(gem_id) \
    X(gem_info) \
    X(gem_level) \
    X(tooltip_text) \
    X(clickable) \
    X(clicked) \
    X(on_click) \
    X(icon) \
    X(first_cooldown) \
    X(_mod_pending_gem_level) \
    X(mod_step_card_area) \
    X(font_hei) \
    X(bbox_bottom) \
    X(cool) \
    X(enemy_area) \
    X(enemy_area_mod) \
    X(card_area) \
    X(card_area_mod) \
    X(_VM_last_created_enemy) \
    X(_VM_last_killed_enemy) \
    X(_VM_last_clicked_button) \
    X(_VM_wave) \
    X(_VM_shovel_flame_rate) \
    /* ↓ game_b.cpp 原来那份文件内懒加载名单（L(sid,"名字")）搬进来的；格式照 gen_rest_names.py，只许往后加 */ \
    X(_VM_sync_exec) X(_VM_cur_block) X(banned_cards_online) X(plant_registry) \
    X(max_level) X(max_shape) X(_file_cache_json_path) X(_VM_loaded_sprite_indices) \
    X(_VM_sprite_temp_cache) X(_pid_reverse) X(_VM_sprite_cache) X(_VM_perm_alias_names) \
    X(_sprite_cache) X(_sprite_state) X(map_sprite_target) X(map_fade_alpha) \
    X(map_fade_step) X(_audio_cache) X(_audio_reverse) X(_VM_prev_subwave) \
    X(_VM_cur_dead_snap) X(object_name) X(_object_name) X(time_limit) \
    X(_VM_wave_auto) X(obj_mouse_ash_death) X(enemy) X(weapon) \
    X(weapon_pool) X(gem) X(gems_pool)

// ── 占位接口（翻译器要、实现还没写）：先留 id 让转换/加载能过 ──
//   ⚠ 顺序 = native 段下标，只能往尾部加
#define PLACEHOLDER_FN_LIST(X) \
    X(audio_group_set_gain) \
    X(audio_is_paused) \
    X(audio_is_playing) \
    X(buffer_load) \
    X(clear_deck) \
    X(deck_slot_count) \
    X(ds_map_add) \
    X(get_card_attire_list) \
    X(get_card_craft_rule) \
    X(get_material_amount) \
    X(get_task_data) \
    X(get_task_state) \
    X(is_card_unlocked) \
    X(is_level_completed) \
    X(is_weapon_unlocked) \
    X(load_custom_deck) \
    X(mod_card_vm_deck_refresh) \
    X(refresh_cookbook_list) \
    X(refresh_level_button) \
    X(refresh_task_list) \
    X(save_file) \
    X(save_to_custom_deck) \
    X(shop_list_recharge) \
    X(sprite_get_width) \
    X(string_width) \
    X(surface_free) \
    X(texture_prefetch) \
    X(update_notices) \
    X(view_get_camera) \
    X(window_get_fullscreen) \
    X(window_has_focus) \
    X(window_set_cursor) \
    X(VM_FlushHooks) \
    X(add_material_amount) \
    X(add_task_progress) \
    X(array_copy) \
    X(audio_pause_all) \
    X(audio_resume_sound) \
    X(audio_sound_gain) \
    X(buffer_read) \
    X(camera_set_view_pos) \
    X(draw_notices) \
    X(draw_sprite) \
    X(edit_task_progress) \
    X(edit_task_state) \
    X(equip_cookbook) \
    X(http_request) \
    X(ini_open) \
    X(is_gem_unlocked) \
    X(sprite_get_height) \
    X(string_height) \
    X(string_length) \
    X(surface_exists) \
    X(upgrade_card_shape) \
    X(window_set_fullscreen) \
    X(audio_pause_sound) \
    X(audio_resume_all) \
    X(buffer_delete) \
    X(equip_attire) \
    X(get_task_progress) \
    X(ini_write_real) \
    X(instance_log_enable) \
    X(is_attire_unlocked) \
    X(is_task_unlocked) \
    X(json_parse) \
    X(mouse_check_button_released) \
    X(string_copy) \
    X(surface_create) \
    X(surface_set_target) \
    X(unequip_cookbook) \
    X(upgrade_card) \
    X(add_net_id) \
    X(audio_group_get_gain) \
    X(deck_slot_is_empty) \
    X(draw_clear_alpha) \
    X(draw_line) \
    X(get_gem_max_level) \
    X(ini_close) \
    X(refresh_reward_button) \
    X(remove_task) \
    X(room_restart) \
    X(string_replace_all) \
    X(surface_reset_target) \
    X(unequip_attire) \
    X(unlock_card) \
    X(unlock_task) \
    X(window_handle) \
    X(deck_ensure_size) \
    X(draw_surface) \
    X(get_gem_craft_rule) \
    X(get_load_audio) \
    X(get_material_info) \
    X(get_plant_shape_data) \
    X(gpu_set_tex_filter) \
    X(mouse_check_button) \
    X(native_enable_ime) \
    X(ord) \
    X(sprite_get_name) \
    X(unlock_weapon) \
    X(version_greater) \
    X(audio_exists) \
    X(deck_slot_max) \
    X(draw_text_ext) \
    X(edit_gem_max_level) \
    X(instance_log_disable) \
    X(native_disable_ime) \
    X(show_question) \
    X(unlock_gem) \
    X(window_enable_borderless_fullscreen) \
    X(add_to_deck) \
    X(edit_gem_level) \
    X(native_stop_mouse_limit) \
    X(unlock_attire) \
    X(url_open) \
    X(native_start_mouse_limit) \
    X(remove_from_deck) \
    X(device_mouse_x_to_gui) \
    X(get_cookbook_data) \
    X(keyboard_check_pressed) \
    X(keyboard_get_string) \
    X(device_mouse_y_to_gui) \
    X(draw_text_ext_transformed) \
    X(mouse_check_button_pressed) \
    X(select_shovel) \
    X(variable_global_set) \
    X(deselect_shovel) \
    X(reset_task_state) \
    X(sort_plants_in_grid) \
    X(get_map_object_data) \
    X(network_broadcast_shovel_remove) \
    X(gui_stack_pop) \
    X(instance_find) \
    X(sh_disconnect) \
    X(game_end) \
    X(game_set_speed) \
    X(refresh_task_progress) \
    X(complete_level) \
    X(gui_stack_to) \
    X(real) \
    X(room_goto) \
    X(mod_sync_props_tick) \
    X(object_is_ancestor) \
    X(create_battle_slots) \
    X(draw_enable_drawevent)

// ── 后补的真实现（不是占位，函数体在 .cpp 里；顺序 = native 段下标，只能往尾部加）──
#define NATIVE_EXTRA_LIST(X) \
    X(irandom) \
    X(chr) \
    X(string_pos) \
    X(file_exists) \
    X(camera_get_view_x) \
    X(string_delete) \
    X(file_text_open_write) \
    X(file_text_write_string) \
    X(file_text_close) \
    X(camera_get_view_y) \
    X(camera_get_view_width) \
    X(camera_get_view_height) \
    X(audio_create_stream) \
    X(room_exists) \
    X(directory_exists) \
    X(directory_create) \
    X(network_destroy) \
    X(variable_struct_remove) \
    X(buffer_create)

// ── 全清单：段顺序 = 池顺序 ──
#define NAME_LIST(X) \
    X(empty) \
    NATIVE_FN_LIST(X) \
    NATIVE_IMPL_LIST(X) \
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
