// ============================================================
// game_b.cpp — 游戏 API（批次 B：卡片 / 精灵·音效 / 波次·结算 / 输入，46 条）
//
//   A 为什么有这些文件：game_api.cpp 只生成 150 条空壳（名字/顺序是契约），
//   实现写成 impl_XXX 放在这儿，再由 patch_game_fns_b() 挂进 kGameFns
//   （fn_table.cpp 的 patch_game_fns() 会按顺序调这三个钩子）。
//
//   本批要什么：要 host 表（g_host_draw / g_host_in / g_host_net），精灵·音效那几条还要 global.* 里的映射表
//
//   写法（照 game_self.cpp 的样子）：
//     static void impl_XXX(Value* mem, const uint8_t* args, int argc, int dst) { … }   // static：本文件私有
//     void patch_game_fns_b() { kGameFns[S_VM_XXX - SID_GAME_BASE] = impl_XXX; … }
//
//   ⚠ 只改本文件！批次之间、和 outer.h / fn_table.cpp / 表名单都别动（会互相踩）。
//   ⚠ 实参就是**槽位**（不是内存地址）：arg_v(mem, args, k) 取值，argslot(args, k) 只取槽号。
//   ⚠ 语义真源是 GML 参考：scripts/scr_command_VM/scr_command_VM.gml（那 150 条的正版实现），
//     老 C++ 有实现的在 transformGML2txt/vm_api.cpp。两条不一致时以 GML 参考为准。
//
//   本批名单（46 条；实现一条对一条）：
// ============================================================
#include "outer.h"
#include "../host/host.h"   // 本批要用宿主表（g_host_draw / g_host_in）和 host_file_exists
#include <cmath>
#include <cstdio>       // snprintf：按键名表 / GetInfo 的数字键
#include <string>
#include <vector>

#define GAME_B_LIST(X) \
    X(VM_BanCard) X(VM_SetCardProp) X(VM_ApplyPlantLevel) X(VM_SetCardSlotProp) \
    X(VM_CalcCardSlotProp) X(VM_GetCardSlotCount) X(VM_GetPreviewCard) X(VM_conveyor_belt_able) \
    X(VM_Slot_add) X(VM_BanAllCard) X(VM_CannelBanCard) X(VM_GetCardProp) \
    X(VM_LoadSprite) X(VM_SetMapBackground) X(VM_GetLoadedSpriteName) X(VM_PlaySound) \
    X(VM_LoadSpriteFrames) X(VM_LoadSpritePerm) X(VM_FreeSpritePerm) X(VM_AliasSprite) \
    X(VM_LoadSpriteFrames_Ex) X(VM_LoadSound) X(VM_LoadSpritePerm_Ex) X(VM_DrawSpriteExt) \
    X(VM_SpriteExists) X(VM_AliasSpritePerm) X(VM_GetSubwave) X(VM_GameWin) \
    X(VM_GameLose) X(VM_GetKilledProp) X(VM_GetTimeLimit) X(VM_SetTimeLimit) \
    X(VM_SetWaveAuto) X(VM_BulletScreenAdd) X(VM_DamageEnemy) X(VM_DamageEnemyAsh) \
    X(VM_BulletScreenAdd_Ex) X(VM_BulletScreenAdd_Exs) X(VM_GetInfo) X(VM_GetMouseX) \
    X(VM_GetMouseY) X(VM_GetMouseCol) X(VM_GetMouseRow) X(VM_GetMousePressed) \
    X(VM_GetKeyDown) X(VM_GetKeyPressed) \

// ============================================================
//  本批实现的总说明（哪些照 GML、哪些绕不过去、哪些必须等宿主）
//
//  ① 调"转发类 native"（79 条名单里没有 C++ 实现的那批）走同一条路：
//     fwd_call(名字池下标, 实参…) 就是内层的 callbin_call —— 和 forward.cpp 的
//     VM_CallFunc(名字, 实参…) 完全等价。本批用到：asset_get_index / get_load_sprite /
//     sprite_add / sprite_exists / deck_get_card_data / get_card_info_simple /
//     network_apply_plant_level / bullet_screen_add_Ex / bullet_screen_add_Exs。
//     这些名字**不在** g_call_dict 里时 fwd_call 直接回 undefined（**不**置错误码）。
//  ② 调"宿主类 native"（HOST_LIST 那 180 条，host_fn.cpp 里现在是空壳）走 host_call：
//     实参摆进外层临时槽 → 拼槽号表 → 调 kNativeFns[sid]。空壳的"没接"信号
//     （VM_E_NOT_IMPL + undefined）就是设计好的那套，这里照着调，不自己判空。
//     本批用到：audio_play_sound / audio_create_stream / send_message / get_grid_position_from_world。
//  ③ 输入 / 绘制直接走 host.h 的表（g_host_in / g_host_draw），没接就是全假 / 不画。
//  ④ 名字：本批要的全局名 / 属性名都在 const_str.h 的 REST_NAME_LIST 里（S_xxx），
//     装载时由 seed_names() 统一灌池 —— 本文件里**不写字面量**。
//     （原来这里是文件内懒加载 L(sid, "名字")，intern 出来的下标和 S_xxx 本来就相同，
//      换成常量后行为一模一样，只是"同一份文本两个真源"没了。）
//     下面代码里用到的名字都列在文件中部那份"名字 → GML 出处"表里。
// ============================================================

namespace {

// ── 数字结果写槽：整数存 INT、否则 FLOAT（和 game_self.cpp / builtin.cpp 一个规矩）──
Value mk_num_v(VM_FLOAT x) {
    if (std::floor(x) == x && x >= (VM_FLOAT)-2147483647.0 && x <= (VM_FLOAT)2147483647.0)
        return mk_int((VM_INT)x);
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}
void wr_num(Value* mem, int dst, VM_FLOAT x) { wr_v(mem, dst, mk_num_v(x)); }

// 第 k 个实参当"名字"（字符串）→ 池下标；不是字符串 / 不在池里 → -1
VM_INT name_arg(Value* mem, const uint8_t* args, int k) {
    Value v = arg_v(mem, args, k);
    if (v.t != MEM_STRING || !str_ok(v.v)) return -1;
    return v.v;
}
// 第 k 个实参当整数（浮点截断；和 game_self 的 num_arg 一个口径）
VM_INT num_arg(Value* mem, const uint8_t* args, int k) { return (VM_INT)arg_num(mem, args, k); }

bool is_num(const Value& v) { return v.t == MEM_INT || v.t == MEM_FLOAT; }
VM_FLOAT numf(const Value& v) { return (v.t == MEM_FLOAT) ? f_of(v.v) : (VM_FLOAT)v.v; }

// GML 的 `if (x)`：数字看值，undefined / 字符串在 GML 里当假
bool truthy(const Value& v) {
    if (v.t == MEM_INT)   return v.v != 0;
    if (v.t == MEM_FLOAT) return f_of(v.v) != (VM_FLOAT)0;
    return false;
}
// GML 的 `x != 0`：数字看值；undefined / 字符串在 GML 里也 != 0（都算真）
bool nonzero(const Value& v) {
    if (v.t == MEM_INT)   return v.v != 0;
    if (v.t == MEM_FLOAT) return f_of(v.v) != (VM_FLOAT)0;
    return true;
}

// 本批要的全局名 / 属性名都在 const_str.h 的 REST_NAME_LIST 里（S_xxx）：
//   seed_names() 装载时按名单灌池 → 这些下标和"运行时 intern 一次"本来就是同一个
//   （intern_str 对已在池里的名字返回同一个下标），所以直接写常量、不再懒加载。
//   名字 → GML 出处（原来写在各个 n_xxx 声明上；文本真源只有名单一处）：
//     S_banned_cards_online / S_plant_registry / S_weapon_pool / S_gems_pool  = global.<同名>
//     S__VM_* / S__sprite_cache / S__sprite_state / S__audio_* / S__file_cache_json_path
//       / S__pid_reverse / S__VM_loaded_sprite_indices / S__VM_perm_alias_names = global.<同名>
//     S_map_sprite_target / S_map_fade_alpha / S_map_fade_step = global.map_*（换名了）
//     S_time_limit = obj_battle.time_limit    S_max_level / S_max_shape = 卡存档属性名
//     S_object_name = GML 属性 object_name    S__object_name = 销毁快照里的 _object_name
//     S_obj_mouse_ash_death = 对象名 obj_mouse_ash_death
//     S_card / S_enemy / S_weapon / S_gem = VM_GetInfo 的 kind 参数

// ============================================================
// 全局变量 / 容器 / 实例变量
// ============================================================
typedef std::map<Value, Value, ValueLess> MapVV;
typedef MapVV::iterator                   MapIt;

Value* gvar(VM_INT sid) { return (sid < 0) ? NULL : global_var_sid(sid); }

// global.<g_sid> 是"活着的"这种容器 → 池下标；没有 / 类型不对 / 已经死了 → -1
int gcont_of(VM_INT g_sid, VM_INT type) {
    if (g_sid < 0) return -1;
    Value* p = gvar(g_sid);
    if (!p || p->t != type || !container_alive(*p)) return -1;
    return (int)p->v;
}
int gmap_of(VM_INT s) { return gcont_of(s, MEM_MAP); }
int garr_of(VM_INT s) { return gcont_of(s, MEM_ARRAY); }
int ggri_of(VM_INT s) { return gcont_of(s, MEM_GRID); }

// 全局数组：没有 / 不是活数组 → 新建一个塞进全局（账照 game_self.cpp 的 ia_global）
int garr_make(VM_INT g_sid) {
    int a = garr_of(g_sid);
    if (a >= 0) return a;
    if (g_sid < 0) return -1;
    Value self{MEM_ARRAY, arr_new()};
    global_var_set_sid(g_sid, self);     // 全局表接手一份
    release(self);                       // 创造者那份交出去
    return (int)self.v;
}

// ── 直接动 ds_map 池（键和值的引用计数在这儿结，和 container.cpp 的 ds_map_* 同一套）──
bool gmap_has(int m, const Value& k) {
    if (m < 0) return false;
    return g_map_pool[(size_t)m].find(k) != g_map_pool[(size_t)m].end();
}
// 读：没有 → undefined。返回的是**借来**的引用（要写槽请走 wr_v，它会 retain）
Value gmap_get(int m, const Value& k) {
    if (m < 0) return mk_undef();
    MapVV& mp = g_map_pool[(size_t)m];
    MapIt it = mp.find(k);
    return (it == mp.end()) ? mk_undef() : it->second;
}
void gmap_set(int m, const Value& k, const Value& v) {
    if (m < 0) return;
    Value self{MEM_MAP, m};
    if (!allow_store(k, self) || !allow_store(v, self)) return;
    MapVV& mp = g_map_pool[(size_t)m];
    MapIt it = mp.find(k);
    if (it != mp.end()) {
        Value ov = it->second;
        retain(v);
        if (ov.t != v.t || ov.v != v.v) release(ov);
        it->second = v;
    } else {
        retain(k); retain(v);
        mp[k] = v;
    }
}
// ds_map_add 的口径：键已经在就不动老值
void gmap_add(int m, const Value& k, const Value& v) {
    if (m < 0) return;
    Value self{MEM_MAP, m};
    if (!allow_store(k, self) || !allow_store(v, self)) return;
    MapVV& mp = g_map_pool[(size_t)m];
    if (mp.find(k) != mp.end()) return;
    retain(k); retain(v);
    mp[k] = v;
}
void gmap_del(int m, const Value& k) {
    if (m < 0) return;
    MapVV& mp = g_map_pool[(size_t)m];
    MapIt it = mp.find(k);
    if (it == mp.end()) return;
    release(it->first);
    release(it->second);
    mp.erase(it);
}

// ── 数组小工具（池元素，引用计数的账在这儿结）──
VM_INT arr_index_of(int a, const Value& v) {
    if (a < 0) return -1;
    const std::vector<Value>& arr = g_arr_pool[(size_t)a];
    for (size_t i = 0; i < arr.size(); i++)
        if (arr[i].t == v.t && arr[i].v == v.v) return (VM_INT)i;
    return -1;
}
void arr_push(int a, const Value& v) {
    if (a < 0) return;
    if (!allow_store(v, Value{MEM_ARRAY, a})) return;
    retain(v);
    g_arr_pool[(size_t)a].push_back(v);
}

// 实例变量读（没有 → undefined；**不加引用**，写槽走 wr_v）
Value iv_get(Instance& in, VM_INT sid) {
    if (sid < 0) return mk_undef();
    Value* p = inst_prop(in, sid);
    return p ? *p : mk_undef();
}

// ============================================================
// 调 native / callbin 的两条路（见文件头 ①②）
// ============================================================
bool fwd_has(VM_INT fn_sid) {
    return fn_sid >= 0 && g_call_dict.find(fn_sid) != g_call_dict.end();
}
Value fwd_call(VM_INT fn_sid, const Value* argv, int n) {
    if (!fwd_has(fn_sid)) return mk_undef();
    return callbin_call(fn_sid, argv, n);
}

// 本文件自己的临时槽：从 OUTER_TMP_SLOT+9 起（forward.cpp 的 native_forward 占 +0..+8，别踩）
static const int GB_TMP = OUTER_TMP_SLOT + 9;

// 调 HOST_LIST 里"等宿主填"的那批：实参摆进临时槽 → 拼槽号表 → 返回值也从临时槽接。
//   返回的那份引用归调用方（拿完记得 release）。
Value host_call(VM_INT fn_sid, Value* mem, const Value* argv, int n) {
    Fn f = fn_of_sid(fn_sid);
    if (!f) return mk_undef();
    if (n < 0) n = 0;
    if (n > 4) n = 4;
    int32_t tbl[5];
    for (int i = 0; i < n; i++) {
        slot_write(mem, GB_TMP + i, argv[i]);
        tbl[i] = GB_TMP + i;
    }
    const int rdst = GB_TMP + 4;
    slot_write(mem, rdst, mk_int(0));
    f(mem, (const uint8_t*)tbl, n, rdst);
    Value r = mem[rdst];
    retain(r);                                       // 先把结果拿到手，再清临时槽
    for (int i = 0; i <= 4; i++) slot_write(mem, GB_TMP + i, mk_int(0));
    return r;
}

// ============================================================
// 网络闸门（GML 的 global.network.* / global._VM_sync_exec）
//   这几条在 GML 里到处都是，本批的写操作全靠它们决定"这次写能不能在本地落"
// ============================================================
bool net_mode_is(VM_INT mode_sid) {
    int nm = gmap_of(S_network);
    if (nm < 0) return false;
    Value m = gmap_get(nm, mk_str_sid(S_mode));
    return (m.t == MEM_STRING && m.v == mode_sid);
}
bool net_client() { return net_mode_is(S_client); }
bool net_server() { return net_mode_is(S_server); }

bool sync_exec() {
    Value* p = gvar(S__VM_sync_exec);
    return p && truthy(*p);
}

// GML 的 vm_inst_write_ok 里那句"_OBJECT_* 块里的写一律本地直改"
bool in_object_block() {
    Value* p = gvar(S__VM_cur_block);
    if (!p || p->t != MEM_STRING || !str_ok(p->v)) return false;
    const std::string& t = str_at(p->v);
    return t.compare(0, 7, "_OBJECT") == 0;          // 等价于 string_pos("_OBJECT", …) == 1
}

// global.network.map_instance_id_net_id 里有没有这个实例（联网实例才有）
bool net_has_net_id(VM_INT inst) {
    int nm = gmap_of(S_network);
    if (nm < 0) return false;
    Value mm = gmap_get(nm, mk_str_sid(S_map_instance_id_net_id));
    if (mm.t != MEM_MAP || !container_alive(mm)) return false;
    return gmap_has((int)mm.v, mk_int(inst));
}

// GML 的 vm_inst_write_ok：这次写能不能在本地执行
bool inst_write_ok(VM_INT inst) {
    if (!inst_get(inst)) return false;
    if (in_object_block()) return true;
    if (!net_client()) return true;
    if (!sync_exec()) return true;
    return !net_has_net_id(inst);
}

// 给所有已连接客户端发一条消息（GML 的 for + send_message(cl, MSG_*, data)）
void net_send_all(Value* mem, int kind, const Value& data) {
    int nm = gmap_of(S_network);
    if (nm < 0) return;
    Value cl = gmap_get(nm, mk_str_sid(S_connected_clients));
    size_t n = 0;
    if      (cl.t == MEM_ARRAY && container_alive(cl)) n = g_arr_pool[(size_t)cl.v].size();
    else if (cl.t == MEM_LIST  && container_alive(cl)) n = g_list_pool[(size_t)cl.v].size();
    else return;
    for (size_t i = 0; i < n; i++) {
        Value sock = (cl.t == MEM_ARRAY) ? g_arr_pool[(size_t)cl.v][i] : g_list_pool[(size_t)cl.v][i];
        Value a[3] = { sock, mk_int(kind), data };
        Value r = host_call(S_send_message, mem, a, 3);
        release(r);
    }
}

// ============================================================
// 实例 / 精灵 / 音频的小工具
// ============================================================
// 建一个对象实例（照 game_self.cpp 的 VM_CreateInstance：挂对象自己的 VM，没有才退回调用方）
VM_INT spawn_obj(VM_INT obj_sid, VM_FLOAT x, VM_FLOAT y, VM_INT depth) {
    if (obj_sid < 0) return NOONE;
    vm_bin* owner = vm_of_object_sid(obj_sid);
    if (!owner) owner = cur_vm();
    if (!owner) return NOONE;
    VM_INT nid = inst_new(*owner, obj_sid);
    inst_var_set_sid(nid, S_x,     mk_num_v(x));
    inst_var_set_sid(nid, S_y,     mk_num_v(y));
    inst_var_set_sid(nid, S_depth, mk_int(depth));
    return nid;
}

// 资源名 → 句柄（asset_get_index 那条转发路）；拿不到 → -1
VM_INT asset_of(const char* name) {
    Value a[1] = { mk_str_sid(intern_str(name)) };
    Value h = fwd_call(S_asset_get_index, a, 1);
    VM_INT v = is_num(h) ? (VM_INT)numf(h) : -1;
    release(h);
    return v;
}

// 贴图名（池下标）→ 精灵句柄：走 get_load_sprite 那条解析链（游戏侧提供）
VM_INT sprite_of_name(VM_INT name_sid) {
    if (name_sid < 0) return -1;
    Value a[1] = { mk_str_sid(name_sid) };
    Value r = fwd_call(S_get_load_sprite, a, 1);
    VM_INT spr = is_num(r) ? (VM_INT)numf(r) : -1;
    release(r);
    return spr;
}

// 精灵句柄还活着吗？——问转发类 native sprite_exists（游戏侧提供）。
//   那张表没接时只能"按非负数字句柄当活着"：否则 VM_DrawSpriteExt 会永远回 -1、什么都不画。
bool sprite_alive(const Value& v) {
    if (!is_num(v)) return false;
    VM_INT spr = (VM_INT)numf(v);
    if (fwd_has(S_sprite_exists)) {
        Value a[1] = { mk_int(spr) };
        Value r = fwd_call(S_sprite_exists, a, 1);
        bool ok = truthy(r);
        release(r);
        return ok;
    }
    return spr >= 0;
}

// global.<cache> 里查名字 → 命中给句柄
bool cache_get(VM_INT cache_g_sid, const Value& key, VM_INT* out) {
    int m = gmap_of(cache_g_sid);
    if (m < 0) return false;
    Value v = gmap_get(m, key);
    if (!is_num(v)) return false;
    *out = (VM_INT)numf(v);
    return true;
}
void cache_put(VM_INT cache_g_sid, const Value& key, const Value& v) {
    int m = gmap_of(cache_g_sid);
    if (m >= 0) gmap_set(m, key, v);
}

// 一条音效：名字 → asset_get_index；名字不是字符串时，数字本身就当本地音频 id（照 GML VM_PlaySound）
void play_sound_value(Value* mem, const Value& name) {
    VM_INT snd = -1;
    if (name.t == MEM_STRING) {
        Value a[1] = { name };
        Value h = fwd_call(S_asset_get_index, a, 1);
        if (is_num(h)) snd = (VM_INT)numf(h);
        release(h);
    } else if (is_num(name)) {
        snd = (VM_INT)numf(name);
    }
    if (snd == -1) return;
    Value b[3] = { mk_int(snd), mk_int(0), mk_int(0) };
    Value r = host_call(S_audio_play_sound, mem, b, 3);
    release(r);
}
void play_sound_asset(Value* mem, const char* name) {
    play_sound_value(mem, mk_str_sid(intern_str(name)));
}

// with (inst) { event_user(0) }
void run_user_event0(Value* mem, VM_INT inst) {
    int32_t tbl[2];
    tbl[0] = GB_TMP;
    tbl[1] = GB_TMP + 1;
    slot_write(mem, GB_TMP,     mk_int(inst));
    slot_write(mem, GB_TMP + 1, mk_int(0));
    run_event_user(mem, (const uint8_t*)tbl, 2, -1);
    slot_write(mem, GB_TMP,     mk_int(0));
    slot_write(mem, GB_TMP + 1, mk_int(0));
}

// ============================================================
// 卡片（12 条）
// ============================================================

// VM_BanCard(card_id)：global.banned_cards_online[? card_id] = true
//   （那张 ds_map 由游戏侧开局建好；还没有就无从禁起 → 空操作）
void impl_VM_BanCard(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int m = gmap_of(S_banned_cards_online);
    if (m < 0) return;
    gmap_set(m, arg_v(mem, args, 0), mk_int(1));
}

// VM_BanAllCard()：把 global.plant_registry 的**全部键**都禁掉
//   GML 先 ds_map_keys_to_array 再遍历；这里同样先抄一份键（往另一张表写，抄下来更稳）
void impl_VM_BanAllCard(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)mem; (void)args; (void)argc; (void)dst;
    int reg = gmap_of(S_plant_registry);
    if (reg < 0) return;                                   // GML: variable_global_exists 不成立就返回
    int ban = gmap_of(S_banned_cards_online);
    if (ban < 0) return;
    std::vector<Value> keys;
    MapVV& mp = g_map_pool[(size_t)reg];
    for (MapIt it = mp.begin(); it != mp.end(); ++it) {
        retain(it->first);
        keys.push_back(it->first);
    }
    for (size_t i = 0; i < keys.size(); i++) gmap_set(ban, keys[i], mk_int(1));
    for (size_t i = 0; i < keys.size(); i++) release(keys[i]);
}

// VM_CannelBanCard(card_id)：ds_map_delete(global.banned_cards_online, card_id)
void impl_VM_CannelBanCard(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    gmap_del(gmap_of(S_banned_cards_online), arg_v(mem, args, 0));
}

// ── 把 global.grid_plants 里 col/row 范围内的卡片实例收集出来（-1 = 该方向不限）──
//   GML 就是"双循环 → ds_grid_get → ds_list 遍历"，这里一模一样；只是先收集再动手，
//   免得被调的东西（属性写）把实例销毁了、后面的格子读到复用的槽位
void plants_in_rect(VM_INT col, VM_INT row, std::vector<VM_INT>& out) {
    int gi = ggri_of(S_grid_plants);
    if (gi < 0) return;
    Grid& g = g_grid_pool[(size_t)gi];
    VM_INT cols = g.w, rows = g.h;
    // GML 用的是 global.grid_cols / grid_rows；没设过就退回网格自身尺寸
    Value* pc = gvar(S_grid_cols);
    Value* pr = gvar(S_grid_rows);
    if (pc && is_num(*pc)) cols = (VM_INT)numf(*pc);
    if (pr && is_num(*pr)) rows = (VM_INT)numf(*pr);
    if (cols > g.w) cols = g.w;                            // 别越过网格自己的尺寸
    if (rows > g.h) rows = g.h;
    VM_INT c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    VM_INT r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    for (VM_INT r = r1; r <= r2; r++) {
        if (r < 0 || r >= rows) continue;
        for (VM_INT c = c1; c <= c2; c++) {
            if (c < 0 || c >= cols) continue;
            Value cell = g.cells[(size_t)r * (size_t)g.w + (size_t)c];
            if (cell.t != MEM_LIST || !container_alive(cell)) continue;
            const std::vector<Value>& lst = g_list_pool[(size_t)cell.v];
            for (size_t i = 0; i < lst.size(); i++) {
                if (lst[i].t != MEM_INT) continue;
                if (!inst_get(lst[i].v)) continue;
                out.push_back(lst[i].v);
            }
        }
    }
}

// VM_SetCardProp(col, row, name, prop, value)
//   跳过 plant_id == "player" 的那个底座；name == "all" 时所有卡片都改，否则只改 plant_id 相同的
void impl_VM_SetCardProp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 5) return;
    if (net_client() && sync_exec()) return;               // GML: 客户端重放服务器通知时不动
    VM_INT prop_sid = name_arg(mem, args, 3);
    if (prop_sid < 0) return;
    VM_INT name_sid = name_arg(mem, args, 2);
    bool all = (name_sid == S_all);
    Value val = arg_v(mem, args, 4);
    std::vector<VM_INT> ids;
    plants_in_rect(num_arg(mem, args, 0), num_arg(mem, args, 1), ids);
    for (size_t i = 0; i < ids.size(); i++) {
        Instance* in = inst_get(ids[i]);
        if (!in) continue;
        Value pid = iv_get(*in, S_plant_id);
        if (pid.t == MEM_STRING && pid.v == S_player) continue;
        if (!all && !(pid.t == MEM_STRING && pid.v == name_sid)) continue;
        inst_prop_set(*in, prop_sid, val);                 // 账由内层结
    }
    // 服务器广播（MSG_VM_NOTIFY + json + 客户端列表）见文件尾"要宿主原语"一节
}

// VM_ApplyPlantLevel(inst_id)：让植物按星级/技能/形态重算实际数值
//   走转发类 native network_apply_plant_level(inst, true)（游戏侧那份实现）
void apply_plant_level(Value* mem, VM_INT inst) {
    (void)mem;
    Value a[2] = { mk_int(inst), mk_int(1) };
    Value r = fwd_call(S_network_apply_plant_level, a, 2);
    release(r);
}
void impl_VM_ApplyPlantLevel(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    VM_INT inst = num_arg(mem, args, 0);
    if (inst != -1 && !inst_get(inst)) return;             // GML: 实例不在又不是 -1 → 返回
    if (net_client() && sync_exec() && net_has_net_id(inst)) return;   // 联网实例：等服务器
    apply_plant_level(mem, inst);
    if (inst == -1) {                                      // -1 = 全场卡片都刷一遍
        std::vector<VM_INT> ids;
        VM_INT n = obj_count(S_obj_card_parent);
        for (VM_INT k = 0; k < n; k++) {
            VM_INT id = inst_at(S_obj_card_parent, k);
            if (id >= 0 && inst_get(id)) ids.push_back(id);
        }
        for (size_t i = 0; i < ids.size(); i++) apply_plant_level(mem, ids[i]);
    }
}

// ── obj_card_slot 那一批的筛选（VM_SetCardSlotProp / VM_CalcCardSlotProp 共用）──
//   GML: name == "all" / -1 → 全部；数字 → slot_index 相同；字符串 → card_id 相同
bool slot_match(Instance& in, const Value& name) {
    if ((name.t == MEM_STRING && name.v == S_all) || (is_num(name) && numf(name) == (VM_FLOAT)-1))
        return true;
    if (is_num(name)) {
        Value si = iv_get(in, S_slot_index);
        return is_num(si) && numf(si) == numf(name);
    }
    if (name.t == MEM_STRING) {
        Value cid = iv_get(in, S_card_id);
        return (cid.t == MEM_STRING && cid.v == name.v);
    }
    return false;
}

// 遍历 obj_card_slot 的活实例（GML 的 with(obj_card_slot)）
void for_each_slot(Value* mem, const uint8_t* args, int dst, bool calc, int argc) {
    (void)dst;
    if (argc < 3) return;                                  // Set 要 3 个、Calc 要 4 个（少的那个按 undefined 走）
    Value name = arg_v(mem, args, 0);
    VM_INT prop_sid = name_arg(mem, args, 1);
    if (prop_sid < 0) return;
    VM_INT n = obj_count(S_obj_card_slot);
    for (VM_INT k = 0; k < n; k++) {
        VM_INT id = inst_at(S_obj_card_slot, k);
        Instance* in = inst_get(id);
        if (!in) continue;
        if (!slot_match(*in, name)) continue;
        if (calc) {
            VM_INT op = (argc > 2) ? num_arg(mem, args, 2) : 0;
            Value val = (argc > 3) ? arg_v(mem, args, 3) : mk_undef();
            Value cur = iv_get(*in, prop_sid);
            if (!is_num(cur) || !is_num(val)) continue;    // _calc_op: 不是数字就不动
            VM_FLOAT a = numf(cur), b = numf(val), r = a;
            switch (op) {
            case 0: r = a + b; break;
            case 1: r = a - b; break;
            case 2: r = a * b; break;
            case 3: if (b != (VM_FLOAT)0) r = a / b; else continue; break;
            default: continue;
            }
            inst_prop_set(*in, prop_sid, mk_num_v(r));
        } else {
            if (!inst_write_ok(id)) continue;              // GML: vm_inst_write_ok(id)
            inst_prop_set(*in, prop_sid, arg_v(mem, args, 2));
        }
    }
}

// VM_SetCardSlotProp(name, prop, value)
void impl_VM_SetCardSlotProp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    for_each_slot(mem, args, dst, false, argc);
}

// VM_CalcCardSlotProp(name, prop, op, val)：op 0=加 1=减 2=乘 3=除
//   ⚠ GML 的 _calc_op 里 op=3（除）**没有**除零保护，这里补一个（除 0 跳过）
void impl_VM_CalcCardSlotProp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    for_each_slot(mem, args, dst, true, argc);
}

// VM_GetCardSlotCount()：instance_number(obj_card_slot)
void impl_VM_GetCardSlotCount(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, obj_count(S_obj_card_slot));
}

// VM_GetPreviewCard()：手牌预览的 card_id；没有预览实例 → -1
void impl_VM_GetPreviewCard(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    Instance* in = inst_get(inst_at(S_obj_card_preview, 0));
    if (!in) { wr_i(mem, dst, -1); return; }
    wr_v(mem, dst, iv_get(*in, S_card_id));
}

// VM_conveyor_belt_able(enable)：global._VM_conveyor_belt = (enable != 0)
void impl_VM_conveyor_belt_able(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    global_var_set_sid(S__VM_conveyor_belt, mk_int(nonzero(arg_v(mem, args, 0)) ? 1 : 0));
}

// VM_Slot_add(card_id)：往传送带加一张卡 → 返回卡槽实例 id；失败 -1，满了 noone
void impl_VM_Slot_add(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT cid = name_arg(mem, args, 0);
    if (cid < 0) { wr_i(mem, dst, -1); return; }
    Value da[2] = { mk_str_sid(cid), mk_int(0) };
    Value cd = fwd_call(S_deck_get_card_data, da, 2);       // 玩家卡池里的数据（不限出战卡组）
    if (cd.t != MEM_MAP || !container_alive(cd)) { release(cd); wr_i(mem, dst, -1); return; }
    int cm = (int)cd.v;
    int ba = garr_of(S__VM_conveyor_belt_arr);              // global._VM_conveyor_belt_arr
    VM_INT have = (ba >= 0) ? (VM_INT)g_arr_pool[(size_t)ba].size() : 0;
    if (have > 14) { release(cd); wr_i(mem, dst, NOONE); return; }   // GML: return noone
    VM_INT id = spawn_obj(S_obj_card_slot, (VM_FLOAT)(535 + 15 * 90), (VM_FLOAT)90, -5);
    if (id < 0) { release(cd); wr_i(mem, dst, -1); return; }
    inst_var_set_sid(id, S_cost,          mk_int(0));
    inst_var_set_sid(id, S_cooldown,      mk_int(0));
    inst_var_set_sid(id, S_card_obj,      gmap_get(cm, mk_str_sid(S_obj)));
    inst_var_set_sid(id, S_card_spr,      gmap_get(cm, mk_str_sid(S_sprite)));
    inst_var_set_sid(id, S_place_preview, gmap_get(cm, mk_str_sid(S_place_preview)));
    inst_var_set_sid(id, S_description,   gmap_get(cm, mk_str_sid(S_description)));
    inst_var_set_sid(id, S_shape,         gmap_get(cm, mk_str_sid(S_shape)));
    inst_var_set_sid(id, S_slot_index,    mk_int(have + 1));
    inst_var_set_sid(id, S_card_id,       mk_str_sid(cid));
    inst_var_set_sid(id, S_depth,         mk_int(-2000));
    if (ba >= 0) arr_push(ba, mk_int(id));
    release(cd);
    wr_i(mem, dst, id);
}

// VM_GetCardProp(card_id, prop)：按属性名读一张卡
//   存档类（get_card_info_simple）: shape / level / skill / max_level / max_shape
//   卡池类（deck_get_card_data）  : plant_type / feature_type / target_card / cost / cooldown
//   其它属性名 / 卡不存在 → undefined
void impl_VM_GetCardProp(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(mem, dst, mk_undef()); return; }
    VM_INT cid = name_arg(mem, args, 0);
    VM_INT prop = name_arg(mem, args, 1);
    if (cid < 0 || prop < 0 || cid == S_empty) { wr_v(mem, dst, mk_undef()); return; }
    if (!fwd_has(S_get_card_info_simple) && !fwd_has(S_deck_get_card_data)) {
        wr_v(mem, dst, mk_undef());                        // 两条链都没接：没什么可读的
        return;
    }
    // 存档类
    if (prop == S_shape || prop == S_level || prop == S_skill ||
        prop == S_max_level || prop == S_max_shape) {
        Value a[1] = { mk_str_sid(cid) };
        Value info = fwd_call(S_get_card_info_simple, a, 1);
        Value out = mk_undef();
        if (info.t == MEM_MAP && container_alive(info)) out = gmap_get((int)info.v, mk_str_sid(prop));
        wr_v(mem, dst, out);
        release(info);
        return;
    }
    // 卡池类：先拿存档里的 shape 定位形态
    if (prop == S_plant_type || prop == S_feature_type || prop == S_target_card ||
        prop == S_cost || prop == S_cooldown) {
        VM_INT shape = 0;
        Value a[1] = { mk_str_sid(cid) };
        Value info = fwd_call(S_get_card_info_simple, a, 1);
        if (info.t == MEM_MAP && container_alive(info)) {
            Value sv = gmap_get((int)info.v, mk_str_sid(S_shape));
            if (is_num(sv)) shape = (VM_INT)numf(sv);
        }
        release(info);
        Value b[2] = { mk_str_sid(cid), mk_int(shape) };
        Value cd = fwd_call(S_deck_get_card_data, b, 2);
        Value out = mk_undef();
        if (cd.t == MEM_MAP && container_alive(cd)) out = gmap_get((int)cd.v, mk_str_sid(prop));
        wr_v(mem, dst, out);
        release(cd);
        return;
    }
    wr_v(mem, dst, mk_undef());
}

// ============================================================
// 精灵 / 音效（14 条）
// ============================================================

// 池下标 → 文本，用来拼文件路径（文件 API 收 std::string）。
//   ⚠ 这是本文件里唯一不得不"取文本 / 拼文本"的地方：路径拼装绕不过去；
//     VM 的**值语义**仍然全程按池下标比（下面没有一处拿文本比 VM 值）。
void sprite_paths(VM_INT name_sid, std::vector<std::string>& out) {
    Value* p = gvar(S__file_cache_json_path);
    if (p && p->t == MEM_STRING && str_ok(p->v)) {
        std::string pre = str_at(p->v);
        if (!pre.empty() && pre[pre.size() - 1] != '/' && pre[pre.size() - 1] != '\\') pre += "/";
        out.push_back(pre + "../" + str_at(name_sid));
    }
    out.push_back(str_at(name_sid));                       // 裸文件名兜底
    // ⚠ GML 还有第三个候选："mod 卡 JSON 所在目录 / name"（global.__vm.mod_dir）。
    //   那是 GML 侧 VM 结构体上的字段，新内层没有对应原语 → 这个候选**没做**（见文件尾）
}

// global._VM_loaded_sprite_indices 尾追一个名字（没有这个数组就建一个 —— GML 里开局就是 []）
void push_loaded_sprite(VM_INT name_sid) {
    int a = garr_make(S__VM_loaded_sprite_indices);
    if (a < 0) return;
    arr_push(a, mk_str_sid(name_sid));
}

// _VM_LoadSpriteFile / _VM_LoadSpriteFileEx / _VM_LoadSpriteFile_Ex 的公共体：
//   临时缓存 → 候选路径 → file_exists → sprite_add → 记两个缓存
//   sprite_add 是转发类 native（游戏侧提供）；没接就是 undefined → 这里当失败
VM_INT sprite_load_file(VM_INT name_sid, VM_INT frames, VM_INT xo, VM_INT yo) {
    VM_INT cached = -1;
    if (cache_get(S__VM_sprite_temp_cache, mk_str_sid(name_sid), &cached)) return cached;
    std::vector<std::string> paths;
    sprite_paths(name_sid, paths);
    int tmp_m = gmap_of(S__VM_sprite_temp_cache);
    int rev_m = gmap_of(S__pid_reverse);
    for (size_t i = 0; i < paths.size(); i++) {
        if (!host_file_exists(paths[i])) continue;
        Value a[6];
        a[0] = mk_str_sid(intern_str(paths[i]));           // 路径进池 → 当字符串实参传下去
        a[1] = mk_int(frames);
        a[2] = mk_int(0);                                  // false
        a[3] = mk_int(0);                                  // false
        a[4] = mk_int(xo);
        a[5] = mk_int(yo);
        Value r = fwd_call(S_sprite_add, a, 6);
        VM_INT spr = is_num(r) ? (VM_INT)numf(r) : -1;
        release(r);
        if (spr == -1) continue;
        gmap_set(tmp_m, mk_str_sid(name_sid), mk_int(spr));
        gmap_set(rev_m, mk_int(spr), mk_str_sid(name_sid));
        return spr;
    }
    // GML 最后还有"客户端向服务端要文件"那条（_VM_RequestSprite → send_message）——见文件尾
    return -1;
}

// VM_LoadSprite(name)：临时缓存里的一张（1 帧、原点 0）
void impl_VM_LoadSprite(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    push_loaded_sprite(sid);
    wr_i(mem, dst, sprite_load_file(sid, 1, 0, 0));
}

// VM_LoadSpriteFrames(name, frames)：同上，帧数自己给（<=0 / 没给 → 1）
void impl_VM_LoadSpriteFrames(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    push_loaded_sprite(sid);
    VM_INT f = (argc > 1) ? num_arg(mem, args, 1) : 1;
    if (f <= 0) f = 1;
    wr_i(mem, dst, sprite_load_file(sid, f, 0, 0));
}

// VM_LoadSpriteFrames_Ex(name, frames, xorigin, yorigin)：帧数 + 原点都能给（-1 / 没给 → 默认）
void impl_VM_LoadSpriteFrames_Ex(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    push_loaded_sprite(sid);
    VM_INT f = (argc > 1) ? num_arg(mem, args, 1) : 1;
    if (f <= 0) f = 1;
    VM_INT xo = (argc > 2) ? num_arg(mem, args, 2) : 0;
    VM_INT yo = (argc > 3) ? num_arg(mem, args, 3) : 0;
    if (xo == -1) xo = 0;
    if (yo == -1) yo = 0;
    wr_i(mem, dst, sprite_load_file(sid, f, xo, yo));
}

// VM_LoadSpritePerm(name, frames)：加载进**永久**缓存（bin 重载不清）
//   ⚠ 这条 GML 直接拿裸文件名 sprite_add（不走候选路径），也不是 "被释放过" 的那种
void impl_VM_LoadSpritePerm(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    VM_INT perm = S__VM_sprite_cache;
    VM_INT cached = -1;
    if (cache_get(perm, mk_str_sid(sid), &cached)) { wr_i(mem, dst, cached); return; }
    VM_INT f = (argc > 1) ? num_arg(mem, args, 1) : 1;
    if (f <= 0) f = 1;
    Value a[6];
    a[0] = mk_str_sid(sid);
    a[1] = mk_int(f);
    a[2] = mk_int(0);
    a[3] = mk_int(0);
    a[4] = mk_int(0);
    a[5] = mk_int(0);
    Value r = fwd_call(S_sprite_add, a, 6);
    VM_INT spr = is_num(r) ? (VM_INT)numf(r) : -1;
    release(r);
    if (spr == -1) { wr_i(mem, dst, -1); return; }
    cache_put(perm, mk_str_sid(sid), mk_int(spr));
    wr_i(mem, dst, spr);
}

// VM_FreeSpritePerm(name)：从永久缓存里摘掉（顺便摘 _pid_reverse）
//   ⚠ GML 还调 sprite_delete(_spr) 把真精灵也删了 —— 那个名字**不在名单里**、host 表也没有，
//     所以这里只摘缓存（真精灵留给宿主自己回收）。见文件尾。
void impl_VM_FreeSpritePerm(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) return;
    VM_INT perm = S__VM_sprite_cache;
    int pm = gmap_of(perm);
    if (pm < 0) return;
    Value got = gmap_get(pm, mk_str_sid(sid));
    if (got.t == MEM_UNDEFINED) return;                    // GML: 不在缓存里就直接返回
    if (is_num(got)) {
        VM_INT spr = (VM_INT)numf(got);
        VM_INT rev = S__pid_reverse;
        gmap_del(gmap_of(rev), mk_int(spr));
    }
    gmap_del(pm, mk_str_sid(sid));
}

// VM_AliasSprite(new_name, exist_name)：临时缓存里的名字别名（值还是那串名字）
void impl_VM_AliasSprite(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    VM_INT nw = name_arg(mem, args, 0), ex = name_arg(mem, args, 1);
    if (nw < 0 || ex < 0) return;
    VM_INT tmp = S__VM_sprite_temp_cache;
    int tm = gmap_of(tmp);
    if (tm < 0 || !gmap_has(tm, mk_str_sid(ex))) return;   // 被指的那个不在临时缓存里 → 空操作
    gmap_add(tm, mk_str_sid(nw), mk_str_sid(ex));          // ds_map_add：键在就不动
}

// VM_AliasSpritePerm(new_name, exist_name)：永久缓存的别名（值是**精灵 id**）
void impl_VM_AliasSpritePerm(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    VM_INT nw = name_arg(mem, args, 0), ex = name_arg(mem, args, 1);
    if (nw < 0 || ex < 0 || nw == S_empty || ex == S_empty) return;
    VM_INT perm = S__VM_sprite_cache;
    int pm = gmap_of(perm);
    if (pm < 0) return;
    Value got = gmap_get(pm, mk_str_sid(ex));
    if (!is_num(got) || !sprite_alive(got)) return;        // 值必须是真精灵 id
    VM_INT spr = (VM_INT)numf(got);
    gmap_add(pm, mk_str_sid(nw), mk_int(spr));
    VM_INT rev = S__pid_reverse;
    int rm = gmap_of(rev);
    if (rm >= 0 && !gmap_has(rm, mk_int(spr))) gmap_add(rm, mk_int(spr), mk_str_sid(nw));
    // 登记到 _VM_perm_alias_names（[reloadmod] 时按它释放别名）
    int aa = garr_make(S__VM_perm_alias_names);
    if (aa >= 0 && arr_index_of(aa, mk_str_sid(nw)) < 0) arr_push(aa, mk_str_sid(nw));
}

// VM_SpriteExists(name)：这张贴图现在**真的可用**吗（而不是空白占位图）
//   查的顺序照 GML：项目资源 → VM 临时缓存 → VM 永久缓存 → 全局缓存（靠 _sprite_state 区分占位）
void impl_VM_SpriteExists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    Value n = arg_v(mem, args, 0);
    if (n.t != MEM_STRING || !str_ok(n.v) || n.v == S_empty) { wr_i(mem, dst, 0); return; }
    // ① 项目内置资源
    if (fwd_has(S_asset_get_index)) {
        Value a[1] = { n };
        Value h = fwd_call(S_asset_get_index, a, 1);
        bool ok = is_num(h) && numf(h) != (VM_FLOAT)-1 && sprite_alive(h);
        release(h);
        if (ok) { wr_i(mem, dst, 1); return; }
    }
    // ② VM 临时缓存 / ③ VM 永久缓存
    VM_INT cached = -1;
    if (cache_get(S__VM_sprite_temp_cache, n, &cached) && sprite_alive(mk_int(cached))) {
        wr_i(mem, dst, 1);
        return;
    }
    if (cache_get(S__VM_sprite_cache, n, &cached) && sprite_alive(mk_int(cached))) {
        wr_i(mem, dst, 1);
        return;
    }
    // ④ 全局贴图缓存：混着真图和占位图，看 _sprite_state（empty_load == 0 就是占位）
    int sc = gmap_of(S__sprite_cache);
    if (sc >= 0 && gmap_has(sc, n)) {
        Value* sp = gvar(S__sprite_state);
        Value state = mk_int(0);
        if (sp && sp->t == MEM_MAP && container_alive(*sp) && gmap_has((int)sp->v, n))
            state = gmap_get((int)sp->v, n);
        Value spr = gmap_get(sc, n);
        if (nonzero(state) && sprite_alive(spr)) { wr_i(mem, dst, 1); return; }
    }
    wr_i(mem, dst, 0);
}

// VM_GetLoadedSpriteName(index)：第 index 个加载过的贴图名；越界 → ""
void impl_VM_GetLoadedSpriteName(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_v(mem, dst, mk_str_sid(S_empty)); return; }
    VM_INT k = num_arg(mem, args, 0);
    int a = garr_of(S__VM_loaded_sprite_indices);
    if (a < 0 || k < 0 || k >= (VM_INT)g_arr_pool[(size_t)a].size()) {
        wr_v(mem, dst, mk_str_sid(S_empty));
        return;
    }
    wr_v(mem, dst, g_arr_pool[(size_t)a][(size_t)k]);
}

// VM_SetMapBackground(name, step)：换地图底图 + 淡入
void impl_VM_SetMapBackground(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) return;
    VM_INT spr = -1;
    VM_INT cached = -1;
    if (cache_get(S__sprite_cache, mk_str_sid(sid), &cached)) spr = cached;
    else spr = sprite_of_name(sid);
    global_var_set_sid(S_map_sprite_target, mk_int(spr));
    global_var_set_sid(S_map_fade_alpha,    mk_int(0));
    if (argc > 1) {
        VM_FLOAT step = arg_num(mem, args, 1);
        if (step > (VM_FLOAT)0) global_var_set_sid(S_map_fade_step, mk_num_v(step));
    }
}

// VM_PlaySound(name)：内置音效名，或 VM_LoadSound 返回的音频 id
void impl_VM_PlaySound(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    play_sound_value(mem, arg_v(mem, args, 0));
}

// VM_LoadSound(path)：本地 wav → 音频 id（走 _audio_cache）
//   audio_create_stream 是 HOST_LIST 里等宿主填的那条；宿主没接就是 undefined → 这里当失败
//   （GML 写的是 `!= -1`，但宿主空壳回的是 undefined；undefined 也当失败才安全）
void impl_VM_LoadSound(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    Value pv = arg_v(mem, args, 0);
    if (pv.t != MEM_STRING || !str_ok(pv.v)) { wr_i(mem, dst, -1); return; }
    VM_INT ac = S__audio_cache;
    int cm = gmap_of(ac);
    VM_INT cached = -1;
    if (cm >= 0 && cache_get(ac, pv, &cached)) { wr_i(mem, dst, cached); return; }
    std::vector<std::string> paths;
    sprite_paths(pv.v, paths);
    for (size_t i = 0; i < paths.size(); i++) {
        if (!host_file_exists(paths[i])) continue;
        Value a[1] = { mk_str_sid(intern_str(paths[i])) };
        Value r = host_call(S_audio_create_stream, mem, a, 1);
        VM_INT aud = (is_num(r) && numf(r) != (VM_FLOAT)-1) ? (VM_INT)numf(r) : -1;
        release(r);
        if (aud == -1) continue;
        gmap_set(cm, pv, mk_int(aud));
        gmap_set(gmap_of(S__audio_reverse), mk_int(aud), pv);
        wr_i(mem, dst, aud);
        return;
    }
    wr_i(mem, dst, -1);
}

// VM_LoadSpritePerm_Ex(name, frames, xorigin, yorigin)：永久缓存 + 指定原点（走候选路径）
//   ⚠ 和 VM_LoadSpritePerm 的差别：这条会先找文件（_file_cache_json_path/../name、裸名）
void impl_VM_LoadSpritePerm_Ex(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    VM_INT f = (argc > 1) ? num_arg(mem, args, 1) : 1;
    if (f <= 0) f = 1;
    VM_INT xo = (argc > 2) ? num_arg(mem, args, 2) : 0;
    VM_INT yo = (argc > 3) ? num_arg(mem, args, 3) : 0;
    if (xo == -1) xo = 0;
    if (yo == -1) yo = 0;
    VM_INT perm = S__VM_sprite_cache;
    VM_INT cached = -1;
    if (cache_get(perm, mk_str_sid(sid), &cached)) { wr_i(mem, dst, cached); return; }
    std::vector<std::string> paths;
    sprite_paths(sid, paths);
    int pm = gmap_of(perm);
    int rm = gmap_of(S__pid_reverse);
    for (size_t i = 0; i < paths.size(); i++) {
        if (!host_file_exists(paths[i])) continue;
        Value a[6];
        a[0] = mk_str_sid(intern_str(paths[i]));
        a[1] = mk_int(f);
        a[2] = mk_int(0);
        a[3] = mk_int(0);
        a[4] = mk_int(xo);
        a[5] = mk_int(yo);
        Value r = fwd_call(S_sprite_add, a, 6);
        VM_INT spr = is_num(r) ? (VM_INT)numf(r) : -1;
        release(r);
        if (spr == -1) continue;
        gmap_set(pm, mk_str_sid(sid), mk_int(spr));
        gmap_set(rm, mk_int(spr), mk_str_sid(sid));
        wr_i(mem, dst, spr);
        return;
    }
    wr_i(mem, dst, -1);
}

// VM_DrawSpriteExt(spr, subimg, x, y, xscale, yscale, rot, alpha)
//   等价 draw_sprite_ext(spr, sub, x, y, xs, ys, rot, c_white, alpha)，回 0；贴图不在 → -1
//   贴图名（字符串）先过 get_load_sprite；画就交给 host 的 g_host_draw.sprite_ext
void impl_VM_DrawSpriteExt(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    Value raw = arg_v(mem, args, 0);
    VM_INT spr = -1;
    if (raw.t == MEM_STRING)      spr = str_ok(raw.v) ? sprite_of_name(raw.v) : -1;
    else if (is_num(raw))         spr = (VM_INT)numf(raw);
    if (!sprite_alive(mk_int(spr))) { wr_i(mem, dst, -1); return; }
    VM_FLOAT sub = (argc > 1) ? arg_num(mem, args, 1) : 0;
    VM_FLOAT px  = (argc > 2) ? arg_num(mem, args, 2) : 0;
    VM_FLOAT py  = (argc > 3) ? arg_num(mem, args, 3) : 0;
    VM_FLOAT xs  = (argc > 4) ? arg_num(mem, args, 4) : 1;
    VM_FLOAT ys  = (argc > 5) ? arg_num(mem, args, 5) : 1;
    VM_FLOAT rot = (argc > 6) ? arg_num(mem, args, 6) : 0;
    VM_FLOAT alp = (argc > 7) ? arg_num(mem, args, 7) : 1;
    if (xs == (VM_FLOAT)0) xs = 1;                         // GML: undefined / 0 → 1
    if (ys == (VM_FLOAT)0) ys = 1;
    g_host_draw.sprite_ext((int)spr, (double)px, (double)py, (double)xs, (double)ys,
                           (double)rot, (double)alp, 16777215 /* c_white */, (int)sub);
    wr_i(mem, dst, 0);
}

// ============================================================
// 波次 / 结算（13 条）
// ============================================================

// VM_GetSubwave()：GML 是 real(global._VM_prev_subwave)
void impl_VM_GetSubwave(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    Value* p = gvar(S__VM_prev_subwave);
    if (!p) { wr_i(mem, dst, 0); return; }                 // 全局还没建：real(undefined) 当 0
    wr_v(mem, dst, is_num(*p) ? mk_num_v(numf(*p)) : mk_int(0));
}

// VM_GameWin()：置暂停 / 结束标志 → 弹结算实例 → 播音效；服务端还要广播 MSG_GAME_OVER=10
void game_over_common(Value* mem, bool win) {
    if (net_client() && sync_exec()) return;               // GML: 客户端重放时跳过
    if (net_server()) {
        Value r = mk_int(win ? 1 : 0);
        net_send_all(mem, 10 /* MSG_GAME_OVER */, r);
    }
    global_var_set_sid(S_is_paused, mk_int(1));
    global_var_set_sid(S_game_over, mk_int(1));
    VM_INT id = spawn_obj(S_obj_game_over,
                          (VM_FLOAT)((double)host_draw_width()  / 2.0),
                          (VM_FLOAT)((double)host_draw_height() / 2.0),
                          -3001);
    if (win && id >= 0) {                                  // GML: 只有胜利那条设 spr_win
        VM_INT spr = asset_of("spr_win");
        if (spr != -1) inst_var_set_sid(id, S_sprite_index, mk_int(spr));
    }
    play_sound_asset(mem, win ? "snd_win" : "snd_lose");
}

void impl_VM_GameWin(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; (void)dst;
    game_over_common(mem, true);
}

void impl_VM_GameLose(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; (void)dst;
    game_over_common(mem, false);
}

// VM_GetKilledProp(prop)：销毁事件期间读"刚死那个实例"的属性快照 global._VM_cur_dead_snap
//   特殊：object_name → 快照里的 _object_name；mouse_id 空/没有 → 用 _object_name 去掉 "obj_" 前缀
void impl_VM_GetKilledProp(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_v(mem, dst, mk_undef()); return; }
    VM_INT prop = name_arg(mem, args, 0);
    if (prop < 0) { wr_v(mem, dst, mk_undef()); return; }
    int sm = gmap_of(S__VM_cur_dead_snap);
    if (sm < 0) { wr_v(mem, dst, mk_undef()); return; }
    if (prop == S_object_name) {
        wr_v(mem, dst, gmap_get(sm, mk_str_sid(S__object_name)));
        return;
    }
    Value v = gmap_get(sm, mk_str_sid(prop));
    bool empty_str = (v.t == MEM_STRING && str_ok(v.v) && str_at(v.v).empty());
    if (v.t != MEM_UNDEFINED && !(prop == S_mouse_id && empty_str)) {
        wr_v(mem, dst, v);
        return;
    }
    if (prop == S_mouse_id) {                              // 兜底：对象名去掉前 4 个字符（obj_）
        Value on = gmap_get(sm, mk_str_sid(S__object_name));
        if (on.t == MEM_STRING && str_ok(on.v)) {
            const std::string& t = str_at(on.v);
            if (t.size() >= 4) { wr_v(mem, dst, mk_str_sid(intern_str(t.substr(4)))); return; }
        }
    }
    wr_v(mem, dst, mk_undef());
}

// VM_GetTimeLimit()：obj_battle.time_limit（== -1 表示这关没倒计时 → undefined）
void impl_VM_GetTimeLimit(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    Instance* in = inst_get(inst_at(S_obj_battle, 0));
    if (!in) { wr_v(mem, dst, mk_undef()); return; }
    Value t = iv_get(*in, S_time_limit);
    if (!is_num(t) || numf(t) == (VM_FLOAT)-1) { wr_v(mem, dst, mk_undef()); return; }
    wr_num(mem, dst, numf(t));
}

// VM_SetTimeLimit(frames)：改倒计时；没有战斗 / 没倒计时 / 参数不对 → undefined，成功回新值
void impl_VM_SetTimeLimit(Value* mem, const uint8_t* args, int argc, int dst) {
    if (net_client() && sync_exec()) { wr_v(mem, dst, mk_undef()); return; }
    if (argc < 1) { wr_v(mem, dst, mk_undef()); return; }
    Value fv = arg_v(mem, args, 0);
    if (!is_num(fv) || numf(fv) < (VM_FLOAT)0) { wr_v(mem, dst, mk_undef()); return; }
    Instance* in = inst_get(inst_at(S_obj_battle, 0));
    if (!in) { wr_v(mem, dst, mk_undef()); return; }
    VM_INT tl = S_time_limit;
    Value t = iv_get(*in, tl);
    if (!is_num(t) || numf(t) == (VM_FLOAT)-1) { wr_v(mem, dst, mk_undef()); return; }
    inst_prop_set(*in, tl, fv);
    // 服务器广播（MSG_VM_NOTIFY）见文件尾
    wr_v(mem, dst, fv);
}

// VM_SetWaveAuto(enabled)：global._VM_wave_auto = (enabled != 0)，回 1/0
void impl_VM_SetWaveAuto(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    bool on = nonzero(arg_v(mem, args, 0));
    global_var_set_sid(S__VM_wave_auto, mk_int(on ? 1 : 0));
    wr_i(mem, dst, on ? 1 : 0);
}

// ── 子弹：15 / 18 / 20 个实参原样交给转发类 native ──
//   VM_BulletScreenAdd 就是 bullet_screen_add 的接口，而 GML 的 bullet_screen_add 与
//   bullet_screen_add_Ex 是同一个实现（前者只是后者的默认参数版）→ 两条都落到 _Ex；
//   VM_BulletScreenAdd_Exs 落到 bullet_screen_add_Exs。函数字典里没有 → -1
//   （GML：管理器或贴图不存在时也是 -1）
void bullet_forward(Value* mem, const uint8_t* args, int argc, int dst, VM_INT fn_sid, int want) {
    if (!fwd_has(fn_sid)) { wr_i(mem, dst, -1); return; }
    int n = (argc < want) ? argc : want;
    if (n < 0) n = 0;
    Value av[CALL_MAX_ARGS];
    for (int i = 0; i < n; i++) av[i] = arg_v(mem, args, i);
    Value r = callbin_call(fn_sid, av, n);
    wr_v(mem, dst, r);
    release(r);
}

void impl_VM_BulletScreenAdd(Value* mem, const uint8_t* args, int argc, int dst) {
    bullet_forward(mem, args, argc, dst, S_bullet_screen_add_Ex, 15);
}
void impl_VM_BulletScreenAdd_Ex(Value* mem, const uint8_t* args, int argc, int dst) {
    bullet_forward(mem, args, argc, dst, S_bullet_screen_add_Ex, 18);
}
void impl_VM_BulletScreenAdd_Exs(Value* mem, const uint8_t* args, int argc, int dst) {
    bullet_forward(mem, args, argc, dst, S_bullet_screen_add_Exs, 20);
}

// ── 敌人受击 ──
//   GML 的 damage_enemy / damage_enemy_ash 只用"实例变量 + event_user + audio"，C++ 这边齐备，
//   所以**不**经 HOST_LIST 里的 damage_enemy 空壳（那会让每次命中都置 VM_E_NOT_IMPL），
//   直接照 GML 参考在本地做。damage_enemy_ash 本来就不在名单里，只能这么做。
//   语义差异记在文件尾。
VM_INT damage_enemy_local(Value* mem, VM_INT inst, const Value& dmg, const Value& type, bool ash) {
    Instance* in = inst_get(inst);
    if (!in) return 0;                                     // GML: !instance_exists → 0
    Value tp = type;
    if (tp.t == MEM_UNDEFINED || (tp.t == MEM_STRING && tp.v == S_empty)) tp = mk_str_sid(S_normal);

    // with (inst) { if (variable_instance_exists(id,"hit_sound")) audio_play_sound(hit_sound,0,0); … }
    if (inst_prop(*in, S_hit_sound)) {
        Value snd = iv_get(*in, S_hit_sound);
        if (is_num(snd)) {
            Value b[3] = { snd, mk_int(0), mk_int(0) };
            Value r = host_call(S_audio_play_sound, mem, b, 3);
            release(r);
        }
    }
    if (!ash) {
        inst_prop_set(*in, S_damage_amount, dmg);
        inst_prop_set(*in, S_damage_type, tp);
        run_user_event0(mem, inst);                        // event_user(0) = 敌人自己的受击事件
        return 1;
    }
    // 灰烬伤害：接得下（hp > dmg）就正常结算，接不下就一击必杀换成灰烬
    Value hp = iv_get(*in, S_hp);
    if (is_num(hp) && numf(hp) > numf(dmg)) {
        inst_prop_set(*in, S_damage_amount, dmg);
        inst_prop_set(*in, S_damage_type, tp);
        run_user_event0(mem, inst);
        return 1;
    }
    // 先把要用的值抄下来：下面要建实例 / 销实例，in 那个槽位不能再依赖
    VM_FLOAT x = numf(iv_get(*in, S_x));
    VM_FLOAT y = numf(iv_get(*in, S_y));
    VM_FLOAT d = numf(iv_get(*in, S_depth));
    bool special = false;
    Value* sa = inst_prop(*in, S_special_ash);            // GML: variable_instance_exists 也要成立
    if (sa) special = nonzero(*sa);
    Value spr = iv_get(*in, S_sprite_index);
    Value idx = iv_get(*in, S_image_index);
    VM_INT ash_id = spawn_obj(S_obj_mouse_ash_death, x, y - (VM_FLOAT)20, (VM_INT)d);
    if (ash_id >= 0 && special) {                          // 灰烬类敌人把外形也带过去
        inst_var_set_sid(ash_id, S_special_ash,   mk_int(1));
        inst_var_set_sid(ash_id, S_sprite_index,  spr);
        inst_var_set_sid(ash_id, S_image_index,   idx);
    }
    inst_destroy(inst);
    return 1;
}

// VM_DamageEnemy(inst, dmg, dmg_type)：0 → 0；联网实例客户端不结算 → 0
void impl_VM_DamageEnemy(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT inst = num_arg(mem, args, 0);
    if (inst == 0) { wr_i(mem, dst, 0); return; }
    if (!inst_write_ok(inst)) { wr_i(mem, dst, 0); return; }
    Value dmg = (argc > 1) ? arg_v(mem, args, 1) : mk_int(0);
    Value tp  = (argc > 2) ? arg_v(mem, args, 2) : mk_undef();
    wr_i(mem, dst, damage_enemy_local(mem, inst, dmg, tp, false));
}

// VM_DamageEnemyAsh(inst, dmg, dmg_type)
void impl_VM_DamageEnemyAsh(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT inst = num_arg(mem, args, 0);
    if (inst == 0) { wr_i(mem, dst, 0); return; }
    if (!inst_write_ok(inst)) { wr_i(mem, dst, 0); return; }
    Value dmg = (argc > 1) ? arg_v(mem, args, 1) : mk_int(0);
    Value tp  = (argc > 2) ? arg_v(mem, args, 2) : mk_undef();
    wr_i(mem, dst, damage_enemy_local(mem, inst, dmg, tp, true));
}

// ── VM_GetInfo(kind, id, 字段1, 字段2, …)：按路径查注册表 ──
//   kind: "card"→global.plant_registry / "enemy"→enemy_map / "weapon"→weapon_pool / "gem"→gems_pool
//   每一段：① 是数字且当前是数组 → 下标；② ds_map / struct 的键（数字段自动转字符串键）；
//   走到最后只放行 数字 / 字符串（数组 / 结构体 / ds_map 本身 → undefined）
//   变长参数：遇到 undefined 就停（和 GML 的 vm_args_of_16 一样）
VM_INT sids_of_num(VM_INT k) {
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "%d", (int)k);
    if (n < 0) n = 0;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    return intern_str(std::string(buf, (size_t)n));
}
void impl_VM_GetInfo(Value* mem, const uint8_t* args, int argc, int dst) {
    Value a[16];
    int n = 0;
    for (int i = 0; i < argc && i < 16; i++) {
        Value v = arg_v(mem, args, i);
        if (v.t == MEM_UNDEFINED) break;
        a[n++] = v;
    }
    if (n < 3) { wr_v(mem, dst, mk_undef()); return; }
    if (a[0].t != MEM_STRING || !str_ok(a[0].v)) { wr_v(mem, dst, mk_undef()); return; }
    if (a[1].t != MEM_STRING || !str_ok(a[1].v) || a[1].v == S_empty) { wr_v(mem, dst, mk_undef()); return; }

    VM_INT root_g;
    VM_INT kind = a[0].v;
    if      (kind == S_card)   root_g = S_plant_registry;
    else if (kind == S_enemy)  root_g = S_enemy_map;
    else if (kind == S_weapon) root_g = S_weapon_pool;
    else if (kind == S_gem)    root_g = S_gems_pool;
    else { wr_v(mem, dst, mk_undef()); return; }

    int rm = gmap_of(root_g);
    if (rm < 0 || !gmap_has(rm, mk_str_sid(a[1].v))) { wr_v(mem, dst, mk_undef()); return; }
    Value node = gmap_get(rm, mk_str_sid(a[1].v));
    for (int i = 2; i < n; i++) {
        Value seg = a[i];
        Value next = mk_undef();
        bool got = false;
        if (is_num(seg) && node.t == MEM_ARRAY && container_alive(node)) {   // ① 数组下标
            VM_INT k = (VM_INT)std::floor((double)numf(seg));
            std::vector<Value>& arr = g_arr_pool[(size_t)node.v];
            if (k >= 0 && k < (VM_INT)arr.size()) { next = arr[(size_t)k]; got = true; }
        }
        if (!got) {                                                          // ② ds_map / struct
            Value key = is_num(seg)
                      ? mk_str_sid(sids_of_num((VM_INT)std::floor((double)numf(seg))))
                      : seg;
            if (key.t == MEM_STRING && str_ok(key.v) &&
                node.t == MEM_MAP && container_alive(node) && gmap_has((int)node.v, key)) {
                next = gmap_get((int)node.v, key);
                got = true;
            }
        }
        if (!got) { wr_v(mem, dst, mk_undef()); return; }
        node = next;
    }
    if (node.t == MEM_INT || node.t == MEM_FLOAT || node.t == MEM_STRING) { wr_v(mem, dst, node); return; }
    wr_v(mem, dst, mk_undef());
}

// ============================================================
// 输入（7 条）—— 直接走 host 的输入表（没接 = 全假 / 0）
// ============================================================

void impl_VM_GetMouseX(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_num(mem, dst, (VM_FLOAT)g_host_in.mouse_x());
}
void impl_VM_GetMouseY(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_num(mem, dst, (VM_FLOAT)g_host_in.mouse_y());
}

// VM_GetMouseCol / Row：GML 是 get_grid_position_from_world(mouse_x, mouse_y, true) 的 .col / .row
//   那条 native 在 HOST_LIST 里等宿主填；没接就是 undefined → 这里也回 undefined
void mouse_grid_pos(Value* mem, int dst, bool want_row) {
    Value a[3];
    a[0] = mk_num_v((VM_FLOAT)g_host_in.mouse_x());
    a[1] = mk_num_v((VM_FLOAT)g_host_in.mouse_y());
    a[2] = mk_int(1);
    Value gp = host_call(S_get_grid_position_from_world, mem, a, 3);
    bool found = false;
    Value out = mk_undef();
    if (gp.t == MEM_MAP && container_alive(gp)) {
        Value k = mk_str_sid(want_row ? S_row : S_col);
        if (gmap_has((int)gp.v, k)) { out = gmap_get((int)gp.v, k); found = true; }
    }
    if (found) wr_v(mem, dst, out);
    else       wr_v(mem, dst, mk_undef());
    release(gp);
}
void impl_VM_GetMouseCol(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    mouse_grid_pos(mem, dst, false);
}
void impl_VM_GetMouseRow(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    mouse_grid_pos(mem, dst, true);
}

// VM_GetMousePressed(button)：1=左 2=右 3=中（GML 的 mb_left/right/middle）→ host 那边 0/1/2
void impl_VM_GetMousePressed(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT btn = num_arg(mem, args, 0);
    if (btn < 1 || btn > 3) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, g_host_in.mouse_down((int)btn - 1) ? 1 : 0);
}

// ── GML 的 _vm_key_name_to_code：按键名 → GM 的键码 ──
//   这里必须按**文本**比（按键名本来就是文本），不是 VM 的值语义
std::string ascii_lower(const std::string& s) {
    std::string o = s;
    for (size_t i = 0; i < o.size(); i++)
        if (o[i] >= 'A' && o[i] <= 'Z') o[i] = (char)(o[i] - 'A' + 'a');
    return o;
}
int key_code_of(VM_INT sid) {
    if (sid < 0 || !str_ok(sid)) return -1;
    static const struct { const char* name; int code; } kTab[] = {
        {"space", 32}, {"enter", 13}, {"escape", 27}, {"tab", 9}, {"shift", 16},
        {"ctrl", 17}, {"alt", 18}, {"up", 38}, {"down", 40}, {"left", 37},
        {"right", 39}, {"backspace", 8}, {"delete", 46}, {"home", 36}, {"end", 35},
        {"pageup", 33}, {"pagedown", 34},
        {"f1", 112}, {"f2", 113}, {"f3", 114}, {"f4", 115}, {"f5", 116}, {"f6", 117},
        {"f7", 118}, {"f8", 119}, {"f9", 120}, {"f10", 121}, {"f11", 122}, {"f12", 123},
    };
    const std::string& raw = str_at(sid);
    std::string low = ascii_lower(raw);
    for (size_t i = 0; i < sizeof(kTab) / sizeof(kTab[0]); i++)
        if (low == kTab[i].name) return kTab[i].code;
    if (raw.size() == 1) {                                 // 单字符：ord(string_upper(name))
        unsigned char c = (unsigned char)raw[0];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        return (int)c;
    }
    return -1;                                             // 认不出来的键名（GML 会 shell_print 一句，这里不打印）
}

// VM_GetKeyDown(key)：按住
void impl_VM_GetKeyDown(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    int code = key_code_of(name_arg(mem, args, 0));
    wr_i(mem, dst, (code >= 0 && g_host_in.key_down(code)) ? 1 : 0);
}
// VM_GetKeyPressed(key)：这一帧刚按下
void impl_VM_GetKeyPressed(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    int code = key_code_of(name_arg(mem, args, 0));
    wr_i(mem, dst, (code >= 0 && g_host_in.key_pressed(code)) ? 1 : 0);
}

}  // namespace

// ============================================================
// 挂表：46 条，一条不能少（下标 = S_VM_xxx - SID_GAME_BASE）
// ============================================================
void patch_game_fns_b() {
    // 卡片 12
    kGameFns[S_VM_BanCard            - SID_GAME_BASE] = impl_VM_BanCard;
    kGameFns[S_VM_SetCardProp        - SID_GAME_BASE] = impl_VM_SetCardProp;
    kGameFns[S_VM_ApplyPlantLevel    - SID_GAME_BASE] = impl_VM_ApplyPlantLevel;
    kGameFns[S_VM_SetCardSlotProp    - SID_GAME_BASE] = impl_VM_SetCardSlotProp;
    kGameFns[S_VM_CalcCardSlotProp   - SID_GAME_BASE] = impl_VM_CalcCardSlotProp;
    kGameFns[S_VM_GetCardSlotCount   - SID_GAME_BASE] = impl_VM_GetCardSlotCount;
    kGameFns[S_VM_GetPreviewCard     - SID_GAME_BASE] = impl_VM_GetPreviewCard;
    kGameFns[S_VM_conveyor_belt_able - SID_GAME_BASE] = impl_VM_conveyor_belt_able;
    kGameFns[S_VM_Slot_add           - SID_GAME_BASE] = impl_VM_Slot_add;
    kGameFns[S_VM_BanAllCard         - SID_GAME_BASE] = impl_VM_BanAllCard;
    kGameFns[S_VM_CannelBanCard      - SID_GAME_BASE] = impl_VM_CannelBanCard;
    kGameFns[S_VM_GetCardProp        - SID_GAME_BASE] = impl_VM_GetCardProp;
    // 精灵 / 音效 14
    kGameFns[S_VM_LoadSprite         - SID_GAME_BASE] = impl_VM_LoadSprite;
    kGameFns[S_VM_SetMapBackground   - SID_GAME_BASE] = impl_VM_SetMapBackground;
    kGameFns[S_VM_GetLoadedSpriteName - SID_GAME_BASE] = impl_VM_GetLoadedSpriteName;
    kGameFns[S_VM_PlaySound          - SID_GAME_BASE] = impl_VM_PlaySound;
    kGameFns[S_VM_LoadSpriteFrames   - SID_GAME_BASE] = impl_VM_LoadSpriteFrames;
    kGameFns[S_VM_LoadSpritePerm     - SID_GAME_BASE] = impl_VM_LoadSpritePerm;
    kGameFns[S_VM_FreeSpritePerm     - SID_GAME_BASE] = impl_VM_FreeSpritePerm;
    kGameFns[S_VM_AliasSprite        - SID_GAME_BASE] = impl_VM_AliasSprite;
    kGameFns[S_VM_LoadSpriteFrames_Ex - SID_GAME_BASE] = impl_VM_LoadSpriteFrames_Ex;
    kGameFns[S_VM_LoadSound          - SID_GAME_BASE] = impl_VM_LoadSound;
    kGameFns[S_VM_LoadSpritePerm_Ex  - SID_GAME_BASE] = impl_VM_LoadSpritePerm_Ex;
    kGameFns[S_VM_DrawSpriteExt      - SID_GAME_BASE] = impl_VM_DrawSpriteExt;
    kGameFns[S_VM_SpriteExists       - SID_GAME_BASE] = impl_VM_SpriteExists;
    kGameFns[S_VM_AliasSpritePerm    - SID_GAME_BASE] = impl_VM_AliasSpritePerm;
    // 波次 / 结算 13
    kGameFns[S_VM_GetSubwave         - SID_GAME_BASE] = impl_VM_GetSubwave;
    kGameFns[S_VM_GameWin            - SID_GAME_BASE] = impl_VM_GameWin;
    kGameFns[S_VM_GameLose           - SID_GAME_BASE] = impl_VM_GameLose;
    kGameFns[S_VM_GetKilledProp      - SID_GAME_BASE] = impl_VM_GetKilledProp;
    kGameFns[S_VM_GetTimeLimit       - SID_GAME_BASE] = impl_VM_GetTimeLimit;
    kGameFns[S_VM_SetTimeLimit       - SID_GAME_BASE] = impl_VM_SetTimeLimit;
    kGameFns[S_VM_SetWaveAuto        - SID_GAME_BASE] = impl_VM_SetWaveAuto;
    kGameFns[S_VM_BulletScreenAdd    - SID_GAME_BASE] = impl_VM_BulletScreenAdd;
    kGameFns[S_VM_DamageEnemy        - SID_GAME_BASE] = impl_VM_DamageEnemy;
    kGameFns[S_VM_DamageEnemyAsh     - SID_GAME_BASE] = impl_VM_DamageEnemyAsh;
    kGameFns[S_VM_BulletScreenAdd_Ex - SID_GAME_BASE] = impl_VM_BulletScreenAdd_Ex;
    kGameFns[S_VM_BulletScreenAdd_Exs - SID_GAME_BASE] = impl_VM_BulletScreenAdd_Exs;
    kGameFns[S_VM_GetInfo            - SID_GAME_BASE] = impl_VM_GetInfo;
    // 输入 7
    kGameFns[S_VM_GetMouseX          - SID_GAME_BASE] = impl_VM_GetMouseX;
    kGameFns[S_VM_GetMouseY          - SID_GAME_BASE] = impl_VM_GetMouseY;
    kGameFns[S_VM_GetMouseCol        - SID_GAME_BASE] = impl_VM_GetMouseCol;
    kGameFns[S_VM_GetMouseRow        - SID_GAME_BASE] = impl_VM_GetMouseRow;
    kGameFns[S_VM_GetMousePressed    - SID_GAME_BASE] = impl_VM_GetMousePressed;
    kGameFns[S_VM_GetKeyDown         - SID_GAME_BASE] = impl_VM_GetKeyDown;
    kGameFns[S_VM_GetKeyPressed      - SID_GAME_BASE] = impl_VM_GetKeyPressed;
}

// ============================================================
// 文件尾：实现时做的取舍 / 还缺的宿主原语（**都在这里，代码里没有静默乱来**）
//
// 一、宿主原语缺失（host.h 里没有，只能经 HOST_LIST 的空壳或干脆做不了）
//   1. 音频：host.h 只有 in / draw / net 三张表 + 文件 / 缓冲，**没有音频表**。
//      audio_play_sound / audio_create_stream 都在 HOST_LIST（host_fn.cpp 的空壳，一调就置
//      VM_E_NOT_IMPL）。VM_PlaySound / VM_LoadSound / 受击音效 / 胜负音效都只能从这儿出去。
//   2. sprite_delete：VM_FreeSpritePerm 要删真精灵，但 sprite_delete 连名单里都没有
//      （const_str.h 里没有 S_sprite_delete）→ 现在只摘缓存，真精灵留给宿主回收。
//   3. 贴图加载：sprite_add / sprite_exists / get_load_sprite / asset_get_index 都是**转发类**
//      （游戏侧用 callbin 提供）→ 本文件经 fwd_call 走同一条路。函数字典里没有时：
//      LoadSprite* 回 -1、SpriteExists 回 0、DrawSpriteExt 只在句柄是数字时照画。
//   4. _VM_RequestSprite（本地文件不在时向服务端要文件）：要 send_message + 网络原语，没做。
//   5. mod_dir 路径候选（VM_LoadSpritePerm_Ex 的第三候选）：GML 读 global.__vm.mod_dir，
//      新内层没有对应原语 → 候选里没有它。
//
// 二、联网广播没做：VM_SetCardProp / VM_ApplyPlantLevel / VM_SetTimeLimit 在服务端会
//   json_stringify + send_message(MSG_VM_NOTIFY) 逐客户端发。那要构造 JSON、拼消息文本，
//   本批只用 MSG_GAME_OVER（数值 10，参数简单）做了 GameWin / GameLose 那两条广播。
//   **读侧闸门**（global.network.mode / _VM_sync_exec / map_instance_id_net_id）都照 GML 做了，
//   所以"客户端不该本地改"的那半边语义是完整的。
//
// 三、语义上主动改的地方（都以 GML 参考为准，只改了不可表达 / 明显是坑的那点）
//   1. VM_DamageEnemy 不经 HOST_LIST 的 damage_enemy 空壳，改成和 VM_DamageEnemyAsh 共用本地实现
//      （GML 里这两个函数只用实例变量 + event_user(0) + audio，C++ 齐备）。理由：空壳每命中一次
//      就置一次 VM_E_NOT_IMPL，会把真正的 g_vm_err 冲掉；damage_enemy_ash 本来也不在名单里。
//   2. VM_CalcCardSlotProp 的 op=3（除）补了除零保护（GML 的 _calc_op 会直接用 0 除）。
//   3. VM_LoadSound 把 audio_create_stream 的 undefined 当失败（GML 只判 `!= -1`，
//      而宿主空壳回的是 undefined —— 不这么判会把 undefined 塞进 _audio_cache 再返回）。
//   4. 负数实例 id 的包装（global._VM_id_to_real）没做：新内层的实例 id 就是它自己的 id，
//      没有"客户端包装 id"这层（game_self.cpp 的 VM_IsDestroyed 也是这个口径）。
//   5. VM_GetSubwave 按 GML 的 real() 口径回数字（全局没建时 0），不是 game_self 那种 undefined。
//   6. VM_ApplyPlantLevel 的 -1（全场）分支先抄一份实例 id 再逐个刷，
//      免得 network_apply_plant_level 里销毁/新建实例把遍历搞乱（GML 的 with 也没防这个）。
// ============================================================
