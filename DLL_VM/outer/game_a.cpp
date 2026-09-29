// ============================================================
// game_a.cpp — 游戏 API（批次 A：实例 / 战场 / 索敌 / 生成 / 上次事件，42 条）
//
//   A 为什么有这些文件：game_api.cpp 只生成 150 条空壳（名字/顺序是契约），
//   实现写成 impl_XXX 放在这儿，再由 patch_game_fns_a() 挂进 kGameFns
//   （fn_table.cpp 的 patch_game_fns() 会按顺序调这三个钩子）。
//
//   本批要什么：多半只差"已有的内层·外层函数 + 自举数学"（inst_* / inst_var_* / ds_* / global.* / 网格换算）
//
//   写法（照 game_self.cpp 的样子）：
//     static void impl_XXX(Value* mem, const uint8_t* args, int argc, int dst) { … }   // static：本文件私有
//     void patch_game_fns_a() { kGameFns[S_VM_XXX - SID_GAME_BASE] = impl_XXX; … }
//
//   ⚠ 只改本文件！批次之间、和 outer.h / fn_table.cpp / 表名单都别动（会互相踩）。
//   ⚠ 实参就是**槽位**（不是内存地址）：arg_v(mem, args, k) 取值，argslot(args, k) 只取槽号。
//   ⚠ 语义真源是 GML 参考：scripts/scr_command_VM/scr_command_VM.gml（那 150 条的正版实现），
//     老 C++ 有实现的在 transformGML2txt/vm_api.cpp。两条不一致时以 GML 参考为准。
//
//   本批名单（42 条；实现一条对一条）—— 下面这份宏就是清单本体，
//   每一条在文件尾的 patch_game_fns_a() 里有一条对应的挂表行（一对一，43 行里除示例外正好 42 条）。
// ============================================================
#define GAME_A_LIST(X) \
    X(VM_CreatePlatform) X(VM_SpawnPlant) X(VM_SpawnEnemy) X(VM_SpawnBoss) \
    X(VM_SpawnObject) X(VM_SpawnCats) X(VM_SpawnPlantsRandom) X(VM_CreateButton) \
    X(VM_SpawnBatMouse) X(VM_HomingBulletAdd) X(VM_SetTerrain) X(VM_ClearPlants) \
    X(VM_ClearMapObjects) X(VM_SetRowFeature) X(VM_ClearPlantsByType) X(VM_WakePlants) \
    X(VM_GetTerrain) X(VM_GetEnemyCount) X(VM_GetPlantCount) X(VM_GetPlantCountAt) \
    X(VM_GetPlantAt) X(VM_SwapPlants) X(VM_SwapPlantRects) X(VM_CompactColumn) \
    X(VM_CompactRow) X(VM_CompactColumnRev) X(VM_CompactRowRev) X(VM_CellCount) \
    X(VM_CellItem) X(VM_CellContains) X(VM_CatInRow) X(VM_MapObj) \
    X(VM_GetLastBoss) X(VM_GetLastIdlePlatform) X(VM_GetLastBossStateChangeId) X(VM_GetLastBossOldState) \
    X(VM_GetLastBossNewState) X(VM_EnemyInRange) X(VM_GetHomingTarget) X(VM_GetInstancesInRange) \
    X(VM_SetEnemyProp) X(VM_CanPlace) \

#include "outer.h"
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ============================================================
// 本文件落地时的几条判断（都写在实现处，这里先列个总账）：
//
// ① 引擎原语自己算：get_world_position_from_grid / calculate_plant_depth 在 GML 里就是
//    几个 global 的乘加（scripts/get_grid_position_from_world、scripts/card_depth），
//    新架构里 host 表那两个同名 native 还是空壳 —— 本文件按 GML 的算式自己算，
//    不再往宿主那边扔一个一定会置 VM_E_NOT_IMPL 的调用。
//
// ② 别的 native 照递：GML 的这批 VM_* 大量复用 instance_create_depth / get_load_sprite /
//    deck_get_card_data / asset_get_index 这些 native。这里不重写它们的语义，按 native 的
//    调用约定递过去（forward.cpp 调 kGameFns 也是这个路子，见 call_fn()）。
//    那些还是转发壳（等宿主注册 callbin）的，递过去会照它们的规矩回 undefined / -1，
//    本文件不再另外补一套语义。
//
// ③ 联机不做：GML 里 _VM_id_to_real / MSG_* / VM_ClientWrapId 那一整套是网络层的事，
//    这份 VM 里头没有网络（也没名字），一律跳过；_VM_create_counter 照常自增（它是
//    本地可见的记账），客户端负号包装 id 那条路不存在（没有 client 模式）。
//
// ④ card_created / card_destroyed 是**转发类 native**（要宿主注册 callbin）。本批的
//    SpawnPlant / ClearPlants* / SpawnPlantsRandom 要"进格子列表 / 出格子列表 / 记 last_*"，
//    这几个 VM 侧效果本文件自己做（plant_link / plant_kill），**不**再把整段转发出去：
//    避免宿主没接时每株植物都置一个 VM_E_FN_NOT_FOUND，也避免两端各做一遍簿记。
//    （挂载点 _VM_CARD_CREATED / 音效 / 网络同步因此不会从这里发出 —— 记在文件尾的差异里。）
// ============================================================

namespace {

// ============================================================
// 名字懒加载
//   core/const_str.h 是共享文件（别的批次在同时改），不许往里加名字；
//   本文件要用的、名单里没有的名字，第一次用到时 intern 一次
//   （dispatch.cpp 的 user_event_init 就是这个套路），之后只是一次哈希查找。
// ============================================================
VM_INT intern_once(const char* t) {
    static std::unordered_map<std::string, VM_INT> c;
    std::string k(t);
    std::unordered_map<std::string, VM_INT>::iterator it = c.find(k);
    if (it != c.end()) return it->second;
    VM_INT sid = intern_str(k);
    c[k] = sid;
    return sid;
}

// ============================================================
// 实参 / 回值（和 game_self.cpp 同一口径，各文件自己的匿名命名空间里各一份）
// ============================================================
VM_INT num_arg(Value* mem, const uint8_t* args, int k) {
    return (VM_INT)arg_num(mem, args, k);
}
// 第 k 个实参当"名字"→ 池下标；不是字符串 / 不在池里 → -1
VM_INT name_arg(Value* mem, const uint8_t* args, int k) {
    Value v = arg_v(mem, args, k);
    if (v.t != MEM_STRING || !str_ok(v.v)) return -1;
    return v.v;
}

// 数字结果写槽：整数存 INT、否则 FLOAT（和 builtin.cpp / game_self.cpp 一个规矩）
Value ga_num(VM_FLOAT x) {
    if (std::floor(x) == x && x >= (VM_FLOAT)-2147483647.0 && x <= (VM_FLOAT)2147483647.0)
        return mk_int((VM_INT)x);
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}
void wr_num(Value* mem, int dst, VM_FLOAT x) { wr_v(mem, dst, ga_num(x)); }

// ============================================================
// global.* 读
// ============================================================
// 全局数字（没有 / 不是数字 → def）
VM_FLOAT gnum(VM_INT sid, VM_FLOAT def) {
    Value* p = global_var_sid(sid);
    if (!p) return def;
    if (p->t == MEM_FLOAT) return f_of(p->v);
    if (p->t == MEM_INT)   return (VM_FLOAT)p->v;
    return def;
}
VM_INT gnum_i(VM_INT sid, VM_INT def) { return (VM_INT)gnum(sid, (VM_FLOAT)def); }

// ============================================================
// 实例属性读（读不到 → def）
// ============================================================
VM_FLOAT prop_num(Instance* in, VM_INT sid, VM_FLOAT def) {
    if (!in) return def;
    Value* p = inst_prop(*in, sid);
    if (!p) return def;
    if (p->t == MEM_FLOAT) return f_of(p->v);
    if (p->t == MEM_INT)   return (VM_FLOAT)p->v;
    return def;
}
VM_INT prop_num_i(Instance* in, VM_INT sid, VM_INT def) { return (VM_INT)prop_num(in, sid, (VM_FLOAT)def); }
// 实例上的字符串 → 池下标（不是字符串 / 没有 → -1）
VM_INT prop_str(Instance* in, VM_INT sid) {
    if (!in) return -1;
    Value* p = inst_prop(*in, sid);
    if (!p || p->t != MEM_STRING || !str_ok(p->v)) return -1;
    return p->v;
}

// ============================================================
// struct / ds_map 字段（只读，不动引用计数）
// ============================================================
bool map_field(const Value& m, VM_INT key_sid, Value* out) {
    if (m.t != MEM_MAP || !container_alive(m)) return false;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[(size_t)m.v];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(mk_str_sid(key_sid));
    if (it == mp.end()) return false;
    if (out) *out = it->second;
    return true;
}

// ============================================================
// 网格换算（照 GML 的算式；host 表里的同名 native 现在是空壳）
// ============================================================
void grid_dims(int* cols, int* rows) {
    *cols = gnum_i(S_grid_cols, 0);
    *rows = gnum_i(S_grid_rows, 0);
}
// get_world_position_from_grid：格心 = offset + col*cell + cell/2
void grid_world(int col, int row, VM_FLOAT* x, VM_FLOAT* y) {
    VM_FLOAT ox = gnum(S_grid_offset_x, 0), oy = gnum(S_grid_offset_y, 0);
    VM_FLOAT cx = gnum(S_grid_cell_size_x, 0), cy = gnum(S_grid_cell_size_y, 0);
    *x = ox + (VM_FLOAT)col * cx + cx / 2;
    *y = oy + (VM_FLOAT)row * cy + cy / 2;
}
// get_grid_position_from_world 的回绕（负 / 超界 → 折进扩展区），SwapPlants 那批要
int grid_wrap_col(int col) {
    int cols = gnum_i(S_grid_cols, 0);
    if (col < 0) col += cols + 64;
    if (col >= cols + 64) col -= cols + 64;
    return col;
}
int grid_wrap_row(int row) {
    int rows = gnum_i(S_grid_rows, 0);
    if (row < 0) row += rows + 64;
    if (row >= rows + 64) row -= rows + 64;
    return row;
}
// calculate_plant_depth：-10 - row*45 - col*5，按层级名再扣 0..4，夹到 [-330,-10]
VM_FLOAT plant_depth(int col, int row, VM_INT plant_type) {
    VM_FLOAT d = -10 - (VM_FLOAT)row * 45 - (VM_FLOAT)col * 5;
    const VM_INT lay[5] = { S_lilypad, S_shield_inner, S_normal, S_shield_outer, S_coffee };
    for (int i = 0; i < 5; i++) {
        if (plant_type >= 0 && plant_type == lay[i]) { d -= (VM_FLOAT)i; break; }
    }
    if (d < -330) d = -330;
    if (d > -10)  d = -10;
    return d;
}

// ============================================================
// 植物格子：global.grid_plants 的 (col,row) 那格（ds_list）
// ============================================================
std::vector<Value>* plant_cell(int col, int row) {
    Value* g = global_var_sid(S_grid_plants);
    if (!g || g->t != MEM_GRID || !container_alive(*g)) return NULL;
    Grid& gr = g_grid_pool[(size_t)g->v];
    if (col < 0 || col >= gr.w || row < 0 || row >= gr.h) return NULL;
    Value cv = gr.cells[(size_t)row * (size_t)gr.w + (size_t)col];
    if (cv.t != MEM_LIST || !container_alive(cv)) return NULL;
    return &g_list_pool[(size_t)cv.v];
}
// 这一格还活着的植物 id（按列表顺序）
void cell_plant_ids(int col, int row, std::vector<VM_INT>* out) {
    std::vector<Value>* lst = plant_cell(col, row);
    if (!lst) return;
    for (size_t i = 0; i < lst->size(); i++) {
        const Value& e = (*lst)[i];
        if (e.t != MEM_INT) continue;
        if (!inst_get(e.v)) continue;
        out->push_back(e.v);
    }
}
// 清空这一格（元素引用要放掉）
void cell_clear(int col, int row) {
    std::vector<Value>* lst = plant_cell(col, row);
    if (!lst) return;
    for (size_t i = 0; i < lst->size(); i++) release((*lst)[i]);
    lst->clear();
}
void cell_add(int col, int row, VM_INT id) {
    std::vector<Value>* lst = plant_cell(col, row);
    if (!lst) return;
    Value v = mk_int(id);
    retain(v);                                   // 进列表 = 多一个引用（int 是空操作，写法对齐容器）
    lst->push_back(v);
}

// 把植物从它所在格的列表里摘掉（= card_destroyed 的 VM 侧效果）
void plant_unlink(VM_INT id) {
    Instance* in = inst_get(id);
    if (!in) return;
    int col = prop_num_i(in, S_grid_col, -1);
    int row = prop_num_i(in, S_grid_row, -1);
    if (col < 0 || row < 0) return;
    std::vector<Value>* lst = plant_cell(col, row);
    if (!lst) return;
    for (size_t i = 0; i < lst->size(); i++) {
        if ((*lst)[i].t == MEM_INT && (*lst)[i].v == id) {
            release((*lst)[i]);
            lst->erase(lst->begin() + (std::vector<Value>::difference_type)i);
            return;
        }
    }
}
// 销毁一株植物：先从格子列表摘掉 → 记 last_destroyed → 走销毁唯一入口（Destroy 事件在里面跑）
void plant_kill(VM_INT id) {
    if (!inst_get(id)) return;
    plant_unlink(id);
    global_var_set_sid(S__VM_last_destroyed_card, mk_int(id));
    inst_destroy(id);
}

// 绑定小物件跟着植物走（update_plant_bindings 里 stars / sleep / water 那三支）
void bind_update(VM_INT id) {
    Instance* in = inst_get(id);
    if (!in) return;
    VM_FLOAT px = prop_num(in, S_x, 0), py = prop_num(in, S_y, 0), pd = prop_num(in, S_depth, 0);
    const VM_INT  kid[3] = { S_banding_star_obj, S_banding_sleep_obj, S_banding_water_obj };
    const VM_FLOAT dx[3] = { 0, -15, 0 };
    const VM_FLOAT dy[3] = { -5, -20, 0 };
    const VM_FLOAT dd[3] = { -1, -1, 5 };
    for (int k = 0; k < 3; k++) {
        Value* p = inst_prop(*in, kid[k]);
        if (!p || p->t != MEM_INT || !inst_get(p->v)) continue;
        VM_INT c = p->v;
        inst_var_set_sid(c, S_x,     ga_num(px + dx[k]));
        inst_var_set_sid(c, S_y,     ga_num(py + dy[k]));
        inst_var_set_sid(c, S_depth, ga_num(pd + dd[k]));
    }
}

// 把一株植物摆到 (col,row) 的格心：位置 / 格号 / 深度 / 绑定（Swap·Compact 那批共用）
void plant_place(VM_INT id, int col, int row) {
    Instance* in = inst_get(id);
    if (!in) return;
    VM_FLOAT x, y;
    grid_world(col, row, &x, &y);
    inst_var_set_sid(id, S_x, ga_num(x));
    inst_var_set_sid(id, S_y, ga_num(y));
    inst_var_set_sid(id, S_grid_col, mk_int(col));
    inst_var_set_sid(id, S_grid_row, mk_int(row));
    inst_var_set_sid(id, S_depth, ga_num(plant_depth(col, row, prop_str(in, S_plant_type))));
    bind_update(id);
}

// ============================================================
// 地形：global.grid_terrains[row][col] 是个 struct
// ============================================================
bool terrain_cell(int col, int row, Value* out) {
    Value* g = global_var_sid(S_grid_terrains);
    if (!g || g->t != MEM_ARRAY || !container_alive(*g)) return false;
    std::vector<Value>& rows = g_arr_pool[(size_t)g->v];
    if (row < 0 || row >= (int)rows.size()) return false;
    const Value& rv = rows[(size_t)row];
    if (rv.t != MEM_ARRAY || !container_alive(rv)) return false;
    std::vector<Value>& cs = g_arr_pool[(size_t)rv.v];
    if (col < 0 || col >= (int)cs.size()) return false;
    *out = cs[(size_t)col];
    return true;
}
VM_INT terrain_type(int col, int row) {
    Value cell;
    if (!terrain_cell(col, row, &cell)) return -1;
    Value t;
    if (!map_field(cell, S_type, &t)) return -1;
    return (t.t == MEM_STRING && str_ok(t.v)) ? t.v : -1;
}
// 写 terrain.type：照 ds_map_set 的规矩（先 allow_store，再 retain 新的 / release 旧的）
void terrain_set(int col, int row, const Value& nv) {
    Value cell;
    if (!terrain_cell(col, row, &cell)) return;
    if (cell.t != MEM_MAP || !container_alive(cell)) return;
    Value key = mk_str_sid(S_type);
    std::map<Value, Value, ValueLess>& mp = g_map_pool[(size_t)cell.v];
    if (!allow_store(key, cell)) return;
    if (!allow_store(nv, cell)) return;
    std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
    if (it != mp.end()) {
        Value ov = it->second;
        retain(nv);
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        it->second = nv;
    } else {
        retain(key);
        retain(nv);
        mp[key] = nv;
    }
}

// ============================================================
// 扁平一维表的一格（VM_CellCount 那套：下标 = row*(grid_cols+2)+col）
// ============================================================
// 数组存不存在 / 某下标能不能取（cell 是不是数组由调用方再判）
std::vector<Value>* flat_table(VM_INT name_sid) {
    Value* g = global_var_sid(name_sid);
    if (!g || g->t != MEM_ARRAY || !container_alive(*g)) return NULL;
    return &g_arr_pool[(size_t)g->v];
}
// 读一格的数字（has_enemy_* 的前缀和就是这个形状）
bool arr_num_at(const std::vector<Value>& a, int idx, VM_FLOAT* out) {
    if (idx < 0 || idx >= (int)a.size()) return false;
    const Value& v = a[(size_t)idx];
    if (v.t == MEM_INT)   { *out = (VM_FLOAT)v.v; return true; }
    if (v.t == MEM_FLOAT) { *out = f_of(v.v);     return true; }
    return false;
}

// ============================================================
// 收集某个对象名下所有活实例 id（with(obj) 的等价物）
//   先全部抄出来再动手：销毁 / 创建会改内层的容器，边遍历边动会跳格子
// ============================================================
void collect_objs(VM_INT obj_sid, std::vector<VM_INT>* out) {
    VM_INT n = obj_count(obj_sid);
    for (VM_INT k = 0; k < n; k++) {
        VM_INT id = inst_at(obj_sid, k);
        if (id >= 0) out->push_back(id);
    }
}

// ============================================================
// 调一个 native
//   GML 参考里 VM_* 大量复用别的 native（instance_create_depth / get_load_sprite /
//   deck_get_card_data …），这里不重写它们的语义，按 native 的调用约定递过去。
//   临时槽：forward.cpp 用 OUTER_TMP_SLOT +0..+8，本文件用 +9..+31（两层嵌套各 11 个）。
//   ⚠ 返回值带一份**自己的引用**（容器由调用方 release）；嵌套超过两层 → undefined。
// ============================================================
const int GA_TMP_BASE = OUTER_TMP_SLOT + 9;
const int GA_ARGS     = 10;
int g_ga_depth = 0;

Value call_fn(Value* mem, VM_INT sid, const Value* argv, int n) {
    Fn f = fn_of_sid(sid);
    if (!f || g_ga_depth > 1) return mk_undef();
    if (n > GA_ARGS) n = GA_ARGS;
    if (n < 0) n = 0;
    int base = GA_TMP_BASE + g_ga_depth * (GA_ARGS + 1);
    int32_t tbl[GA_ARGS];
    for (int i = 0; i < n; i++) {
        wr_v(mem, base + i, argv[i]);
        tbl[i] = base + i;
    }
    int dst = base + GA_ARGS;
    wr_v(mem, dst, mk_int(0));
    g_ga_depth++;
    f(mem, (const uint8_t*)tbl, n, dst);
    g_ga_depth--;
    Value r = mem[dst];
    retain(r);                                        // 先自己拿一份（下面清槽会放掉槽里那份）
    for (int i = 0; i <= GA_ARGS; i++) slot_write(mem, base + i, mk_int(0));
    return r;
}
// 只关心一个数字结果的那种调用
VM_INT call_fn_i(Value* mem, VM_INT sid, const Value* argv, int n, VM_INT def) {
    Value r = call_fn(mem, sid, argv, n);
    VM_INT out = def;
    if (r.t == MEM_INT)        out = r.v;
    else if (r.t == MEM_FLOAT) out = (VM_INT)f_of(r.v);
    release(r);
    return out;
}

// ============================================================
// deck_get_card_data(card_id, shape) → struct（拿不到 → false）
// ============================================================
bool card_data_of(Value* mem, const Value& card_id, VM_INT shape, Value* out) {
    if (card_id.t != MEM_STRING || !str_ok(card_id.v)) return false;
    if (!fn_of_sid(S_deck_get_card_data)) return false;   // 宿主没给这条 → 当拿不到
    Value av[2];
    av[0] = card_id;
    av[1] = mk_int(shape < 0 ? 0 : shape);
    Value cd = call_fn(mem, S_deck_get_card_data, av, 2);
    if (cd.t == MEM_UNDEFINED || cd.t == MEM_INT) { release(cd); return false; }  // undefined / noone(-4)
    if (cd.t != MEM_MAP) { release(cd); return false; }
    *out = cd;                                            // 引用交给调用方（用完 release）
    return true;
}

// ============================================================
// 建一株植物（GML 的 spawn_plant + card_created 的 VM 侧部分）
//   简化（都记在文件尾）：平台偏移 / 替换旧卡 / 放置特效 / 联机同步不做
// ============================================================
struct PlantProp { VM_INT sid; Value v; };

VM_INT spawn_one_plant(Value* mem, VM_INT obj_sid, int col, int row,
                       const PlantProp* props, int nprops, VM_INT card_id_sid) {
    if (obj_sid < 0) return -1;
    VM_FLOAT x, y;
    grid_world(col, row, &x, &y);
    Value av[4];
    av[0] = ga_num(x);
    av[1] = ga_num(y);
    av[2] = mk_int(0);
    av[3] = mk_str_sid(obj_sid);
    // mod 卡：建实例前把身份写进 pending（mod 卡的 Create 靠它认自己是谁）
    bool is_mod = (obj_sid == S_obj_card_mod);
    if (is_mod && card_id_sid >= 0) global_var_set_sid(S__mod_pending_card_id, mk_str_sid(card_id_sid));
    VM_INT id = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    if (is_mod && card_id_sid >= 0) global_var_set_sid(S__mod_pending_card_id, mk_str_sid(S_empty));
    if (id == NOONE || !inst_get(id)) return -1;

    for (int i = 0; i < nprops; i++) inst_var_set_sid(id, props[i].sid, props[i].v);

    // 深度要等实例建好再算：plant_type 是它自己的 Create 事件写的
    VM_INT pt = prop_str(inst_get(id), S_plant_type);
    inst_var_set_sid(id, S_depth, ga_num(plant_depth(col, row, pt)));

    // _VM_id：本 VM 的自增号（联网那套不用，只留本地可见的记账）
    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));
    inst_var_set_sid(id, intern_once("_VM_id"), mk_int(ctr));

    plant_place(id, col, row);          // 格号 + 绑定（位置已经对，重设一次无害且统一）
    cell_add(col, row, id);             // card_created 的 ds_list_add
    global_var_set_sid(S__VM_last_created_card, mk_int(id));
    return id;
}

}  // namespace

// ============================================================
// 生成 / 平台（9 条）
// ============================================================
// VM_CreatePlatform(col,row,width,length,axis,distance,idle,spr) → 平台实例 id
//   位置 = 格心 - 半格 - 35（照 GML）；axis==0 → "y"，否则 "x"
static void impl_VM_CreatePlatform(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 8) { wr_i(mem, dst, -1); return; }
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_INT width    = num_arg(mem, args, 2);
    VM_INT length   = num_arg(mem, args, 3);
    VM_INT axis     = num_arg(mem, args, 4);
    VM_FLOAT distance = arg_num(mem, args, 5);
    VM_FLOAT idle     = arg_num(mem, args, 6);
    Value spr = arg_v(mem, args, 7);

    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));

    VM_FLOAT px, py;
    grid_world(col, row, &px, &py);
    VM_FLOAT cx = gnum(S_grid_cell_size_x, 0), cy = gnum(S_grid_cell_size_y, 0);
    Value av[4];
    av[0] = ga_num(px - cx / 2);
    av[1] = ga_num(py - cy / 2 - 35);
    av[2] = mk_int(800);
    av[3] = mk_str_sid(S_obj_platform);
    VM_INT plat = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    if (plat == NOONE || !inst_get(plat)) { wr_i(mem, dst, -1); return; }

    inst_var_set_sid(plat, S_start_col, mk_int(col));
    inst_var_set_sid(plat, S_start_row, mk_int(row));
    inst_var_set_sid(plat, S_width,     mk_int(width));
    inst_var_set_sid(plat, S_length,    mk_int(length));
    // "y" / "x" 这两个文本本来就是名单里的（内置字段名 x / y 占的就是这两个池下标）
    inst_var_set_sid(plat, S_move_axis, mk_str_sid(axis == 0 ? S_y : S_x));
    inst_var_set_sid(plat, intern_once("move_distance"),          ga_num(distance));
    inst_var_set_sid(plat, intern_once("initial_offset"),         mk_int(0));
    inst_var_set_sid(plat, intern_once("initial_idle_duration"),  mk_int(0));
    inst_var_set_sid(plat, intern_once("boundary_idle_duration"), ga_num(idle));
    inst_var_set_sid(plat, intern_once("_VM_id"),                 mk_int(ctr));

    // sprite：GML 走 get_load_sprite 的缓存链（宿主没接就回 -1，照写）
    Value gs = call_fn(mem, S_get_load_sprite, &spr, 1);
    if (gs.t == MEM_INT && gs.v != -1) inst_var_set_sid(plat, S_sprite_index, gs);
    release(gs);
    wr_i(mem, dst, plat);
}

// VM_SpawnPlant(card_id,col,row,shape,level,skill) → 植物实例 id
//   col / row 传 -1 = 整列 / 整行批量种（批量时回 0，照 GML）
static void impl_VM_SpawnPlant(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    Value card_id = arg_v(mem, args, 0);
    int col = num_arg(mem, args, 1);
    int row = num_arg(mem, args, 2);
    VM_INT shape = (argc > 3) ? num_arg(mem, args, 3) : 0;
    VM_INT level = (argc > 4) ? num_arg(mem, args, 4) : 0;
    VM_INT skill = (argc > 5) ? num_arg(mem, args, 5) : 0;

    Value cd;
    if (!card_data_of(mem, card_id, shape, &cd)) { wr_i(mem, dst, -1); return; }
    Value obj;
    if (!map_field(cd, S_obj, &obj) || obj.t != MEM_STRING || !str_ok(obj.v)) { release(cd); wr_i(mem, dst, -1); return; }
    VM_INT obj_sid = obj.v;
    release(cd);

    PlantProp props[3];
    int np = 0;
    if (level != -1) { props[np].sid = S_current_level; props[np].v = mk_int(level); np++; }
    if (skill != -1) { props[np].sid = S_skill;         props[np].v = mk_int(skill); np++; }
    if (shape != -1) { props[np].sid = S_shape;         props[np].v = mk_int(shape); np++; }

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    bool batch = (col == -1 || row == -1);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;

    VM_INT last = -1;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            VM_INT id = spawn_one_plant(mem, obj_sid, c, r, props, np, card_id.v);
            if (id >= 0) last = id;
        }
    }
    if (batch) { wr_i(mem, dst, 0); return; }        // 批量：回 0（GML 一致）
    wr_i(mem, dst, last);
}

// VM_SpawnEnemy(type,row,hp_override) → 敌人实例 id
static void impl_VM_SpawnEnemy(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT type_sid = name_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_FLOAT hp = (argc > 2) ? arg_num(mem, args, 2) : (VM_FLOAT)0;
    if (type_sid < 0) { wr_i(mem, dst, -1); return; }

    Value* em = global_var_sid(S_enemy_map);
    Value info, obj;
    if (!em || !map_field(*em, type_sid, &info)) { wr_i(mem, dst, -1); return; }
    if (!map_field(info, intern_once("_obj"), &obj) || obj.t != MEM_STRING || !str_ok(obj.v)) {
        wr_i(mem, dst, -1); return;
    }

    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    VM_FLOAT px, py;
    grid_world(cols, row, &px, &py);                 // GML：从 grid_cols 那一格出发
    global_var_set_sid(S__mod_pending_enemy_id, mk_str_sid(type_sid));   // mod 敌人建之前写 pending

    Value av[4];
    av[0] = ga_num(px + 30);
    av[1] = ga_num(py + 38);
    av[2] = mk_int(0);
    av[3] = mk_str_sid(obj.v);
    VM_INT id = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    global_var_set_sid(S__mod_pending_enemy_id, mk_str_sid(S_empty));
    if (id == NOONE || !inst_get(id)) { wr_i(mem, dst, -1); return; }

    if (hp > 0) {
        inst_var_set_sid(id, S_hp,    ga_num(hp));
        inst_var_set_sid(id, S_maxhp, ga_num(hp));
    }
    inst_var_set_sid(id, intern_once("_VM_id"), mk_int(ctr));
    wr_i(mem, dst, id);
}

// VM_SpawnBoss(type,row,hp_override) → BOSS 实例 id
static void impl_VM_SpawnBoss(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT type_sid = name_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_FLOAT hp = (argc > 2) ? arg_num(mem, args, 2) : (VM_FLOAT)0;
    if (type_sid < 0) { wr_i(mem, dst, -1); return; }

    Value* em = global_var_sid(S_enemy_map);
    Value info, obj;
    if (!em || !map_field(*em, type_sid, &info)) { wr_i(mem, dst, -1); return; }
    if (!map_field(info, intern_once("_obj"), &obj) || obj.t != MEM_STRING || !str_ok(obj.v)) {
        wr_i(mem, dst, -1); return;
    }

    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));

    VM_FLOAT px, py;
    grid_world(10, row, &px, &py);                   // GML：从第 10 列出发
    global_var_set_sid(S__mod_pending_enemy_id, mk_str_sid(type_sid));

    Value av[4];
    av[0] = ga_num(px - 80);
    av[1] = ga_num(py + 30);
    av[2] = mk_int(-200);
    av[3] = mk_str_sid(obj.v);
    VM_INT id = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    global_var_set_sid(S__mod_pending_enemy_id, mk_str_sid(S_empty));
    if (id == NOONE || !inst_get(id)) { wr_i(mem, dst, -1); return; }

    if (hp > 0) {
        inst_var_set_sid(id, S_hp,    ga_num(hp));
        inst_var_set_sid(id, S_maxhp, ga_num(hp));
    }
    inst_var_set_sid(id, intern_once("_VM_id"), mk_int(ctr));
    // obj_battle.boss_count++
    VM_INT bat = inst_at(S_obj_battle, 0);
    if (bat >= 0) inst_var_set_sid(bat, intern_once("boss_count"), mk_int(prop_num_i(inst_get(bat), intern_once("boss_count"), 0) + 1));
    global_var_set_sid(intern_once("_VM_last_boss"), mk_int(id));
    wr_i(mem, dst, id);
}

// VM_SpawnObject(obj_name,col,row) → 最后一个实例 id
//   名字不以 "obj_" 开头就补上；col/row 传 -1 = 整列 / 整行
static void impl_VM_SpawnObject(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_INT name_sid = name_arg(mem, args, 0);
    int col = num_arg(mem, args, 1);
    int row = num_arg(mem, args, 2);
    if (name_sid < 0) { wr_i(mem, dst, -1); return; }

    // 对象名必须能查出资源：asset_get_index 是转发类 native（宿主给不了就 -1）
    const std::string& nm = str_at(name_sid);
    VM_INT full_sid = name_sid;
    if (nm.size() < 4 || nm.compare(0, 4, str_at(S_obj_)) != 0) {
        // 前缀 "obj_" 是定死的文本 → 只认名单里的 S_obj_（之前这里是字面量，等于两个真源）
        //   拼出来的整名是动态串，仍照旧 intern（动态字符串允许）
        full_sid = intern_str(std::string(str_at(S_obj_)) + nm);
    }
    Value av[1];
    av[0] = mk_str_sid(full_sid);
    VM_INT obj_sid = call_fn_i(mem, S_asset_get_index, av, 1, -1);
    if (obj_sid < 0) { wr_i(mem, dst, -1); return; }

    // 三种地图对象的深度 / Y 偏移（照 GML 的 switch）
    VM_INT depth = -1200;
    VM_FLOAT y_off = -35;
    if (full_sid == intern_once("obj_mouse_hole"))        { depth = -5;  y_off = 0; }
    else if (full_sid == intern_once("obj_pharaoh_hole")) { depth = -5;  y_off = 0; }
    else if (full_sid == intern_once("obj_cloud"))        { depth = 10;  y_off = -10; }

    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;

    VM_INT last = -1;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            VM_FLOAT px, py;
            grid_world(c, r, &px, &py);
            Value av2[4];
            av2[0] = ga_num(px);
            av2[1] = ga_num(py + y_off);
            av2[2] = mk_int(depth);
            av2[3] = mk_str_sid(obj_sid);
            VM_INT id = call_fn_i(mem, S_instance_create_depth, av2, 4, NOONE);
            if (id == NOONE || !inst_get(id)) continue;
            inst_var_set_sid(id, S_row, mk_int(r));
            inst_var_set_sid(id, S_col, mk_int(c));
            inst_var_set_sid(id, intern_once("_VM_id"), mk_int(ctr));
            last = id;
        }
    }
    wr_i(mem, dst, last);
}

// VM_SpawnCats(enable)：只是写一个开关全局（真正生成猫的是关卡侧）
static void impl_VM_SpawnCats(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    global_var_set_sid(intern_once("_VM_spawn_cats"), mk_int(num_arg(mem, args, 0) != 0 ? 1 : 0));
}

// VM_SpawnPlantsRandom(x,y,w,h,shape,level,skill,card1..card9)
//   矩形区域内"空格子"随机种一张卡；没有有效卡名 / w,h<=0 → 什么都不做
static void impl_VM_SpawnPlantsRandom(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 6) return;
    int x = num_arg(mem, args, 0);
    int y = num_arg(mem, args, 1);
    int w = num_arg(mem, args, 2);
    int h = num_arg(mem, args, 3);
    if (w <= 0 || h <= 0) return;
    Value shape_v = arg_v(mem, args, 4);
    Value level_v = arg_v(mem, args, 5);
    Value skill_v = (argc > 6) ? arg_v(mem, args, 6) : mk_undef();
    VM_INT shape = (shape_v.t == MEM_UNDEFINED) ? -1 : (VM_INT)((shape_v.t == MEM_FLOAT) ? f_of(shape_v.v) : shape_v.v);
    VM_INT level = (level_v.t == MEM_UNDEFINED) ? -1 : (VM_INT)((level_v.t == MEM_FLOAT) ? f_of(level_v.v) : level_v.v);
    VM_INT skill = (skill_v.t == MEM_UNDEFINED) ? -1 : (VM_INT)((skill_v.t == MEM_FLOAT) ? f_of(skill_v.v) : skill_v.v);

    // 收集有效卡名（7..15；undefined / -1 / "" 跳过）
    VM_INT cards[9];
    int nc = 0;
    for (int k = 7; k < 16 && k < argc; k++) {
        Value v = arg_v(mem, args, k);
        if (v.t == MEM_UNDEFINED) continue;
        if (v.t == MEM_INT && v.v == -1) continue;
        if (v.t == MEM_STRING) {
            if (!str_ok(v.v) || v.v == S_empty) continue;
            cards[nc++] = v.v;
        }
    }
    if (nc == 0) return;

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = x, c2 = x + w - 1, r1 = y, r2 = y + h - 1;
    if (c1 < 0) c1 = 0; if (c1 > cols - 1) c1 = cols - 1;
    if (c2 < 0) c2 = 0; if (c2 > cols - 1) c2 = cols - 1;
    if (r1 < 0) r1 = 0; if (r1 > rows - 1) r1 = rows - 1;
    if (r2 < 0) r2 = 0; if (r2 > rows - 1) r2 = rows - 1;

    for (int c = c1; c <= c2; c++) {
        for (int r = r1; r <= r2; r++) {
            std::vector<Value>* lst = plant_cell(c, r);
            if (!lst || !lst->empty()) continue;                 // 格子已有东西 → 跳过
            VM_INT pick = cards[(int)(rand() % nc)];
            Value cd;
            if (!card_data_of(mem, mk_str_sid(pick), shape, &cd)) continue;
            Value obj;
            bool ok = map_field(cd, S_obj, &obj) && obj.t == MEM_STRING && str_ok(obj.v);
            release(cd);
            if (!ok) continue;
            PlantProp props[3];
            int np = 0;
            if (level != -1) { props[np].sid = S_current_level; props[np].v = mk_int(level); np++; }
            if (skill != -1) { props[np].sid = S_skill;         props[np].v = mk_int(skill); np++; }
            if (shape != -1) { props[np].sid = S_shape;         props[np].v = mk_int(shape); np++; }
            spawn_one_plant(mem, obj.v, c, r, props, np, pick);
        }
    }
}

// VM_CreateButton(x,y,sprite,scale,idle,hover,press) → 按钮实例 id
//   GML 用的是 Button 构造器的方法链（set_sprite/set_scale/...），落到 VM 世界就是
//   写它 state 结构体里的那几个字段（objects/Button/Create_0.gml 里那几个名字）。
static void impl_VM_CreateButton(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_FLOAT x = arg_num(mem, args, 0);
    VM_FLOAT y = arg_num(mem, args, 1);
    Value spr_name = arg_v(mem, args, 2);
    // GML：is_undefined 或 -1 都回默认值
    VM_FLOAT scale = (argc > 3 && arg_v(mem, args, 3).t != MEM_UNDEFINED) ? arg_num(mem, args, 3) : (VM_FLOAT)1;
    VM_FLOAT idle  = (argc > 4 && arg_v(mem, args, 4).t != MEM_UNDEFINED) ? arg_num(mem, args, 4) : (VM_FLOAT)0;
    VM_FLOAT hover = (argc > 5 && arg_v(mem, args, 5).t != MEM_UNDEFINED) ? arg_num(mem, args, 5) : (VM_FLOAT)1;
    VM_FLOAT press = (argc > 6 && arg_v(mem, args, 6).t != MEM_UNDEFINED) ? arg_num(mem, args, 6) : (VM_FLOAT)2;
    if (scale == -1) scale = 1;
    if (idle  == -1) idle  = 0;
    if (hover == -1) hover = 1;
    if (press == -1) press = 2;

    Value spr = call_fn(mem, S_get_load_sprite, &spr_name, 1);

    Value av[4];
    av[0] = ga_num(x);
    av[1] = ga_num(y);
    av[2] = mk_int(-5000);
    av[3] = mk_str_sid(intern_once("Button"));
    VM_INT id = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    if (id == NOONE || !inst_get(id)) { release(spr); wr_i(mem, dst, -1); return; }

    // state 是 Create 里建好的结构体；字段名不在名单里的用 intern_once 补
    Value* sp = inst_var_sid(id, intern_once("state"));
    if (sp && sp->t == MEM_MAP && container_alive(*sp)) {
        std::map<Value, Value, ValueLess>& mp = g_map_pool[(size_t)sp->v];
        const VM_INT keys[7] = {
            S_sprite, intern_once("scale"), intern_once("offset_x"), intern_once("offset_y"),
            intern_once("frame_idle"), intern_once("frame_hover"), intern_once("frame_press")
        };
        Value vals[7];
        vals[0] = spr;
        vals[1] = ga_num(scale);
        vals[2] = ga_num(x);
        vals[3] = ga_num(y);
        vals[4] = ga_num(idle);
        vals[5] = ga_num(hover);
        vals[6] = ga_num(press);
        for (int k = 0; k < 7; k++) {
            Value key = mk_str_sid(keys[k]);
            if (!allow_store(key, *sp) || !allow_store(vals[k], *sp)) continue;
            std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
            if (it != mp.end()) {
                Value ov = it->second;
                retain(vals[k]);
                if (ov.t != vals[k].t || ov.v != vals[k].v) release(ov);
                it->second = vals[k];
            } else {
                retain(key);
                retain(vals[k]);
                mp[key] = vals[k];
            }
        }
    }
    release(spr);
    wr_i(mem, dst, id);
}

// VM_SpawnBatMouse(col,row) → 蝙蝠鼠实例 id（目标标记 + 本体两只）
static void impl_VM_SpawnBatMouse(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    int b_col = num_arg(mem, args, 0);
    int b_row = num_arg(mem, args, 1);

    VM_INT ctr = gnum_i(intern_once("_VM_create_counter"), 0) + 1;
    global_var_set_sid(intern_once("_VM_create_counter"), mk_int(ctr));

    VM_FLOAT px, py;
    grid_world(b_col, b_row, &px, &py);
    VM_FLOAT rh = gnum(S_room_height, 0);

    Value av[4];
    av[0] = ga_num(px + 10);
    av[1] = ga_num(py - rh);
    av[2] = mk_int(-500);
    av[3] = mk_str_sid(intern_once("obj_bat_mouse_target"));
    VM_INT tgt = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    if (tgt == NOONE || !inst_get(tgt)) { wr_i(mem, dst, -1); return; }
    inst_var_set_sid(tgt, intern_once("target_col"), mk_int(b_col));
    inst_var_set_sid(tgt, intern_once("target_row"), mk_int(b_row));

    av[3] = mk_str_sid(intern_once("obj_bat_mouse"));
    VM_INT bat = call_fn_i(mem, S_instance_create_depth, av, 4, NOONE);
    if (bat == NOONE || !inst_get(bat)) { wr_i(mem, dst, -1); return; }
    inst_var_set_sid(bat, intern_once("target_col"), mk_int(b_col));
    inst_var_set_sid(bat, intern_once("target_row"), mk_int(b_row));
    inst_var_set_sid(bat, intern_once("banding_target_inst"), mk_int(tgt));   // 名字不在名单里 → 懒加载
    inst_var_set_sid(bat, intern_once("_VM_id"), mk_int(ctr));
    wr_i(mem, dst, bat);
}

// VM_HomingBulletAdd(…)：GML 就是原样转给 homing_bullet_add。
//   参数表本来就是槽号表 → 原样递给那条 native（省掉一次 11 个槽的搬运）。
static void impl_VM_HomingBulletAdd(Value* mem, const uint8_t* args, int argc, int dst) {
    Fn f = fn_of_sid(S_homing_bullet_add);
    if (!f) { wr_i(mem, dst, -1); return; }
    f(mem, args, argc, dst);
}

// ============================================================
// 战场 / 格子（地形 · 植物 · 地图物件）
// ============================================================
// VM_SetTerrain(col,row,type)：col/row 传 -1 = 所有列 / 行
static void impl_VM_SetTerrain(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    Value nv = arg_v(mem, args, 2);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    for (int r = r1; r <= r2; r++)
        for (int c = c1; c <= c2; c++) terrain_set(c, r, nv);
}

// VM_ClearPlants(col,row)：player 不清；其余走 plant_kill（摘列表 + Destroy）
static void impl_VM_ClearPlants(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    std::vector<VM_INT> ids;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            ids.clear();
            cell_plant_ids(c, r, &ids);              // 先抄下来：销毁会改列表
            for (size_t i = ids.size(); i > 0; i--) {
                VM_INT id = ids[i - 1];
                Instance* in = inst_get(id);
                if (!in) continue;
                if (prop_str(in, S_plant_id) == S_player) continue;   // 玩家本体不清
                plant_kill(id);
            }
        }
    }
}

// VM_ClearMapObjects(col,row,obj_name)
//   obj_name == "all" → 七种地图物件全清；否则补 "obj_" 前缀后 asset_get_index 查
//   col/row 只有**同时**为 -1 才是"不看位置全清"，否则按实例的 row/col 比区间
static void impl_VM_ClearMapObjects(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_INT name_sid = name_arg(mem, args, 2);

    static const char* kAllNames[7] = {
        "obj_obstacle", "obj_wind_tunnel", "obj_lava", "obj_barrier",
        "obj_fog", "obj_cloud", "obj_seawater"
    };
    VM_INT targets[7];
    int nt = 0;
    if (name_sid == S_all) {
        for (int i = 0; i < 7; i++) targets[nt++] = intern_once(kAllNames[i]);
    } else if (name_sid >= 0) {
        const std::string& nm = str_at(name_sid);
        // 前缀同上：文本从池里取（S_obj_），拼接结果的 intern 是动态字符串
        VM_INT full = (nm.size() >= 4 && nm.compare(0, 4, str_at(S_obj_)) == 0)
                    ? name_sid : intern_str(std::string(str_at(S_obj_)) + nm);
        Value av[1];
        av[0] = mk_str_sid(full);
        VM_INT obj_sid = call_fn_i(mem, S_asset_get_index, av, 1, -1);
        if (obj_sid >= 0) targets[nt++] = obj_sid;
    }

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    bool all = (col == -1 && row == -1);

    std::vector<VM_INT> ids;
    for (int t = 0; t < nt; t++) {
        ids.clear();
        collect_objs(targets[t], &ids);
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (all) { inst_destroy(ids[i]); continue; }
            int ir = prop_num_i(in, S_row, -1), ic = prop_num_i(in, S_col, -1);
            if (ir >= r1 && ir <= r2 && ic >= c1 && ic <= c2) inst_destroy(ids[i]);
        }
    }
}

// VM_SetRowFeature(row,feature)：row == -1 = 所有行；feature 直接写进行属性数组
static void impl_VM_SetRowFeature(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    int row = num_arg(mem, args, 0);
    Value fv = arg_v(mem, args, 1);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int r1 = (row == -1) ? 0 : row;
    int r2 = (row == -1) ? rows - 1 : row;
    if (rows <= 0) return;

    VM_INT sid = intern_once("row_feature");
    Value* g = global_var_sid(sid);
    // 没有 / 不是数组 → 按 grid_rows 造一个（GML 那边 row_feature 是常驻全局数组）
    if (!g || g->t != MEM_ARRAY || !container_alive(*g)) {
        Value self{MEM_ARRAY, arr_new()};
        global_var_set_sid(sid, self);               // 全局表接手一份引用
        release(self);                               // 创造者那份交出去
        g = global_var_sid(sid);
        if (!g) return;
    }
    std::vector<Value>& a = g_arr_pool[(size_t)g->v];
    if ((int)a.size() < rows) a.resize((size_t)rows, mk_int(0));
    for (int r = r1; r <= r2; r++) {
        if (r < 0 || r >= (int)a.size()) continue;
        Value ov = a[(size_t)r];
        retain(fv);
        if (ov.t != fv.t || ov.v != fv.v) release(ov);
        a[(size_t)r] = fv;
    }
}

// VM_ClearPlantsByType(name)：name == -1 → 清所有非玩家植物；否则按 card_data.obj 找那一类
static void impl_VM_ClearPlantsByType(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    Value nv = arg_v(mem, args, 0);
    std::vector<VM_INT> ids;

    if (nv.t == MEM_INT && nv.v == -1) {
        collect_objs(S_obj_card_parent, &ids);
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (prop_str(in, S_plant_id) == S_player) continue;
            plant_kill(ids[i]);
        }
        return;
    }
    if (nv.t != MEM_STRING || !str_ok(nv.v)) return;
    Value cd;
    if (!card_data_of(mem, nv, 0, &cd)) return;
    Value obj;
    bool ok = map_field(cd, S_obj, &obj) && obj.t == MEM_STRING && str_ok(obj.v);
    release(cd);
    if (!ok) return;
    collect_objs(obj.v, &ids);
    for (size_t i = 0; i < ids.size(); i++) {
        Instance* in = inst_get(ids[i]);
        if (!in) continue;
        if (prop_str(in, S_plant_id) == S_player) continue;
        plant_kill(ids[i]);
    }
}

// VM_WakePlants(col,row)：睡着的（state == CARD_STATE.SLEEP == 4）非玩家植物 → awake_buff_timer = 1
static void impl_VM_WakePlants(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    std::vector<VM_INT> ids;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            ids.clear();
            cell_plant_ids(c, r, &ids);
            for (size_t i = 0; i < ids.size(); i++) {
                Instance* in = inst_get(ids[i]);
                if (!in) continue;
                if (prop_str(in, S_plant_id) == S_player) continue;
                if (prop_num_i(in, S_state, -1) != 4) continue;
                inst_var_set_sid(ids[i], S_awake_buff_timer, mk_int(1));
            }
        }
    }
}

// VM_GetTerrain(col,row)：0 normal / 1 water / 2 obstacle / -1 越界
static void impl_VM_GetTerrain(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    if (row < 0 || row >= rows || col < 0 || col >= cols) { wr_i(mem, dst, -1); return; }
    VM_INT t = terrain_type(col, row);
    if (t == S_water)    { wr_i(mem, dst, 1); return; }
    if (t == S_obstacle) { wr_i(mem, dst, 2); return; }
    wr_i(mem, dst, 0);
}

// VM_GetEnemyCount() / VM_GetPlantCount()：就是两个对象名下的实例数
static void impl_VM_GetEnemyCount(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, obj_count(S_obj_enemy_parent));
}
static void impl_VM_GetPlantCount(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, obj_count(S_obj_card_parent));
}

// VM_GetPlantCountAt(col,row,type)：type == "all" 不限；col/row == -1 整列 / 整行
static void impl_VM_GetPlantCountAt(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, 0); return; }
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_INT type_sid = name_arg(mem, args, 2);
    bool all_cols = (col == -1), all_rows = (row == -1);
    bool all_type = (type_sid == S_all);
    if (all_cols && all_rows && all_type) { wr_i(mem, dst, obj_count(S_obj_card_parent)); return; }

    std::vector<VM_INT> ids;
    if (all_cols && all_rows) {                       // 全场按类型数
        collect_objs(S_obj_card_parent, &ids);
        int n = 0;
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (!all_type && prop_str(in, S_plant_id) != type_sid) continue;
            n++;
        }
        wr_i(mem, dst, n);
        return;
    }

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = all_cols ? 0 : col, c2 = all_cols ? cols - 1 : c1;
    int r1 = all_rows ? 0 : row, r2 = all_rows ? rows - 1 : r1;
    if (c1 < 0) c1 = 0; if (c1 > cols - 1) c1 = cols - 1;
    if (c2 < 0) c2 = 0; if (c2 > cols - 1) c2 = cols - 1;
    if (r1 < 0) r1 = 0; if (r1 > rows - 1) r1 = rows - 1;
    if (r2 < 0) r2 = 0; if (r2 > rows - 1) r2 = rows - 1;
    int n = 0;
    for (int c = c1; c <= c2; c++) {
        for (int r = r1; r <= r2; r++) {
            ids.clear();
            cell_plant_ids(c, r, &ids);
            for (size_t i = 0; i < ids.size(); i++) {
                Instance* in = inst_get(ids[i]);
                if (!in) continue;
                if (!all_type && prop_str(in, S_plant_id) != type_sid) continue;
                n++;
            }
        }
    }
    wr_i(mem, dst, n);
}

// VM_GetPlantAt(col,row,layer)：layer == "all" 或 plant_type 命中 → 第一个实例 id；没有 → -1
static void impl_VM_GetPlantAt(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_INT layer = name_arg(mem, args, 2);
    bool all = (layer == S_all);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    if (c1 < 0) c1 = 0; if (c1 > cols - 1) c1 = cols - 1;
    if (c2 < 0) c2 = 0; if (c2 > cols - 1) c2 = cols - 1;
    if (r1 < 0) r1 = 0; if (r1 > rows - 1) r1 = rows - 1;
    if (r2 < 0) r2 = 0; if (r2 > rows - 1) r2 = rows - 1;
    std::vector<VM_INT> ids;
    for (int c = c1; c <= c2; c++) {
        for (int r = r1; r <= r2; r++) {
            ids.clear();
            cell_plant_ids(c, r, &ids);
            for (size_t i = 0; i < ids.size(); i++) {
                Instance* in = inst_get(ids[i]);
                if (!in) continue;
                if (!all && prop_str(in, S_plant_type) != layer) continue;
                wr_i(mem, dst, ids[i]);
                return;
            }
        }
    }
    wr_i(mem, dst, -1);
}

// ============================================================
// 交换 / 压缩（6 条）—— 都是"抄快照 → 清格 → 按新格号摆回去"
// ============================================================
// VM_SwapPlants(col1,row1,col2,row2)：交换两个格子的全部植物
static void impl_VM_SwapPlants(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 4) return;
    int c1 = grid_wrap_col(num_arg(mem, args, 0));
    int r1 = grid_wrap_row(num_arg(mem, args, 1));
    int c2 = grid_wrap_col(num_arg(mem, args, 2));
    int r2 = grid_wrap_row(num_arg(mem, args, 3));
    if (c1 == c2 && r1 == r2) return;

    std::vector<VM_INT> p1, p2;
    cell_plant_ids(c1, r1, &p1);
    cell_plant_ids(c2, r2, &p2);
    cell_clear(c1, r1);
    cell_clear(c2, r2);
    for (size_t i = 0; i < p1.size(); i++) { plant_place(p1[i], c2, r2); cell_add(c2, r2, p1[i]); }
    for (size_t i = 0; i < p2.size(); i++) { plant_place(p2[i], c1, r1); cell_add(c1, r1, p2[i]); }
}

// VM_SwapPlantRects(x1,y1,w,h,x2,y2)：两块等大矩形区域互换（支持重叠）
//   GML 用 "c,r" 字符串当 ds_map 键；这里就是 C++ 的局部表（不落 VM 容器，省一次池分配）
static void impl_VM_SwapPlantRects(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 6) return;
    int x1 = num_arg(mem, args, 0);
    int y1 = num_arg(mem, args, 1);
    int w  = num_arg(mem, args, 2);
    int h  = num_arg(mem, args, 3);
    int x2 = num_arg(mem, args, 4);
    int y2 = num_arg(mem, args, 5);
    if (w <= 0 || h <= 0) return;
    if (x1 == x2 && y1 == y2) return;

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    typedef std::pair<int, int> Cell;
    std::map<Cell, std::vector<VM_INT> > snap;

    // ① 快照（两边都在界内才收）
    for (int dc = 0; dc < w; dc++) {
        for (int dr = 0; dr < h; dr++) {
            int c1 = x1 + dc, r1 = y1 + dr, c2 = x2 + dc, r2 = y2 + dr;
            if (c1 < 0 || c1 >= cols || r1 < 0 || r1 >= rows) continue;
            if (c2 < 0 || c2 >= cols || r2 < 0 || r2 >= rows) continue;
            Cell k1(c1, r1), k2(c2, r2);
            if (snap.find(k1) == snap.end()) cell_plant_ids(c1, r1, &snap[k1]);
            if (snap.find(k2) == snap.end()) cell_plant_ids(c2, r2, &snap[k2]);
        }
    }
    // ② 清空（每格只清一次）
    std::set<Cell> cleared;
    for (int dc = 0; dc < w; dc++) {
        for (int dr = 0; dr < h; dr++) {
            int c1 = x1 + dc, r1 = y1 + dr, c2 = x2 + dc, r2 = y2 + dr;
            if (c1 < 0 || c1 >= cols || r1 < 0 || r1 >= rows) continue;
            if (c2 < 0 || c2 >= cols || r2 < 0 || r2 >= rows) continue;
            Cell k1(c1, r1), k2(c2, r2);
            if (cleared.insert(k1).second) cell_clear(c1, r1);
            if (cleared.insert(k2).second) cell_clear(c2, r2);
        }
    }
    // ③ 放回：A 侧 → B 侧，B 侧 → A 侧（重叠格以 A 侧为准 = 处理过就删掉快照）
    for (int dc = 0; dc < w; dc++) {
        for (int dr = 0; dr < h; dr++) {
            int c1 = x1 + dc, r1 = y1 + dr, c2 = x2 + dc, r2 = y2 + dr;
            if (c1 < 0 || c1 >= cols || r1 < 0 || r1 >= rows) continue;
            if (c2 < 0 || c2 >= cols || r2 < 0 || r2 >= rows) continue;
            Cell k1(c1, r1), k2(c2, r2);
            std::map<Cell, std::vector<VM_INT> >::iterator it = snap.find(k1);
            if (it != snap.end()) {
                for (size_t i = 0; i < it->second.size(); i++) {
                    plant_place(it->second[i], c2, r2);
                    cell_add(c2, r2, it->second[i]);
                }
                snap.erase(it);
            }
            it = snap.find(k2);
            if (it != snap.end()) {
                for (size_t i = 0; i < it->second.size(); i++) {
                    plant_place(it->second[i], c1, r1);
                    cell_add(c1, r1, it->second[i]);
                }
                snap.erase(it);
            }
        }
    }
}

// 压缩的公共骨架：按 line 方向收快照 → 清 → 从 start 端紧密放回
//   axis_col = true：沿列方向（行内压缩），false：沿行方向（列内压缩）
void compact_run(Value* mem, const std::vector<int>& lines, bool along_rows, bool rev) {
    (void)mem;
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int span = along_rows ? cols : rows;             // 一条线有多少格
    int fixed_max = along_rows ? rows : cols;
    std::vector<std::vector<VM_INT> > snap;
    for (size_t li = 0; li < lines.size(); li++) {
        int ln = lines[li];
        if (ln < 0 || ln >= fixed_max) continue;
        snap.clear();
        for (int s = 0; s < span; s++) {
            int c = along_rows ? s : ln;
            int r = along_rows ? ln : s;
            std::vector<VM_INT> ids;
            cell_plant_ids(c, r, &ids);
            if (!ids.empty()) snap.push_back(ids);
        }
        for (int s = 0; s < span; s++) {
            int c = along_rows ? s : ln;
            int r = along_rows ? ln : s;
            cell_clear(c, r);
        }
        int start = rev ? (span - (int)snap.size()) : 0;
        for (size_t i = 0; i < snap.size(); i++) {
            int s = start + (int)i;
            int c = along_rows ? s : ln;
            int r = along_rows ? ln : s;
            for (size_t k = 0; k < snap[i].size(); k++) {
                plant_place(snap[i][k], c, r);
                cell_add(c, r, snap[i][k]);
            }
        }
    }
}
static void impl_VM_CompactColumn(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int col = num_arg(mem, args, 0);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    if (c1 < 0 || c2 >= cols) return;
    std::vector<int> lines;
    for (int c = c1; c <= c2; c++) lines.push_back(c);
    compact_run(mem, lines, false, false);
}
static void impl_VM_CompactRow(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int row = num_arg(mem, args, 0);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    if (r1 < 0 || r2 >= rows) return;
    std::vector<int> lines;
    for (int r = r1; r <= r2; r++) lines.push_back(r);
    compact_run(mem, lines, true, false);
}
static void impl_VM_CompactColumnRev(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int col = num_arg(mem, args, 0);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    if (c1 < 0 || c2 >= cols) return;
    std::vector<int> lines;
    for (int c = c1; c <= c2; c++) lines.push_back(c);
    compact_run(mem, lines, false, true);
}
static void impl_VM_CompactRowRev(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    int row = num_arg(mem, args, 0);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    if (r1 < 0 || r2 >= rows) return;
    std::vector<int> lines;
    for (int r = r1; r <= r2; r++) lines.push_back(r);
    compact_run(mem, lines, true, true);
}

// ============================================================
// 二维扁平表查询（3 条）—— name 是**全局变量名**，表是扁平的：idx = j*(grid_cols+2)+i
//   异常一律 -1（GML 口径；老 C++ 的 CellContains 回 0 / CellItem 回 undefined 是旧账）
// ============================================================
// 取到 (name,i,j) 对应的那一格（必须是数组）；失败返回 NULL 并写 *err
std::vector<Value>* flat_cell(VM_INT name_sid, int i, int j) {
    std::vector<Value>* tbl = flat_table(name_sid);
    if (!tbl) return NULL;
    int cols = gnum_i(S_grid_cols, 0), rows = gnum_i(S_grid_rows, 0);
    if (i < 0 || i >= cols) return NULL;
    if (j < 0 || j >= rows) return NULL;
    int idx = j * (cols + 2) + i;
    if (idx < 0 || idx >= (int)tbl->size()) return NULL;
    const Value& cell = (*tbl)[(size_t)idx];
    if (cell.t != MEM_ARRAY || !container_alive(cell)) return NULL;
    return &g_arr_pool[(size_t)cell.v];
}
static void impl_VM_CellCount(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    std::vector<Value>* cell = flat_cell(name_arg(mem, args, 0), num_arg(mem, args, 1), num_arg(mem, args, 2));
    wr_i(mem, dst, cell ? (VM_INT)cell->size() : -1);
}
static void impl_VM_CellItem(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 4) { wr_i(mem, dst, -1); return; }
    std::vector<Value>* cell = flat_cell(name_arg(mem, args, 0), num_arg(mem, args, 1), num_arg(mem, args, 2));
    if (!cell) { wr_i(mem, dst, -1); return; }
    VM_INT k = num_arg(mem, args, 3);
    if (k < 0 || k >= (VM_INT)cell->size()) { wr_i(mem, dst, -1); return; }
    wr_v(mem, dst, (*cell)[(size_t)k]);
}
static void impl_VM_CellContains(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 4) { wr_i(mem, dst, -1); return; }
    std::vector<Value>* cell = flat_cell(name_arg(mem, args, 0), num_arg(mem, args, 1), num_arg(mem, args, 2));
    if (!cell) { wr_i(mem, dst, -1); return; }
    Value key = arg_v(mem, args, 3);
    for (size_t i = 0; i < cell->size(); i++) {
        const Value& e = (*cell)[i];
        if (e.t == key.t && e.v == key.v) { wr_i(mem, dst, 1); return; }
    }
    wr_i(mem, dst, 0);
}

// ============================================================
// 猫 / 地图物件（2 条）
// ============================================================
// VM_CatInRow(row)：该行第一只 obj_cat 的实例 id（row 必须是数字），没有 → -1
static void impl_VM_CatInRow(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, -1); return; }
    Value rv = arg_v(mem, args, 0);
    if (rv.t != MEM_INT && rv.t != MEM_FLOAT) { wr_i(mem, dst, -1); return; }
    VM_INT want = (rv.t == MEM_FLOAT) ? (VM_INT)std::floor(f_of(rv.v)) : rv.v;
    std::vector<VM_INT> ids;
    collect_objs(S_obj_cat, &ids);
    for (size_t i = 0; i < ids.size(); i++) {
        Instance* in = inst_get(ids[i]);
        if (!in) continue;
        Value* p = inst_prop(*in, S_row);
        if (!p) continue;                              // variable_instance_exists(id,"row")
        VM_FLOAT r = (p->t == MEM_FLOAT) ? f_of(p->v) : (p->t == MEM_INT ? (VM_FLOAT)p->v : 0);
        if ((VM_INT)r == want) { wr_i(mem, dst, ids[i]); return; }
    }
    wr_i(mem, dst, -1);
}

// VM_MapObj(col,row[,name])：读 global.cell_terrain_flag[row*grid_cols+col] 的位
//   bit0 obstacle / bit1 mucus / bit2 lava / bit3 seawater
//   bit4 barrier / bit5 fog / bit6 cloud / bit7 wind_tunnel
//   返回 1 / 0；name == "list" 时返回逗号拼的名字串；名字不认识 → -1
static void impl_VM_MapObj(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT cv = arg_v(mem, args, 0).t, rv = arg_v(mem, args, 1).t;
    if (cv != MEM_INT && cv != MEM_FLOAT) { wr_i(mem, dst, -1); return; }
    if (rv != MEM_INT && rv != MEM_FLOAT) { wr_i(mem, dst, -1); return; }
    int col = num_arg(mem, args, 0);
    int row = num_arg(mem, args, 1);
    VM_INT name_sid = (argc > 2) ? name_arg(mem, args, 2) : S_empty;

    static const char* kBitNames[8] = {
        "obstacle", "mucus", "lava", "seawater", "barrier", "fog", "cloud", "wind_tunnel"
    };
    VM_INT bits[8];
    for (int i = 0; i < 8; i++) bits[i] = intern_once(kBitNames[i]);

    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    if (cols <= 0 || rows <= 0) { wr_i(mem, dst, 0); return; }
    std::vector<Value>* flags = flat_table(intern_once("cell_terrain_flag"));
    if (!flags || (int)flags->size() != cols * rows) { wr_i(mem, dst, 0); return; }

    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;

    // "list"：这一片上有哪些（拼名字串 —— 名字都是自己 intern 的那 8 个，拼出来再 intern 一次）
    if (name_sid == intern_once("list")) {
        std::string out;
        for (int b = 0; b < 8; b++) {
            bool hit = false;
            for (int r = r1; r <= r2 && !hit; r++) {
                if (r < 0 || r >= rows) continue;
                for (int c = c1; c <= c2 && !hit; c++) {
                    if (c < 0 || c >= cols) continue;
                    Value fv = (*flags)[(size_t)(r * cols + c)];
                    VM_INT v = (fv.t == MEM_FLOAT) ? (VM_INT)f_of(fv.v) : fv.v;
                    if ((v & (1 << b)) != 0) hit = true;
                }
            }
            if (hit) {
                if (!out.empty()) out += ",";
                out += str_at(bits[b]);
            }
        }
        wr_v(mem, dst, mk_str_sid(intern_str(out)));
        return;
    }

    int mask = 0;
    if (name_sid == S_empty || name_sid == S_all) {
        mask = 255;
    } else {
        int idx = -1;
        for (int i = 0; i < 8; i++) if (bits[i] == name_sid) { idx = i; break; }
        if (idx < 0) { wr_i(mem, dst, -1); return; }
        mask = 1 << idx;
    }
    for (int r = r1; r <= r2; r++) {
        if (r < 0 || r >= rows) continue;
        for (int c = c1; c <= c2; c++) {
            if (c < 0 || c >= cols) continue;
            Value fv = (*flags)[(size_t)(r * cols + c)];
            VM_INT v = (fv.t == MEM_FLOAT) ? (VM_INT)f_of(fv.v) : fv.v;
            if ((v & mask) != 0) { wr_i(mem, dst, 1); return; }
        }
    }
    wr_i(mem, dst, 0);
}

// ============================================================
// "上一次事件"（5 条）—— 都只是读全局（GML 外面那层 VM_ClientWrapId 是本 VM 不做的联网包装）
// ============================================================
// 读一个"实例 id"型全局：没有 / 不是数字 → -1
void last_id_out(Value* mem, int dst, const char* name) {
    Value* p = global_var_sid(intern_once(name));
    if (!p) { wr_i(mem, dst, -1); return; }
    if (p->t == MEM_INT)   { wr_i(mem, dst, p->v); return; }
    if (p->t == MEM_FLOAT) { wr_num(mem, dst, f_of(p->v)); return; }
    wr_i(mem, dst, -1);
}
static void impl_VM_GetLastBoss(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; last_id_out(mem, dst, "_VM_last_boss");
}
static void impl_VM_GetLastIdlePlatform(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; last_id_out(mem, dst, "_VM_last_idle_platform");
}
static void impl_VM_GetLastBossStateChangeId(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; last_id_out(mem, dst, "_VM_last_boss_state_change_id");
}
static void impl_VM_GetLastBossOldState(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; last_id_out(mem, dst, "_VM_last_boss_old_state");
}
static void impl_VM_GetLastBossNewState(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc; last_id_out(mem, dst, "_VM_last_boss_new_state");
}

// ============================================================
// 索敌 / 范围（3 条）
// ============================================================
// VM_EnemyInRange(r1,r2,c1,c2,type)：查 global.has_enemy_* 的前缀和（场外那两列也算）
static void impl_VM_EnemyInRange(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 5) { wr_i(mem, dst, 0); return; }
    int r1 = num_arg(mem, args, 0), r2 = num_arg(mem, args, 1);
    int c1 = num_arg(mem, args, 2), c2 = num_arg(mem, args, 3);
    VM_INT type_sid = name_arg(mem, args, 4);
    if (!global_var_sid(S_has_enemy_normal)) { wr_i(mem, dst, 0); return; }   // 前缀和还没建 → 没敌人

    VM_INT arrs[6];
    int na = 0;
    if      (type_sid == S_normal)     arrs[na++] = S_has_enemy_normal;
    else if (type_sid == S_obstacle)   arrs[na++] = S_has_enemy_obstacle;
    else if (type_sid == S_diver)      arrs[na++] = S_has_enemy_diver;
    else if (type_sid == S_air)        arrs[na++] = S_has_enemy_air;
    else if (type_sid == S_dance)      arrs[na++] = S_has_enemy_dance;
    else if (type_sid == S_underground) arrs[na++] = S_has_enemy_underground;
    else if (type_sid == S_empty || type_sid == S_all) {
        arrs[na++] = S_has_enemy_normal;     arrs[na++] = S_has_enemy_obstacle;
        arrs[na++] = S_has_enemy_diver;      arrs[na++] = S_has_enemy_air;
        arrs[na++] = S_has_enemy_dance;      arrs[na++] = S_has_enemy_underground;
    } else { wr_i(mem, dst, 0); return; }

    if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
    if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    if (r1 < 0) r1 = 0; if (r1 > rows - 1) r1 = rows - 1;
    if (r2 < 0) r2 = 0; if (r2 > rows - 1) r2 = rows - 1;
    if (c1 < 0) c1 = 0; if (c1 > cols + 1) c1 = cols + 1;
    if (c2 < 0) c2 = 0; if (c2 > cols + 1) c2 = cols + 1;
    int stride = cols + 2;

    for (int r = r1; r <= r2; r++) {
        int base = r * stride;
        for (int k = 0; k < na; k++) {
            std::vector<Value>* a = flat_table(arrs[k]);
            if (!a) continue;
            VM_FLOAT hi = 0, lo = 0;
            if (!arr_num_at(*a, base + c2, &hi)) continue;
            if (c1 > 0 && !arr_num_at(*a, base + c1 - 1, &lo)) continue;
            if (hi - lo > 0) { wr_i(mem, dst, 1); return; }
        }
    }
    wr_i(mem, dst, 0);
}

// VM_GetHomingTarget(type)：最左（同 x 血量最高）的存活敌人 id；没有 → -1
//   GML 每帧缓存一次快照（_VM_homing_cache）；这里每调一次就扫一遍 —— 结果等价，
//   少的只是同一帧里多次调用的重复扫描（缓存是纯性能优化，见文件尾说明）。
static void impl_VM_GetHomingTarget(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT type_sid = (argc > 0) ? name_arg(mem, args, 0) : S_empty;
    if (type_sid == S_all) type_sid = S_empty;
    std::vector<VM_INT> ids;
    collect_objs(S_obj_enemy_parent, &ids);

    VM_INT best = -1;
    VM_FLOAT best_x = 0, best_hp = -1;
    bool found = false;
    for (size_t i = 0; i < ids.size(); i++) {
        Instance* in = inst_get(ids[i]);
        if (!in) continue;
        VM_FLOAT hp = prop_num(in, S_hp, 0);
        VM_FLOAT y  = prop_num(in, S_y, 0);
        if (hp <= 0 || y <= 0) continue;                      // hp > 0 && y > 0
        VM_INT tt = prop_str(in, S_target_type);
        if (type_sid != S_empty && tt != type_sid) continue;
        VM_FLOAT x = prop_num(in, S_x, 0);
        if (!found || x < best_x || (x == best_x && hp > best_hp)) {
            found = true;
            best_x = x;
            best_hp = hp;
            best = ids[i];
        }
    }
    wr_i(mem, dst, best);
}

// VM_GetInstancesInRange(arr,r1,r2,c1,c2,kind,type)：清空命名数组 → 收实例 id → 回数量
//   kind "enemy" 查 global.enemy_array（扁平二维：行距 grid_cols+2）；"card" 查植物格子
//   type == "all" 当"不限"
static void impl_VM_GetInstancesInRange(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 7) { wr_i(mem, dst, 0); return; }
    VM_INT name_sid = name_arg(mem, args, 0);
    int r1 = num_arg(mem, args, 1), r2 = num_arg(mem, args, 2);
    int c1 = num_arg(mem, args, 3), c2 = num_arg(mem, args, 4);
    VM_INT kind_sid = name_arg(mem, args, 5);
    VM_INT type_sid = name_arg(mem, args, 6);
    if (type_sid == S_all) type_sid = S_empty;

    vm_bin* vm = cur_vm();
    if (!vm || name_sid < 0) { wr_i(mem, dst, 0); return; }
    std::vector<Value>* arr = vm_array_make(vm, name_sid);
    for (size_t i = 0; i < arr->size(); i++) release((*arr)[i]);
    arr->clear();

    if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
    if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    if (r1 < 0) r1 = 0; if (r1 > rows - 1) r1 = rows - 1;
    if (r2 < 0) r2 = 0; if (r2 > rows - 1) r2 = rows - 1;

    VM_INT count = 0;
    if (kind_sid == intern_once("enemy")) {
        if (!global_var_sid(S_enemy_array)) { wr_i(mem, dst, 0); return; }
        std::vector<Value>* ea = flat_table(S_enemy_array);
        if (!ea) { wr_i(mem, dst, 0); return; }
        if (c1 < 0) c1 = 0; if (c1 > cols + 1) c1 = cols + 1;
        if (c2 < 0) c2 = 0; if (c2 > cols + 1) c2 = cols + 1;
        int stride = cols + 2;
        for (int r = r1; r <= r2; r++) {
            int base = r * stride;
            for (int c = c1; c <= c2; c++) {
                int idx = base + c;
                if (idx < 0 || idx >= (int)ea->size()) continue;
                const Value& cell = (*ea)[(size_t)idx];
                if (cell.t != MEM_ARRAY || !container_alive(cell)) continue;
                std::vector<Value>& cellv = g_arr_pool[(size_t)cell.v];
                for (size_t i = 0; i < cellv.size(); i++) {
                    if (cellv[i].t != MEM_INT) continue;
                    Instance* in = inst_get(cellv[i].v);
                    if (!in || prop_num(in, S_hp, 0) <= 0) continue;
                    if (type_sid != S_empty && prop_str(in, S_target_type) != type_sid) continue;
                    retain(cellv[i]);
                    arr->push_back(cellv[i]);
                    count++;
                }
            }
        }
    } else if (kind_sid == intern_once("card")) {
        if (c1 < 0) c1 = 0; if (c1 > cols - 1) c1 = cols - 1;
        if (c2 < 0) c2 = 0; if (c2 > cols - 1) c2 = cols - 1;
        std::vector<VM_INT> ids;
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                ids.clear();
                cell_plant_ids(c, r, &ids);
                for (size_t i = 0; i < ids.size(); i++) {
                    Instance* in = inst_get(ids[i]);
                    if (!in) continue;
                    if (prop_str(in, S_plant_id) == S_player) continue;   // 和 VM_SetCardProp 一致：跳过角色
                    if (type_sid != S_empty) {
                        VM_INT pt = prop_str(in, S_plant_type);
                        if (prop_str(in, S_plant_id) != type_sid && pt != type_sid) continue;
                    }
                    Value v = mk_int(ids[i]);
                    retain(v);
                    arr->push_back(v);
                    count++;
                }
            }
        }
    }
    wr_i(mem, dst, count);
}

// ============================================================
// 批量改敌人属性 / 能不能种（2 条）
// ============================================================
// VM_SetEnemyProp(name,prop,value)：name == "all" → 所有非 BOSS 敌人；
//   否则 global.enemy_map[name]._obj 那个对象下的非 BOSS 敌人
static void impl_VM_SetEnemyProp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    VM_INT name_sid = name_arg(mem, args, 0);
    VM_INT prop_sid = name_arg(mem, args, 1);
    Value val = arg_v(mem, args, 2);
    if (name_sid < 0 || prop_sid < 0) return;

    VM_INT obj_sid = -1;
    if (name_sid == S_all) {
        obj_sid = S_obj_enemy_parent;
    } else {
        Value* em = global_var_sid(S_enemy_map);
        Value info, obj;
        if (!em || !map_field(*em, name_sid, &info)) return;
        if (!map_field(info, intern_once("_obj"), &obj)) return;
        if (obj.t != MEM_STRING || !str_ok(obj.v)) return;
        obj_sid = obj.v;
    }
    std::vector<VM_INT> ids;
    collect_objs(obj_sid, &ids);
    for (size_t i = 0; i < ids.size(); i++) {
        Instance* in = inst_get(ids[i]);
        if (!in) continue;
        if (prop_num(in, S_is_boss, 0) != 0) continue;     // BOSS 跳过
        inst_var_set_sid(ids[i], prop_sid, val);
    }
}

// VM_CanPlace(card_id,col,row)：走玩家手牌那套种植规则（can_place_at_position 的 VM 版）
//   plant_type / feature_type / target_card 从 deck_get_card_data 的 struct 里取
static void impl_VM_CanPlace(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, 0); return; }
    Value card_id = arg_v(mem, args, 0);
    if (card_id.t != MEM_STRING || !str_ok(card_id.v) || card_id.v == S_empty) { wr_i(mem, dst, 0); return; }
    int col = num_arg(mem, args, 1);
    int row = num_arg(mem, args, 2);

    // shape 先从存档信息里拿（拿不到就 0），再取卡池数据
    VM_INT shape = 0;
    if (fn_of_sid(S_get_card_info_simple)) {
        Value av[1];
        av[0] = card_id;
        Value si = call_fn(mem, S_get_card_info_simple, av, 1);
        Value sv;
        if (si.t == MEM_MAP && map_field(si, S_shape, &sv)) {
            if (sv.t == MEM_INT)   shape = sv.v;
            else if (sv.t == MEM_FLOAT) shape = (VM_INT)f_of(sv.v);
        }
        release(si);
    }
    Value cd;
    if (!card_data_of(mem, card_id, shape, &cd)) { wr_i(mem, dst, 0); return; }
    Value pt_v, ft_v, tc_v;
    bool has_pt = map_field(cd, S_plant_type,  &pt_v);
    bool has_ft = map_field(cd, S_feature_type, &ft_v);
    bool has_tc = map_field(cd, S_target_card,  &tc_v);
    release(cd);
    VM_INT plant_type  = (has_pt && pt_v.t == MEM_STRING && str_ok(pt_v.v)) ? pt_v.v : -1;
    VM_INT feature_type = (has_ft && ft_v.t == MEM_STRING && str_ok(ft_v.v)) ? ft_v.v : -1;
    VM_INT target_card = (has_tc && tc_v.t == MEM_STRING && str_ok(tc_v.v)) ? tc_v.v : -1;

    // can_place_at_position 拿到的是"世界坐标反算回来的格号" → 负值/超界会回绕
    col = grid_wrap_col(col);
    row = grid_wrap_row(row);
    int cols = 0, rows = 0;
    grid_dims(&cols, &rows);
    if (col < 0 || col >= cols || row < 0 || row >= rows) { wr_i(mem, dst, 0); return; }
    if (gnum_i(S_debug, 0) != 0) { wr_i(mem, dst, 1); return; }   // debug 下直接放行

    VM_INT terr = terrain_type(col, row);
    if (terr == S_obstacle && plant_type != S_coffee) { wr_i(mem, dst, 0); return; }

    bool replace = (gnum_i(S_replace_placement, 0) != 0);
    std::vector<VM_INT> ids;
    cell_plant_ids(col, row, &ids);

    // target_card（底座卡）那条支路：先看有没有同类，再看底座
    if (target_card != S_none) {
        bool same = false;
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (prop_str(in, S_plant_type) == plant_type && !replace && feature_type != S_upgrade) same = true;
        }
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (prop_str(in, S_plant_id) == target_card && !same) { wr_i(mem, dst, 1); return; }
        }
        wr_i(mem, dst, 0);
        return;
    }

    if (plant_type == S_lilypad) {
        if (terr != S_water) { wr_i(mem, dst, 0); return; }
        if (ids.empty()) { wr_i(mem, dst, 1); return; }
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if ((prop_str(in, S_plant_type) == S_lilypad && !replace) ||
                (prop_str(in, S_plant_type) != S_lilypad && prop_str(in, S_feature_type) == S_water)) {
                wr_i(mem, dst, 0); return;
            }
        }
        wr_i(mem, dst, 1);
        return;
    }
    if (plant_type == S_coffee) {
        wr_i(mem, dst, 1);              // 空 / 不空都放行（原版就是一路 return true）
        return;
    }
    if (plant_type == S_shield_outer || plant_type == S_normal) {
        const VM_INT self_type = plant_type;
        // 普通地形（非水）
        if (terr != S_water) {
            if (feature_type == S_water) { wr_i(mem, dst, 0); return; }
            if (ids.empty()) { wr_i(mem, dst, 1); return; }
            if (replace) { wr_i(mem, dst, 1); return; }
            for (size_t i = 0; i < ids.size(); i++) {
                Instance* in = inst_get(ids[i]);
                if (!in) continue;
                if (prop_str(in, S_plant_type) != self_type) continue;
                if (self_type == S_normal) {
                    // 同类相斥，但 king_bun / king_tbun 的吸收组合除外
                    VM_INT oft = prop_str(in, S_feature_type);
                    bool skip = ((feature_type == S_bun || feature_type == S_king_bun) &&
                                 (oft == S_bun || oft == S_king_bun)) ||
                                ((feature_type == S_tbun || feature_type == S_king_tbun) &&
                                 (oft == S_tbun || oft == S_king_tbun));
                    if (skip) continue;
                }
                wr_i(mem, dst, 0);
                return;
            }
            wr_i(mem, dst, 1);
            return;
        }
        // 水上
        if (feature_type == S_dwarf) { wr_i(mem, dst, 0); return; }
        bool has_same = false;
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (prop_str(in, S_plant_type) == self_type && !replace) has_same = true;
        }
        if (feature_type == S_water || feature_type == S_amphi) {
            for (size_t i = 0; i < ids.size(); i++) {
                Instance* in = inst_get(ids[i]);
                if (!in) continue;
                if (prop_str(in, S_plant_type) == S_lilypad || has_same) { wr_i(mem, dst, 0); return; }
            }
            wr_i(mem, dst, 1);
            return;
        }
        for (size_t i = 0; i < ids.size(); i++) {
            Instance* in = inst_get(ids[i]);
            if (!in) continue;
            if (prop_str(in, S_plant_type) == S_lilypad && !has_same) { wr_i(mem, dst, 1); return; }
        }
        wr_i(mem, dst, 0);
        return;
    }
    // 其它类型：空地才让种
    std::vector<Value>* lst = plant_cell(col, row);
    wr_i(mem, dst, (!lst || lst->empty()) ? 1 : 0);
}

// ============================================================
// 挂表：42 条一条不落
// ============================================================
void patch_game_fns_a() {
    kGameFns[S_VM_CreatePlatform        - SID_GAME_BASE] = impl_VM_CreatePlatform;
    kGameFns[S_VM_SpawnPlant            - SID_GAME_BASE] = impl_VM_SpawnPlant;
    kGameFns[S_VM_SpawnEnemy            - SID_GAME_BASE] = impl_VM_SpawnEnemy;
    kGameFns[S_VM_SpawnBoss             - SID_GAME_BASE] = impl_VM_SpawnBoss;
    kGameFns[S_VM_SpawnObject           - SID_GAME_BASE] = impl_VM_SpawnObject;
    kGameFns[S_VM_SpawnCats             - SID_GAME_BASE] = impl_VM_SpawnCats;
    kGameFns[S_VM_SpawnPlantsRandom     - SID_GAME_BASE] = impl_VM_SpawnPlantsRandom;
    kGameFns[S_VM_CreateButton          - SID_GAME_BASE] = impl_VM_CreateButton;
    kGameFns[S_VM_SpawnBatMouse         - SID_GAME_BASE] = impl_VM_SpawnBatMouse;
    kGameFns[S_VM_HomingBulletAdd       - SID_GAME_BASE] = impl_VM_HomingBulletAdd;
    kGameFns[S_VM_SetTerrain            - SID_GAME_BASE] = impl_VM_SetTerrain;
    kGameFns[S_VM_ClearPlants           - SID_GAME_BASE] = impl_VM_ClearPlants;
    kGameFns[S_VM_ClearMapObjects       - SID_GAME_BASE] = impl_VM_ClearMapObjects;
    kGameFns[S_VM_SetRowFeature         - SID_GAME_BASE] = impl_VM_SetRowFeature;
    kGameFns[S_VM_ClearPlantsByType     - SID_GAME_BASE] = impl_VM_ClearPlantsByType;
    kGameFns[S_VM_WakePlants            - SID_GAME_BASE] = impl_VM_WakePlants;
    kGameFns[S_VM_GetTerrain            - SID_GAME_BASE] = impl_VM_GetTerrain;
    kGameFns[S_VM_GetEnemyCount         - SID_GAME_BASE] = impl_VM_GetEnemyCount;
    kGameFns[S_VM_GetPlantCount         - SID_GAME_BASE] = impl_VM_GetPlantCount;
    kGameFns[S_VM_GetPlantCountAt       - SID_GAME_BASE] = impl_VM_GetPlantCountAt;
    kGameFns[S_VM_GetPlantAt            - SID_GAME_BASE] = impl_VM_GetPlantAt;
    kGameFns[S_VM_SwapPlants            - SID_GAME_BASE] = impl_VM_SwapPlants;
    kGameFns[S_VM_SwapPlantRects        - SID_GAME_BASE] = impl_VM_SwapPlantRects;
    kGameFns[S_VM_CompactColumn         - SID_GAME_BASE] = impl_VM_CompactColumn;
    kGameFns[S_VM_CompactRow            - SID_GAME_BASE] = impl_VM_CompactRow;
    kGameFns[S_VM_CompactColumnRev      - SID_GAME_BASE] = impl_VM_CompactColumnRev;
    kGameFns[S_VM_CompactRowRev         - SID_GAME_BASE] = impl_VM_CompactRowRev;
    kGameFns[S_VM_CellCount             - SID_GAME_BASE] = impl_VM_CellCount;
    kGameFns[S_VM_CellItem              - SID_GAME_BASE] = impl_VM_CellItem;
    kGameFns[S_VM_CellContains          - SID_GAME_BASE] = impl_VM_CellContains;
    kGameFns[S_VM_CatInRow              - SID_GAME_BASE] = impl_VM_CatInRow;
    kGameFns[S_VM_MapObj                - SID_GAME_BASE] = impl_VM_MapObj;
    kGameFns[S_VM_GetLastBoss           - SID_GAME_BASE] = impl_VM_GetLastBoss;
    kGameFns[S_VM_GetLastIdlePlatform   - SID_GAME_BASE] = impl_VM_GetLastIdlePlatform;
    kGameFns[S_VM_GetLastBossStateChangeId - SID_GAME_BASE] = impl_VM_GetLastBossStateChangeId;
    kGameFns[S_VM_GetLastBossOldState   - SID_GAME_BASE] = impl_VM_GetLastBossOldState;
    kGameFns[S_VM_GetLastBossNewState   - SID_GAME_BASE] = impl_VM_GetLastBossNewState;
    kGameFns[S_VM_EnemyInRange          - SID_GAME_BASE] = impl_VM_EnemyInRange;
    kGameFns[S_VM_GetHomingTarget       - SID_GAME_BASE] = impl_VM_GetHomingTarget;
    kGameFns[S_VM_GetInstancesInRange   - SID_GAME_BASE] = impl_VM_GetInstancesInRange;
    kGameFns[S_VM_SetEnemyProp          - SID_GAME_BASE] = impl_VM_SetEnemyProp;
    kGameFns[S_VM_CanPlace              - SID_GAME_BASE] = impl_VM_CanPlace;
}

// ============================================================
// 和 GML 参考（scripts/scr_command_VM/scr_command_VM.gml）的差异账
//   全部都是"这份 VM 里没有那件东西"造成的，语义骨架照 GML 抄：
//
// 1. 联机那一整套不做：network.mode / _VM_id_to_real / add_net_id / MSG_* /
//    VM_ClientWrapId / _VM_send 都不存在。所以 CreatePlatform / SpawnEnemy / SpawnBoss /
//    SpawnObject / SpawnPlant / SpawnPlantsRandom / SpawnBatMouse / SwapPlants /
//    SwapPlantRects / CompactColumn/Row(Rev) 里那些"服务端广播 / 客户端负号 id"全跳过；
//    _VM_create_counter 照常自增（本地可见的 _VM_id 用它的号）。
// 2. card_created / card_destroyed 是**转发类 native**（本 VM 里还没有宿主的 callbin）。
//    SpawnPlant / SpawnPlantsRandom / ClearPlants / ClearPlantsByType 里对应的 VM 侧效果
//    本文件自己做：进出 grid_plants 列表 + 写 _VM_last_created_card / _VM_last_destroyed_card。
//    因此 **_VM_CARD_CREATED / _VM_CARD_DESTROYED 挂载点不会从这里发**（要 VM_QueueHook，
//    那是宿主/热块的事）。
// 3. spawn_plant（GML 脚本，不是 native）被内联成简化版 spawn_one_plant：
//    做「建实例 → 格心位置 → plant_depth → 写 props → 进格子列表 → 取 _VM_id」；
//    **不做**平台偏移（platform_id/platform_offset）、同类替换、放置特效 / 音效、
//    player 角色那条武器链。这几样都要引擎侧数据（global.network.map_net_id_instance_id、
//    obj_place_effect、get_weapon_info …）。
// 4. update_plant_bindings 只做了三支绑定小物件（banding_star_obj / banding_sleep_obj /
//    banding_water_obj 的 x/y/depth）；_move_instance_map 里"玩家武器/护盾跟着走"那支没做
//    （要读实例的 object_index，这份 VM 没暴露）。
// 5. VM_GetHomingTarget 少了 GML 的**每帧快照缓存**（global._VM_homing_cache）：
//    这里每调一次重扫一遍。结果和"每帧一次快照"等价，少的是同一帧内多次调用时的重复扫描。
// 6. VM_CanPlace 把 can_place_at_position（GML 脚本）内联成本地规则；它依赖
//    deck_get_card_data / get_card_info_simple 两个转发类 native 给出
//    plant_type / feature_type / target_card —— 宿主没接时一律回 0（不能种）。
// 7. VM_CreateButton 用 Button 对象的 state 结构体字段代替 set_sprite/set_scale/
//    set_position/set_frames 那条方法链（字段名照 objects/Button/Create_0.gml：
//    sprite / scale / offset_x / offset_y / frame_idle / frame_hover / frame_press）；
//    refresh_hitbox / sprite_get_width 那几步要宿主，没做。
// 8. VM_GetLastBoss / GetLastIdlePlatform / GetLastBossStateChange* / Old* / New*：
//    只读全局；全局没有值时回 -1（GML 是 real(undefined)）。
// 9. VM_MapObj 的 "list" 模式要在运行时拼一次逗号串（本文件唯一拼字符串的地方），
//    拼完 intern 一次；其余名字比较一律走池下标。
// 10. VM_CellItem / VM_CellContains 的异常返回照 **GML**（都是 -1）；
//     老 C++（transformGML2txt/vm_container.cpp）那边是 undefined / 0，属于旧账。
// 11. get_world_position_from_grid / calculate_plant_depth / get_grid_position_from_world
//     没走 host 表（那三条现在是 VM_E_NOT_IMPL 空壳），按 GML 的算式本地算
//     （grid_world / plant_depth / grid_wrap_*）。宿主以后若把这三条实现成同样的算式，
//     两边结果一致，不冲突。
// ============================================================
