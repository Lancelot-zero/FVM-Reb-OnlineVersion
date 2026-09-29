// ============================================================
// vm.h — FVM-Reborn 字节码解释器（C++ 版）公共声明
//
// 覆盖原 GML 版 VM 的 17 个操作码，并新增：
//     OP_NATIVE = 18   调用内部函数
//     OP_LOAD_ELEM = 19  取地址处的值
//
// 内存：一块 char 内存，每槽 8 字节 [type:int32][value:int32]
//       取槽用 int* 索引：m[槽*2] = 类型，m[槽*2+1] = 值
//
// ⚠ 浮点是 32 位（float），GML 的 real 是 64 位 double —— 这是**已知取舍**，就这么定了：
//     · 数值都不大，相对精度 ~7 位有效数字，绝对误差 1e-5 级，够用
//     · bin 里的浮点字面量也是 f32（[type:u8][value: f32]），编译那一刻就定了精度
//     · 会露头的两种形式，看到别懵：① 浮点精确比较（x == 100.5）② floor/ceil 边界
//       掉一格 → 表现为"差 1 像素 / 差 1 帧"
//     · 真要升 double：运行期槽 8 → 16 字节（vm.cpp 里 68 处 *2 索引 + 25 处 float 强转）
//       + bin 里字面量 4 → 8 字节（翻译器/dump_bin/平台侧 VM_Decode 三处都要改，
//       老 bin 会错位 → 加版本号或全量重转）
// 内建：两个函数指针表派发，参数直接指字节码里的参数表（不复制）
//
// 实现分在几个 cpp：
//     vm.cpp           执行循环 + 装载 + 入口
//     vm_api.cpp       150 个游戏 API 空壳 + kGameFns 表
//     vm_native.cpp    内部函数空壳 + kNativeFns 表
//     vm_container.cpp 数组 / 字典 / 结构体 / 二维表
//     vm_instance.cpp  实例表 + 对象名→VM 表 + 全局变量表
//     vm_builtin.cpp   常规内置函数（数学 / 类型判断 / 字符串 / 坐标换算）
//     kernel.cpp       和"块"打交道的（run_block / run_event_user）
//     hot/*.cpp        硬编码的热块（一个对象一个文件；见 hot/hot_common.h 的说明）
//
// ------------------------------------------------------------
// .bin 文件布局：
//     [i32 字符串个数]
//         重复: [i32 字节长度][UTF-8 字节]
//     [i32 字节码长度][i32 块名在字符串池里的下标][字节码]     ← 重复到文件尾
//
// 指令编码：
//     OP_ASSIGN    dst(s32) type(u8) value(MEM_STRING: u16 | MEM_FLOAT: f32 | 其它: s32)
//     OP_COPY      dst(s32) src(s32)
//     OP_ADD~MOD   dst(s32) a(s32) b(s32)
//     OP_EQ~LTE    dst(s32) a(s32) b(s32)
//     OP_CALL      fid(u16) argc(u8) dst(s32) arg(s32)*argc     ← dst = -1 表示无返回值
//     OP_IF        cond(s32) true_ip(s32) false_ip(s32)         ← ip = 块内绝对字节偏移
//     OP_JMP       ip(s32)
//     OP_HALT
//     OP_NATIVE    fid(u16) argc(u8) dst(s32) arg(s32)*argc     ← 新增，格式同 OP_CALL
//     OP_LOAD_ELEM dst(s32) src(s32)                            ← src 槽里是内存地址
//     OP_BAND~SHR  dst(s32) a(s32) b(s32)                       ← 位运算
//     OP_BNOT      dst(s32) a(s32)
// ============================================================

#ifndef FVM_VM_H
#define FVM_VM_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================
// 操作码
// ============================================================
enum Opcode {
    OP_ASSIGN = 1,
    OP_COPY   = 2,
    OP_ADD    = 3,
    OP_SUB    = 4,
    OP_MUL    = 5,
    OP_DIV    = 6,
    OP_MOD    = 7,
    OP_EQ     = 8,
    OP_NEQ    = 9,
    OP_GT     = 10,
    OP_GTE    = 11,
    OP_LT     = 12,
    OP_LTE    = 13,
    OP_CALL   = 14,
    OP_IF     = 15,
    OP_JMP    = 16,
    OP_HALT   = 17,
    OP_NATIVE = 18,
    OP_LOAD_ELEM = 19,   // 取容器元素：dst = 池[容器索引][下标/键]
    OP_BAND   = 20,      // 按位与：dst = a & b
    OP_BOR    = 21,      // 按位或：dst = a | b
    OP_BXOR   = 22,      // 按位异或：dst = a ^ b
    OP_BNOT   = 23,      // 按位取反：dst = ~a（一元，只有 dst a）
    OP_SHL    = 24,      // 左移：dst = a << b
    OP_SHR    = 25,      // 右移：dst = a >> b
};

// 内存槽类型（和编译器一致，直接存在槽的类型段里）
static const int MEM_INT    = 0;
static const int MEM_FLOAT  = 1;
static const int MEM_STRING = 2;
static const int MEM_ARRAY  = 3;   // vector<Value>：槽里存池索引
static const int MEM_MAP    = 4;   // map<Value,Value>：槽里存池索引
static const int MEM_GRID   = 5;   // 二维表：一维连续内存 + 宽高
static const int MEM_UNDEFINED = 6; // GML 的 undefined（is_undefined 就认这个类型）
static const int MEM_LIST   = 7;   // ds_list：槽里存池索引

// 池里的元素：和一个内存槽同构（类型 + 值）
struct Value { int t; int v; };

// 是不是容器句柄（值是池下标的那种）。递归销毁 / 引用计数就靠这个判断。
static inline bool is_container(const Value& v) {
    return v.t == MEM_ARRAY || v.t == MEM_MAP || v.t == MEM_GRID || v.t == MEM_LIST;
}
static inline bool is_container(int t) { return is_container(Value{t, 0}); }
struct Grid {
    int w;
    int h;
    std::vector<Value> cells;
};

// ============================================================
// 实例
//   实例是引擎级的东西（跟 ds_map 那种全局 id 一个道理，全部 VM 共用一份），
//   所以表放外面做全局，不放 Vm 里。
//   一个实例 = 对象名 + 它自己的变量表（名字 → 槽值，就是那 8 字节）
//   实例 id = 在表里的下标；0 保留给"全局/当前"，不作实例
// ============================================================
class Vm;   // Instance 里要存 owner 指针，先声明一下（完整定义在下面）

// 对象名 → 拥有它的 VM（全局一张表，定义在 vm_instance.cpp）
//   一个对象的块只装在它自己的 VM 里；跨对象派发（跑别人的块、run_event_user）
//   就靠这张表找到"那个对象归谁管"。Vm::load_object 时登记，Vm 挂了自动摘掉。
// 对象名**池下标** → VM（运行时零哈希；文本→下标只在装载期做）
extern std::unordered_map<int, Vm*> g_vm_of_object;
Vm* vm_of_object(const std::string& obj);     // 没装载这个对象 → NULL（内部 str_find 一次）
Vm* vm_of_object_sid(int sid);               // 名字池下标在手上时用它（零哈希）

// 全局变量表：名字 → 值（全局一张，定义在 vm_instance.cpp）
//   GML 的 global.x 是进程级的，所有对象/实例共用一份，所以不放 Vm 里。
//   VM 挂掉也不清（全局变量活到进程结束）。
//   游戏 API 里 inst == 0 就是"读/写全局变量"的那个哨兵。
// 全局变量表（GML 的 global.x）：名字的**池下标** → 值（整数 key）
extern std::unordered_map<int, Value> g_globals;

// 池下标版：手上已经有下标时用（零哈希）
Value* global_var_sid(int sid);                            // 没有 → NULL
void   global_var_set_sid(int sid, Value v);               // 没有就建
bool   global_exists_sid(int sid);

// 名字版：一次哈希换下标（名字不在池里 = 不可能存在）。动态名字走这条
Value* global_var(const std::string& name);
void   global_var_set(const std::string& name, Value v);
bool   global_exists(const std::string& name);

// mod VM 注册表：mod id（enemy_id / card_id / bullet_id …）→ 管这个 mod 的 VM
//   平台那边的对应物是 global.mod_enemy_vms / mod_card_vms / mod_bullet_vms 那几张
//   "id → vm 结构体"的表（src_mod.gml）。我们这边 vm 就是一个 Vm。
//   mod 对象（obj_card_mod / obj_enemy_mod / obj_bullet_mod）的硬编码块靠它找到
//   "这个实例属于哪个 mod 的 VM"，再去那个 VM 里跑 _OBJECT_STEP / _OBJECT_DRAW …
//   谁注册：宿主装载 mod 的时候；Vm 析构时自动摘掉（不留悬空指针）。
// mod id 的**池下标** → VM（和 g_vm_of_object 统一：运行时零哈希）
extern std::unordered_map<int, Vm*> g_mod_vms;
Vm*  mod_vm_sid(int sid);                                   // mod id 池下标 → VM（零哈希）
Vm*  mod_vm(const std::string& mod_id);                      // 文本版（内部 str_find 一次）
void mod_vm_register(const std::string& mod_id, Vm* vm);

// 函数表条数：要和 kNativeFns / kGameFns 的实际长度一致
//   （两个 .cpp 里有 static_assert 卡住；加函数只能往表尾加，这里跟着改）
static const int NATIVE_FN_COUNT = 341;
static const int GAME_FN_COUNT   = 150;

// 预置段的固定基址（段边界；下标本身看 names.h 的 S_xxx）
//   0 = 空串 | 1..178 = native 名字 | 179..328 = game 名字 | 329..347 = 19 个内置变量名
enum {
    SID_EMPTY       = 0,
    SID_NATIVE_BASE = 1,
    SID_GAME_BASE   = SID_NATIVE_BASE + NATIVE_FN_COUNT,
    SID_FIELD_BASE  = SID_GAME_BASE + GAME_FN_COUNT
};

// ── 19 个内置变量：一份名单，序号就是 Instance::_inner 的下标 ──
//   名字同时也预置进全局字符串池（SID_FIELD_BASE 起连续 19 个），所以
//   "属性名的池下标"落在这一段时就是 `_inner[下标 - SID_FIELD_BASE]`，一次偏移直取。
//   名单只在这里写一次：枚举、名字表、默认值表全从它生成，顺序不可能漂。
#define INNER_FIELD_LIST(X) \
    X(x) X(y) X(sprite_index) X(image_index) X(image_speed)                    \
    X(image_xscale) X(image_yscale) X(image_alpha) X(image_angle) X(image_blend) \
    X(depth) X(speed) X(direction) X(hspeed) X(vspeed)                         \
    X(gravity) X(gravity_direction) X(friction) X(visible)

enum {
#define FIELD_ENUM(NAME) F_##NAME,
    INNER_FIELD_LIST(FIELD_ENUM)
#undef FIELD_ENUM
    FIELD_COUNT
};

struct Instance {
    // ── id 和槽位是两个东西 ──
    //   id   ：实例的身份，**只增不减**（永不复用）→ 旧引用不会撞上新实例
    //   槽位 ：在 g_instances 里的位置，**可复用**（死了进 g_inst_free）
    int                          id;
    Vm*                          vm;     // 属于哪个 VM（VM 死的时候要把名下的都收掉）

    // ── 常用内置变量（19 个）──
    //   GML 里这些就是实例变量，宿主（位移、碰撞、绘制）天天读写，所以集中占一个数组，
    //   不走 vars 那张 map。名单照项目里的实际使用频率挑的（2073 个 gml 统计）。
    //   都存成 Value（和内存槽同一个 8 字节结构），这样读写就是一次偏移。
    Value _inner[FIELD_COUNT];

    // 其它实例变量：名字的**池下标** → 值（整数 key；数据不需要有序，用 unordered）
    std::unordered_map<int, Value> vars;
};

// 内置字段填默认值（inst_new 里调）
void inst_init(Instance& in);

// 字段序号 → 那个 Value（越界不查，调用方保证序号合法）
static inline Value& inst_f(Instance& in, int f) { return in._inner[f]; }

// 字段名的池下标（预置段里连续 19 个）
static inline int field_sid(int f) { return SID_FIELD_BASE + f; }
static inline bool sid_is_field(int sid) {
    return sid >= SID_FIELD_BASE && sid < SID_FIELD_BASE + FIELD_COUNT;
}
static inline int field_of_sid(int sid) { return sid - SID_FIELD_BASE; }

// 名字 / 池下标 → 内置字段的指针；不是内置字段就返回 NULL（调用方再去看 vars）
Value* inst_field(Instance& in, const std::string& name);
Value* inst_field_sid(Instance& in, int sid);

// ⚠ 就两条硬规矩（Initialize/Create 里想直接写字段时按这个来）：
//   · 名字在 INNER_FIELD_LIST 那 19 个里 → 写 inst_f(in, F_xxx)（就是 _inner 的一格）
//   · 不是内置 → 只能落 vars（in->vars[name]）；写反了两边读不到一起
//   · 数字/字符串不需要引用计数；**容器句柄**要 retain/release（走 hot 的 set_var）

// 实例表：全局一份，下标 = 实例 id，0 不用（0 = "全局/当前实例"的约定值）
//   池化管理，跟四个容器池一个套路：
//     销毁 → 下标进复用队列（内容先留着，等复用时再清）
//     新建 → 先看队列里有没有回收的下标，没有再往表尾新增
extern std::vector<Instance> g_instances;
// 活着的实例：实例 id（只增不减） → 槽位索引（可复用）
extern std::unordered_map<int, int> g_inst_alive;

int       inst_new(Vm* vm, int obj_sid);               // 建一个实例（对象名用**池下标**），返回 id
Instance* inst_get(int id);                            // 按 **实例 id** 取（没有 / 已销毁 → NULL）
void      inst_kill(int id);                           // 只回收（不跑事件）：移除 + 槽位进复用队列
void      inst_destroy(int id);                        // 销毁**唯一入口**：跑 Destroy 事件 + inst_kill
int       inst_count_sid(int sid);                     // 某对象还活着几个（名字池下标）
int       inst_at_sid(int sid, int k);                 // 某对象的第 k 个活实例 id
int       inst_count(const std::string& obj);          // 同上，文本版（内部转一次下标）
int       inst_at(const std::string& obj, int k);      // 某对象的第 k 个活实例 id（没有 → -1）
Value*    inst_var_sid(int id, int sid);               // 同上，池下标已经在手上（零哈希）
void      inst_var_set_sid(int id, int sid, Value v);

struct ValueLess {
    bool operator()(const Value& a, const Value& b) const {
        if (a.t != b.t) return a.t < b.t;
        return a.v < b.v;
    }
};
// 内存槽个数（VM_Create 的默认 mem_size）
//   0 号 = 返回值；1..4095 = 字面量池；4096 起是各块的局部段（到 LOCAL_MAX 为止）
//   末尾再留一小段给 C++ 硬编码块当临时槽（hot/ 里的热函数调函数用）
static const int SCRATCH_BASE  = 32768;
static const int SCRATCH_SLOTS = 64;
static const int MEM_SIZE      = SCRATCH_BASE + SCRATCH_SLOTS;

// OP_CALL / OP_NATIVE 的 dst 特殊值：无返回值
static const int VOID_DST = -1;

class Vm;

// 取指用的小端读取（4 字节的 memcpy 编译完就是一条 mov，不额外花时间）
static inline int32_t ld32(const uint8_t* p) { int32_t v;    memcpy(&v, p, 4); return v; }
static inline uint16_t ld16(const uint8_t* p) { uint16_t v;   memcpy(&v, p, 2); return v; }
static inline float   ldf (const uint8_t* p) { float v;      memcpy(&v, p, 4); return v; }

// 参数布局：[fid(u16)][argc(u8)][dst(s32)][arg(s32)*argc]
//   args 指向参数表开头，argc 个参数各是 s32 的槽号
//   dst  = 结果槽号（-1 = 无返回值）
typedef void (*Fn)(Vm& vm, const uint8_t* args, int argc, int dst);

// 函数表条数：要和 kNativeFns / kGameFns 的实际长度一致
//   （两个 .cpp 里有 static_assert 卡住；加函数只能往表尾加，这里跟着改）
//   ⚠ 定义在 struct Instance 之前，因为预置段的基址（SID_*）要用它算。

// ============================================================
// 全局字符串池（所有 Vm 共用一张，不再是每个 Vm 一份）
//   文本 → 下标，intern 去重。Value.t == MEM_STRING 时，v 就是这里的下标。
//   装载 bin / callbin 时把它自带的池并进来，并把字节码里内联的字符串常量
//   改写成全局下标（remap_block_strings）—— 于是同一个字符串在任何 VM 里都是
//   同一个下标：比较能退化成整数比较，跨 VM 传值也不用重新 intern。
//   ⚠ 单线程：只在装载期和 intern 时写，平时只读。
//   ⚠ 下标在字节码里是 u16（OP_ASSIGN），池上限 65535；超了装载时直接报错。
//   ⚠ 卸载一个 Vm **不回收**它带来的字符串（可能还有别人在用）→ 池只增不减。
// ============================================================
extern std::vector<std::string>             g_strings;
extern std::unordered_map<std::string, int> g_str_map;

bool str_ok(int i);                       // 下标是否合法（0 <= i < 池大小）
const std::string& str_at(int i);         // 取字符串（越界 → 空串，不抛）
int  str_find(const std::string& s);      // 名字 → 下标（**只查不建**，没有 → -1）
int  intern_str(const std::string& s);    // 名字 → 下标（没有就建），返回全局下标

// 池下标 → 字符串值（下标是编译期常量 S_xxx 时用；零哈希）
static inline Value mk_str_sid(int sid) {
    Value v; v.t = MEM_STRING; v.v = sid;
    return v;
}

// ── 预置段：函数名也进池，下标直接当函数编号用 ──
//   第一次碰池之前（intern_str 会自己兜一下）按 kNativeNames / kGameNames 的顺序
//   intern 一遍，于是固定成：
//     下标 0                        = 空串
//     1 .. NATIVE_FN_COUNT          = native 名字（顺序 = kNativeFns）
//     往后 GAME_FN_COUNT 个          = game 名字（顺序 = kGameFns）
//     再 FIELD_COUNT 个             = 19 个内置变量名（顺序 = INNER_FIELD_LIST）
//   所以"名字在池里的下标"就能直接换算成函数/字段：看落在哪个区间，减掉基址取数组。
//   按名字调内置因此不用再线性扫描；拿编译期常量 SID_* 的调用点连哈希都省了。
//   （基址 SID_* 定义在上面 struct Instance 之前）

void pool_seed();                         // 把预置段灌进池（幂等；intern_str 会自己调）
Fn   fn_of_sid(int sid);                  // 池下标 → 函数（不是函数名 → NULL）
Fn   fn_by_name(const char* name);        // 名字 → 函数（一次哈希；找不到 → NULL）

// ============================================================
// 游戏 API（顺序 = 原 VM 的 FUNC_DEFS，不能改）
// ============================================================
#define GAME_FN_LIST(X)                                                                        \
    X(VM_BanCard) X(VM_SetCardLevelCap) X(VM_SetMaxSlots) X(VM_ShellPrint) X(VM_ShowNotice)    \
    X(VM_CreatePlatform) X(VM_SpawnPlant) X(VM_SpawnEnemy) X(VM_SpawnBoss) X(VM_LoadSprite)    \
    X(VM_SpawnObject) X(VM_SetProp) X(VM_GetWave) X(VM_GetSubwave) X(VM_GetProp)               \
    X(VM_GetLastBoss) X(VM_GetLastCreatedEnemy) X(VM_GetLastKilledEnemy)                       \
    X(VM_GetLastCreatedCard) X(VM_GetLastDestroyedCard) X(VM_GetFlame) X(VM_SetFlame)          \
    X(VM_SetTerrain) X(VM_ClearPlants) X(VM_Random) X(VM_GetLastIdlePlatform)                  \
    X(VM_SetPlatformParams) X(VM_SetMapBackground) X(VM_SetDrawSlot) X(VM_GetLoadedSpriteName) \
    X(VM_GameWin) X(VM_GameLose) X(VM_SetDrawSlot_front) X(VM_SpawnCats)                       \
    X(VM_ClearMapObjects) X(VM_BanGem) X(VM_SetRowFeature) X(VM_ClearPlantsByType)             \
    X(VM_WakePlants) X(VM_SetCardProp) X(VM_SetEnemyProp) X(VM_ShowNoticeDur)                  \
    X(VM_SetEventEnabled) X(VM_RefreshPlatformSnapshots) X(VM_GetMouseX) X(VM_GetMouseY)       \
    X(VM_GetMouseCol) X(VM_GetMouseRow) X(VM_GetTerrain) X(VM_GetMousePressed)                 \
    X(VM_GetKeyDown) X(VM_GetKeyPressed) X(VM_GetEnemyCount) X(VM_GetPlantCount)               \
    X(VM_GetPlantCountAt) X(VM_PlaySound) X(VM_GetPlantAt) X(VM_SwapPlants)                    \
    X(VM_SwapPlantRects) X(VM_CompactColumn) X(VM_CompactRow) X(VM_CompactColumnRev)           \
    X(VM_CompactRowRev) X(VM_SpawnPlantsRandom) X(VM_CreateButton) X(VM_GetLastClickedButton)  \
    X(VM_LoadSpriteFrames) X(VM_ApplyPlantLevel) X(VM_LoadSpritePerm) X(VM_FreeSpritePerm)     \
    X(VM_SetCardSlotProp) X(VM_CalcCardSlotProp) X(VM_GetCardSlotCount) X(VM_GetPreviewCard)   \
    X(VM_AliasSprite) X(VM_LoadSpriteFrames_Ex) X(VM_GetLastBossStateChangeId)                 \
    X(VM_GetLastBossOldState) X(VM_GetLastBossNewState) X(VM_ArrayGet) X(VM_ArraySet)          \
    X(VM_ArrayDel) X(VM_ArrayADD) X(VM_ArraySize) X(VM_ArrayClear) X(VM_ArrayClearAll)         \
    X(VM_SetNoticeStyle) X(VM_conveyor_belt_able) X(VM_Slot_add) X(VM_BanAllCard)              \
    X(VM_CannelBanCard) X(VM_BanWeapon) X(VM_BanSuperWeapon) X(VM_BanShield)                   \
    X(VM_SetCardShapeCap) X(VM_SetCardSkillCap) X(VM_GetKilledProp) X(VM_IsUndefined)          \
    X(VM_IsDestroyed) X(VM_LoadSound) X(VM_Floor) X(VM_Ceil) X(VM_SpawnBatMouse)               \
    X(VM_GetTimeLimit) X(VM_SetTimeLimit) X(VM_SetDrawSlotEx) X(VM_SetDrawSlotEx_front)        \
    X(VM_SetWaveAuto) X(VM_SetWave) X(VM_GetCurCard) X(VM_EnemyInRange) X(VM_GetHomingTarget)  \
    X(VM_GetInstancesInRange) X(VM_CreateInstance) X(VM_LoadSpritePerm_Ex)                     \
    X(VM_SetShovelFlameRate) X(VM_BulletScreenAdd) X(VM_DrawSpriteExt) X(VM_RunStep)           \
    X(VM_DestroyInstance) X(VM_GetCardProp) X(VM_CanPlace) X(VM_CallFunc) X(VM_FuncExists)     \
    X(VM_FuncDesc) X(VM_SpriteExists) X(VM_AliasSpritePerm) X(VM_ArrayExists) X(VM_CellCount)  \
    X(VM_CellItem) X(VM_CellContains) X(VM_ArrayContains) X(VM_InstArrayExists)                \
    X(VM_InstArraySize) X(VM_InstArrayItem) X(VM_InstArraySet) X(VM_InstArrayAdd)              \
    X(VM_InstArrayDel) X(VM_InstArrayClear) X(VM_InstArrayContains) X(VM_HomingBulletAdd)      \
    X(VM_DamageEnemy) X(VM_DamageEnemyAsh) X(VM_BulletScreenAdd_Ex) X(VM_BulletScreenAdd_Exs)  \
    X(VM_GetInfo) X(VM_CatInRow) X(VM_MapObj) X(VM_GetInstanceCount) X(VM_GetInstanceAt)

extern Fn  kGameFns[];          // 不是 const：已实现的那几个在启动时按名字换进表里

// ============================================================
// 内部函数（OP_NATIVE 调用）
//   从「卡片 / 敌人 / 子弹」的原生 GML 里提出来、现有 VM 没有的那批
//   id = 这个顺序 + 表尾追加的那几个（见 vm_native.cpp）
//   已经写实的（run_block、数学、数组、实例、全局…）实现在
//   vm_container.cpp / vm_instance.cpp / vm_builtin.cpp / kernel.cpp，
//   不在这里生成空壳，名字挂在 vm_native.cpp 表尾
// ============================================================
#define NATIVE_FN_LIST(X) \
    X(_find_right_cell) \
    X(alarm_get) \
    X(armor_drop) \
    X(array_deep_copy) \
    X(asset_get_index) \
    X(audio_stop_sound) \
    X(boss_array_shuffle) \
    X(boss_random) \
    X(bresenham) \
    X(buffer_exists) \
    X(bullet_screen_add_Ex) \
    X(bullet_screen_add_Exs) \
    X(Camera_Shock) \
    X(can_hit) \
    X(can_place_at_position) \
    X(can_target_on) \
    X(card_created) \
    X(card_equipped_attire_id) \
    X(clear_train_body) \
    X(connectOnesToTwos) \
    X(create_train_body) \
    X(create_train_body_order) \
    X(create_train_body_pos) \
    X(deck_get_card_data) \
    X(draw_self) \
    X(draw_set_colour) \
    X(draw_sprite_ext) \
    X(draw_text) \
    X(effect_create_above) \
    X(event_perform) \
    X(find_priority_enemy) \
    X(get_attire_info) \
    X(get_card_info_simple) \
    X(get_gem_index) \
    X(get_gem_info) \
    X(get_gem_level) \
    X(get_load_sprite) \
    X(get_plant_data_with_skill) \
    X(get_weapon_info) \
    X(homing_bullet_add) \
    X(instance_position) \
    X(into_act) \
    X(is_cookbook_equipped) \
    X(mod_base_tick) \
    X(mod_enemy_init) \
    X(mod_inst_add) \
    X(mod_inst_del) \
    X(move_towards_point) \
    X(network_apply_plant_level) \
    X(network_spawn_enemy) \
    X(object_get_name) \
    X(place_meeting) \
    X(point_in_rectangle) \
    X(position_meeting) \
    X(remove_train_body) \
    X(screen_flash) \
    X(set_net_id) \
    X(shader_get_uniform) \
    X(shader_reset) \
    X(shader_set) \
    X(show_notice) \
    X(sprite_add) \
    X(sprite_collision_mask) \
    X(sprite_exists) \
    X(sprite_get_number) \
    X(sprite_set_offset) \
    X(variable_struct_exists) X(vm_hook_run) \
    X(card_destroyed) X(get_nearest_grid_position) \
    X(collision_other) \
    X(inst_array_read) X(inst_array_assign) X(inst_array_index) X(inst_array_delete) \
    X(inst_array_pop) X(inst_array_resize) X(inst_array_shuffle) X(inst_array_create)

// 补的实例数组内部函数（不算游戏 API，不走 FUNC_DEFS / OP_CALL）
//   游戏那边只有 8 个 VM_InstArray*（Exists/Size/Item/Set/Add/Del/Clear/Contains），
//   缺的这几件用内部函数补，实现时对着 GML 的原意来：
//     inst_array_read(inst, name)           把实例变量里的数组读出来放进 VM 的数组池，
//                                           返回句柄（没有 / 不是数组 → 给个空数组的句柄）
//     inst_array_assign(inst, name, handle) 把池里的数组整个写进实例变量（替换）
//     inst_array_index(inst, name, value)   某个值在数组里的下标，没有 → -1
//                                           （对 GML 的 array_get_index）
//     inst_array_delete(inst, name, k, n)   从下标 k 起删 n 个（对 GML 的 array_delete）
//     inst_array_pop(inst, name)            弹出最后一个并返回（对 GML 的 array_pop）
//     inst_array_resize(inst, name, n)      改长度（对 GML 的 array_resize）
//     inst_array_shuffle(inst, name)        打乱（对 GML 的 array_shuffle）
//     inst_array_create(inst, name, n [,v]) 建成 n 个（可选填 v，默认 0）的数组（对 GML 的 array_create）
//   有 read/assign 这两个桥就够用了：
//     嵌套字面量 x = [[1,2],[3]] → 在池里搭好（array_literal/array_push）再 assign
//     array_deep_copy            → read 到池里，array_deep_copy，再 assign 回去
//     array_push(list, [a,b])    → read → array_push → assign

extern const Fn  kNativeFns[];
extern Fn        kGameFns[];      // 不是 const：已实现的那几个在启动时按名字换进表里

// 有真实现的游戏 API：启动时按名字把表项换成真函数（id 顺序不能动，所以不在名单里增删）
//   实体在 vm_callbin.cpp（VM_CallFunc / VM_FuncExists / VM_FuncDesc）和以后别的 .cpp
//   谁调：Vm 构造时调一次（g_game_fns_patched 挡住重复）
void patch_game_fns();
extern bool g_game_fns_patched;

// 按名字取内部函数 / 游戏 API 的 id（找不到 → -1）
//   给 C++ 硬编码块（hot/）调函数用，也方便报错
int native_id(const char* name);
int game_id(const char* name);

// 跑"硬编码的热块"（hot/hot_table.cpp）：有 → 跑 C++ 版并返回 true；没有 → false（让解释器去跑）
//   _sid 版：对象名和块名的**池下标**已经在手上（零哈希、零字符串比较）
bool run_hot_block(Vm& vm, const std::string& obj, const std::string& block);
bool run_hot_block_sid(Vm& vm, int obj_sid, int block_sid);
// 有没有这个热块（不跑）。派发事件前先问一下：父对象不在 VM 的块表里，
// 但它的 Create/Destroy 是硬编码的，别当成"块找不到"报错
bool has_hot_block(const std::string& obj, const std::string& block);
bool has_hot_block_sid(int obj_sid, int block_sid);
bool has_hot_object_sid(int obj_sid);
// 跑某个实例的某个事件，**沿父对象往上找同名块**（GML 的事件继承；event_user / 碰撞 / 自定义事件都走这条）。
//   硬编码这边找到并跑了 → true；一条都没有 → false（调用方再走解释器那条路）
bool run_hot_event_inherited(Vm& vm, int inst, const char* block);

// 函数返回值约定：被调块用 OP_COPY 把返回值写进 0 号槽（RET_SLOT），然后 OP_HALT。
// 调用方 run_block 回来之后直接读 0 号槽。字面量池从 1 号槽开始，0 号不给别人用。
static const int RET_SLOT = 0;

// ============================================================
// 容器 / 内核（实现在 vm_container.cpp / kernel.cpp）
// ============================================================
void run_block(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_create(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_destroy(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_clear(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_add(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_delete(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_size(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_find_value(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_list_find_index(Vm& vm, const uint8_t* args, int argc, int dst);
void array_literal(Vm& vm, const uint8_t* args, int argc, int dst);
void array_get(Vm& vm, const uint8_t* args, int argc, int dst);
void array_set(Vm& vm, const uint8_t* args, int argc, int dst);
void array_pop(Vm& vm, const uint8_t* args, int argc, int dst);
void array_clear(Vm& vm, const uint8_t* args, int argc, int dst);
void array_contains(Vm& vm, const uint8_t* args, int argc, int dst);
void array_push(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_create(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_set(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_delete(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_size(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_clear(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_keys_to_array(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_destroy(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_map_find_value(Vm& vm, const uint8_t* args, int argc, int dst);
void struct_get(Vm& vm, const uint8_t* args, int argc, int dst);
void struct_set(Vm& vm, const uint8_t* args, int argc, int dst);
void struct_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_create(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_get(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_set(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_width(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_height(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_clear(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_grid_destroy(Vm& vm, const uint8_t* args, int argc, int dst);
void merge_colour(Vm& vm, const uint8_t* args, int argc, int dst);
void VM_Execute(Vm& vm, const uint8_t* args, int argc, int dst);

// ============================================================
// 内置函数（实现在 vm_container.cpp / vm_instance.cpp / vm_builtin.cpp / kernel.cpp）
//
//   约定：已经写实的名字要从 NATIVE_FN_LIST 里拿掉、改挂到 vm_native.cpp 表尾
//   （id 是"宏名单 + 表尾"拼出来的，翻译器也照这个顺序算 id，两边必须一致）。
//
//   哪些该写实：**VM 自己能算出来的**（数学、类型判断、容器、实例表、全局变量、
//   网格坐标换算）就写。依赖引擎的（精灵尺寸、碰撞、绘制、音频、网络、shader）
//   仍然留空壳——那些是宿主的活，VM 里没有精灵/房间的几何数据。
// ============================================================

// ── 容器 / 数组（vm_container.cpp）──
//   ds_exists(句柄, 类型)：类型常量照 GML：ds_type_list=0、ds_type_map=1、
//   ds_type_grid=3（ds_type_queue=2、ds_type_stack=4、ds_type_priority=6 我们没实现）
void array_length(Vm& vm, const uint8_t* args, int argc, int dst);
void array_get_index(Vm& vm, const uint8_t* args, int argc, int dst);
void array_delete(Vm& vm, const uint8_t* args, int argc, int dst);
void array_resize(Vm& vm, const uint8_t* args, int argc, int dst);
void array_create(Vm& vm, const uint8_t* args, int argc, int dst);
void array_shuffle(Vm& vm, const uint8_t* args, int argc, int dst);
void ds_exists(Vm& vm, const uint8_t* args, int argc, int dst);

// 只读访问容器的口子（池是 static，外面看不见；给 json_stringify 这类遍历用）
int   container_size(const Value& v);                                     // 数组/列表/map 的元素个数
Value container_at(const Value& v, int i);                                // 数组/列表第 i 个（越界 → undefined）
std::vector<std::pair<Value, Value> > map_pairs(const Value& v);          // map/struct 的全部键值
// 只读查键：不动引用计数、不碰 scratch 槽（"借来的"句柄也能安全查）
bool  map_find(const Value& v, const Value& key, Value* out);

// ── 实例（vm_instance.cpp）──
//   instance_destroy / instance_create_depth 会跑对方的 Destroy / Create 事件
//   （跟 GML 一样），所以里面走 run_block_on
void instance_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void instance_number(Vm& vm, const uint8_t* args, int argc, int dst);
void instance_create_depth(Vm& vm, const uint8_t* args, int argc, int dst);
void instance_destroy(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_instance_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_instance_get(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_instance_set(Vm& vm, const uint8_t* args, int argc, int dst);

// ── 常规内置（vm_builtin.cpp）──
void floor(Vm& vm, const uint8_t* args, int argc, int dst);
void round(Vm& vm, const uint8_t* args, int argc, int dst);
void ceil(Vm& vm, const uint8_t* args, int argc, int dst);
void abs(Vm& vm, const uint8_t* args, int argc, int dst);
void max(Vm& vm, const uint8_t* args, int argc, int dst);
void min(Vm& vm, const uint8_t* args, int argc, int dst);
void clamp(Vm& vm, const uint8_t* args, int argc, int dst);
void lerp(Vm& vm, const uint8_t* args, int argc, int dst);
void random(Vm& vm, const uint8_t* args, int argc, int dst);
void irandom(Vm& vm, const uint8_t* args, int argc, int dst);
void chr(Vm& vm, const uint8_t* args, int argc, int dst);
void string_pos(Vm& vm, const uint8_t* args, int argc, int dst);
void file_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void camera_get_view_x(Vm& vm, const uint8_t* args, int argc, int dst);
void string_delete(Vm& vm, const uint8_t* args, int argc, int dst);
void file_text_open_write(Vm& vm, const uint8_t* args, int argc, int dst);
void file_text_write_string(Vm& vm, const uint8_t* args, int argc, int dst);
void file_text_close(Vm& vm, const uint8_t* args, int argc, int dst);
void camera_get_view_y(Vm& vm, const uint8_t* args, int argc, int dst);
void camera_get_view_width(Vm& vm, const uint8_t* args, int argc, int dst);
void camera_get_view_height(Vm& vm, const uint8_t* args, int argc, int dst);
void audio_create_stream(Vm& vm, const uint8_t* args, int argc, int dst);
void room_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void directory_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void directory_create(Vm& vm, const uint8_t* args, int argc, int dst);
void network_destroy(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_struct_remove(Vm& vm, const uint8_t* args, int argc, int dst);
void buffer_create(Vm& vm, const uint8_t* args, int argc, int dst);
void random_range(Vm& vm, const uint8_t* args, int argc, int dst);
void irandom_range(Vm& vm, const uint8_t* args, int argc, int dst);
void sin(Vm& vm, const uint8_t* args, int argc, int dst);
void cos(Vm& vm, const uint8_t* args, int argc, int dst);
void degtorad(Vm& vm, const uint8_t* args, int argc, int dst);
void lengthdir_x(Vm& vm, const uint8_t* args, int argc, int dst);
void lengthdir_y(Vm& vm, const uint8_t* args, int argc, int dst);
void point_direction(Vm& vm, const uint8_t* args, int argc, int dst);
void point_distance(Vm& vm, const uint8_t* args, int argc, int dst);
void is_real(Vm& vm, const uint8_t* args, int argc, int dst);
void is_string(Vm& vm, const uint8_t* args, int argc, int dst);
void is_array(Vm& vm, const uint8_t* args, int argc, int dst);
void is_struct(Vm& vm, const uint8_t* args, int argc, int dst);
void is_undefined(Vm& vm, const uint8_t* args, int argc, int dst);
void string(Vm& vm, const uint8_t* args, int argc, int dst);
void json_stringify(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_global_exists(Vm& vm, const uint8_t* args, int argc, int dst);
void variable_global_get(Vm& vm, const uint8_t* args, int argc, int dst);
void merge_color(Vm& vm, const uint8_t* args, int argc, int dst);
void make_color_rgb(Vm& vm, const uint8_t* args, int argc, int dst);
void calculate_plant_depth(Vm& vm, const uint8_t* args, int argc, int dst);
void get_grid_position_from_world(Vm& vm, const uint8_t* args, int argc, int dst);
void get_world_position_from_grid(Vm& vm, const uint8_t* args, int argc, int dst);
// damage_enemy(inst, dmg, dmg_type)：走**敌人自己的受击事件**（闪白/音效/护盾/自定义 Other_10）
//   脚本原意见 scripts/scr_command_VM/scr_command_VM.gml:2765，别改成直接扣血
void damage_enemy(Vm& vm, const uint8_t* args, int argc, int dst);

// ── 引擎侧：留空壳/占位（宿主实现）──
//   audio_play_sound / send_message 什么都不做（GML 里本来就无返回值）；
//   show_debug_message 打到 stdout，方便看 VM 里到底走了哪条路
void audio_play_sound(Vm& vm, const uint8_t* args, int argc, int dst);
void send_message(Vm& vm, const uint8_t* args, int argc, int dst);
void show_debug_message(Vm& vm, const uint8_t* args, int argc, int dst);

// ── 绘制 / 字符串度量 / 网络广播：**宿主负责**，这里先只留签名（空实现）──
//   Player / Effects 那 59 个对象要用（宝石的 Draw、闪屏、技能广播）
//   ⚠ 空实现 = 不写 dst → 取返回值的地方拿到 0：
//     绘制族拿 0 无所谓（画本来就不在 VM 里）；
//     string_width_ext / string_height_ext 要字体系统量；网络广播要宿主发。
//   ceil / make_color_rgb 是纯数学，已经写实了（vm_builtin.cpp）。
//   挂的是 vm_native.cpp 表尾（不是 NATIVE_FN_LIST）→ 不动已有 id
void draw_set_color(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_set_alpha(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_set_font(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_set_halign(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_set_valign(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_rectangle(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_rectangle_color(Vm& vm, const uint8_t* args, int argc, int dst);
void draw_roundrect(Vm& vm, const uint8_t* args, int argc, int dst);
void string_width_ext(Vm& vm, const uint8_t* args, int argc, int dst);
void string_height_ext(Vm& vm, const uint8_t* args, int argc, int dst);
void network_broadcast_active_skill(Vm& vm, const uint8_t* args, int argc, int dst);

// ── 块（kernel.cpp）──
bool run_block_on(Vm& vm, int obj_sid, int inst, int block_sid);   // 全用池下标
void run_event_user(Vm& vm, const uint8_t* args, int argc, int dst);

// ============================================================
// 块 / 文件
// ============================================================
struct Block {
    int name_sid;      // 块名的**池下标**
    std::vector<uint8_t> code;
};

// ============================================================
// 虚拟机
// ============================================================

// ============================================================
// 每个 VM 名下的实例容器（数组 / map 双模式，上层只调接口）
//   count < 16  → 数组：连续内存，取第 k 个 arr[k] O(1)
//   跨过 16     → 转 map + 游标，**转了不回**（避免在临界值反复转换）
//   两种模式都保序（id 升序 = 创建顺序）：
//     · 数组模式删除用**位移**（不用 swap-erase）
//     · map 模式 key 是 id，天然升序
//   map 模式：cur = 上次查询命中的节点，cur_rank = 它"第几大"
//     查询第 k 个：|k-cur_rank| / k / (n-1-k) 三条路选最短走
//     插入位置 p：p <= cur_rank → cur_rank++（游标指向的节点没变，序号 +1）
//     删除位置 p：p == cur_rank → 游标挪到下一个（rank 不变），没有下一个就退到前一个（rank--）
//                 p <  cur_rank → cur_rank--
//                 p >  cur_rank → 不变
//   ⚠ std::map 的迭代器只在**自己被删**时失效 → 增删别人时游标一直有效
// ============================================================
struct InstBox {
    bool                         use_map;
    std::vector<int>             arr;      // 数组模式：id 升序
    std::map<int, int>           mp;       // map 模式：id → 槽位
    std::map<int, int>::iterator cur;      // 游标
    int                          cur_rank; // 游标是第几大

    InstBox() : use_map(false), cur_rank(0) { cur = mp.end(); }

    int  count() const { return use_map ? (int)mp.size() : (int)arr.size(); }
    int  at(int k);                        // 第 k 个 → id（越界 → -1）
    void add(int id, int slot);            // 新增（顺带判断要不要转 map）
    void erase(int id);                    // 删除（顺带调游标 / rank）
    void ids(std::vector<int>& out) const; // 遍历用：拷一份 id
    void clear() { arr.clear(); mp.clear(); use_map = false; cur_rank = 0; cur = mp.end(); }

private:
    void to_map();                         // 数组 → map（带 hint 插入，摊还 O(n)）
};

// 内层只记"错误码 + 参数"，**格式化文本在外层 log.cpp** ✓
enum VmErrCode {
    VM_OK = 0,
    VM_E_BLOCK_NOT_FOUND,     // a1 = 块名池下标
    VM_E_UNKNOWN_OPCODE,      // a1 = opcode，a2 = 块名池下标
    VM_E_UNKNOWN_NATIVE,      // a1 = native id
    VM_E_UNKNOWN_GAME_FN,     // a1 = game fn id
    VM_E_NONINT_OPERAND,      // a1 = 块名池下标
    VM_E_BIN_TOO_MANY_STRS,   // a1 = 上限
    VM_E_BIN_HEAD,            // a1 = 文件名字符串池下标（外层给）
    VM_E_BIN_BLOCK,           // a1 = 块序号, a2 = 偏移
    VM_E_BIN_TRUNC            // a1 = 需要, a2 = 实际
};

struct Vm {
    // 字符串池是**全局**的（g_strings / g_str_map，见文件上面那一段）：
    //   Value.v 对字符串来说就是全局下标，任何一个 VM 里的字符串都能互相比、互相传。
    char*                    mem;       // 纯字节内存：每槽 8 字节 [type:int32][value:int32]

    // ── 一个 bin = 一个 Vm = 一个对象 ──
    std::unordered_map<int, Block> blocks;   // 块名的池下标 → 块（整数 key，不用有序）

    // 这个 Vm 名下的实例（inst_new 插入、inst_kill 摘掉、析构时按它收尾）
    //   ⚠ 存**实例 id** 不是指针：全局实例表会扩容重分配，销毁的下标还会被复用
    InstBox instances;      // 这个 VM 名下的实例（数组/map 双模式，见 InstBox）


    // 最近一次错误（**只存码和参数，文本在外层 log.cpp**）

    // 碰撞事件里的 other（GML 的 other / other.id）——宿主派发 _OBJECT_COLLISION_* 之前设好。
    bool owns_mem;

    // 全 0 = MEM_INT + 值 0，和 GML 的槽初值一致
    explicit Vm(bool no_mem) {
        mem = no_mem ? NULL : new char[MEM_SIZE * 8]();
        owns_mem = !no_mem;
        if (!g_game_fns_patched) { g_game_fns_patched = true; patch_game_fns(); }
    }
    Vm() : Vm(false) {}
    ~Vm() { destroy_owned_instances(); unregister_objects(); if (owns_mem) delete[] mem; }

    // 把自己名下的实例全部销毁（定义在 vm_instance.cpp）
    void destroy_owned_instances();

    // 从"对象名 → VM"那张全局表里摘掉自己（定义在 vm_instance.cpp）
    void unregister_objects();

    // 装一个 bin：定下"这个 Vm 是哪个对象"，把它的块表灌进来
    //   同时登记到"对象名 → VM"那张全局表（跨对象的派发靠它找过来）
    void load_object(const std::string& object_name, const std::vector<Block>& blks) {
        int sid = intern_str(object_name);            // 装载期把名字进池，运行时只用下标
        std::unordered_map<int, Vm*>::iterator it = g_vm_of_object.find(sid);
        if (it != g_vm_of_object.end() && it->second != this) { /* 名字已被别的 Vm 占了 */ }
        g_vm_of_object[sid] = this;
        obj = sid;                            // 直接用上面 intern 出来的下标
        blocks.clear();
        for (size_t i = 0; i < blks.size(); i++)
            blocks[str_at(blks[i].name_sid)] = blks[i];     // key = 块名的池下标
    }

    // 查块：没有就返回 NULL（一个 Vm 只有一个对象，所以不用给对象名）
    //   两种入口：手上已经有池下标的走 _sid 版（零哈希），只有名字的走名字版
    const Block* find_block_sid(int sid) const;
    const Block* find_block(const std::string& name) const;

    // 找块并执行（硬编码块优先；嵌套调用自己会恢复上下文）
    bool run_named(const std::string& name);
    bool run_named_sid(int sid);

    // ── 执行一个块（实现在 vm.cpp）──
    bool run(const Block& blk);
};

// ============================================================
// callbin —— 外部函数（一个文件一个函数，装进字典，靠 VM_CallFunc 调）
//   文件格式：头 + 跟普通 bin 一模一样的"字符串池 + 块"
//     [i32 magic 'CLBN'] [i32 槽位总数] [i32 参数个数]
//     [i32 name_len][name] [i32 desc_len][desc]
//   它有自己的 Vm，但**没有自己的 mem**：每次被调用，mem 指向共享内存里的新一帧
//     frame = g_call_mem + g_call_off*8，g_call_off += 槽位总数（返回时减回去）
//     帧布局：槽 0 = 返回值，槽 1..argc = 实参，字面量/局部从 argc+1 起
//   共享内存不够 → 打印报错、强制结束（顶层 VM_CallFunc 返回 undefined）
// ============================================================
// .bin / .callbin 共用：从 d[off] 开始读"字符串池 + 块表"（实现在 vm.cpp）
bool parse_bin_body(const uint8_t* d, size_t size, size_t off,
                    std::vector<std::string>& strings, std::vector<Block>& blocks);

// 装载期：把 bin 自带的字符串池并进全局池，并把每个块里内联的字符串常量
//   改写成全局下标（实现在 vm.cpp）。返回 false = 字节码走飞了（指令布局对不上）
//   或池超过 65535 —— 两种情况都必须当成装载失败，不能让错的下标悄悄进池。
bool remap_block_strings(std::vector<Block>& blocks,
                         const std::vector<std::string>& local_pool);

static const int    CALLBIN_MAGIC    = 0x4E424C43;      // 'C''L''B''N'（小端读）
static const size_t CALL_MEM_SLOTS   = 131072;          // 共享内存 1 MB / 8

extern std::vector<char> g_call_mem;                    // 固定大小共享内存（首次调用时分配）
extern size_t            g_call_off;                    // 当前偏移（槽）

// "强制结束整条调用链"的旗子：callbin 触到上限（层数太深 / 共享内存不够）时置上，
//   解释器每条指令前看一眼就立刻收手 → 一层层 false 传回去 → 顶层 VM_CallFunc 返回 undefined。
//   （只让最里层返回 undefined 是不够的：外层拿到 undefined 还会接着算）
extern bool g_call_abort;

struct CallBin {
    std::string name;      // 字典 key（VM_CallFunc 的第一个参数）
    std::string desc;      // VM_FuncDesc 用
    int         argc;      // 参数个数
    int         slots;     // 一帧要多少槽
    Vm*         vm;        // 它自己的 Vm（无独立 mem）
};
extern std::map<int, CallBin> g_call_dict;      // 函数名的**池下标** → callbin

bool callbin_load(const std::string& path);             // 读一个 .callbin 进字典
void callbin_clear();                                   // 清字典（热重载用）

// 调一个 callbin：argv 是实参，返回值写 ret（找不到/参数个数不对/内存不够 → false）
bool callbin_invoke(Vm& caller, int sid, const Value* argv, int argn, Value* ret);

// 游戏 API 的真实现（id 122/123/124）——**名字带 impl_**：
//   因为 vm_api.cpp 里那三个名字已经被宏生成的 static 空壳占住了，
//   在同一个 TU 里写 VM_CallFunc 会跟空壳撞名（补丁表也会指到空壳自己）。
void impl_VM_CallFunc(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_FuncExists(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_FuncDesc(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_GetProp(Vm& vm, const uint8_t* args, int argc, int dst);   // vm_instance.cpp
void impl_VM_SetProp(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_SetTerrain(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_GetTerrain(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_ClearPlants(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_ClearPlantsByType(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_WakePlants(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_GetPlantAt(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_GetPlantCountAt(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_CellCount(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_CellItem(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_CellContains(Vm& vm, const uint8_t* args, int argc, int dst);
void impl_VM_GetPlantCount(Vm& vm, const uint8_t* args, int argc, int dst);

// ============================================================
// 槽读写（vm_container.cpp 和以后的内部函数实现都用这套）
// ============================================================
static inline int argslot(const uint8_t* a, int k) {
    int32_t s; memcpy(&s, a + k * 4, 4); return (int)s;
}
static inline int rd_i(Vm& vm, int slot) { return ((int*)vm.mem)[slot * 2 + 1]; }
static inline int rd_t(Vm& vm, int slot) { return ((int*)vm.mem)[slot * 2]; }
static inline Value rd_v(Vm& vm, int slot) {
    Value v; v.t = ((int*)vm.mem)[slot * 2]; v.v = ((int*)vm.mem)[slot * 2 + 1]; return v;
}
// ============================================================
// 引用计数（实现在 vm_container.cpp）
//   retain  —— 多一个"存储位置"存着这个句柄
//   release —— 少一个；减到 0 就把下标放进该池的删除队列，
//              **不递归释放**，等这个槽被复用时再去放里面的残留引用
//   非容器（int / float / string / undefined）都是空操作
// ============================================================
void retain(const Value& v);
void release(const Value& v);

// 写一个槽：先把新值 retain，再把槽里原来那个值 release
static inline void slot_write(Vm& vm, int slot, Value v) {
    if (slot < 0) return;
    Value old = rd_v(vm, slot);
    retain(v);
    if (old.t != v.t || old.v != v.v) release(old);
    ((int*)vm.mem)[slot * 2]     = v.t;
    ((int*)vm.mem)[slot * 2 + 1] = v.v;
}
static inline void wr_v(Vm& vm, int slot, Value v) { slot_write(vm, slot, v); }
static inline void wr_i(Vm& vm, int slot, int x) {
    if (slot < 0) return;
    Value v; v.t = MEM_INT; v.v = x;
    slot_write(vm, slot, v);
}



// ============================================================
// 字符串名字表（S_xxx）放最后：它要用上面几个 *_LIST 宏（native / game / 字段）
// ============================================================
#include "names.h"

// callbin 的函数体块名（池下标）—— 在 names.h 之后，所以 S__CALL_ENTRY 已声明
static const int CALLBIN_ENTRY = S__CALL_ENTRY;

#endif // FVM_VM_H

