// ============================================================
// vm_api.cpp — 游戏 API（OP_CALL 调用）
//   顺序 = 原 VM 的 FUNC_DEFS，不能改（gm2bin.py 按这个顺序算 id）
//   现在全是空壳，一个函数一个空体，直接在里面写就行
// ============================================================

#include "vm.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <unordered_map>

// ============================================================
// 实例数组那一批（VM_InstArray*）要实现成什么样 —— 对着原版抄
//   原版在 scripts/scr_command_VM/scr_command_VM.gml（2370 行起），
//   操作的是**实例变量里真正的 GML 数组**：variable_instance_get/set。
//   inst = 0 表示全局：名字当 global 变量名，走 variable_global_get/set。
//
//   参数一律是「槽号」，从槽里取值（vm_arg 那套）；返回 1 / 0 / -1。
//
//   VM_InstArrayExists(inst, name)        有且是数组 → 1，否则 0
//   VM_InstArraySize  (inst, name)        长度；实例/变量不存在或不是数组 → -1
//   VM_InstArrayItem  (inst, name, k)     第 k 个；越界 / 不存在 / 不是数组 → -1
//   VM_InstArraySet   (inst, name, k, v)  k 必须已在范围内（**不自动扩容**），
//                                         变量必须已存在且是数组（**不自动建**）→ 越界 -1
//   VM_InstArrayAdd   (inst, name, v)     尾部追加；实例那支不自动建数组（-1），
//                                         全局那支（inst=0）不存在会自动建
//   VM_InstArrayDel   (inst, name, k)     删第 k 个；越界 → -1
//   VM_InstArrayClear (inst, name)        清成 []。
//                                         ⚠ 我们是拿它当「建空数组」用的（`x = []` 编成它），
//                                           所以实现要按「变量不存在就建一个空数组」来写。
//                                           原版（scr_command_VM.gml:2564）是「不存在 / 不是数组
//                                           → 直接 -1」，这一点我们对不齐，实现时别照抄原版；
//                                           要跟平台那边一起改的话，也改 scr_command_VM 的同名函数。
//   VM_InstArrayContains(inst, name, v)   有 → 1，没有 → 0，异常 → -1
//
//   还有几件原版做了、这边也要跟上的事：
//     · 实例 id：负数是在用的包装 id（global._VM_id_to_real 那张表），先解回真实 id；
//       往数组里写的值如果是包装 id，也要先解包，别把负数写进引擎侧的数据
//     · 读出来的值如果是个实例 id，要包成包装 id 再返回（原版：VM_ClientWrapId）
//     · 联网：写之前 vm_inst_write_ok（客户端不写，等服务器广播），
//            写完 vm_inst_broadcast_prop(inst, name, 整个数组) 广播
//     · 数组是值语义：改完必须整个写回去（GML 侧 variable_instance_set）
// ============================================================

#define GAME_STUB(NAME) \
    static void NAME(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
GAME_FN_LIST(GAME_STUB)
#undef GAME_STUB

// 表本身不是 const：下面那张"已实现"表会在启动时按**名字**把对应槽换成真函数
//   （id 顺序 = FUNC_DEFS，一个都不能动，所以不能在 GAME_FN_LIST 里增删）
Fn kGameFns[] = {
#define GAME_ENTRY(NAME) NAME,
    GAME_FN_LIST(GAME_ENTRY)
#undef GAME_ENTRY
};
// 条数定义在 vm.h（预置段要用它算下标基址）；这里卡住"表长度没变"
static_assert(sizeof(kGameFns) / sizeof(kGameFns[0]) == GAME_FN_COUNT,
              "kGameFns 的长度和 vm.h 的 GAME_FN_COUNT 对不上");

// 有真实现的游戏 API：**池下标** → 函数（下标就是 names.h 里 S_VM_xxx 的值）
//   实体在 vm_callbin.cpp / vm_instance.cpp 等处
struct GameImpl { int sid; Fn fn; };

// ============================================================
// game API 的真实现（一批一批加；只加实现，**不动 GAME_FN_LIST 的顺序**）
//   表顺序是平台契约（LabMapCompiler/compiler_defs.h 的 150 条 + 已编译 mod 的 id），
//   所以这里的做法永远是：kGameImpls 里把某个 sid 的空壳换成真实现。
// ============================================================
static double gnum_at(Vm& vm, const uint8_t* args, int k) {   // 取第 k 个实参当数字
    int slot = argslot(args, k);
    int t = rd_t(vm, slot), iv = rd_i(vm, slot);
    if (t == MEM_FLOAT) { float f2; memcpy(&f2, &iv, 4); return (double)f2; }
    return (double)iv;
}
static Value gundef() { Value u; u.t = MEM_UNDEFINED; u.v = 0; return u; }
static void gwr_num(Vm& vm, int dst, double x) {      // wr_num 在 vm_builtin.cpp 里是 static，这里自己来
    Value v;
    if (x == (double)(int)x) { v.t = MEM_INT; v.v = (int)x; }
    else { v.t = MEM_FLOAT; float f = (float)x; memcpy(&v.v, &f, 4); }
    wr_v(vm, dst, v);
}

void impl_VM_Floor(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; gwr_num(vm, dst, floor(gnum_at(vm, args, 0)));
}
void impl_VM_Ceil(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; gwr_num(vm, dst, ceil(gnum_at(vm, args, 0)));
}
void impl_VM_Random(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; gwr_num(vm, dst, ((double)rand() / (double)RAND_MAX) * gnum_at(vm, args, 0));
}
void impl_VM_IsUndefined(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value v = (argc > 0) ? rd_v(vm, argslot(args, 0)) : gundef();
    gwr_num(vm, dst, (v.t == MEM_UNDEFINED) ? 1.0 : 0.0);
}
void impl_VM_IsDestroyed(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int id = (argc > 0) ? (int)gnum_at(vm, args, 0) : 0;
    gwr_num(vm, dst, (inst_get(id) == NULL) ? 1.0 : 0.0);
}
// VM_GetInstanceCount("obj_x")：某对象还活着几个
void impl_VM_GetInstanceCount(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    if (argc < 1) { gwr_num(vm, dst, 0); return; }
    Value v = rd_v(vm, argslot(args, 0));
    if (v.t != MEM_STRING || !str_ok(v.v)) { gwr_num(vm, dst, 0); return; }
    gwr_num(vm, dst, (double)inst_count(str_at(v.v)));
}
// VM_GetInstanceAt("obj_x", k)：第 k 个活实例 id（没有 → -1）
void impl_VM_GetInstanceAt(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    if (argc < 2) { gwr_num(vm, dst, -1); return; }
    Value v = rd_v(vm, argslot(args, 0));
    if (v.t != MEM_STRING || !str_ok(v.v)) { gwr_num(vm, dst, -1); return; }
    gwr_num(vm, dst, (double)inst_at(str_at(v.v), (int)gnum_at(vm, args, 1)));
}
// VM_GetCurCard()：当前卡（hot/ 维护的全局 _VM_cur_card）
void impl_VM_GetCurCard(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    Value* p = global_var_sid(S__VM_cur_card);
    wr_v(vm, dst, p ? *p : gundef());
}


// ── 批次 2：状态/计数类（全部对接 VM 里已有的全局；hot/ 也在用同一批名字）──
static void g_get_g(Vm& vm, int sid, int dst) {          // 读全局：没有 → undefined
    Value* p = global_var_sid(sid);
    wr_v(vm, dst, p ? *p : gundef());
}
static void g_set_g(Vm& vm, const uint8_t* args, int argc, int sid) {   // 写全局：没给值就写 1
    if (argc < 1) { Value one; one.t = MEM_INT; one.v = 1; global_var_set_sid(sid, one); return; }
    global_var_set_sid(sid, rd_v(vm, argslot(args, 0)));
}

void impl_VM_GetLastCreatedCard(Vm& vm, const uint8_t* a, int c, int d)   { (void)a; (void)c; g_get_g(vm, S__VM_last_created_card, d); }
void impl_VM_GetLastDestroyedCard(Vm& vm, const uint8_t* a, int c, int d) { (void)a; (void)c; g_get_g(vm, S__VM_last_destroyed_card, d); }
void impl_VM_GetLastCreatedEnemy(Vm& vm, const uint8_t* a, int c, int d)  { (void)a; (void)c; g_get_g(vm, S__VM_last_created_enemy, d); }
void impl_VM_GetLastKilledEnemy(Vm& vm, const uint8_t* a, int c, int d)   { (void)a; (void)c; g_get_g(vm, S__VM_last_killed_enemy, d); }
void impl_VM_GetLastClickedButton(Vm& vm, const uint8_t* a, int c, int d) { (void)a; (void)c; g_get_g(vm, S__VM_last_clicked_button, d); }
void impl_VM_GetFlame(Vm& vm, const uint8_t* a, int c, int d)             { (void)a; (void)c; g_get_g(vm, S_flame, d); }
void impl_VM_GetWave(Vm& vm, const uint8_t* a, int c, int d)              { (void)a; (void)c; g_get_g(vm, S__VM_wave, d); }

void impl_VM_SetFlame(Vm& vm, const uint8_t* a, int c, int d)             { (void)d; g_set_g(vm, a, c, S_flame); }
void impl_VM_SetWave(Vm& vm, const uint8_t* a, int c, int d)              { (void)d; g_set_g(vm, a, c, S__VM_wave); }
void impl_VM_SetShovelFlameRate(Vm& vm, const uint8_t* a, int c, int d)   { (void)d; g_set_g(vm, a, c, S__VM_shovel_flame_rate); }
void impl_VM_SetCardLevelCap(Vm& vm, const uint8_t* a, int c, int d)      { (void)d; g_set_g(vm, a, c, S__VM_card_level_cap); }
void impl_VM_SetCardShapeCap(Vm& vm, const uint8_t* a, int c, int d)      { (void)d; g_set_g(vm, a, c, S__VM_card_shape_cap); }
void impl_VM_SetCardSkillCap(Vm& vm, const uint8_t* a, int c, int d)      { (void)d; g_set_g(vm, a, c, S__VM_card_skill_cap); }
void impl_VM_BanWeapon(Vm& vm, const uint8_t* a, int c, int d)            { (void)d; g_set_g(vm, a, c, S__VM_ban_weapon); }
void impl_VM_BanSuperWeapon(Vm& vm, const uint8_t* a, int c, int d)       { (void)d; g_set_g(vm, a, c, S__VM_ban_super_weapon); }
void impl_VM_BanShield(Vm& vm, const uint8_t* a, int c, int d)            { (void)d; g_set_g(vm, a, c, S__VM_ban_shield); }


// ── 批次 3a：VM_Array*（命名数组，建在 Vm::named_arrays 上；key = 名字的池下标）──
static int g_name_sid(Vm& vm, const uint8_t* args, int k) {   // 取第 k 个实参当"名字"→ 池下标
    Value v = rd_v(vm, argslot(args, k));
    if (v.t != MEM_STRING || !str_ok(v.v)) return -1;
    return v.v;
}
static std::vector<Value>* g_arr(Vm& vm, const uint8_t* args, int k, bool create) {
    int sid = g_name_sid(vm, args, k);
    if (sid < 0) return NULL;
    std::vector<Value>* p = NULL;
    std::unordered_map<int, std::vector<Value>>::iterator it = vm.named_arrays.find(sid);
    if (it != vm.named_arrays.end()) p = &it->second;
    else if (create) p = &vm.named_arrays[sid];
    return p;
}
static bool g_val_eq(Vm& vm, const Value& a, const Value& b) {
    (void)vm;
    if (a.t != b.t) {
        // int / float 混用也算相等
        bool an = (a.t == MEM_INT || a.t == MEM_FLOAT), bn = (b.t == MEM_INT || b.t == MEM_FLOAT);
        if (!(an && bn)) return false;
        if (a.t == MEM_FLOAT) { float f; memcpy(&f, &a.v, 4); return (double)f == (double)b.v; }
        float f; memcpy(&f, &b.v, 4); return (double)a.v == (double)f;
    }
    return a.v == b.v;
}

void impl_VM_ArrayGet(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c;
    std::vector<Value>* p = g_arr(vm, a, 0, false);
    int i = (int)gnum_at(vm, a, 1);
    if (!p || i < 0 || i >= (int)p->size()) { wr_v(vm, d, gundef()); return; }
    wr_v(vm, d, (*p)[i]);
}
void impl_VM_ArraySet(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c; (void)d;
    std::vector<Value>* p = g_arr(vm, a, 0, true);
    if (!p) return;
    int i = (int)gnum_at(vm, a, 1);
    if (i < 0) return;
    if (i >= (int)p->size()) p->resize((size_t)i + 1);
    (*p)[i] = rd_v(vm, argslot(a, 2));
}
void impl_VM_ArrayADD(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c; (void)d;
    std::vector<Value>* p = g_arr(vm, a, 0, true);
    if (p) p->push_back(rd_v(vm, argslot(a, 1)));
}
void impl_VM_ArrayDel(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c; (void)d;
    std::vector<Value>* p = g_arr(vm, a, 0, false);
    int i = (int)gnum_at(vm, a, 1);
    if (p && i >= 0 && i < (int)p->size()) p->erase(p->begin() + i);
}
void impl_VM_ArraySize(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c;
    std::vector<Value>* p = g_arr(vm, a, 0, false);
    gwr_num(vm, d, p ? (double)p->size() : 0.0);
}
void impl_VM_ArrayClear(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c; (void)d;
    std::vector<Value>* p = g_arr(vm, a, 0, false);
    if (p) p->clear();
}
void impl_VM_ArrayClearAll(Vm& vm, const uint8_t* a, int c, int d) {
    (void)a; (void)c; (void)d;
    vm.named_arrays.clear();
}
void impl_VM_ArrayExists(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c;
    gwr_num(vm, d, g_arr(vm, a, 0, false) ? 1.0 : 0.0);
}
void impl_VM_ArrayContains(Vm& vm, const uint8_t* a, int c, int d) {
    (void)c;
    std::vector<Value>* p = g_arr(vm, a, 0, false);
    if (!p) { gwr_num(vm, d, 0); return; }
    Value key = rd_v(vm, argslot(a, 1));
    for (size_t i = 0; i < p->size(); i++) if (g_val_eq(vm, (*p)[i], key)) { gwr_num(vm, d, 1); return; }
    gwr_num(vm, d, 0);
}


// ── 批次 3b：实例创建/销毁（用 VM 自己的实例表）──
// VM_CreateInstance("obj_x" [, x, y, depth])：建实例并返回 id；对象名错 → noone(-4)
void impl_VM_CreateInstance(Vm& vm, const uint8_t* a, int c, int d) {
    if (c < 1) { gwr_num(vm, d, -4); return; }
    Value v = rd_v(vm, argslot(a, 0));
    if (v.t != MEM_STRING || !str_ok(v.v)) { gwr_num(vm, d, -4); return; }
    int id = inst_new(&vm, str_at(v.v));
    if (id < 0) { gwr_num(vm, d, -4); return; }
    if (c > 1) inst_var_set_sid(id, S_x,     rd_v(vm, argslot(a, 1)));
    if (c > 2) inst_var_set_sid(id, S_y,     rd_v(vm, argslot(a, 2)));
    if (c > 3) inst_var_set_sid(id, S_depth, rd_v(vm, argslot(a, 3)));
    gwr_num(vm, d, (double)id);
}
// VM_DestroyInstance(inst)：销毁一个实例
void impl_VM_DestroyInstance(Vm& vm, const uint8_t* a, int c, int d) {
    (void)d;
    int id = (c > 0) ? (int)gnum_at(vm, a, 0) : -1;
    if (id >= 0) inst_destroy(id);          // 走唯一入口（含 Destroy 事件）
}

static const GameImpl kGameImpls[] = {
    { S_VM_CallFunc,   impl_VM_CallFunc   },
    { S_VM_FuncExists, impl_VM_FuncExists },
    { S_VM_FuncDesc,   impl_VM_FuncDesc   },
    { S_VM_GetProp,    impl_VM_GetProp    },
    { S_VM_SetProp,    impl_VM_SetProp    },
    // ── 新加的真实现（顺序不变，只是换掉空壳）──
    { S_VM_Floor,              impl_VM_Floor              },
    { S_VM_Ceil,               impl_VM_Ceil               },
    { S_VM_Random,             impl_VM_Random             },
    { S_VM_IsUndefined,        impl_VM_IsUndefined        },
    { S_VM_IsDestroyed,        impl_VM_IsDestroyed        },
    { S_VM_GetInstanceCount,   impl_VM_GetInstanceCount   },
    { S_VM_GetInstanceAt,      impl_VM_GetInstanceAt      },
    { S_VM_GetCurCard,         impl_VM_GetCurCard         },
    // ── 批次 2：状态/计数 ──
    { S_VM_GetLastCreatedCard,   impl_VM_GetLastCreatedCard   },
    { S_VM_GetLastDestroyedCard, impl_VM_GetLastDestroyedCard },
    { S_VM_GetLastCreatedEnemy,  impl_VM_GetLastCreatedEnemy  },
    { S_VM_GetLastKilledEnemy,   impl_VM_GetLastKilledEnemy   },
    { S_VM_GetLastClickedButton, impl_VM_GetLastClickedButton },
    { S_VM_GetFlame,             impl_VM_GetFlame             },
    { S_VM_GetWave,              impl_VM_GetWave              },
    { S_VM_SetFlame,             impl_VM_SetFlame             },
    { S_VM_SetWave,              impl_VM_SetWave              },
    { S_VM_SetShovelFlameRate,   impl_VM_SetShovelFlameRate   },
    { S_VM_SetCardLevelCap,      impl_VM_SetCardLevelCap      },
    { S_VM_SetCardShapeCap,      impl_VM_SetCardShapeCap      },
    { S_VM_SetCardSkillCap,      impl_VM_SetCardSkillCap      },
    { S_VM_BanWeapon,            impl_VM_BanWeapon            },
    { S_VM_BanSuperWeapon,       impl_VM_BanSuperWeapon       },
    { S_VM_BanShield,            impl_VM_BanShield            },
    // ── 批次 3a：数组 ──
    { S_VM_ArrayGet,       impl_VM_ArrayGet       },
    { S_VM_ArraySet,       impl_VM_ArraySet       },
    { S_VM_ArrayADD,       impl_VM_ArrayADD       },
    { S_VM_ArrayDel,       impl_VM_ArrayDel       },
    { S_VM_ArraySize,      impl_VM_ArraySize      },
    { S_VM_ArrayClear,     impl_VM_ArrayClear     },
    { S_VM_ArrayClearAll,  impl_VM_ArrayClearAll  },
    { S_VM_ArrayExists,    impl_VM_ArrayExists    },
    { S_VM_ArrayContains,  impl_VM_ArrayContains  },
    // ── 批次 3b：实例 ──
    { S_VM_CreateInstance,   impl_VM_CreateInstance   },
    { S_VM_DestroyInstance,  impl_VM_DestroyInstance  },
    // ── 批次 4：格子/地形（只读）──
    { S_VM_SetTerrain,     impl_VM_SetTerrain     },
    { S_VM_GetTerrain,     impl_VM_GetTerrain     },
    { S_VM_CellCount,      impl_VM_CellCount      },
    { S_VM_CellItem,       impl_VM_CellItem       },
    { S_VM_CellContains,   impl_VM_CellContains   },
    { S_VM_GetPlantCount,  impl_VM_GetPlantCount  },
    { S_VM_ClearPlants,         impl_VM_ClearPlants         },
    { S_VM_ClearPlantsByType,   impl_VM_ClearPlantsByType   },
    { S_VM_WakePlants,          impl_VM_WakePlants          },
    { S_VM_GetPlantAt,          impl_VM_GetPlantAt          },
    { S_VM_GetPlantCountAt,     impl_VM_GetPlantCountAt     },
};

bool g_game_fns_patched = false;

void patch_game_fns() {
    for (size_t g = 0; g < sizeof(kGameImpls) / sizeof(kGameImpls[0]); g++) {
        int i = kGameImpls[g].sid - SID_GAME_BASE;      // 池下标 → game id
        if (i < 0 || i >= GAME_FN_COUNT) {
            printf("[vm] patch_game_fns: 下标 %d 不在 game 段\n", kGameImpls[g].sid);
            continue;
        }
        kGameFns[i] = kGameImpls[g].fn;
    }
}

// 名字 → game id：走全局字符串池（预置段里函数名占 1..N），一次哈希就够
//   没预置的（脚本里现造的字符串）自然落在别的区间 → -1
int game_id(const char* name) {
    int sid = str_find(name);
    if (sid < 0) return -1;
    if (sid < SID_GAME_BASE || sid >= SID_GAME_BASE + GAME_FN_COUNT) return -1;
    return sid - SID_GAME_BASE;
}
