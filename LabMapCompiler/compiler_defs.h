#pragma once
#include <string>
#include <vector>
#include <cstring>

// ============================================================
// 操作码 — 和 VM 端保持一致
// ============================================================
enum Opcode {
    OP_ASSIGN   = 1,   // dst(u32) type(u8) value(s32/u16)
    OP_COPY     = 2,   // dst(u32) src(u32)
    OP_ADD      = 3,   // dst(u32) a(u32) b(u32)
    OP_SUB      = 4,
    OP_MUL      = 5,
    OP_DIV      = 6,
    OP_MOD      = 7,
    OP_EQ       = 8,
    OP_NEQ      = 9,
    OP_GT       = 10,
    OP_GTE      = 11,
    OP_LT       = 12,
    OP_LTE      = 13,
    OP_CALL     = 14,  // func_id(u16) arg_count(u8) dst(s32) [addr(s32)*]
    OP_IF       = 15,  // cond_addr(u32) true_ip(s32) false_ip(s32)
    OP_JMP      = 16,
    OP_HALT     = 17,
};

// 内存类型
enum MemType {
    MEM_INT    = 0,
    MEM_FLOAT  = 1,
    MEM_STRING = 2,
};

// 参数类型（值与 MEM_* 对齐，方便直接比较）
enum ParamType {
    PT_INT    = 0,
    PT_FLOAT  = 1,
    PT_STRING = 2,
    PT_ANY    = 3,  // 接受任意类型（变长 or SetProp 第3参）
};

inline const char* param_type_name(int t) {
    switch (t) {
        case PT_INT:    return "int";
        case PT_FLOAT:  return "float";
        case PT_STRING: return "string";
        default:        return "?";
    }
}

// CALL dst 特殊值
static const int VOID_DST = -1;

// ============================================================
// 函数定义（仅游戏 API，算术已内联为操作码）
// ============================================================
struct FuncDef {
    const char* name;
    int param_count;                    // -1=varargs, >=0=fixed count
    std::vector<int> param_types;       // empty=all default INT
    int return_type;                    // PT_INT(default) / PT_STRING / PT_FLOAT
};

// ============================================================
// 块名表
// ============================================================
static const std::vector<const char*> BLOCK_NAMES = {
    "_VM_ROOM_READY_ENTRY",
    "_VM_BATTLE_START",
    "_VM_CARD_CREATED",
    "_VM_CARD_DESTROYED",
    "_VM_CARD_DAMAGED",
    "_VM_ENEMY_SPAWNED",
    "_VM_ENEMY_KILLED",
    "_VM_ENEMY_DAMAGED",
    "_VM_WAVE_START",
    "_VM_WAVE_END",
    "_VM_SUBWAVE_START",
    "_VM_SUBWAVE_END",
    "_VM_PLAYER_DAMAGED",
    "_VM_PLATFORM_IDLE_END",
    "_VM_MOUSE_LEFT",
    "_VM_MOUSE_RIGHT",
    "_VM_KEY_PRESSED",
    "_VM_FRAME",
    "_VM_TIMER_5f",
    "_VM_TIMER_10f",
    "_VM_TIMER_15f",
    "_VM_TIMER_30f",
    "_VM_TIMER_60f",
    "_VM_BUTTON_CLICKED",
    "_VM_CARD_PREVIEW_PICKED",
    "_VM_BOSS_STATE_CHANGE",
    "_OBJECT_CFG",
    "_OBJECT_CREATE",
    "_OBJECT_STEP",
    "_OBJECT_DRAW",
    "_OBJECT_DESTROY",
    "_OBJECT_MOUSE_ENTER",
    "_OBJECT_MOUSE_LEAVE",
    "_OBJECT_CLICK",
};

// ============================================================
// 字符串参数合法值集合 — 和游戏数据同步维护
// ============================================================

// 敌人 ID（VM_SpawnEnemy / VM_SpawnBoss）
static const std::vector<const char*> VALID_ENEMY_IDS = {
    "abyss_pharaoh","airbrone_explosive_mouse","aircraft_carrier",
    "angelababy","apple_duck_mouse","apple_football_fan_mouse","arno",
    "arson_mouse","assault_mouse","bat_mouse","blonde_mary",
    "butterfly_mouse","can_mouse","captain_america_mouse",
    "caribbean_mouse","charge_spring_mouse","clownfish_mouse",
    "conch_mouse","cucumber_normal_mouse","cucumber_paper_boat_mouse",
    "dentist_mouse","diver_mouse","dragon_boat_mouse","duck_mouse",
    "eel_mouse","egg_iron_pan_mouse","egg_tropical_fish_mouse",
    "electric_jellyfish","engineering_vehicle_mouse","flagship_mouse",
    "flight_barrier_mouse","flute_mouse","fog_julie",
    "football_fan_mouse","frog_prince_mouse","garbage_track_mouse",
    "ghost_mouse","giant_mouse","glider_mouse","hazelnut_cannon_mouse",
    "hells_messenger","hercules","hot_vajra","huang_xiaoming",
    "hulk_mouse","ice_residue","iron_diver_mouse","iron_man_mouse",
    "iron_pan_mouse","irritable_jack","jet_mouse",
    "kamikaze_glider_mouse","kangaroo","kof_submarine_mouse",
    "lambo_mouse","landlady_mouse","landmine_vehicle_mouse",
    "lieutenant_buzz","lobster_knight","machine_bee",
    "machine_beehive_mouse","machine_bomb_mouse","machine_flag_mouse",
    "machine_football_fan_mouse","machine_iron_pan_mouse",
    "machine_mouse","machine_normal_mouse","machine_shark_1",
    "machine_shark_2","machine_skateboard_mouse","magician_mouse",
    "mario_mouse","mermaid_mary","minion_mouse","mirror_mouse","mole",
    "mouse_train_1","mouse_train_2","mouse_train_3","naruto_mouse",
    "needle_baron","ninja_mouse","non_mainstream_mouse","normal_mouse",
    "orange_prince_mouse","oyster_mouse","panda_mouse",
    "paper_boat_mouse","paratrooper_mouse","penguin_mouse","pete",
    "pink_paul","pope_mouse","priest_mouse","repairman_mouse",
    "roller_skating_mouse","rowboat_mouse","rumble","sardine_mouse",
    "sawblade_mouse","seahorse_mouse","shy_landlady_mouse",
    "skateboard_mouse","snail_mouse","soar_mouse","soldier_mouse",
    "special_armour_mouse","spider_man_mouse","submarine_mouse",
    "swordfish_mouse","tangerine_skateboard_mouse","taro_toho_mouse",
    "temple_pharaoh","thor","thug_submarine_mouse",
    "tropical_fish_mouse","trumpeter_mouse","undersea_can_mouse",
    "undersea_captain_mouse","undersea_diver_mouse",
    "undersea_panda_mouse","undersea_penguin_mouse",
    "undersea_repairman_mouse","undersea_submarine_1",
    "undersea_submarine_2","war_god","warrior_mouse",
    "waste_flying_mouse","water_penguin_mouse","water_taro_toho_mouse",
    "windmill_fish_mouse","wrestler_mouse","zombie_with_flower_pot",
    "zombie_with_wallnut"
};

// 卡片 ID（VM_SpawnPlant / VM_BanCard）
static const std::vector<const char*> VALID_CARD_IDS = {
    "aquarius_elve","beef_hotpot","brazier","bull_firework","cat_box",
    "cat_chest","cherry_pudding","chili_powder","chocolate_bread",
    "chocolate_cannon","chocolate_pult","coal_starfish","coffee_cup",
    "coffee_grounds","coffee_pot","coke_bomb","cotton_candy",
    "curry_lobster_cannon","delicacy_firework","double_ice_long_bao",
    "double_long_bao","double_water_pipe","dragon_fruit","durian",
    "egg_boiler_pult","firework_dragon","fishbone","flour_sack",
    "fruit_tart","gatlin_ice_long_bao","gatlin_long_bao","goblet_lamp",
    "hamburger","horseshoe_crab_bread","hotdog_cannon",
    "ice_bucket_bomb","ice_cream","ice_egg_boiler_pult","ice_long_bao",
    "iron_fishbone","kettle_bomb","king_long_bao",
    "king_triple_long_bao","large_fire","lightning_baguette",
    "magic_chicken","melon_shield","mouse_clip","oden_pot","oil_lamp",
    "pan_fried_bun","pineapple_explosive_bread","pizza_oven",
    "rabbit_lantern","rotating_coffee_pot","salad_pult","sausage",
    "skewer_bomb","small_fire","soda_bubble","spicy_pot","steel_wool",
    "stinky_tofu_pult","sugar_ball_pult","takoyaki","tang_hu_lu",
    "tar_sprayer","toast_bread","triple_ice_long_bao","triple_long_bao",
    "triple_wine_rack","ventilation_fan","water_tea_cup","whisky_bomb",
    "wine_bottle_bomb","wooden_cork","wooden_plate","xiao_long_bao",
    "xinjiang_fried_noodles"
};

// 物件名（VM_SpawnObject / VM_ClearMapObjects）
static const std::vector<const char*> VALID_OBJECT_NAMES = {
    // 基础物件
    "obstacle","lava","seawater","wind_tunnel","mouse_hole",
    "pharaoh_hole","buzz_wind","cloud","ladder","barrier","fog",
    // boss 产物 — 子弹/弹道
    "arno_bullet","arno_bullet_effect","arson_bullet","blonde_mary_bullet",
    "electric_jellyfish_bullet","hercules_laser","ice_residue_ball","ice_residue_bullet",
    "iron_man_bullet","lobster_knight_bullet","mermaid_mary_bullet","paul_bullet",
    "rumble_laser","rumble_missile","shark_1_bullet","mouse_train_1_bullet",
    "mouse_train_2_bullet","julie_missile","pete_missile","pete_claw","pete_spike",
    "baron_bats","baron_blade","baron_needle","messenger_ignis_fatuus","messenger_mace",
    "messenger_poop","machine_shark_2_wind",
    // boss 产物 — 召唤物/技能
    "angelababy_star","angelababy_summon","angelababy_target","arson_mouse",
    "captain_rainbow","captain_shield","iron_man",
    "irritable_jack_fire","irritable_jack_rock_skill_3","irritable_jack_rock_skill_4",
    "mario_cave","mario_pipeline","mermaid_mary_music","mermaid_mary_wave",
    "mouse_train_3_butter","mouse_train_3_explode","pharaoh_bandage","pharaoh_coffin",
    "spider_man_mouse_web","vajra_lava","vajra_lightning","vajra_spike",
    "war_god_duck","war_god_summon","war_god_wood","xiaoming_text",
};

/// 检查字符串是否在集合中
inline bool str_in_set(const std::string& s, const std::vector<const char*>& set) {
    for (auto& v : set) if (s == v) return true;
    return false;
}

// ============================================================
// 函数名表 — 顺序必须和 VM 端注册顺序一致
// ============================================================
static std::vector<FuncDef> FUNC_DEFS = {
    {"VM_BanCard",        1,  {PT_STRING}},                                          // 0
    {"VM_SetCardLevelCap",1},                                                        // 1
    {"VM_SetMaxSlots",    1},                                                        // 2
    {"VM_ShellPrint",    -1},                                                        // 3 — 变长
    {"VM_ShowNotice",    -1},                                                        // 4 — 变长
    {"VM_CreatePlatform", 8,  {PT_INT,PT_INT,PT_INT,PT_INT,PT_INT,PT_INT,PT_INT,PT_STRING}}, // 5
    {"VM_SpawnPlant",    6,  {PT_STRING,PT_INT,PT_INT,PT_INT,PT_INT,PT_INT}},       // 6
    {"VM_SpawnEnemy",    3,  {PT_STRING,PT_INT,PT_INT}},                             // 7
    {"VM_SpawnBoss",     3,  {PT_STRING,PT_INT,PT_INT}},                             // 8
    {"VM_LoadSprite",    1,  {PT_STRING}},                                           // 9
    {"VM_SpawnObject",   3,  {PT_STRING,PT_INT,PT_INT}},                             // 10
    {"VM_SetProp",       3,  {PT_INT,PT_STRING,PT_ANY}},                             // 11
    {"VM_GetWave",             0},                                                   // 12
    {"VM_GetSubwave",          0},                                                   // 13
    {"VM_GetProp",             2,  {PT_INT,PT_STRING},                     PT_ANY},  // 14 — 返回类型取决于属性名
    {"VM_GetLastBoss",         0},                                                   // 15
    {"VM_GetLastCreatedEnemy", 0},                                                   // 16
    {"VM_GetLastKilledEnemy",  0},                                                   // 17
    {"VM_GetLastCreatedCard",  0},                                                   // 18
    {"VM_GetLastDestroyedCard",0},                                                   // 19
    {"VM_GetFlame",          0},                                                     // 20
    {"VM_SetFlame",          1},                                                     // 21
    {"VM_SetTerrain",        3,  {PT_INT,PT_INT,PT_STRING}},                         // 22
    {"VM_ClearPlants",       2},                                                     // 23
    {"VM_Random",            2},                                                     // 24
    {"VM_GetLastIdlePlatform",0},                                                    // 25
    {"VM_SetPlatformParams", 5},                                                     // 26
    {"VM_SetMapBackground", 2,  {PT_STRING, PT_FLOAT}},                             // 27
    {"VM_SetDrawSlot",    5,  {PT_INT, PT_STRING, PT_INT, PT_INT, PT_FLOAT}},      // 28
    {"VM_GetLoadedSpriteName", 1,  {PT_INT},  PT_STRING},                            // 29
    {"VM_GameWin",               0},                                                // 30
    {"VM_GameLose",              0},                                                // 31
    {"VM_SetDrawSlot_front",     5,  {PT_INT, PT_STRING, PT_INT, PT_INT, PT_FLOAT}},  // 32
    {"VM_SpawnCats",             1},                                                // 33
    {"VM_ClearMapObjects",      3,  {PT_INT, PT_INT, PT_STRING}},                  // 34
    {"VM_BanGem",               1,  {PT_STRING}},                                   // 35
    {"VM_SetRowFeature",        2,  {PT_INT, PT_STRING}},                           // 36
    {"VM_ClearPlantsByType",    1,  {PT_STRING}},                                   // 37
    {"VM_WakePlants",           2,  {PT_INT, PT_INT}},                              // 38
    {"VM_SetCardProp",          5,  {PT_INT, PT_INT, PT_STRING, PT_STRING, PT_ANY}},// 39
    {"VM_SetEnemyProp",         3,  {PT_STRING, PT_STRING, PT_ANY}},               // 40
    {"VM_ShowNoticeDur",       -1},                                                  // 41
    {"VM_SetEventEnabled",    1},                                                     // 42
    {"VM_RefreshPlatformSnapshots", 0},                                               // 43
    {"VM_GetMouseX",             0,  {},  PT_INT},                                   // 44
    {"VM_GetMouseY",             0,  {},  PT_INT},                                   // 45
    {"VM_GetMouseCol",           0,  {},  PT_INT},                                   // 46
    {"VM_GetMouseRow",           0,  {},  PT_INT},                                   // 47
    {"VM_GetTerrain",            2},                                                  // 48
    {"VM_GetMousePressed",       1},                                                  // 49
    {"VM_GetKeyDown",            1,  {PT_STRING}},                                      // 50
    {"VM_GetKeyPressed",         1,  {PT_STRING}},                                      // 51
    {"VM_GetEnemyCount",         0},                                                  // 52
    {"VM_GetPlantCount",         0},                                                  // 53
    {"VM_GetPlantCountAt",       3,  {PT_INT, PT_INT, PT_STRING}},                    // 54
    {"VM_PlaySound",             1,  {PT_STRING}},                                    // 55
    {"VM_GetPlantAt",            3,  {PT_INT, PT_INT, PT_STRING}},                   // 56
    {"VM_SwapPlants",            4,  {PT_INT, PT_INT, PT_INT, PT_INT}},              // 57
    {"VM_SwapPlantRects",        6,  {PT_INT, PT_INT, PT_INT, PT_INT, PT_INT, PT_INT}}, // 58
    {"VM_CompactColumn",         1,  {PT_INT}},                                         // 59
    {"VM_CompactRow",            1,  {PT_INT}},                                         // 60
    {"VM_CompactColumnRev",      1,  {PT_INT}},                                         // 61
    {"VM_CompactRowRev",         1,  {PT_INT}},                                         // 62
    {"VM_SpawnPlantsRandom",    16, {PT_INT,PT_INT,PT_INT,PT_INT, PT_INT,PT_INT,PT_INT, PT_STRING,PT_STRING,PT_STRING,PT_STRING,PT_STRING,PT_STRING,PT_STRING,PT_STRING,PT_STRING}}, // 63
    {"VM_CreateButton",          7,  {PT_INT, PT_INT, PT_STRING, PT_FLOAT, PT_INT, PT_INT, PT_INT}}, // 64
    {"VM_GetLastClickedButton",  0},                                                    // 65
    {"VM_LoadSpriteFrames",      2,  {PT_STRING, PT_INT}},                             // 66
    {"VM_ApplyPlantLevel",       1,  {PT_INT}},                                         // 67
    {"VM_LoadSpritePerm",        2,  {PT_STRING, PT_INT}},                             // 68
    {"VM_FreeSpritePerm",        1,  {PT_STRING}},                                      // 69
    {"VM_SetCardSlotProp",       3,  {PT_STRING, PT_STRING, PT_ANY}},                  // 70
    {"VM_CalcCardSlotProp",      4,  {PT_STRING, PT_STRING, PT_INT, PT_ANY}},         // 71
    {"VM_GetCardSlotCount",      0},                                                    // 72
    {"VM_GetPreviewCard",        0,  {},  PT_STRING},                                   // 73 — 返回 PT_STRING 或 -1
    {"VM_AliasSprite",           2,  {PT_STRING, PT_STRING}},                           // 74
    {"VM_LoadSpriteFrames_Ex",   4,  {PT_STRING, PT_INT, PT_INT, PT_INT}},             // 75
    {"VM_GetLastBossStateChangeId", 0},                                                 // 76
    {"VM_GetLastBossOldState",      0},                                                 // 77
    {"VM_GetLastBossNewState",      0},                                                 // 78
    {"VM_ArrayGet",                 2,  {PT_STRING, PT_INT},                 PT_ANY},  // 79 — 返回类型取决于存的元素
    {"VM_ArraySet",                 3,  {PT_STRING, PT_INT, PT_ANY}},                   // 80
    {"VM_ArrayDel",                 2,  {PT_STRING, PT_INT}},                           // 81
    {"VM_ArrayADD",                 2,  {PT_STRING, PT_ANY}},                           // 82
    {"VM_ArraySize",                1,  {PT_STRING},                       PT_INT},     // 83
    {"VM_ArrayClear",               1,  {PT_STRING}},                                   // 84
    {"VM_ArrayClearAll",            0},                                                 // 85
    {"VM_SetNoticeStyle",           4,  {PT_FLOAT, PT_INT, PT_INT, PT_INT}},           // 86
    {"VM_conveyor_belt_able",       1,  {PT_INT}},                                      // 87
    {"VM_Slot_add",                 1,  {PT_STRING}},                                   // 88
    {"VM_BanAllCard",               0},                                                 // 89
    {"VM_CannelBanCard",            1,  {PT_STRING}},                                   // 90
    {"VM_BanWeapon",                0},                                                 // 91
    {"VM_BanSuperWeapon",           0},                                                 // 92
    {"VM_BanShield",                0},                                                 // 93
    {"VM_SetCardShapeCap",          1},                                                 // 94
    {"VM_SetCardSkillCap",          1},                                                 // 95
    {"VM_GetKilledProp",            1,  {PT_STRING},                        PT_ANY},      // 96 — 当前销毁事件对象属性
    {"VM_IsUndefined",              1,  {PT_ANY},                           PT_INT},      // 97 — 判断值是否为 undefined
    {"VM_IsDestroyed",              1,  {PT_INT},                           PT_INT},      // 98 — 判断实例ID是否已被销毁
    {"VM_LoadSound",                1,  {PT_STRING},                        PT_INT},      // 99 — 加载本地音频文件，返回音频ID
    {"VM_Floor",                    1,  {PT_ANY},                           PT_ANY},      // 100 — 向下取整，非数字返回 undefined
    {"VM_Ceil",                     1,  {PT_ANY},                           PT_ANY},      // 101 — 向上取整，非数字返回 undefined
    {"VM_SpawnBatMouse",            2,  {PT_INT, PT_INT},                   PT_INT},      // 102 — 在指定格子生成蝙蝠鼠
    {"VM_GetTimeLimit",             0,  {},                                  PT_ANY},      // 103 — 获取关卡倒计时（帧），无倒计时返回 undefined
    {"VM_SetTimeLimit",             1,  {PT_INT},                            PT_ANY},      // 104 — 设置关卡倒计时（帧），无倒计时返回 undefined
    {"VM_SetDrawSlotEx",            8,  {PT_INT, PT_STRING, PT_INT, PT_INT, PT_FLOAT, PT_FLOAT, PT_FLOAT, PT_FLOAT}}, // 105
    {"VM_SetDrawSlotEx_front",      8,  {PT_INT, PT_STRING, PT_INT, PT_INT, PT_FLOAT, PT_FLOAT, PT_FLOAT, PT_FLOAT}}, // 106
    {"VM_SetWaveAuto",              1,  {PT_INT}},                                                        // 107
    {"VM_SetWave",                  2,  {PT_INT, PT_INT}},                                                 // 108
    {"VM_GetCurCard",              0,  {},                                              PT_INT},          // 109 — 当前块所属的 mod 卡实例 id
    {"VM_EnemyInRange",            5,  {PT_INT, PT_INT, PT_INT, PT_INT, PT_STRING},     PT_INT},          // 110 — 矩形范围是否存在敌人（前缀和，type: normal/obstacle/diver/air/dance/underground/all）
    {"VM_GetHomingTarget",         1,  {PT_STRING},                                     PT_INT},          // 111 — 最左且血量最高的敌人 id（追踪索敌，param: 敌人类型，""/all=任意；每帧全场只扫一次）
    {"VM_GetInstancesInRange",     7,  {PT_STRING, PT_INT, PT_INT, PT_INT, PT_INT, PT_STRING, PT_STRING}, PT_INT}, // 112 — 收集范围内敌人/卡片到 VM 数组（卡片可按 plant_id/plant_type 筛）
    {"VM_CreateInstance",          3,  {PT_STRING, PT_FLOAT, PT_FLOAT},                 PT_INT},          // 113 — 按像素坐标创建实例（子弹等）
    {"VM_LoadSpritePerm_Ex",       4,  {PT_STRING, PT_INT, PT_INT, PT_INT},             PT_INT},          // 114 — 永久缓存贴图带原点，不被 bin 重载清理
    {"VM_SetShovelFlameRate",      1,  {PT_FLOAT},                                       PT_ANY},          // 115 — 铲子返还火苗系数（-1=原逻辑，0~1=直接用）
    {"VM_BulletScreenAdd",        15,  {PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY}, PT_INT}, // 116 — 屏幕弹幕管理器：贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字
    {"VM_DrawSpriteExt",           8,  {PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY}, PT_INT}, // 117 — 直接 draw_sprite_ext（贴图,子图,x,y,xscale,yscale,rot,alpha；固定 c_white），只能在 _OBJECT_DRAW 里用
    {"VM_RunStep",                 2,  {PT_INT, PT_INT}, PT_INT}, // 118 — 让实例额外跑 N 轮 Step（实例,轮数），一轮 = Begin Step + Step + End Step，做加速用
    {"VM_DestroyInstance",         1,  {PT_INT}, PT_INT}, // 119 — 直接销毁一个实例（会触发它的 Destroy 事件）
    {"VM_GetCardProp",             2,  {PT_STRING, PT_STRING}, PT_ANY}, // 120 — 按属性名读一张卡的单值（返回类型取决于属性名）：存档类 shape/level/skill/max_level/max_shape，卡池类 plant_type/feature_type/target_card/cost/cooldown；失败返回 undefined
    {"VM_CanPlace",                3,  {PT_STRING, PT_INT, PT_INT}}, // 121 — 按游戏正规种植规则判断某格能不能种这张卡（地形/障碍/水域莲叶/护盾层/底座卡/替换开关全都算），1=能 0=不能
    {"VM_CallFunc",               -1}, // 122 — 变长：按名字调用独立字典 global._VM_call_dict 里的函数；名字对编译器只是字符串，不校验。第一个参数是函数名，后面是实参
    {"VM_FuncExists",              1,  {PT_STRING}}, // 123 — 字典里有没有这个函数，1=有 0=没有（VM_CallFunc 的配套）
    {"VM_FuncDesc",                1,  {PT_STRING}, PT_STRING}, // 124 — 返回字典里登记的该函数说明字符串，没有返回 ""（VM_CallFunc 的配套）
    {"VM_SpriteExists",            1,  {PT_STRING}}, // 125 — 贴图现在真的可用吗（项目资源→VM临时缓存→VM永久缓存→全局缓存，并排除 get_load_sprite 的空白占位图），1=可用 0=不可用
    {"VM_AliasSpritePerm",         2,  {PT_STRING, PT_STRING}}, // 126 — 把名字永久指向一个已在永久缓存里的贴图（存精灵 id，进房间不会被清；[reloadmod] 时释放），配合 VM_SpriteExists 做「内置有就用内置、没有才外置覆盖」
    {"VM_ArrayExists",             1,  {PT_STRING}}, // 127 — 全局二维表是否存在（存在且是数组）1/0
    {"VM_CellCount",               3,  {PT_STRING, PT_INT, PT_INT}}, // 128 — 表里第 i 列 j 行那格的元素个数；表不存在/非数组/i,j 越界 → -1
    {"VM_CellItem",                4,  {PT_STRING, PT_INT, PT_INT, PT_INT}}, // 129 — 表里第 i 列 j 行那格的第 k 个元素（k 从 0 起）；越界 → -1
    {"VM_CellContains",            4,  {PT_STRING, PT_INT, PT_INT, PT_ANY}}, // 130 — 表里第 i 列 j 行那格有没有这个值，1/0；越界 → -1
    {"VM_ArrayContains",           2,  {PT_STRING, PT_ANY}}, // 131 — 整张表里有没有这个值，1/0；表不存在 → -1
    {"VM_InstArrayExists",         2,  {PT_INT, PT_STRING}}, // 132 — 实例身上有没有这个一维数组，1/0
    {"VM_InstArraySize",           2,  {PT_INT, PT_STRING}, PT_INT}, // 133 — 实例身上一维数组的长度；实例/变量不存在 → -1
    {"VM_InstArrayItem",           3,  {PT_INT, PT_STRING, PT_INT}, PT_ANY}, // 134 — 实例身上一维数组的第 k 个元素；越界 → -1
    {"VM_InstArraySet",            4,  {PT_INT, PT_STRING, PT_INT, PT_ANY}}, // 135 — 改实例身上一维数组的第 k 个元素；失败 → -1
    {"VM_InstArrayAdd",            3,  {PT_INT, PT_STRING, PT_ANY}}, // 136 — 往实例身上一维数组末尾追加；失败 → -1
    {"VM_InstArrayDel",            3,  {PT_INT, PT_STRING, PT_INT}}, // 137 — 删实例身上一维数组的第 k 个元素；失败 → -1
    {"VM_InstArrayClear",          2,  {PT_INT, PT_STRING}}, // 138 — 清空实例身上的一维数组；失败 → -1
    {"VM_InstArrayContains",       3,  {PT_INT, PT_STRING, PT_ANY}}, // 139 — 实例身上一维数组里有没有这个值，1/0；不存在 → -1
    {"VM_HomingBulletAdd",        11,  {PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY}}, // 140 — 追踪弹：贴图,缩放,x,y,速度,伤害,可命中类型,销毁对象,mod名字,销毁贴图,模式（只打自己锁定的目标，像素距离 90x85 命中；自转 6 度/帧、伤害类型 normal 都固定）
    {"VM_DamageEnemy",             3,  {PT_INT, PT_ANY, PT_STRING}}, // 141 — 给敌人造成伤害：走敌人自己的受击事件（闪白/音效/护盾，含自定义 Other_10），比直接改 hp 正确
    {"VM_DamageEnemyAsh",          3,  {PT_INT, PT_ANY, PT_STRING}}, // 142 — 灰烬伤害：接不下就一击必杀换成 obj_mouse_ash_death（照抄原版 obj_power_god_bullet_1，不看护盾）
    {"VM_BulletScreenAdd_Ex",     18,  {PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY}}, // 143 — 屏幕弹幕(带卡片效果)：同 VM_BulletScreenAdd，多【伤害类型】【标志数值 flag】【出生角度】；flag bit1=过火/bit2=解冻/4,8,16…=自定义，进格子中心带时与卡片的 bullet_flag 取且运算，>0 就应用并消位；angle 给后向子弹用（180=倒过来）
    {"VM_BulletScreenAdd_Exs",    20,  {PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY, PT_ANY}}, // 144 — 同 VM_BulletScreenAdd_Ex，多【ty 目标行的世界 y，-1=不渐变】【lk 每帧靠拢比例，默认 0.15】：先朝目标行拐过去再直线飞（原版水管弹的 y 向 lerp），到位自动吸附并停掉渐变
    {"VM_GetInfo",                -1,  {}, PT_ANY}, // 145 — 变长：按注册表逐级查一个对象的信息（类别, id, 字段1[, 字段2, ...]）。类别 card/enemy/weapon/gem；字符串段=取字段，数字段=取下标（落在 ds_map 上自动按字符串键查，注册表的 shapes/upgrades 键就是 "0"/"3"）；查到数组/结构体本身返回 undefined
    {"VM_CatInRow",                1,  {PT_INT}, PT_INT}, // 146 — 该行第一只猫（obj_cat，海底图的螃蟹同对象）的实例 id；没有返回 -1
    {"VM_MapObj",                 -1,  {}, PT_ANY}, // 147 — 变长：查格子上有没有地图物品（列, 行[, "名字"]），读 global.cell_terrain_flag 的位。名字 obstacle/mucus/lava/seawater/barrier/fog/cloud/wind_tunnel；"all"/""=任意一种；"list"=返回逗号分隔名字串；列或行传 -1 = 该方向不限；名字不认识返回 -1
    {"VM_GetInstanceCount",        1,  {PT_STRING}, PT_INT}, // 148 — 某个对象类的实例个数（类名就是 obj_xxxx，和游戏里 object 名一致）；名字不存在返回 -1
    {"VM_GetInstanceAt",           2,  {PT_STRING, PT_INT}, PT_INT}, // 149 — 某个对象类第 k 个实例的 id（k 从 0 起）；越界 / 名字不存在返回 -1
};

// ============================================================
// 查询函数
// ============================================================
inline int find_block(const std::string& name) {
    for (int i = 0; i < (int)BLOCK_NAMES.size(); i++)
        if (name == BLOCK_NAMES[i]) return i;
    return -1;
}

// 自定义块名：_DEFINE_BLOCK_xxxx（xxxx = 字母/数字/下划线，至少一个字符）
inline bool is_custom_block_name(const std::string& s) {
    static const char* PRE = "_DEFINE_BLOCK_";
    const size_t n = strlen(PRE);
    if (s.size() <= n) return false;
    if (s.compare(0, n, PRE) != 0) return false;
    for (size_t i = n; i < s.size(); i++) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') continue;
        return false;
    }
    return true;
}

inline int find_func(const std::string& name) {
    for (int i = 0; i < (int)FUNC_DEFS.size(); i++)
        if (name == FUNC_DEFS[i].name) return i;
    return -1;
}
