// ============================================================
// vm_api.cpp — 游戏 API（OP_CALL 调用）
//   顺序 = 原 VM 的 FUNC_DEFS，不能改（gm2bin.py 按这个顺序算 id）
//   现在全是空壳，一个函数一个空体，直接在里面写就行
// ============================================================

#include "vm.h"

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
static const GameImpl kGameImpls[] = {
    { S_VM_CallFunc,   impl_VM_CallFunc   },
    { S_VM_FuncExists, impl_VM_FuncExists },
    { S_VM_FuncDesc,   impl_VM_FuncDesc   },
    { S_VM_GetProp,    impl_VM_GetProp    },
    { S_VM_SetProp,    impl_VM_SetProp    },
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
