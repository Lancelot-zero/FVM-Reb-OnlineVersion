// ============================================================
// outer.h — 外层：所有函数（实现）的声明 + 分类
//
//   外层只往内层调（VM_RUN / 实例 / 池），内层不认识外层。
//
//   两类：
//     转发类   没有 C++ 实现的 native → 拼一次 VM_CallFunc(名字池下标, 实参…) 交出去
//     内置类   真的在 C++ 里实现的（自举：只用 VM 自己的数据）
//               剩下的**不能自举**（要游戏引擎 / 平台）→ 留出空壳，等宿主接
//
//   按"写哪个文件"分组（见 outer_split.h）：
//     forward.cpp    转发（79）              VM_CallFunc 那条路
//     container.cpp  容器（44）   [自举]     数组 / ds_list / ds_map / ds_grid / struct
//     inst_fn.cpp    实例（10）   [自举]     instance_* / variable_instance_*
//     dispatch.cpp   派发（3）    [自举]     run_block / run_event_user / VM_Execute
//     builtin.cpp    内置（42）   [自举]     数学 / 类型 / 字符串 / 全局 / 坐标 / 随机
//     host_fn.cpp    宿主（180）  [不自举]   文件 / 音频 / 相机 / 绘制 / 网络 / 游戏侧 —— 留出来
//     game_api.cpp   游戏 API（150）[不自举] VM_*（走 OP_CALL）—— 空壳留着等宿主（还 101 条）
//     game_self.cpp  游戏 API 里的自举那批（46）[自举]  数组 / 实例 / 属性 / 全局读写 —— 已实做
//     callbin_fn.cpp callbin 入口（VM_CallFunc / FuncExists / FuncDesc）—— 已实做
//     hot/           热块：一个对象一个文件的整块事件（还没开始）
// ============================================================
#pragma once
#include "../core/vm_def.h"
#include "outer_split.h"

// ============================================================
// 表里每一个名字都得有一个定义（少一个 = 链接报错，多一个 = 重复符号）
// ============================================================
#define OUTER_DECL(NAME) void NAME(Value* mem, const uint8_t* args, int argc, int dst);
NATIVE_FN_LIST(OUTER_DECL)
NATIVE_IMPL_LIST(OUTER_DECL)
GAME_FN_LIST(OUTER_DECL)
#undef OUTER_DECL

// ============================================================
// 机制
// ============================================================
// 转发：把实参原样交给 VM_CallFunc(名字, 实参…)
void native_forward(Value* mem, const uint8_t* args, int argc, int dst, VM_INT name_sid);

// 池下标 → 函数（native 段；不是 native 名 → NULL）
//   段顺序 = 表顺序（见 core/const_str.h），所以就是一次减法
Fn  fn_of_sid(VM_INT sid);

// 派发一个块：先问热块，没有再把字节码块交给 VM_RUN（定义在 dispatch.cpp）
bool run_block_on(vm_bin& vm, VM_INT obj_sid, VM_INT inst, VM_INT block_sid);

// 把"已经实做的 game API"换进表里（空壳 → impl_*），启动时调一次
void patch_game_fns();
void outer_init();                     // 幂等；业务层 / 测试开局调一次

// 已实做的 game API（表里那些空壳会被它们换掉）
void impl_VM_CallFunc  (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_FuncExists(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_FuncDesc  (Value* mem, const uint8_t* args, int argc, int dst);

// 自举的那 46 条（只用 VM 自己的数据，见 game_self.cpp）：
//   命名数组 9 / 实例数组 8 / 实例·属性 8 / 杂项 4 / 全局读写 17
void impl_VM_ArrayGet   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArraySet   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayDel   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayADD   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArraySize  (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayClear (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayClearAll(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayExists(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_ArrayContains(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayExists  (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArraySize    (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayItem    (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArraySet     (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayAdd     (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayDel     (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayClear   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_InstArrayContains(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetProp   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetProp   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_IsUndefined(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_IsDestroyed(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetInstanceCount(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetInstanceAt(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_CreateInstance(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_DestroyInstance(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_Random (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_Floor  (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_Ceil   (Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_RunStep(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetFlame(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetFlame(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetWave(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetWave(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetShovelFlameRate(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetCardLevelCap(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetCardShapeCap(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_SetCardSkillCap(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_BanWeapon(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_BanSuperWeapon(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_BanShield(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetCurCard(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetLastCreatedCard(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetLastDestroyedCard(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetLastCreatedEnemy(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetLastKilledEnemy(Value* mem, const uint8_t* args, int argc, int dst);
void impl_VM_GetLastClickedButton(Value* mem, const uint8_t* args, int argc, int dst);

// ── 出站通道（host_push.cpp）：**唯一**一条 VM → 宿主引擎的内置函数 ──
//   不在这两张 508 条表里，走"名字 → 实现"那条路挂（见 fn_dll.cpp 的注册口）：
//   GML 调 VM_HostPush(a, b, c) → 编成 VM_CallFunc("VM_HostPush", …) → 落到这里
//   语义：把实参按 host_push.cpp 里那份格式编码后**追加**到出站共享内存，回写入字节数
void VM_HostPush(Value* mem, const uint8_t* args, int argc, int dst);
// 登记 VM_HostPush（幂等；vm_find_dll_fn 第一次查表时会调，outer_init() 也调一次）
//   ⚠ 故意**不做**静态构造函数注册：那会碰 pool.cpp 的字符串池，而跨编译单元的
//     全局对象构造顺序未定义（理由写在 host_push.cpp 里）
void host_push_register_once();

// ── 入站通道（packet.cpp）：引擎 → VM 的**唯一**入口，和上面那条正好反方向 ──
//   引擎的链路：host_packet_apply(buf, size) → host/host_packet.cpp 转发 →
//               VM_ApplyPacket（outer_init() 里用 host_packet_set_handler 注册进来）
//   包格式 / 坏包判定 / 返回值约定见 packet.cpp 的文件头（那是唯一真源）
//     成功 → 0；坏包或失败 → -1，并 vm_set_err(VM_E_BAD_PACKET, 原因, 原始数值)
//   ⚠ 签名必须和 host.h 的 HostPacketFn 一模一样（int (*)(void*, int)）
int VM_ApplyPacket(void* buf, int size);

// 引擎造包用的两个口（包里的字符串值带的是**池下标**，引擎没法凭空造）：
//   VM_InternText  文本 → 池下标（没有就建）；参数不对 → -1
//   VM_SidOf       文本 → 池下标（**只查不建**）；没有 / 参数不对 → -1
//   ⚠ 跨 DLL 只过 POD（const char* → int）；返回值是 int，和池下标同宽
int VM_InternText(const char* utf8);
int VM_SidOf(const char* utf8);

// ============================================================
// "名字 → 实现" 的两张表 + 统一查找（fn_dll.cpp）
//   ① DLL 函数表（本文件这套注册口）  ② callbin 表（内层 g_call_dict）
//   查找顺序：先 DLL 表（命中**直通**：同一份 mem/args/dst，零搬运），再 callbin 表
// ============================================================
struct VMFuncDesc { const char* name; Fn fn; };

Fn   vm_find_dll_fn(VM_INT name_sid);                 // 没有 → NULL
void vm_call_by_name(VM_INT name_sid, Value* mem, const uint8_t* args, int argc, int dst);

// 注册口（给插件 DLL 用；名字在这儿 intern 一次，运行时零文本）
//   ⚠ 注册进来的函数和表里的 native/game **完全同形**：
//       void fn(Value* mem, const uint8_t* args, int argc, int dst)   // 实参=槽号，结果写 dst
//     —— 所以命中时不用包一层，直接调。
//   ⚠ 跨 DLL 只走 POD；别把 std::string / std::vector 放进签名。
int  VM_RegisterFunc (const char* name, Fn fn);       // 成功 1 / 参数不对 0
int  VM_RegisterFuncs(const VMFuncDesc* list, int count);
void VM_UnregisterFunc(const char* name);             // 插件卸载前调，别留悬空指针
int  VM_FuncApiVersion();                             // 版本不匹配就别注册

// inst_fn.cpp 里的两个辅助 + noone：game_self.cpp 也要用（**别再复制一份**）
//   inst_arg  ：第 k 个实参当"实例 id / 对象名"看（返回 2=对象名、1=实例 id、0=都不是）
//   inst_pick ："实例 id / 对象名" → 一个具体实例 id（对象名 = 名下第 0 个；没有 → -1）
//   NOONE     ：GML 的 noone(-4)；不能拿 inst_at 的 -1 顶替（mod 用 `!= noone` 判）
extern const VM_INT NOONE;
int   inst_arg (Value* mem, const uint8_t* args, int k, VM_INT* out_id, VM_INT* out_obj);
VM_INT inst_pick(VM_INT id, VM_INT obj, int kind);

// 游戏 API 的分批实现（剩下那 101 条要引擎原语的分三批写；每批一个文件）
//   各自把实现好的 impl_* 挂进 kGameFns，patch_game_fns() 会挨个调它们 ——
//   这样三批能分开写、互不踩文件（也不用各自往 fn_table.cpp 里塞行）
void patch_game_fns_a();     // game_a.cpp  批次 A：实例 / 战场 / 索敌 / 生成 / 上次事件
void patch_game_fns_b();     // game_b.cpp  批次 B：卡片 / 精灵·音效 / 波次·结算 / 输入
void patch_game_fns_c();     // game_c.cpp  批次 C：开关 / 横幅

// 外层自己的临时槽（拼转发参数 / 热块调函数用；在字面量段和局部段之后）
static const int OUTER_TMP_SLOT = SCRATCH_BASE + 32;   // +0..+8 用
static const int OUTER_TMP_N    = 9;

// ============================================================
// 热块（一个对象一个文件的整块事件）—— 还没开始写，先留缝
//   has_* ：这个对象的这个块有没有 C++ 硬编码版本
//   run_* ：跑它（返回 false = 没有，调用方回落到字节码）
// ============================================================
bool has_hot_block_sid(VM_INT obj_sid, VM_INT block_sid);
bool run_hot_block_sid(vm_bin& vm, VM_INT obj_sid, VM_INT block_sid);
bool run_hot_event_inherited(vm_bin& vm, VM_INT inst_id, VM_INT event_sid);
