// ============================================================
// fn_table.cpp — 两张表本体（顺序 = core/const_str.h 的名单 = 编译器烘进 bin 的 id）
//
//   ⚠ 这两张表的**顺序就是契约**：
//     OP_NATIVE 的 fid → kNativeFns[fid]
//     OP_CALL  的 fid → kGameFns[fid]
//   名字能加，顺序不能动（理由见 const_str.h 的说明）。
//
//   ⚠ 老代码（transformGML2txt/vm_native.cpp）的表顺序是
//     FN_LIST | 占位段 | 后补段 | 表尾，和编译器的顺序**对不上** ——
//     从真实 bin 里解 fid 看得很清楚（fid 130 编译器说是 floor、老表以为是
//     string_length，而 floor 在 step 事件里被调了 1135 次）。
//     新代码按编译器的顺序来，所以 sid 和 fid 就是一次减法。
// ============================================================
#include "outer.h"
#include "../host/host.h"       // host_packet_set_handler（入站包的注册口，见 outer_init）

Fn kNativeFns[] = {
#define FN_ENTRY(NAME) NAME,
    NATIVE_FN_LIST(FN_ENTRY)
    NATIVE_IMPL_LIST(FN_ENTRY)
#undef FN_ENTRY
};

Fn kGameFns[] = {
#define FN_ENTRY(NAME) NAME,
    GAME_FN_LIST(FN_ENTRY)
#undef FN_ENTRY
};

const int kNativeFnCount = (int)(sizeof(kNativeFns) / sizeof(kNativeFns[0]));
const int kGameFnCount   = (int)(sizeof(kGameFns)   / sizeof(kGameFns[0]));

static_assert((int)(sizeof(kNativeFns) / sizeof(kNativeFns[0])) == NATIVE_FN_COUNT,
              "native 表的条数和名单对不上");
static_assert((int)(sizeof(kGameFns) / sizeof(kGameFns[0])) == GAME_FN_COUNT,
              "game 表的条数和名单对不上");

// 池下标 → native 函数
//   段顺序 = 表顺序 → 一次减法；越界 / 不是 native 名 → NULL
Fn fn_of_sid(VM_INT sid) {
    if (sid < SID_NATIVE_BASE || sid >= SID_GAME_BASE) return NULL;
    VM_INT fid = sid - SID_NATIVE_BASE;
    if (fid < 0 || fid >= kNativeFnCount) return NULL;
    return kNativeFns[fid];
}

// ============================================================
// 已实做的 game API 换进表里
//   空壳（名单宏生成的那个）和实做（impl_*）是两个函数，这里按池下标算出 fid 换掉。
//   只有**真的写完了**的才列在这儿。
// ============================================================
static bool g_game_fns_patched = false;

void patch_game_fns() {
    if (g_game_fns_patched) return;
    g_game_fns_patched = true;
    kGameFns[S_VM_CallFunc   - SID_GAME_BASE] = impl_VM_CallFunc;
    kGameFns[S_VM_FuncExists - SID_GAME_BASE] = impl_VM_FuncExists;
    kGameFns[S_VM_FuncDesc   - SID_GAME_BASE] = impl_VM_FuncDesc;

    // ── 自举的那 46 条（game_self.cpp）──
    //   命名数组 9
    kGameFns[S_VM_ArrayGet      - SID_GAME_BASE] = impl_VM_ArrayGet;
    kGameFns[S_VM_ArraySet      - SID_GAME_BASE] = impl_VM_ArraySet;
    kGameFns[S_VM_ArrayDel      - SID_GAME_BASE] = impl_VM_ArrayDel;
    kGameFns[S_VM_ArrayADD      - SID_GAME_BASE] = impl_VM_ArrayADD;
    kGameFns[S_VM_ArraySize     - SID_GAME_BASE] = impl_VM_ArraySize;
    kGameFns[S_VM_ArrayClear    - SID_GAME_BASE] = impl_VM_ArrayClear;
    kGameFns[S_VM_ArrayClearAll - SID_GAME_BASE] = impl_VM_ArrayClearAll;
    kGameFns[S_VM_ArrayExists   - SID_GAME_BASE] = impl_VM_ArrayExists;
    kGameFns[S_VM_ArrayContains - SID_GAME_BASE] = impl_VM_ArrayContains;
    //   实例数组 8
    kGameFns[S_VM_InstArrayExists   - SID_GAME_BASE] = impl_VM_InstArrayExists;
    kGameFns[S_VM_InstArraySize     - SID_GAME_BASE] = impl_VM_InstArraySize;
    kGameFns[S_VM_InstArrayItem     - SID_GAME_BASE] = impl_VM_InstArrayItem;
    kGameFns[S_VM_InstArraySet      - SID_GAME_BASE] = impl_VM_InstArraySet;
    kGameFns[S_VM_InstArrayAdd      - SID_GAME_BASE] = impl_VM_InstArrayAdd;
    kGameFns[S_VM_InstArrayDel      - SID_GAME_BASE] = impl_VM_InstArrayDel;
    kGameFns[S_VM_InstArrayClear    - SID_GAME_BASE] = impl_VM_InstArrayClear;
    kGameFns[S_VM_InstArrayContains - SID_GAME_BASE] = impl_VM_InstArrayContains;
    //   实例 / 属性 8
    kGameFns[S_VM_GetProp          - SID_GAME_BASE] = impl_VM_GetProp;
    kGameFns[S_VM_SetProp          - SID_GAME_BASE] = impl_VM_SetProp;
    kGameFns[S_VM_IsUndefined      - SID_GAME_BASE] = impl_VM_IsUndefined;
    kGameFns[S_VM_IsDestroyed      - SID_GAME_BASE] = impl_VM_IsDestroyed;
    kGameFns[S_VM_GetInstanceCount - SID_GAME_BASE] = impl_VM_GetInstanceCount;
    kGameFns[S_VM_GetInstanceAt    - SID_GAME_BASE] = impl_VM_GetInstanceAt;
    kGameFns[S_VM_CreateInstance   - SID_GAME_BASE] = impl_VM_CreateInstance;
    kGameFns[S_VM_DestroyInstance  - SID_GAME_BASE] = impl_VM_DestroyInstance;
    //   杂项 4
    kGameFns[S_VM_Random  - SID_GAME_BASE] = impl_VM_Random;
    kGameFns[S_VM_Floor   - SID_GAME_BASE] = impl_VM_Floor;
    kGameFns[S_VM_Ceil    - SID_GAME_BASE] = impl_VM_Ceil;
    kGameFns[S_VM_RunStep - SID_GAME_BASE] = impl_VM_RunStep;
    //   全局读写 17
    kGameFns[S_VM_GetFlame             - SID_GAME_BASE] = impl_VM_GetFlame;
    kGameFns[S_VM_SetFlame             - SID_GAME_BASE] = impl_VM_SetFlame;
    kGameFns[S_VM_GetWave              - SID_GAME_BASE] = impl_VM_GetWave;
    kGameFns[S_VM_SetWave              - SID_GAME_BASE] = impl_VM_SetWave;
    kGameFns[S_VM_SetShovelFlameRate   - SID_GAME_BASE] = impl_VM_SetShovelFlameRate;
    kGameFns[S_VM_SetCardLevelCap      - SID_GAME_BASE] = impl_VM_SetCardLevelCap;
    kGameFns[S_VM_SetCardShapeCap      - SID_GAME_BASE] = impl_VM_SetCardShapeCap;
    kGameFns[S_VM_SetCardSkillCap      - SID_GAME_BASE] = impl_VM_SetCardSkillCap;
    kGameFns[S_VM_BanWeapon            - SID_GAME_BASE] = impl_VM_BanWeapon;
    kGameFns[S_VM_BanSuperWeapon       - SID_GAME_BASE] = impl_VM_BanSuperWeapon;
    kGameFns[S_VM_BanShield            - SID_GAME_BASE] = impl_VM_BanShield;
    kGameFns[S_VM_GetCurCard           - SID_GAME_BASE] = impl_VM_GetCurCard;
    kGameFns[S_VM_GetLastCreatedCard   - SID_GAME_BASE] = impl_VM_GetLastCreatedCard;
    kGameFns[S_VM_GetLastDestroyedCard - SID_GAME_BASE] = impl_VM_GetLastDestroyedCard;
    kGameFns[S_VM_GetLastCreatedEnemy  - SID_GAME_BASE] = impl_VM_GetLastCreatedEnemy;
    kGameFns[S_VM_GetLastKilledEnemy   - SID_GAME_BASE] = impl_VM_GetLastKilledEnemy;
    kGameFns[S_VM_GetLastClickedButton - SID_GAME_BASE] = impl_VM_GetLastClickedButton;

    // ── 剩下那 101 条（要引擎原语）分三批写，各自挂自己那批 ──
    patch_game_fns_a();
    patch_game_fns_b();
    patch_game_fns_c();
}

// 启动时调一次（幂等）。除了换 game 表项，顺手把按名字挂的那条内置函数（VM_HostPush）
//   登记进 DLL 函数表 —— **不能**放进 patch_game_fns()：那个函数会被下面的静态初始化对象调到，
//   而登记要碰字符串池（跨编译单元构造顺序未定义）；这里和上面的静态对象都是"运行期"路径。
//   入站包也在这儿挂：host_packet_set_handler(VM_ApplyPacket) —— 幂等（重复调就是把同一个
//   指针再赋一次），所以 outer_init() 被调几次都无所谓。**必须**在这儿挂、不能在静态初始化期挂：
//   引擎调 host_packet_apply 的时机由引擎定，VM 侧只能保证"启动过就一定挂好了"。
void outer_init() { patch_game_fns(); host_push_register_once(); host_packet_set_handler(VM_ApplyPacket); }

// 启动时自己换一遍：漏调 outer_init() 的话，已实现的 game API 会静默走空壳
//   （表是 POD 静态初始化，这里只写表项，没有初始化顺序问题）
static struct PatchGameFnsAtStart {
    PatchGameFnsAtStart() { patch_game_fns(); }
} g_patch_at_start;
