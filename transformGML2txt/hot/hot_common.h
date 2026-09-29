// ============================================================
// hot_common.h —— 热函数公共部分（所有对象共用的小工具）
//
//   hot/ 目录的组织：
//     hot_common.h / hot_common.cpp   共用工具（取值、调用、mod VM、父类事件…）
//     （字符串名字在 ../names.h：S_xxx 的枚举值就是池下标）
//     hot_objects.h                   各对象的热块函数声明（注册表要用）
//     hot_table.cpp                   kHotBlocks 注册表 + run_hot_block/has_hot_block
//     hot_<对象>.cpp                  一个对象一个文件，里面只放它的热块
//
//   加新对象：写 hot_<名字>.cpp → 在 hot_objects.h 声明它的函数 → 在 hot_table.cpp 登记
//   ⚠ 这个头文件里的声明是自动扫 hot/*.cpp 生成的（顺手把"定义在使用之后"的问题也挡掉）
// ============================================================
#ifndef FVM_HOT_COMMON_H
#define FVM_HOT_COMMON_H

#include "../vm.h"

// ── 按池下标干活的几个口子（手上已经有 S_xxx 时用，零哈希）──
Value mk_str_sid(int sid);                       // 字符串值（池里已有，不再查）
bool  str_is_sid(const Value* v, int sid);
Value gv_sid(int sid);                           // 读全局（没有 → undefined）
double gnumv_sid(int sid);
bool   gtrue_sid(int sid);

// ── 同一批 helper 的池下标版（名字是编译期常量 S_xxx 时用，零哈希）──
void   set_num_sid(int inst, int sid, double x);
Value  iv_or_sid(int inst, int sid, double dflt);
void   set_var_sid(int inst, int sid, Value v);
double get_num_sid(int inst, int sid);
void   add_num_sid(int inst, int sid, double d);
void   set_iv_sid(int inst, int sid, int n);
void   add_iv_sid(int inst, int sid, int d);
Value* global_var_sid(int sid);
void   global_var_set_sid(int sid, Value v);
bool   global_exists_sid(int sid);
#include <cmath>
#include <cstdlib>
#include <string>


void viset_any(Vm& vm, Vm* mv, int tgt, const Value& name, const Value& val);
bool hot_cardmod_create(Vm& vm);
void cardmod_enemy_search(Vm& vm, int s, Vm* mv);
void cardmod_batch_set(Vm& vm, Vm* mv, int s, double on, const char* aid, const char* aprop, const char* aval);
bool hot_cardmod_step(Vm& vm);
bool hot_cardmod_draw(Vm& vm);
bool hot_cardmod_destroy(Vm& vm);
bool hot_cardmod_mouse(Vm& vm, const char* block);
bool hot_cardmod_click(Vm& vm);
bool hot_cardmod_mouse_enter(Vm& vm);
bool hot_cardmod_mouse_leave(Vm& vm);
bool hot_card_create(Vm& vm);
bool hot_card_destroy(Vm& vm);
bool hot_card_user_event_2(Vm& vm);
bool hot_card_step(Vm& vm);
bool hot_card_draw(Vm& vm);
bool hot_card_draw_64(Vm& vm);
bool hot_card_user_event_0(Vm& vm);
int iv_of(const Value* v, int dflt = 0);
double num_of(const Value* v);
Value mk_num(double x);
Value mk_str(Vm& vm, const char* s);
void set_num(int inst, const char* name, double x);
Value iv_or(int inst, const char* name, double dflt);
void set_var(int inst, const char* name, Value v);
bool str_is(Vm& vm, const Value* v, const char* s);
int enemy_tbit_of(Vm& vm, const Value* t);
double get_num(int inst, const char* name);
void add_num(int inst, const char* name, double d);
bool truthy(const Value* v);
Value gv(Vm& vm, const char* name);
double gnumv(Vm& vm, const char* name);
bool   gtrue(Vm& vm, const char* name);
bool str_eq(Vm& vm, const Value* a, const Value* b);
void set_iv(int inst, const char* name, int n);
void add_iv(int inst, const char* name, int d);
void call_fn_v(Vm& vm, const char* name, const Value* vals, int argc, int dst);
void call_fn(Vm& vm, const char* name, const int* vals, int argc, int dst);
Value call_ret(Vm& vm, const char* name, const Value* vals, int argc);
Value call_ret_held(Vm& vm, const char* name, const Value* vals, int argc);
Value v_getprop(Vm& vm, int inst, const char* name);
Value v_member(Vm& vm, const Value& obj, const char* name);
bool is_undef(const Value& v);
bool eq_num(const Value& v, int n);
Value m_get(Vm& vm, const Value& map, const char* key);
Value v_array(Vm& vm, const Value& arr, const Value& idx);
Value v_map(Vm& vm, const Value& m, const Value& key);
Value v_struct(Vm& vm, const Value& s, const char* key);
Value v_grid(Vm& vm, const Value& g, const Value& c, const Value& r);
bool hot_inherited(Vm& vm, const char* obj, const char* block);
bool inst_is(int id, int base_sid);
Value array_v(Vm& vm, const Value& arr, int i);
void array_setv(Vm& vm, const Value& arr, int i, const Value& v);
double array_n(Vm& vm, const Value& arr, int i);
void array_setn(Vm& vm, const Value& arr, int i, double x);
void m_set(Vm& vm, const Value& map, const char* key, const Value& v);
void m_setnum(Vm& vm, const Value& map, const char* key, double x);
double m_num(Vm& vm, const Value& map, const char* key);
std::string pool_text(const Value* v);
void call_fn_sid(Vm& vm, int sid, const Value* vals, int argc, int dst);
Value call_ret_sid(Vm& vm, int sid, const Value* vals, int argc);
void call_fn_int_sid(Vm& vm, int sid, const int* vals, int argc, int dst);
Value call_ret_held_sid(Vm& vm, int sid, const Value* vals, int argc);
bool str_is_any(Vm& vm, Vm* other, const Value* v, const char* s);
Vm* mod_vm_of_inst(Vm& vm, int inst, const char* idvar);
bool mod_run_block(Vm* mv, int inst, const char* block, const char* cur_global);
void mod_inst_add_v(Vm* mv, int inst, bool with_list);
void mod_inst_del_v(Vm* mv, int inst, bool with_list);
void mod_base_tick_v(int inst);
double m_or_num(Vm& vm, const Value& map, const char* key, double dflt);
bool is_numv(const Value* v);
bool hot_enemymod_create(Vm& vm);
std::string text_pref_mod(Vm& vm, Vm* mv, const Value& v);
bool inst_event_inherited(Vm& vm, int inst, const char* block);
bool hot_enemymod_func_init(Vm& vm);
bool hot_enemymod_step(Vm& vm);
bool hot_enemymod_draw(Vm& vm);
bool hot_enemymod_destroy(Vm& vm);
bool hot_enemymod_mouse(Vm& vm, const char* block);
bool hot_enemymod_click(Vm& vm);
bool hot_enemymod_mouse_enter(Vm& vm);
bool hot_enemymod_mouse_leave(Vm& vm);
bool inst_event(Vm& vm, int inst, const char* block);
bool hot_enemy_create(Vm& vm);
bool hot_enemy_draw(Vm& vm);
bool hot_enemy_draw_64(Vm& vm);
bool hot_enemy_destroy(Vm& vm);
bool hot_enemy_user_event_0(Vm& vm);
double world_y_of_grid(Vm& vm, double col, double row);
double enemy_anim_frame(Vm& vm, int s, bool attack);
int enemy_find_plant(Vm& vm, int s);
bool hot_enemy_step_2(Vm& vm);
bool hot_enemy_step(Vm& vm);
bool hot_homing_create(Vm& vm);
bool hot_homing_draw(Vm& vm);
bool hot_homing_step(Vm& vm);
Value m_chain_sid(Vm& vm, Value root, const int* keys, int n);   // keys 是池下标数组
double make_weapon_gems(Vm& vm, int s, const Value& gem_list, double gem_index_in);
bool hot_player_create(Vm& vm);
bool hot_player_step(Vm& vm);
bool hot_player_step_end(Vm& vm);
bool hot_player_draw(Vm& vm);
bool hot_player_user_event_1(Vm& vm);
bool hot_player_user_event_2(Vm& vm);
bool hot_player_mouse_53(Vm& vm);
bool obj_known(Vm& vm, const std::string& obj);
bool run_hot_block(Vm& vm, const std::string& obj, const std::string& block);
bool has_hot_block(const std::string& obj, const std::string& block);
bool has_hot_block_sid(int obj_sid, int block_sid);
bool has_hot_object(const std::string& obj);
bool has_hot_object_sid(int obj_sid);
bool run_hot_block_sid(Vm& vm, int obj_sid, int block_sid);
bool has_hot_object(const std::string& obj);


// ── 池下标版（名字是 S_xxx 常量时用）──
Value  m_get_sid(Vm& vm, const Value& map, int sid);
Value  v_struct_sid(Vm& vm, const Value& s, int sid);
Value  v_member_sid(Vm& vm, const Value& obj, int sid);
void   m_set_sid(Vm& vm, const Value& map, int sid, const Value& v);
void   m_setnum_sid(Vm& vm, const Value& map, int sid, double x);
double m_num_sid(Vm& vm, const Value& map, int sid);
double m_or_num_sid(Vm& vm, const Value& map, int sid, double dflt);
bool   mod_run_block_sid(Vm* mv, int inst, int sid, const char* cur_global);
bool   inst_is_sid(int id, int sid);
bool   inst_event_sid(Vm& vm, int inst, int sid);
bool   inst_event_inherited_sid(Vm& vm, int inst, int sid);
bool   hot_inherited_sid(Vm& vm, int obj_sid, int block_sid);
bool   hot_enemymod_mouse_sid(Vm& vm, int sid);
Vm*    mod_vm_of_inst_sid(Vm& vm, int inst, int sid);

bool str_is_any_sid(Vm& vm, Vm* other, const Value* v, int sid);

#endif // FVM_HOT_COMMON_H
