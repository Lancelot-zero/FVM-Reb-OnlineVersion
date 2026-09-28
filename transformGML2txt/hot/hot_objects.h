// ============================================================
// hot_objects.h —— 各对象热块函数的声明（注册表 hot_table.cpp 要用）
//   函数名约定：hot_<对象简称>_<事件>，跟 kHotBlocks 里的登记一一对应
// ============================================================
#ifndef FVM_HOT_OBJECTS_H
#define FVM_HOT_OBJECTS_H

#include "../vm.h"


// ---- hot_card_mod.cpp ----
bool hot_cardmod_create(Vm& vm);
bool hot_cardmod_step(Vm& vm);
bool hot_cardmod_draw(Vm& vm);
bool hot_cardmod_destroy(Vm& vm);
bool hot_cardmod_mouse(Vm& vm, const char* block);
bool hot_cardmod_click(Vm& vm);
bool hot_cardmod_mouse_enter(Vm& vm);
bool hot_cardmod_mouse_leave(Vm& vm);

// ---- hot_card_parent.cpp ----
bool hot_card_create(Vm& vm);
bool hot_card_destroy(Vm& vm);
bool hot_card_user_event_2(Vm& vm);
bool hot_card_step(Vm& vm);
bool hot_card_draw(Vm& vm);
bool hot_card_draw_64(Vm& vm);
bool hot_card_user_event_0(Vm& vm);

// ---- hot_enemy_mod.cpp ----
bool hot_enemymod_func_init(Vm& vm);
bool hot_enemymod_step(Vm& vm);
bool hot_enemymod_draw(Vm& vm);
bool hot_enemymod_destroy(Vm& vm);
bool hot_enemymod_mouse(Vm& vm, const char* block);
bool hot_enemymod_click(Vm& vm);
bool hot_enemymod_mouse_enter(Vm& vm);
bool hot_enemymod_mouse_leave(Vm& vm);

// ---- hot_enemy_parent.cpp ----
bool hot_enemy_create(Vm& vm);
bool hot_enemy_draw(Vm& vm);
bool hot_enemy_draw_64(Vm& vm);
bool hot_enemy_destroy(Vm& vm);
bool hot_enemy_user_event_0(Vm& vm);
bool hot_enemy_step_2(Vm& vm);
bool hot_enemy_step(Vm& vm);

// ---- hot_homing_bullet_management.cpp ----
bool hot_homing_create(Vm& vm);
bool hot_homing_draw(Vm& vm);
bool hot_homing_step(Vm& vm);

// ---- hot_player_character.cpp ----
bool hot_player_create(Vm& vm);
bool hot_player_step(Vm& vm);
bool hot_player_step_end(Vm& vm);
bool hot_player_draw(Vm& vm);
bool hot_player_user_event_1(Vm& vm);
bool hot_player_user_event_2(Vm& vm);
bool hot_player_mouse_53(Vm& vm);

// ---- hot_bullet_mod.cpp ----
bool hot_bulletmod_create(Vm& vm);
bool hot_bulletmod_step(Vm& vm);
bool hot_bulletmod_draw(Vm& vm);
bool hot_bulletmod_destroy(Vm& vm);
bool hot_bulletmod_collision(Vm& vm);
bool hot_bulletmod_click(Vm& vm);
bool hot_bulletmod_mouse_enter(Vm& vm);
bool hot_bulletmod_mouse_leave(Vm& vm);

// ---- hot_bullet_screen_management.cpp ----
bool hot_bulletscreen_create(Vm& vm);
bool hot_bulletscreen_step(Vm& vm);
bool hot_bulletscreen_draw(Vm& vm);
bool hot_bulletscreen_destroy(Vm& vm);

// ---- hot_card_slot.cpp ----
bool hot_cardslot_create(Vm& vm);
bool hot_cardslot_step(Vm& vm);
bool hot_cardslot_step_end(Vm& vm);
bool hot_cardslot_draw(Vm& vm);
bool hot_cardslot_draw_73(Vm& vm);
bool hot_cardslot_user_event_0(Vm& vm);
bool hot_cardslot_func_select_slot(Vm& vm);
bool hot_cardslot_func_try_place_once(Vm& vm);

// ---- hot_effect_mod.cpp ----
bool hot_effectmod_create(Vm& vm);
bool hot_effectmod_step(Vm& vm);
bool hot_effectmod_draw(Vm& vm);
bool hot_effectmod_destroy(Vm& vm);
bool hot_effectmod_mouse(Vm& vm, const char* block);
bool hot_effectmod_click(Vm& vm);
bool hot_effectmod_mouse_enter(Vm& vm);
bool hot_effectmod_mouse_leave(Vm& vm);

// ---- hot_weapon_mod.cpp ----
bool hot_weaponmod_create(Vm& vm);
bool hot_weaponmod_step(Vm& vm);
bool hot_weaponmod_draw(Vm& vm);
bool hot_weaponmod_destroy(Vm& vm);
bool hot_weaponmod_mouse(Vm& vm, const char* block);
bool hot_weaponmod_click(Vm& vm);
bool hot_weaponmod_mouse_enter(Vm& vm);
bool hot_weaponmod_mouse_leave(Vm& vm);

// ---- hot_gem_mod.cpp ----
bool hot_gemmod_create(Vm& vm);
bool hot_gemmod_step(Vm& vm);
bool hot_gemmod_draw(Vm& vm);
bool hot_gemmod_destroy(Vm& vm);
bool hot_gemmod_mouse(Vm& vm, const char* block);
bool hot_gemmod_click(Vm& vm);
bool hot_gemmod_mouse_enter(Vm& vm);
bool hot_gemmod_mouse_leave(Vm& vm);

#endif // FVM_HOT_OBJECTS_H
