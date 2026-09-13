#pragma once
#include <string>
#include <vector>

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
};

// ============================================================
// 字符串参数合法值集合 — 和游戏数据同步维护
// ============================================================

// 敌人 ID（VM_SpawnEnemy / VM_SpawnBoss）
static const std::vector<const char*> VALID_ENEMY_IDS = {
    "normal_mouse","football_fan_mouse","iron_pan_mouse","skateboard_mouse",
    "landlady_mouse","zombie_with_flower_pot","machine_mouse","ninja_mouse",
    "minion_mouse","kangaroo","repairman_mouse","diver_mouse","paper_boat_mouse",
    "duck_mouse","tropical_fish_mouse","lambo_mouse","butterfly_mouse",
    "taro_toho_mouse","water_taro_toho_mouse","assault_mouse","frog_prince_mouse",
    "roller_skating_mouse","giant_mouse","mario_mouse","arno","temple_pharaoh",
    "engineering_vehicle_mouse","garbage_track_mouse","mole","glider_mouse",
    "ice_residue","bat_mouse","rumble","abyss_pharaoh","cucumber_paper_boat_mouse",
    "apple_duck_mouse","egg_tropical_fish_mouse","orange_prince_mouse",
    "submarine_mouse","rowboat_mouse","water_penguin_mouse","pink_paul",
    "cucumber_normal_mouse","apple_football_fan_mouse","egg_iron_pan_mouse",
    "tangerine_skateboard_mouse","shy_landlady_mouse","zombie_with_wallnut",
    "caribbean_mouse","penguin_mouse","arson_mouse","non_mainstream_mouse",
    "flute_mouse","panda_mouse","can_mouse","blonde_mary","pete",
    "dragon_boat_mouse","flagship_mouse","thug_submarine_mouse",
    "kof_submarine_mouse","soar_mouse","jet_mouse","dentist_mouse",
    "sawblade_mouse","warrior_mouse","naruto_mouse","hazelnut_cannon_mouse",
    "landmine_vehicle_mouse","waste_flying_mouse","airbrone_explosive_mouse",
    "priest_mouse","pope_mouse","wrestler_mouse","special_armour_mouse",
    "magician_mouse","ghost_mouse","flight_barrier_mouse","hells_messenger",
    "needle_baron","fog_julie","lieutenant_buzz","paratrooper_mouse",
    "irritable_jack","hot_vajra","machine_normal_mouse","machine_football_fan_mouse",
    "machine_iron_pan_mouse","machine_skateboard_mouse","machine_flag_mouse",
    "mirror_mouse","trumpeter_mouse","huang_xiaoming","angelababy",
    "mouse_train_1","soldier_mouse","machine_bomb_mouse","aircraft_carrier",
    "kamikaze_glider_mouse","captain_america_mouse","iron_man_mouse",
    "mouse_train_2","charge_spring_mouse","snail_mouse","machine_beehive_mouse",
    "machine_bee","spider_man_mouse","hulk_mouse","mouse_train_3",
};

// 卡片 ID（VM_SpawnPlant / VM_BanCard）
static const std::vector<const char*> VALID_CARD_IDS = {
    "xiao_long_bao","small_fire","toast_bread","flour_sack","double_long_bao",
    "mouse_clip","coke_bomb","wooden_plate","ice_long_bao","goblet_lamp",
    "coffee_cup","salad_pult","coffee_pot","chocolate_bread","water_tea_cup",
    "ice_bucket_bomb","stinky_tofu_pult","cat_box","kettle_bomb",
    "triple_wine_rack","brazier","large_fire","iron_fishbone","gatlin_long_bao",
    "rotating_coffee_pot","takoyaki","wooden_cork","coffee_grounds",
    "wine_bottle_bomb","double_water_pipe","melon_shield","steel_wool","sausage",
    "fishbone","hamburger","oil_lamp","ventilation_fan","egg_boiler_pult",
    "ice_egg_boiler_pult","chocolate_pult","chocolate_cannon","firework_dragon",
    "double_ice_long_bao","cat_chest","cherry_pudding","skewer_bomb",
    "gatlin_ice_long_bao","aquarius_elve","tar_sprayer","triple_long_bao",
    "triple_ice_long_bao","hotdog_cannon","oden_pot","whisky_bomb",
    "cotton_candy","durian","dragon_fruit","pineapple_explosive_bread",
    "ice_cream","lightning_baguette","bull_firework","magic_chicken",
    "xinjiang_fried_noodles","king_long_bao","king_triple_long_bao",
    "chili_powder","tang_hu_lu","beef_hotpot","spicy_pot","pan_fried_bun",
};

// 物件名（VM_SpawnObject / VM_ClearMapObjects）
static const std::vector<const char*> VALID_OBJECT_NAMES = {
    "obstacle","lava","seawater","wind_tunnel","mouse_hole",
    "pharaoh_hole","buzz_wind","cloud","ladder","barrier",
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
};

// ============================================================
// 查询函数
// ============================================================
inline int find_block(const std::string& name) {
    for (int i = 0; i < (int)BLOCK_NAMES.size(); i++)
        if (name == BLOCK_NAMES[i]) return i;
    return -1;
}

inline int find_func(const std::string& name) {
    for (int i = 0; i < (int)FUNC_DEFS.size(); i++)
        if (name == FUNC_DEFS[i].name) return i;
    return -1;
}
