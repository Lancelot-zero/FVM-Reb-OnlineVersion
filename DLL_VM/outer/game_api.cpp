// ============================================================
// game_api.cpp — 游戏 API（150 条，走 OP_CALL）[大多**不能自举**]
//
//   ★★ 这个文件里**没有实现**，只有空壳 ★★
//
//   实现写在：
//     outer/game_self.cpp    46 条  impl_VM_*        （自举那批，L112~L560）
//     outer/callbin_fn.cpp    3 条  impl_VM_CallFunc / FuncExists / FuncDesc
//   接上的方式：
//     outer/fn_table.cpp 的 patch_game_fns()（L54~L113）把 kGameFns[] 里那 49 个表项
//     从"空壳"换成"impl_*"，**启动时自动跑一遍**（L118 的静态对象；outer_init() 也有一份）。
//   于是：
//     · 调用链是 OP_CALL → kGameFns[fid] → 直接进 game_self.cpp / callbin_fn.cpp 的实现
//     · 本文件里对应的那 **49 条空壳是死代码**，永远调不到 —— **别改它们**
//   要加实现：
//     写一个 impl_XXX(Value* mem, const uint8_t* args, int argc, int dst) 放到
//     game_self.cpp（或自己的新文件），再到 fn_table.cpp 的 patch_game_fns() 里挂一行。
//
//   这批就是"VM_*"：跟游戏引擎打交道（刷怪 / 卡片 / 绘制 / 地形 / 音效 / 结算…）。
//   剩下 101 条要宿主，等宿主那批定完再逐条接。
//
// ── 按"要什么"分组（150 条）──────────────────────────────────
//   给人看的分类：表里的顺序和条数由 const_str.h 的 GAME_FN_LIST 说了算，
//   这里少写 / 写错一条都不影响编译（名单才是唯一真源）。
//   分组由 tools/gen_game_comment.py 从名单生成，**别手改这一块**。
// >>> GAME_GROUPS_BEGIN
//   刷怪/生成 10 条 VM_CreatePlatform VM_SpawnPlant VM_SpawnEnemy VM_SpawnBoss VM_SpawnObject VM_SpawnCats 
//                   VM_SpawnPlantsRandom VM_CreateButton VM_SpawnBatMouse VM_HomingBulletAdd
//   卡片/卡组 12 条 VM_BanCard VM_SetCardProp VM_ApplyPlantLevel VM_SetCardSlotProp VM_CalcCardSlotProp 
//                   VM_GetCardSlotCount VM_GetPreviewCard VM_conveyor_belt_able VM_Slot_add VM_BanAllCard 
//                   VM_CannelBanCard VM_GetCardProp
//   精灵/音效 14 条 VM_LoadSprite VM_SetMapBackground VM_GetLoadedSpriteName VM_PlaySound VM_LoadSpriteFrames 
//                   VM_LoadSpritePerm VM_FreeSpritePerm VM_AliasSprite VM_LoadSpriteFrames_Ex VM_LoadSound 
//                   VM_LoadSpritePerm_Ex VM_DrawSpriteExt VM_SpriteExists VM_AliasSpritePerm
//   战场/地形 22 条 VM_SetTerrain VM_ClearPlants VM_ClearMapObjects VM_SetRowFeature VM_ClearPlantsByType 
//                   VM_WakePlants VM_GetTerrain VM_GetEnemyCount VM_GetPlantCount VM_GetPlantCountAt VM_GetPlantAt 
//                   VM_SwapPlants VM_SwapPlantRects VM_CompactColumn VM_CompactRow VM_CompactColumnRev 
//                   VM_CompactRowRev VM_CellCount VM_CellItem VM_CellContains VM_CatInRow VM_MapObj
//   波次/结算 13 条 VM_GetSubwave VM_GameWin VM_GameLose VM_GetKilledProp VM_GetTimeLimit VM_SetTimeLimit 
//                   VM_SetWaveAuto VM_BulletScreenAdd VM_DamageEnemy VM_DamageEnemyAsh VM_BulletScreenAdd_Ex 
//                   VM_BulletScreenAdd_Exs VM_GetInfo
//   开关/横幅 13 条 VM_SetMaxSlots VM_ShellPrint VM_ShowNotice VM_SetPlatformParams VM_SetDrawSlot 
//                   VM_SetDrawSlot_front VM_BanGem VM_ShowNoticeDur VM_SetEventEnabled VM_RefreshPlatformSnapshots 
//                   VM_SetNoticeStyle VM_SetDrawSlotEx VM_SetDrawSlotEx_front
//   上次事件 5 条   VM_GetLastBoss VM_GetLastIdlePlatform VM_GetLastBossStateChangeId VM_GetLastBossOldState 
//                   VM_GetLastBossNewState
//   范围/索敌 3 条  VM_EnemyInRange VM_GetHomingTarget VM_GetInstancesInRange
//   输入/鼠标 7 条  VM_GetMouseX VM_GetMouseY VM_GetMouseCol VM_GetMouseRow VM_GetMousePressed VM_GetKeyDown 
//                   VM_GetKeyPressed
//   其余 2 条       VM_SetEnemyProp VM_CanPlace
//
//   自举  49 条     VM_SetCardLevelCap VM_SetProp VM_GetWave VM_GetProp VM_GetLastCreatedEnemy VM_GetLastKilledEnemy 
//                   VM_GetLastCreatedCard VM_GetLastDestroyedCard VM_GetFlame VM_SetFlame VM_Random 
//                   VM_GetLastClickedButton VM_ArrayGet VM_ArraySet VM_ArrayDel VM_ArrayADD VM_ArraySize 
//                   VM_ArrayClear VM_ArrayClearAll VM_BanWeapon VM_BanSuperWeapon VM_BanShield VM_SetCardShapeCap 
//                   VM_SetCardSkillCap VM_IsUndefined VM_IsDestroyed VM_Floor VM_Ceil VM_SetWave VM_GetCurCard 
//                   VM_CreateInstance VM_SetShovelFlameRate VM_RunStep VM_DestroyInstance VM_CallFunc VM_FuncExists 
//                   VM_FuncDesc VM_ArrayExists VM_ArrayContains VM_InstArrayExists VM_InstArraySize VM_InstArrayItem 
//                   VM_InstArraySet VM_InstArrayAdd VM_InstArrayDel VM_InstArrayClear VM_InstArrayContains 
//                   VM_GetInstanceCount VM_GetInstanceAt
//   （这一批**已经实现**：impl_VM_* 在 outer/game_self.cpp（46 条）+ outer/callbin_fn.cpp（3 条），
//     启动时由 outer/fn_table.cpp 的 patch_game_fns() 把表项换过去 —— 本文件里对应的空壳是死代码）
// >>> GAME_GROUPS_END
//
//   ⚠ 这个文件里**看不到已实现的那些**：它只生成 150 条空壳（一调到就置 VM_E_NOT_IMPL）。
//     已实现的（自举那 49 条）在别处：
//         outer/game_self.cpp    46 条  impl_VM_*（命名数组 / 实例数组 / 属性 / 杂项 / 全局读写）
//         outer/callbin_fn.cpp    3 条  impl_VM_CallFunc / FuncExists / FuncDesc
//     outer/fn_table.cpp 的 patch_game_fns() 在**启动时**把这 49 个表项从空壳换成 impl_*
//     （kGameFns[fid] 指向谁就是谁）→ 所以这个文件里那 49 条空壳是**死代码**，别去改它们。
//     要加实现：写 impl_XXX 到 game_self.cpp（或自己的文件），再到 patch_game_fns() 挂一行。
// ============================================================
#include "outer.h"

// 空壳：还没实现的（= 要宿主的）走这儿。已实现的启动时会被换掉，见上面那段。
#define STUB(NAME) \
    void NAME(Value* mem, const uint8_t* args, int argc, int dst) { \
        (void)args; (void)argc; \
        vm_set_err(VM_E_NOT_IMPL, S_##NAME, 0); \
        wr_v(mem, dst, mk_undef()); \
    }
GAME_FN_LIST(STUB)
#undef STUB
