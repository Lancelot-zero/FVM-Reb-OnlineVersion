package main

import (
	"fmt"
	"regexp"
	"strings"
)

// ============================================================
// 语法检查器：全部规则依据 help.md（LabMapCompiler 使用手册）
// ============================================================

// 事件块（help.md 事件块表，34 个）
var eventBlocks = map[string]bool{
	"_VM_ROOM_READY_ENTRY": true, "_VM_BATTLE_START": true,
	"_VM_WAVE_START": true, "_VM_WAVE_END": true,
	"_VM_SUBWAVE_START": true, "_VM_SUBWAVE_END": true,
	"_VM_CARD_CREATED": true, "_VM_CARD_DESTROYED": true,
	"_VM_CARD_DAMAGED": true, "_VM_CARD_PREVIEW_PICKED": true,
	"_VM_ENEMY_SPAWNED": true, "_VM_ENEMY_KILLED": true,
	"_VM_ENEMY_DAMAGED": true, "_VM_PLAYER_DAMAGED": true,
	"_VM_BOSS_STATE_CHANGE": true,
	"_VM_PLATFORM_IDLE_END": true,
	"_VM_MOUSE_LEFT":        true, "_VM_MOUSE_RIGHT": true,
	"_VM_KEY_PRESSED": true, "_VM_BUTTON_CLICKED": true,
	"_VM_FRAME":    true,
	"_VM_TIMER_5f": true, "_VM_TIMER_10f": true, "_VM_TIMER_15f": true,
	"_VM_TIMER_30f": true, "_VM_TIMER_60f": true,
	"_OBJECT_CFG": true,"_OBJECT_CREATE": true,"_OBJECT_STEP": true,"_OBJECT_DRAW": true,"_OBJECT_DESTROY":true,
	"_OBJECT_MOUSE_ENTER": true,"_OBJECT_MOUSE_LEAVE": true,"_OBJECT_CLICK": true,
}

// 保留字
var reservedWords = map[string]bool{
	"if": true, "elif": true, "else": true, "halt": true, "exit": true,
	"while": true, "break": true, "continue": true,
}

// 自定义块 _DEFINE_BLOCK_xxx（xxx = 字母/数字/下划线，至少一个字符）
// 定义在最外层；只有事件块（_VM_* / _OBJECT_*）能调用，自定义块之间不能再调
const customBlockPrefix = "_DEFINE_BLOCK_"

func isCustomBlockName(s string) bool {
	if !strings.HasPrefix(s, customBlockPrefix) || len(s) == len(customBlockPrefix) {
		return false
	}
	for i := len(customBlockPrefix); i < len(s); i++ {
		c := s[i]
		if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' {
			continue
		}
		return false
	}
	return true
}

// 敌人 ID（134 个，help.md 参数校验附录）
var enemyIDs = set(
	"normal_mouse", "football_fan_mouse", "iron_pan_mouse", "skateboard_mouse",
	"landlady_mouse", "zombie_with_flower_pot", "machine_mouse", "ninja_mouse",
	"minion_mouse", "kangaroo", "repairman_mouse", "diver_mouse", "paper_boat_mouse",
	"duck_mouse", "tropical_fish_mouse", "lambo_mouse", "butterfly_mouse",
	"taro_toho_mouse", "water_taro_toho_mouse", "assault_mouse", "frog_prince_mouse",
	"roller_skating_mouse", "giant_mouse", "mario_mouse", "arno", "temple_pharaoh",
	"engineering_vehicle_mouse", "garbage_track_mouse", "mole", "glider_mouse",
	"ice_residue", "bat_mouse", "rumble", "abyss_pharaoh", "cucumber_paper_boat_mouse",
	"apple_duck_mouse", "egg_tropical_fish_mouse", "orange_prince_mouse",
	"submarine_mouse", "rowboat_mouse", "water_penguin_mouse", "pink_paul",
	"cucumber_normal_mouse", "apple_football_fan_mouse", "egg_iron_pan_mouse",
	"tangerine_skateboard_mouse", "shy_landlady_mouse", "zombie_with_wallnut",
	"caribbean_mouse", "penguin_mouse", "arson_mouse", "non_mainstream_mouse",
	"flute_mouse", "panda_mouse", "can_mouse", "blonde_mary", "pete",
	"dragon_boat_mouse", "flagship_mouse", "thug_submarine_mouse",
	"kof_submarine_mouse", "soar_mouse", "jet_mouse", "dentist_mouse",
	"sawblade_mouse", "warrior_mouse", "naruto_mouse", "hazelnut_cannon_mouse",
	"landmine_vehicle_mouse", "waste_flying_mouse", "airbrone_explosive_mouse",
	"priest_mouse", "pope_mouse", "wrestler_mouse", "special_armour_mouse",
	"magician_mouse", "ghost_mouse", "flight_barrier_mouse", "hells_messenger",
	"needle_baron", "fog_julie", "lieutenant_buzz", "paratrooper_mouse",
	"irritable_jack", "hot_vajra", "machine_normal_mouse", "machine_football_fan_mouse",
	"machine_iron_pan_mouse", "machine_skateboard_mouse", "machine_flag_mouse",
	"mirror_mouse", "trumpeter_mouse", "huang_xiaoming", "angelababy",
	"mouse_train_1", "soldier_mouse", "machine_bomb_mouse", "aircraft_carrier",
	"kamikaze_glider_mouse", "captain_america_mouse", "iron_man_mouse",
	"mouse_train_2", "charge_spring_mouse", "snail_mouse", "machine_beehive_mouse",
	"machine_bee", "spider_man_mouse", "hulk_mouse", "mouse_train_3",
	"clownfish_mouse", "conch_mouse", "eel_mouse", "electric_jellyfish",
	"hercules", "iron_diver_mouse", "lobster_knight", "machine_shark_1",
	"machine_shark_2", "mermaid_mary", "oyster_mouse", "sardine_mouse",
	"seahorse_mouse", "swordfish_mouse", "thor", "undersea_can_mouse",
	"undersea_captain_mouse", "undersea_diver_mouse", "undersea_panda_mouse",
	"undersea_penguin_mouse", "undersea_repairman_mouse", "undersea_submarine_1",
	"undersea_submarine_2", "war_god", "windmill_fish_mouse",
)

// 卡片 ID（79 个）
var cardIDs = set(
	"xiao_long_bao", "small_fire", "toast_bread", "flour_sack", "double_long_bao",
	"mouse_clip", "coke_bomb", "wooden_plate", "ice_long_bao", "goblet_lamp",
	"coffee_cup", "salad_pult", "coffee_pot", "chocolate_bread", "water_tea_cup",
	"ice_bucket_bomb", "stinky_tofu_pult", "cat_box", "kettle_bomb", "triple_wine_rack",
	"brazier", "large_fire", "iron_fishbone", "gatlin_long_bao", "rotating_coffee_pot",
	"takoyaki", "wooden_cork", "coffee_grounds", "wine_bottle_bomb", "double_water_pipe",
	"melon_shield", "steel_wool", "sausage", "fishbone", "hamburger", "oil_lamp",
	"ventilation_fan", "egg_boiler_pult", "ice_egg_boiler_pult", "chocolate_pult",
	"chocolate_cannon", "firework_dragon", "double_ice_long_bao", "cat_chest",
	"cherry_pudding", "skewer_bomb", "gatlin_ice_long_bao", "aquarius_elve",
	"tar_sprayer", "triple_long_bao", "triple_ice_long_bao", "hotdog_cannon",
	"oden_pot", "whisky_bomb", "cotton_candy", "durian", "dragon_fruit",
	"pineapple_explosive_bread", "ice_cream", "lightning_baguette", "bull_firework",
	"magic_chicken", "xinjiang_fried_noodles", "king_long_bao", "king_triple_long_bao",
	"chili_powder", "tang_hu_lu", "beef_hotpot", "spicy_pot", "pan_fried_bun",
	"coal_starfish", "curry_lobster_cannon", "delicacy_firework", "fruit_tart",
	"horseshoe_crab_bread", "pizza_oven", "rabbit_lantern", "soda_bubble",
	"sugar_ball_pult",
)

// 物件名（65 个，含 boss 产物）
var objectNames = set(
	// 基础物件
	"obstacle", "lava", "seawater", "wind_tunnel", "barrier", "mouse_hole",
	"pharaoh_hole", "buzz_wind", "cloud", "ladder", "fog",
	// boss 产物 — 子弹/弹道
	"arno_bullet", "arno_bullet_effect", "arson_bullet", "blonde_mary_bullet",
	"electric_jellyfish_bullet", "hercules_laser", "ice_residue_ball", "ice_residue_bullet",
	"iron_man_bullet", "lobster_knight_bullet", "mermaid_mary_bullet", "paul_bullet",
	"rumble_laser", "rumble_missile", "shark_1_bullet", "mouse_train_1_bullet",
	"mouse_train_2_bullet", "julie_missile", "pete_missile", "pete_claw", "pete_spike",
	"baron_bats", "baron_blade", "baron_needle", "messenger_ignis_fatuus", "messenger_mace",
	"messenger_poop", "machine_shark_2_wind",
	// boss 产物 — 召唤物/技能
	"angelababy_star", "angelababy_summon", "angelababy_target", "arson_mouse",
	"captain_rainbow", "captain_shield", "iron_man",
	"irritable_jack_fire", "irritable_jack_rock_skill_3", "irritable_jack_rock_skill_4",
	"mario_cave", "mario_pipeline", "mermaid_mary_music", "mermaid_mary_wave",
	"mouse_train_3_butter", "mouse_train_3_explode", "pharaoh_bandage", "pharaoh_coffin",
	"spider_man_mouse_web", "vajra_lava", "vajra_lightning", "vajra_spike",
	"war_god_duck", "war_god_summon", "war_god_wood", "xiaoming_text",
)

// 物件名 + "all"（VM_ClearMapObjects 第三参支持 "all" 清全部）
var objectNamesAll = withExtra(objectNames, "all")

// 宝石名（16 个）
var gemNames = set(
	"laser_gem", "bomb_gem", "cateye_gem", "freeze_gem",
	"flame_recover_gem", "starlight_gem", "attack_gem", "gale_gem",
	"power_gem", "transform_gem", "health_gem", "produce_gem",
	"slow_down_gem", "bleed_gem", "guard_gem", "strength_gem",
)

// 地形类型 / 行属性 / 植物层级
var terrainTypes = set("normal", "water", "obstacle")
var rowFeatures = set("land", "water")
var plantLayers = set("normal", "shield_inner", "lilypad", "shield_outer", "coffee", "all")

// VM_MapObj 的地图物品名（cell_terrain_flag 的 8 个位）+ 两个特殊值
var mapObjNames = set(
	"obstacle", "mucus", "lava", "seawater",
	"barrier", "fog", "cloud", "wind_tunnel",
	"all", "list", "",
)

// VM_GetInfo 的查询类别（只这四个注册表）
var infoKinds = set("card", "enemy", "weapon", "gem")

// 键名（不区分大小写）
var keyNames = func() map[string]bool {
	m := map[string]bool{}
	for c := 'a'; c <= 'z'; c++ {
		m[string(c)] = true
	}
	for c := '0'; c <= '9'; c++ {
		m[string(c)] = true
	}
	for i := 1; i <= 12; i++ {
		m["f"+itoa(i)] = true
	}
	for _, k := range []string{"up", "down", "left", "right", "space", "enter", "escape",
		"tab", "shift", "ctrl", "alt", "backspace", "delete", "home", "end",
		"pageup", "pagedown"} {
		m[k] = true
	}
	return m
}()

func itoa(i int) string {
	if i < 10 {
		return string(rune('0' + i))
	}
	return string(rune('0'+i/10)) + string(rune('0'+i%10))
}

func set(keys ...string) map[string]bool {
	m := make(map[string]bool, len(keys))
	for _, k := range keys {
		m[k] = true
	}
	return m
}

func withExtra(m map[string]bool, extras ...string) map[string]bool {
	out := make(map[string]bool, len(m)+len(extras))
	for k := range m {
		out[k] = true
	}
	for _, k := range extras {
		out[k] = true
	}
	return out
}

// ---- 函数表 ----
// 参数类型与个数来自 compiler_defs.h（编译器源码定义，实测校准）

type paramType int

const (
	ptInt paramType = iota
	ptFloat
	ptString
	ptAny
)

func (t paramType) name() string {
	switch t {
	case ptInt:
		return "int"
	case ptFloat:
		return "float"
	case ptString:
		return "string"
	default:
		return "?"
	}
}

type enumSpec struct {
	label string          // 枚举类别名，用于报错信息
	allow map[string]bool // 合法值
	ci    bool            // 不区分大小写（键名）
	sev   string          // "error"（编译器强制）/ "warning"（仅 help.md 文档约定）
}

type fnSpec struct {
	min, max int              // 参数个数范围；max = -1 表示不限
	types    []paramType      // 参数类型（编译器强制）；nil = 不检查（变长参数）
	enums    map[int]enumSpec // 参数位置（0 起）→ 字符串枚举校验
}

// 编译器实测强制枚举校验的只有 5 处（error）；其余枚举仅文档约定（warning）
var funcTable = map[string]fnSpec{
	// 规则设置
	"VM_BanCard":         {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"卡片ID", cardIDs, false, "error"}}},
	"VM_BanGem":          {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"宝石名", gemNames, false, "warning"}}},
	"VM_SetCardLevelCap": {1, 1, []paramType{ptInt}, nil},
	"VM_SetMaxSlots":     {1, 1, []paramType{ptInt}, nil},
	"VM_SpawnCats":       {1, 1, []paramType{ptInt}, nil},
	// 地图与地形
	"VM_SetTerrain":    {3, 3, []paramType{ptInt, ptInt, ptString}, map[int]enumSpec{2: {"地形类型", terrainTypes, false, "warning"}}},
	"VM_SetRowFeature": {2, 2, []paramType{ptInt, ptString}, map[int]enumSpec{1: {"行属性", rowFeatures, false, "warning"}}},
	"VM_GetTerrain":    {2, 2, []paramType{ptInt, ptInt}, nil},
	// 创建
	"VM_CreatePlatform": {8, 8, []paramType{ptInt, ptInt, ptInt, ptInt, ptInt, ptInt, ptInt, ptString}, nil},
	"VM_SpawnPlant":     {6, 6, []paramType{ptString, ptInt, ptInt, ptInt, ptInt, ptInt}, map[int]enumSpec{0: {"卡片ID", cardIDs, false, "error"}}},
	"VM_SpawnEnemy":     {3, 3, []paramType{ptString, ptInt, ptInt}, map[int]enumSpec{0: {"敌人ID", enemyIDs, false, "error"}}},
	"VM_SpawnBoss":      {3, 3, []paramType{ptString, ptInt, ptInt}, map[int]enumSpec{0: {"敌人ID", enemyIDs, false, "error"}}},
	"VM_SpawnObject":    {3, 3, []paramType{ptString, ptInt, ptInt}, map[int]enumSpec{0: {"物件名", objectNames, false, "error"}}},
	"VM_SpawnPlantsRandom": {16, 16, []paramType{
		ptInt, ptInt, ptInt, ptInt, ptInt, ptInt, ptInt,
		ptString, ptString, ptString, ptString, ptString, ptString, ptString, ptString, ptString,
	}, nil},
	"VM_CreateButton": {7, 7, []paramType{ptInt, ptInt, ptString, ptFloat, ptInt, ptInt, ptInt}, nil},
	// 属性
	"VM_GetProp":         {2, 2, []paramType{ptInt, ptString}, nil},
	"VM_SetProp":         {3, 3, []paramType{ptInt, ptString, ptAny}, nil},
	"VM_SetCardProp":     {5, 5, []paramType{ptInt, ptInt, ptString, ptString, ptAny}, nil},
	"VM_SetEnemyProp":    {3, 3, []paramType{ptString, ptString, ptAny}, nil},
	"VM_ApplyPlantLevel": {1, 1, []paramType{ptInt}, nil},
	"VM_GetKilledProp":   {1, 1, []paramType{ptString}, nil},
	// 查询
	"VM_GetWave":                  {0, 0, nil, nil},
	"VM_GetSubwave":               {0, 0, nil, nil},
	"VM_GetFlame":                 {0, 0, nil, nil},
	"VM_GetEnemyCount":            {0, 0, nil, nil},
	"VM_GetPlantCount":            {0, 0, nil, nil},
	"VM_GetPlantCountAt":          {3, 3, []paramType{ptInt, ptInt, ptString}, nil},
	"VM_GetPlantAt":               {3, 3, []paramType{ptInt, ptInt, ptString}, map[int]enumSpec{2: {"植物层级", plantLayers, false, "warning"}}},
	"VM_GetLastBoss":              {0, 0, nil, nil},
	"VM_GetLastCreatedEnemy":      {0, 0, nil, nil},
	"VM_GetLastKilledEnemy":       {0, 0, nil, nil},
	"VM_GetLastCreatedCard":       {0, 0, nil, nil},
	"VM_GetLastDestroyedCard":     {0, 0, nil, nil},
	"VM_GetLastIdlePlatform":      {0, 0, nil, nil},
	"VM_GetLastClickedButton":     {0, 0, nil, nil},
	"VM_GetLastBossStateChangeId": {0, 0, nil, nil},
	"VM_GetLastBossOldState":      {0, 0, nil, nil},
	"VM_GetLastBossNewState":      {0, 0, nil, nil},
	// 鼠标与键盘
	"VM_GetMouseX":       {0, 0, nil, nil},
	"VM_GetMouseY":       {0, 0, nil, nil},
	"VM_GetMouseCol":     {0, 0, nil, nil},
	"VM_GetMouseRow":     {0, 0, nil, nil},
	"VM_GetMousePressed": {1, 1, []paramType{ptInt}, nil},
	"VM_GetKeyDown":      {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"键名", keyNames, true, "warning"}}},
	"VM_GetKeyPressed":   {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"键名", keyNames, true, "warning"}}},
	// 区域操作
	"VM_ClearMapObjects":   {3, 3, []paramType{ptInt, ptInt, ptString}, map[int]enumSpec{2: {"物件名", objectNamesAll, false, "warning"}}},
	"VM_ClearPlants":       {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_ClearPlantsByType": {1, 1, []paramType{ptString}, nil}, // 编译器不校验枚举
	"VM_WakePlants":        {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_SwapPlants":        {4, 4, []paramType{ptInt, ptInt, ptInt, ptInt}, nil},
	"VM_SwapPlantRects":    {6, 6, []paramType{ptInt, ptInt, ptInt, ptInt, ptInt, ptInt}, nil},
	"VM_CompactColumn":     {1, 1, []paramType{ptInt}, nil},
	"VM_CompactColumnRev":  {1, 1, []paramType{ptInt}, nil},
	"VM_CompactRow":        {1, 1, []paramType{ptInt}, nil},
	"VM_CompactRowRev":     {1, 1, []paramType{ptInt}, nil},
	// 贴图加载
	"VM_LoadSprite":          {1, 1, []paramType{ptString}, nil},
	"VM_LoadSpritePerm":      {2, 2, []paramType{ptString, ptInt}, nil},
	"VM_FreeSpritePerm":      {1, 1, []paramType{ptString}, nil},
	"VM_LoadSpriteFrames":    {2, 2, []paramType{ptString, ptInt}, nil},
	"VM_LoadSpriteFrames_Ex": {4, 4, []paramType{ptString, ptInt, ptInt, ptInt}, nil},
	"VM_GetLoadedSpriteName": {1, 1, []paramType{ptInt}, nil},
	"VM_AliasSprite":         {2, 2, []paramType{ptString, ptString}, nil},
	// 平台
	"VM_SetPlatformParams":        {5, 5, []paramType{ptInt, ptInt, ptInt, ptInt, ptInt}, nil},
	"VM_RefreshPlatformSnapshots": {0, 0, nil, nil},
	// 绘制
	"VM_SetMapBackground":  {2, 2, []paramType{ptString, ptFloat}, nil},
	"VM_SetDrawSlot_front": {5, 5, []paramType{ptInt, ptString, ptInt, ptInt, ptFloat}, nil},
	"VM_SetDrawSlot":       {5, 5, []paramType{ptInt, ptString, ptInt, ptInt, ptFloat}, nil},
	// 工具
	"VM_Random":          {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_SetFlame":        {1, 1, []paramType{ptInt}, nil},
	"VM_PlaySound":       {1, 1, []paramType{ptString}, nil},
	"VM_SetEventEnabled": {1, 1, []paramType{ptInt}, nil},
	"VM_GameWin":         {0, 0, nil, nil},
	"VM_GameLose":        {0, 0, nil, nil},
	// 输出（变长参数；VM_ShellPrint 游戏 VM 实测上限 16 个）
	"VM_ShellPrint":    {0, 16, nil, nil},
	"VM_ShowNotice":    {0, -1, nil, nil},
	"VM_ShowNoticeDur": {0, -1, nil, nil},
	// 卡槽操作
	"VM_SetCardSlotProp":  {3, 3, []paramType{ptString, ptString, ptAny}, nil},
	"VM_CalcCardSlotProp": {4, 4, []paramType{ptString, ptString, ptInt, ptAny}, nil},
	"VM_GetCardSlotCount": {0, 0, nil, nil},
	"VM_GetPreviewCard":   {0, 0, nil, nil},
	// 数组（命名数组，按名存取，跨块共享；每局重置）
	"VM_ArrayGet":      {2, 2, []paramType{ptString, ptInt}, nil},
	"VM_ArraySet":      {3, 3, []paramType{ptString, ptInt, ptAny}, nil},
	"VM_ArrayDel":      {2, 2, []paramType{ptString, ptInt}, nil},
	"VM_ArrayADD":      {2, 2, []paramType{ptString, ptAny}, nil},
	"VM_ArraySize":     {1, 1, []paramType{ptString}, nil},
	"VM_ArrayClear":    {1, 1, []paramType{ptString}, nil},
	"VM_ArrayClearAll": {0, 0, nil, nil},
	// 公告样式
	"VM_SetNoticeStyle": {4, 4, []paramType{ptFloat, ptInt, ptInt, ptInt}, nil},
	// 传送带
	"VM_conveyor_belt_able": {1, 1, []paramType{ptInt}, nil},
	"VM_Slot_add":           {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"卡片ID", cardIDs, false, "warning"}}},
	// 禁用与限制
	"VM_BanAllCard":      {0, 0, nil, nil},
	"VM_CannelBanCard":   {1, 1, []paramType{ptString}, map[int]enumSpec{0: {"卡片ID", cardIDs, false, "error"}}},
	"VM_BanWeapon":       {0, 0, nil, nil},
	"VM_BanSuperWeapon":  {0, 0, nil, nil},
	"VM_BanShield":       {0, 0, nil, nil},
	"VM_SetCardShapeCap": {1, 1, []paramType{ptInt}, nil},
	"VM_SetCardSkillCap": {1, 1, []paramType{ptInt}, nil},
	"VM_IsUndefined":     {1, 1, []paramType{ptAny}, nil},
	"VM_IsDestroyed":     {1, 1, []paramType{ptInt}, nil},
	"VM_LoadSound":       {1, 1, []paramType{ptString}, nil},
	"VM_Floor":         {1, 1, []paramType{ptAny}, nil},
	"VM_Ceil":          {1, 1, []paramType{ptAny}, nil},
	"VM_SpawnBatMouse": {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_GetTimeLimit":  {0, 0, nil, nil},
	"VM_SetTimeLimit":  {1, 1, []paramType{ptInt}, nil},
	"VM_SetDrawSlotEx":       {8, 8, []paramType{ptInt, ptString, ptInt, ptInt, ptFloat, ptFloat, ptFloat, ptFloat}, nil},
	"VM_SetDrawSlotEx_front": {8, 8, []paramType{ptInt, ptString, ptInt, ptInt, ptFloat, ptFloat, ptFloat, ptFloat}, nil},
	"VM_SetWaveAuto":         {1, 1, []paramType{ptInt}, nil},
	"VM_SetWave":             {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_GetCurCard":          {0, 0, nil, nil},
	"VM_EnemyInRange":        {5, 5, []paramType{ptInt, ptInt, ptInt, ptInt, ptString}, nil},
	"VM_GetHomingTarget":     {1, 1, []paramType{ptString}, nil},
	"VM_GetInstancesInRange": {7, 7, []paramType{ptString, ptInt, ptInt, ptInt, ptInt, ptString, ptString}, nil},
	"VM_CreateInstance":      {3, 3, []paramType{ptString, ptFloat, ptFloat}, nil},
	"VM_LoadSpritePerm_Ex":   {4, 4, []paramType{ptString, ptInt, ptInt, ptInt}, nil},
	"VM_SetShovelFlameRate":  {1, 1, []paramType{ptFloat}, nil},
	"VM_BulletScreenAdd":     {15, 15, []paramType{ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny}, nil},
	"VM_DrawSpriteExt":       {8, 8, []paramType{ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny}, nil},
	"VM_RunStep":             {2, 2, []paramType{ptInt, ptInt}, nil},
	"VM_DestroyInstance":     {1, 1, []paramType{ptInt}, nil},
	"VM_GetCardProp":         {2, 2, []paramType{ptString, ptString}, nil},
	"VM_CanPlace":            {3, 3, []paramType{ptString, ptInt, ptInt}, nil},
	"VM_CallFunc":            {1, -1, nil, nil},
	"VM_FuncExists":          {1, 1, []paramType{ptString}, nil},
	"VM_FuncDesc":            {1, 1, []paramType{ptString}, nil},
	"VM_SpriteExists":        {1, 1, []paramType{ptString}, nil},
	"VM_AliasSpritePerm":     {2, 2, []paramType{ptString, ptString}, nil},
	"VM_ArrayExists":         {1, 1, []paramType{ptString}, nil},
	"VM_CellCount":           {3, 3, []paramType{ptString, ptInt, ptInt}, nil},
	"VM_CellItem":            {4, 4, []paramType{ptString, ptInt, ptInt, ptInt}, nil},
	"VM_CellContains":        {4, 4, []paramType{ptString, ptInt, ptInt, ptAny}, nil},
	"VM_ArrayContains":       {2, 2, []paramType{ptString, ptAny}, nil},
	"VM_InstArrayExists":     {2, 2, []paramType{ptInt, ptString}, nil},
	"VM_InstArraySize":       {2, 2, []paramType{ptInt, ptString}, nil},
	"VM_InstArrayItem":       {3, 3, []paramType{ptInt, ptString, ptInt}, nil},
	"VM_InstArraySet":        {4, 4, []paramType{ptInt, ptString, ptInt, ptAny}, nil},
	"VM_InstArrayAdd":        {3, 3, []paramType{ptInt, ptString, ptAny}, nil},
	"VM_InstArrayDel":        {3, 3, []paramType{ptInt, ptString, ptInt}, nil},
	"VM_InstArrayClear":      {2, 2, []paramType{ptInt, ptString}, nil},
	"VM_InstArrayContains":   {3, 3, []paramType{ptInt, ptString, ptAny}, nil},
	"VM_HomingBulletAdd":     {11, 11, []paramType{ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny}, nil},
	"VM_DamageEnemy":         {3, 3, []paramType{ptInt, ptAny, ptString}, nil},
	"VM_DamageEnemyAsh":      {3, 3, []paramType{ptInt, ptAny, ptString}, nil},
	"VM_BulletScreenAdd_Ex":  {18, 18, []paramType{ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny}, nil},
	"VM_BulletScreenAdd_Exs": {20, 20, []paramType{ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny, ptAny}, nil},
	"VM_GetInfo":             {3, 16, nil, map[int]enumSpec{0: {"查询类别", infoKinds, false, "warning"}}},
	"VM_CatInRow":            {1, 1, []paramType{ptInt}, nil},
	"VM_MapObj":              {2, 16, nil, map[int]enumSpec{2: {"地图物品名", mapObjNames, false, "warning"}}},
	"VM_GetInstanceCount":    {1, 1, []paramType{ptString}, nil},
	"VM_GetInstanceAt":       {2, 2, []paramType{ptString, ptInt}, nil},
}

// funcDescs 函数中文描述（悬停提示 / 补全说明，来自 help.md 函数表）
var funcDescs = map[string]string{
	"VM_BanCard":                  "禁用卡片（卡名）",
	"VM_BanGem":                   "禁用宝石（宝石名）",
	"VM_SetCardLevelCap":          "卡片等级上限",
	"VM_SetMaxSlots":              "最大携带卡片数",
	"VM_SpawnCats":                "是否生成初始猫（0或1，默认1）",
	"VM_SetTerrain":               "设地形（列,行,类型；-1=全部；normal/water/obstacle）",
	"VM_SetRowFeature":            "改行属性（行,属性；land/water；-1=所有行）",
	"VM_GetTerrain":               "获取地形（返回 0=normal 1=water 2=obstacle -1=超出）",
	"VM_CreatePlatform":           "创建平台（列,行,宽,高,轴向,距离,停顿帧,贴图；轴向 0=上下 1=左右）",
	"VM_SpawnPlant":               "种植物（卡名,列,行,外形,星级,技能；-1=整行/整列）",
	"VM_SpawnEnemy":               "刷敌人（敌人类型,行,血量）",
	"VM_SpawnBoss":                "刷 BOSS（BOSS类型,行,血量）",
	"VM_SpawnObject":              "刷对象（物件名,列,行）",
	"VM_SpawnPlantsRandom":        "区域内随机种植（x,y,w,h,外形,星级,技能,卡1..卡9；-1=跳过；客户端不执行）",
	"VM_CreateButton":             "创建按钮（x,y,精灵名,缩放,idle,hover,press；后三帧 -1=默认；点击触发 _VM_BUTTON_CLICKED）",
	"VM_GetProp":                  "读实例属性（字符串属性返回字符串，浮点属性返回浮点）",
	"VM_SetProp":                  "改实例属性（联机同步）",
	"VM_SetCardProp":              "按格子和卡名改属性（\"all\"=全部）",
	"VM_SetEnemyProp":             "按敌人类型改属性（\"all\"=全部，跳过 BOSS）",
	"VM_ApplyPlantLevel":          "改完 current_level/skill/shape 后调用，刷新数值（-1=全部植物）",
	"VM_GetKilledProp":            "读当前销毁事件对象的属性快照（仅 _VM_CARD_DESTROYED / _VM_ENEMY_KILLED 期间有效；mouse_id 空串按对象名兜底）",
	"VM_GetWave":                  "当前波数",
	"VM_GetSubwave":               "当前子波数",
	"VM_GetFlame":                 "火苗数",
	"VM_GetEnemyCount":            "场上敌人数量",
	"VM_GetPlantCount":            "场上植物数量",
	"VM_GetPlantCountAt":          "指定格子某类型数量（\"all\"=全部）",
	"VM_GetPlantAt":               "指定格子指定层第一个植物实例 ID（层级 normal/shield_inner/lilypad/shield_outer/coffee，\"all\"=任意；没找到返回 -1）",
	"VM_GetLastBoss":              "最后刷的 BOSS ID",
	"VM_GetLastCreatedEnemy":      "最后刷的敌人 ID",
	"VM_GetLastKilledEnemy":       "最后死的敌人 ID",
	"VM_GetLastCreatedCard":       "最后种的卡片 ID",
	"VM_GetLastDestroyedCard":     "最后销毁的卡片 ID",
	"VM_GetLastIdlePlatform":      "最后结束空闲的平台 ID",
	"VM_GetLastClickedButton":     "最后点击的按钮 ID（没有返回 -1）",
	"VM_GetLastBossStateChangeId": "最后改变状态的 BOSS ID",
	"VM_GetLastBossOldState":      "最后改变状态的 BOSS 的旧状态",
	"VM_GetLastBossNewState":      "最后改变状态的 BOSS 的新状态",
	"VM_GetMouseX":                "鼠标世界 X 坐标",
	"VM_GetMouseY":                "鼠标世界 Y 坐标",
	"VM_GetMouseCol":              "鼠标所在网格列",
	"VM_GetMouseRow":              "鼠标所在网格行",
	"VM_GetMousePressed":          "鼠标按键按下返回 1（1=左 2=右 3=中）",
	"VM_GetKeyDown":               "按键按住返回 1（键名不区分大小写）",
	"VM_GetKeyPressed":            "按键刚按下返回 1（单帧有效）",
	"VM_ClearMapObjects":          "清除地图物件（列,行,物件名；-1=全部；\"all\"=全部物件）",
	"VM_ClearPlants":              "清除格子植物（-1=全部；客户端跳过）",
	"VM_ClearPlantsByType":        "按卡名清除（-1=全部，跳过角色）",
	"VM_WakePlants":               "唤醒睡眠卡片（-1=全部）",
	"VM_SwapPlants":               "交换两格植物",
	"VM_SwapPlantRects":           "交换两个等大矩形区域植物",
	"VM_CompactColumn":            "列向上压缩（-1=所有列）",
	"VM_CompactColumnRev":         "列向下压缩",
	"VM_CompactRow":               "行向左压缩（-1=所有行）",
	"VM_CompactRowRev":            "行向右压缩",
	"VM_LoadSprite":               "加载单帧贴图到临时缓存（bin 重载时自动清理）",
	"VM_LoadSpriteFrames":         "加载多帧贴图，自动均分切割",
	"VM_LoadSpriteFrames_Ex":      "加载多帧贴图并指定原点（x,y；-1=默认0）",
	"VM_LoadSpritePerm":           "永久加载贴图",
	"VM_FreeSpritePerm":           "释放永久加载的贴图",
	"VM_GetLoadedSpriteName":      "获取已加载贴图文件名（越界返回 \"\"）",
	"VM_AliasSprite":              "将新名指向已有贴图缓存（已有名不存在时无操作）",
	"VM_SetPlatformParams":        "重设平台移动参数（实例,轴,距离,停顿,方向）",
	"VM_RefreshPlatformSnapshots": "刷新平台快照",
	"VM_SetMapBackground":         "渐变切换背景（贴图名,步长如 0.02）",
	"VM_SetDrawSlot_front":        "设置前景绘制槽（槽位0~7,贴图,x,y,alpha 0~1；空名清除；配合 _VM_FRAME）",
	"VM_SetDrawSlot":              "设置背景绘制槽（火焰 UI 后面的背景层）",
	"VM_Random":                   "随机整数 [min, max]",
	"VM_SetFlame":                 "设置火苗数",
	"VM_PlaySound":                "播放内置音效（如 \"snd_place1\"），也支持 VM_LoadSound 返回的音频ID",
	"VM_IsUndefined":              "判断值是否为 undefined（返回 1=是 0=不是）",
	"VM_IsDestroyed":              "判断实例ID是否已被销毁（返回 1=已销毁 0=存在）",
	"VM_LoadSound":                "加载本地音频文件（只支持 wav），返回音频ID（失败 -1）（暂时不能使用，等待修复）",
	"VM_Floor":                   "对数字向下取整，非数字返回 undefined",
	"VM_Ceil":                    "对数字向上取整，非数字返回 undefined",
	"VM_SpawnBatMouse":           "在指定格子生成蝙蝠鼠（含降落标记），返回实例 ID",
	"VM_GetTimeLimit":            "获取关卡倒计时剩余帧数（无倒计时返回 undefined）(60帧1秒)",
	"VM_SetTimeLimit":            "设置关卡倒计时帧数（无倒计时返回 undefined；联机自动同步）(60帧1秒)",
	"VM_SetEventEnabled":          "事件系统开关（默认 1）",
	"VM_GameWin":                  "触发胜利",
	"VM_GameLose":                 "触发失败",
	"VM_ShellPrint":               "控制台输出（最多 16 个参数，自动拼接）",
	"VM_ShowNotice":               "屏幕中央通知（参数个数可变）",
	"VM_ShowNoticeDur":            "自定义时长的屏幕通知（最后一个参数为帧数）",
	"VM_SetCardSlotProp":          "修改卡槽属性（\"all\"/-1=全部，数字=槽位，字符串=匹配 card_id）",
	"VM_CalcCardSlotProp":         "卡槽属性四则运算（op: 0=加 1=减 2=乘 3=除，仅数值属性）",
	"VM_GetCardSlotCount":         "获取卡槽数量",
	"VM_GetPreviewCard":           "获取当前手牌 card_id（没有返回 -1）",
	"VM_ArrayGet":                 "读数组元素（数组名,下标；不存在或越界返回 0）",
	"VM_ArraySet":                 "写数组元素（数组名,下标,值；越界自动补 0 扩容）",
	"VM_ArrayDel":                 "删除数组元素（数组名,下标；后面前移）",
	"VM_ArrayADD":                 "数组末尾追加（数组名,值；不存在自动创建）",
	"VM_ArraySize":                "数组长度（不存在返回 0）",
	"VM_ArrayClear":               "清空数组（长度归 0，不存在无操作）",
	"VM_ArrayClearAll":            "清空所有数组",
	"VM_SetNoticeStyle":           "设置公告样式（缩放,R,G,B；-1=该项用默认）",
	"VM_conveyor_belt_able":       "传送带模式开关（0或1；开启后卡组不自动生成卡槽，用 VM_Slot_add 手动加卡）",
	"VM_Slot_add":                 "向传送带队列末尾添加一张卡（卡名；游戏内注册的任意卡片，与玩家是否解锁无关）",
	"VM_BanAllCard":               "禁用全部卡片（遍历全游戏卡片注册表）",
	"VM_CannelBanCard":            "解除指定卡片的禁用（卡名）",
	"VM_BanWeapon":                "禁止角色武器使用",
	"VM_BanSuperWeapon":           "禁止角色超级武器使用",
	"VM_BanShield":                "禁止角色盾牌使用",
	"VM_SetCardShapeCap":          "最大转职（shape）等级限制（-1=不限制）",
	"VM_SetCardSkillCap":          "最大技能等级限制（-1=不限制）",
	"VM_SetDrawSlotEx":           "设置绘制槽位（含旋转缩放），角度/缩放传 -1 或 undefined 用默认值(0/1/1)",
	"VM_SetDrawSlotEx_front":     "设置前景绘制槽位（含旋转缩放），参数同 VM_SetDrawSlotEx",
	"VM_SetWaveAuto":             "波次自动推进开关，1=自动（默认），0=关闭后完全不自动出怪，由脚本 VM_SetWave 控制",
	"VM_SetWave":                 "设置当前波次与子波次索引（0 开始），只改计数不出怪，变化触发对应波次钩子",
	"VM_GetCurCard":              "当前块所属的 mod 卡实例 id",
	"VM_EnemyInRange":            "矩形范围是否存在敌人（前缀和，type: normal/obstacle/diver/air/dance/underground/all）",
	"VM_GetHomingTarget":         "最左且血量最高的敌人 id（追踪索敌，param: 敌人类型，空串或all=任意；每帧全场只扫一次）",
	"VM_GetInstancesInRange":     "收集范围内敌人/卡片到 VM 数组（卡片可按 plant_id/plant_type 筛）",
	"VM_CreateInstance":          "按像素坐标创建实例（子弹等）",
	"VM_LoadSpritePerm_Ex":       "永久缓存贴图带原点，不被 bin 重载清理",
	"VM_SetShovelFlameRate":      "铲子返还火苗系数（-1=原逻辑，0~1=直接用）",
	"VM_BulletScreenAdd":         "屏幕弹幕管理器：贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字",
	"VM_DrawSpriteExt":           "直接 draw_sprite_ext（贴图,子图,x,y,xscale,yscale,rot,alpha；固定 c_white），只能在 _OBJECT_DRAW 里用",
	"VM_RunStep":                 "让实例额外跑 N 轮 Step（实例,轮数），一轮 = Begin Step + Step + End Step，做加速用",
	"VM_DestroyInstance":         "直接销毁一个实例（会触发它的 Destroy 事件）",
	"VM_GetCardProp":             "按属性名读一张卡的单值（返回类型取决于属性名）：存档类 shape/level/skill/max_level/max_shape，卡池类 plant_type/feature_type/target_card/cost/cooldown；失败返回 undefined",
	"VM_CanPlace":                "按游戏正规种植规则判断某格能不能种这张卡（地形/障碍/水域莲叶/护盾层/底座卡/替换开关全都算），1=能 0=不能",
	"VM_CallFunc":                "按名字调用独立字典 global._VM_call_dict 里的函数（名字对编译器只是字符串，不校验）；第一个参数是函数名，后面是实参，最多 15 个",
	"VM_FuncExists":              "字典里有没有这个函数，1=有 0=没有（VM_CallFunc 的配套）",
	"VM_FuncDesc":                "返回字典里登记的该函数说明字符串，没有登记就返回空串（VM_CallFunc 的配套）",
	"VM_SpriteExists":            "贴图现在真的可用吗（项目资源→VM临时缓存→VM永久缓存→全局缓存，并排除 get_load_sprite 的空白占位图），1=可用 0=不可用",
	"VM_AliasSpritePerm":         "把名字永久指向一个已在永久缓存里的贴图（存精灵 id，进房间不会被清，[reloadmod] 时释放），配合 VM_SpriteExists 做「内置有就用内置、没有才外置覆盖」",
	"VM_ArrayExists":             "全局二维表存不存在（存在且是数组）1=有 0=没有；表名指 global.<表名>",
	"VM_CellCount":               "表里第 i 列 j 行那一格的元素个数；表不存在 / 不是数组 / i,j 越界 → -1，所以 n = VM_CellCount(...) 之后直接 while 是安全的",
	"VM_CellItem":                "表里第 i 列 j 行那一格的第 k 个元素（k 从 0 起）；越界 → -1",
	"VM_CellContains":            "表里第 i 列 j 行那一格有没有这个值，1=有 0=没有；越界 → -1",
	"VM_ArrayContains":           "命名一维数组里找值（和 VM_ArrayADD / VM_ArraySet / VM_ArrayGet 同一套，不是 enemy_array 那种全局二维表）：返回它第一次出现的下标（从 0 起）；没有这个值 / 数组不存在 → -1",
	"VM_InstArrayExists":         "实例身上有没有这个一维数组，1=有 0=没有",
	"VM_InstArraySize":           "实例身上一维数组的长度；实例不存在 / 变量不存在 → -1",
	"VM_InstArrayItem":           "实例身上一维数组的第 k 个元素；越界 → -1",
	"VM_InstArraySet":            "改实例身上一维数组的第 k 个元素；失败 → -1",
	"VM_InstArrayAdd":            "往实例身上一维数组的末尾追加一个值；失败 → -1",
	"VM_InstArrayDel":            "删掉实例身上一维数组的第 k 个元素；失败 → -1",
	"VM_InstArrayClear":          "清空实例身上的一维数组；失败 → -1",
	"VM_InstArrayContains":       "实例身上一维数组里有没有这个值，1=有 0=没有；实例/变量不存在 → -1",
	"VM_HomingBulletAdd":         "追踪弹管理器：贴图,缩放,x,y,速度,伤害,可命中类型,销毁对象,mod名字,销毁贴图,模式（模式 0=只追全场最左的敌人，1=优先本行正前方 150 像素内血最多的敌人；每帧重新索敌，命中按 normal 走护盾，打中一个就消失）",
	"VM_DamageEnemy":             "给敌人造成伤害（敌人id,伤害,伤害类型）：走敌人自己的受击事件，闪白 / 受击音效 / 护盾判定全都对，比直接改 hp 正确。伤害类型 normal=有盾只打盾，pierce=盾血一起掉，其它=无视护盾",
	"VM_DamageEnemyAsh":          "灰烬伤害（敌人id,伤害,伤害类型）：伤害接得下就正常结算，接不下就一击必杀、原地换成 obj_mouse_ash_death（不看护盾，同原版大力神）",
	"VM_BulletScreenAdd_Ex":      "屏幕弹幕管理器（带卡片效果）：贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度。标志数值=这颗子弹还能接受哪些类别的卡片效果：bit1=过火(换火弹贴图+放大1.8+音效)、bit2=解冻(换包子贴图+音效，不启用增益、冰冻帧清零、之后或上bit1)、4/8/16…=自定义（效果全看卡片字段）。子弹进格子中心带时与卡片的 bullet_flag 取且运算，>0 就应用并消位，同类卡只生效一次。角度=出生角度（后向子弹传180）",
	"VM_BulletScreenAdd_Exs":     "同 VM_BulletScreenAdd_Ex，最后多两个行渐变参数：目标行y（要拐去那一行的世界 y，不想渐变传 -1）、靠拢比例（每帧朝目标 y 靠拢的比例，原版水管弹用 0.15，别传 0）。渐变到位（|目标行y - y| <= 8）自动吸附并停掉渐变，之后是纯直线；途中经过的行照常结算。20 个参数都要写满",
	"VM_GetInfo":                 "变长（3~16 个参数）：按注册表逐级查一个对象的信息，写法 VM_GetInfo(类别, id, 字段1[, 字段2, ...])。类别只有 card/enemy/weapon/gem；字符串参数=取字段，数字参数=取下标（落在 ds_map 上自动按字符串键查，注册表的 shapes/upgrades 键就是 \"0\"/\"3\"）；查到数组/结构体本身返回 undefined，只有数字/字符串才返回。card 读 global.plant_registry（大池子，不是存档那份）",
	"VM_CatInRow":                "该行第一只猫（obj_cat，海底图的螃蟹同对象）的实例 id；没有返回 -1",
	"VM_MapObj":                  "变长（2~16 个参数）：查格子上有没有地图物品，VM_MapObj(列, 行[, \"名字\"])。读 global.cell_terrain_flag 的位：obstacle/mucus/lava/seawater/barrier/fog/cloud/wind_tunnel；\"all\" 或 \"\"=任意一种；\"list\"=返回逗号分隔名字串；列或行传 -1 = 该方向不限；名字不认识返回 -1；战斗中表由 obj_battle 每帧重建，战斗外返回 0",
	"VM_GetInstanceCount":        "某个对象类的实例个数（类名就是游戏里的 object 名 obj_xxxx，必须写全）；名字不存在 / 不是对象 → -1",
	"VM_GetInstanceAt":           "某个对象类第 k 个实例的 id（k 从 0 起）；越界 / 名字不存在 → -1",
}

// funcSigs 函数原型（由 sync_vmfuncs.py 从 help.md 函数表自动提取）
// 悬停提示的标题行，例如 VM_GetProp(实例ID, "属性名")
var funcSigs = map[string]string{
	"VM_AliasSprite": "VM_AliasSprite(\"新名\", \"已有名\")",
	"VM_AliasSpritePerm": "VM_AliasSpritePerm(\"新名\",\"已有名\")",
	"VM_ArrayADD": "VM_ArrayADD(\"数组名\",值)",
	"VM_ArrayClear": "VM_ArrayClear(\"数组名\")",
	"VM_ArrayClearAll": "VM_ArrayClearAll()",
	"VM_ArrayContains": "VM_ArrayContains(\"数组名\",值)",
	"VM_ArrayDel": "VM_ArrayDel(\"数组名\",下标)",
	"VM_ArrayExists": "VM_ArrayExists(\"表名\")",
	"VM_ArrayGet": "VM_ArrayGet(\"数组名\",下标)",
	"VM_ArraySet": "VM_ArraySet(\"数组名\",下标,值)",
	"VM_ArraySize": "VM_ArraySize(\"数组名\")",
	"VM_BanAllCard": "VM_BanAllCard()",
	"VM_BanCard": "VM_BanCard(\"卡名\")",
	"VM_BanGem": "VM_BanGem(\"宝石名\")",
	"VM_BanShield": "VM_BanShield()",
	"VM_BanSuperWeapon": "VM_BanSuperWeapon()",
	"VM_BanWeapon": "VM_BanWeapon()",
	"VM_BulletScreenAdd": "VM_BulletScreenAdd(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字)",
	"VM_BulletScreenAdd_Ex": "VM_BulletScreenAdd_Ex(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度)",
	"VM_BulletScreenAdd_Exs": "VM_BulletScreenAdd_Exs(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度,目标行y,靠拢比例)",
	"VM_GetInfo": "VM_GetInfo(\"card\", \"small_fire\", \"shapes\", 0, \"upgrades\", 3, \"atk\")",
	"VM_CatInRow": "VM_CatInRow(行号)",
	"VM_MapObj": "VM_MapObj(列,行,\"名字\")",
	"VM_GetInstanceCount": "VM_GetInstanceCount(\"obj_cat\")",
	"VM_GetInstanceAt": "VM_GetInstanceAt(\"obj_cat\",k)",
	"VM_CalcCardSlotProp": "VM_CalcCardSlotProp(\"name\", \"prop\", op, val)",
	"VM_CallFunc": "VM_CallFunc(\"函数名\", 参数...)",
	"VM_CanPlace": "VM_CanPlace(\"卡名\",列,行)",
	"VM_CannelBanCard": "VM_CannelBanCard(\"卡名\")",
	"VM_Ceil": "VM_Ceil(值)",
	"VM_CellContains": "VM_CellContains(\"表名\",i,j,值)",
	"VM_CellCount": "VM_CellCount(\"表名\",i,j)",
	"VM_CellItem": "VM_CellItem(\"表名\",i,j,k)",
	"VM_ClearMapObjects": "VM_ClearMapObjects(列,行,\"物件名\")",
	"VM_ClearPlants": "VM_ClearPlants(列,行)",
	"VM_ClearPlantsByType": "VM_ClearPlantsByType(\"卡名\")",
	"VM_CompactColumn": "VM_CompactColumn(列)",
	"VM_CompactColumnRev": "VM_CompactColumnRev(列)",
	"VM_CompactRow": "VM_CompactRow(行)",
	"VM_CompactRowRev": "VM_CompactRowRev(行)",
	"VM_CreateButton": "VM_CreateButton(x,y,\"精灵名\",缩放, idle, hover, press)",
	"VM_CreateInstance": "VM_CreateInstance(\"对象名\",x,y)",
	"VM_CreatePlatform": "VM_CreatePlatform(列,行,宽,高,轴向,距离,停顿帧,贴图)",
	"VM_DamageEnemy": "VM_DamageEnemy(敌人id, 伤害, 伤害类型)",
	"VM_DamageEnemyAsh": "VM_DamageEnemyAsh(敌人id, 伤害, 伤害类型)",
	"VM_DestroyInstance": "VM_DestroyInstance(实例ID)",
	"VM_DrawSpriteExt": "VM_DrawSpriteExt(\"贴图名\",子图,x,y,xscale,yscale,角度,alpha)",
	"VM_EnemyInRange": "VM_EnemyInRange(行1,行2,列1,列2,\"类型\")",
	"VM_Floor": "VM_Floor(值)",
	"VM_FuncDesc": "VM_FuncDesc(\"函数名\")",
	"VM_FuncExists": "VM_FuncExists(\"函数名\")",
	"VM_GameLose": "VM_GameLose()",
	"VM_GameWin": "VM_GameWin()",
	"VM_GetCardProp": "VM_GetCardProp(\"卡名\",\"属性名\")",
	"VM_GetCardSlotCount": "VM_GetCardSlotCount()",
	"VM_GetCurCard": "VM_GetCurCard()",
	"VM_GetEnemyCount": "VM_GetEnemyCount()",
	"VM_GetFlame": "VM_GetFlame()",
	"VM_GetHomingTarget": "VM_GetHomingTarget(\"类型\")",
	"VM_GetInstancesInRange": "VM_GetInstancesInRange(\"数组名\",行1,行2,列1,列2,\"enemy\"或\"card\",\"筛选\")",
	"VM_GetKeyDown": "VM_GetKeyDown(\"键名\")",
	"VM_GetKeyPressed": "VM_GetKeyPressed(\"键名\")",
	"VM_GetKilledProp": "VM_GetKilledProp(\"属性名\")",
	"VM_GetLastBoss": "VM_GetLastBoss()",
	"VM_GetLastBossNewState": "VM_GetLastBossNewState()",
	"VM_GetLastBossOldState": "VM_GetLastBossOldState()",
	"VM_GetLastBossStateChangeId": "VM_GetLastBossStateChangeId()",
	"VM_GetLastClickedButton": "VM_GetLastClickedButton()",
	"VM_GetLastCreatedCard": "VM_GetLastCreatedCard()",
	"VM_GetLastCreatedEnemy": "VM_GetLastCreatedEnemy()",
	"VM_GetLastDestroyedCard": "VM_GetLastDestroyedCard()",
	"VM_GetLastIdlePlatform": "VM_GetLastIdlePlatform()",
	"VM_GetLastKilledEnemy": "VM_GetLastKilledEnemy()",
	"VM_GetLoadedSpriteName": "VM_GetLoadedSpriteName(序号)",
	"VM_GetMouseCol": "VM_GetMouseCol()",
	"VM_GetMousePressed": "VM_GetMousePressed(按键)",
	"VM_GetMouseRow": "VM_GetMouseRow()",
	"VM_GetMouseX": "VM_GetMouseX()",
	"VM_GetMouseY": "VM_GetMouseY()",
	"VM_GetPlantAt": "VM_GetPlantAt(列,行,\"层级\")",
	"VM_GetPlantCount": "VM_GetPlantCount()",
	"VM_GetPlantCountAt": "VM_GetPlantCountAt(列,行,\"类型\")",
	"VM_GetPreviewCard": "VM_GetPreviewCard()",
	"VM_GetProp": "VM_GetProp(实例ID, \"属性名\")",
	"VM_GetSubwave": "VM_GetSubwave()",
	"VM_GetTerrain": "VM_GetTerrain(列, 行)",
	"VM_GetTimeLimit": "VM_GetTimeLimit()",
	"VM_GetWave": "VM_GetWave()",
	"VM_HomingBulletAdd": "VM_HomingBulletAdd(贴图,缩放,x,y,速度,伤害,可命中类型,销毁对象,mod名字,销毁贴图,模式)",
	"VM_InstArrayAdd": "VM_InstArrayAdd(id,\"数组名\",值)",
	"VM_InstArrayClear": "VM_InstArrayClear(id,\"数组名\")",
	"VM_InstArrayContains": "VM_InstArrayContains(id,\"数组名\",值)",
	"VM_InstArrayDel": "VM_InstArrayDel(id,\"数组名\",k)",
	"VM_InstArrayExists": "VM_InstArrayExists(id,\"数组名\")",
	"VM_InstArrayItem": "VM_InstArrayItem(id,\"数组名\",k)",
	"VM_InstArraySet": "VM_InstArraySet(id,\"数组名\",k,值)",
	"VM_InstArraySize": "VM_InstArraySize(id,\"数组名\")",
	"VM_IsDestroyed": "VM_IsDestroyed(实例ID)",
	"VM_IsUndefined": "VM_IsUndefined(值)",
	"VM_LoadSound": "VM_LoadSound(\"路径\")",
	"VM_LoadSprite": "VM_LoadSprite(\"文件名\")",
	"VM_LoadSpriteFrames": "VM_LoadSpriteFrames(\"文件名\", 帧数)",
	"VM_LoadSpriteFrames_Ex": "VM_LoadSpriteFrames_Ex(\"文件名\", 帧数, x, y)",
	"VM_LoadSpritePerm_Ex": "VM_LoadSpritePerm_Ex(\"文件名\", 帧数, 原点X, 原点Y)",
	"VM_PlaySound": "VM_PlaySound(\"音效名\")",
	"VM_Random": "VM_Random(min, max)",
	"VM_RefreshPlatformSnapshots": "VM_RefreshPlatformSnapshots()",
	"VM_RunStep": "VM_RunStep(实例, 轮数)",
	"VM_SetCardLevelCap": "VM_SetCardLevelCap(等级)",
	"VM_SetCardProp": "VM_SetCardProp(列,行,\"卡名\",\"属性\",值)",
	"VM_SetCardShapeCap": "VM_SetCardShapeCap(等级)",
	"VM_SetCardSkillCap": "VM_SetCardSkillCap(等级)",
	"VM_SetCardSlotProp": "VM_SetCardSlotProp(\"name\", \"prop\", val)",
	"VM_SetDrawSlot": "VM_SetDrawSlot(槽位,\"贴图名\",x,y,alpha)",
	"VM_SetDrawSlotEx": "VM_SetDrawSlotEx(槽位,\"贴图名\",x,y,alpha,角度,xscale,yscale)",
	"VM_SetDrawSlotEx_front": "VM_SetDrawSlotEx_front(槽位,\"贴图名\",x,y,alpha,角度,xscale,yscale)",
	"VM_SetDrawSlot_front": "VM_SetDrawSlot_front(槽位,\"贴图名\",x,y,alpha)",
	"VM_SetEnemyProp": "VM_SetEnemyProp(\"类型\",\"属性\",值)",
	"VM_SetEventEnabled": "VM_SetEventEnabled(0或1)",
	"VM_SetFlame": "VM_SetFlame(数量)",
	"VM_SetMapBackground": "VM_SetMapBackground(\"贴图名\", 步长)",
	"VM_SetMaxSlots": "VM_SetMaxSlots(数量)",
	"VM_SetNoticeStyle": "VM_SetNoticeStyle(缩放, R, G, B)",
	"VM_SetPlatformParams": "VM_SetPlatformParams(实例,轴,距离,停顿,方向)",
	"VM_SetProp": "VM_SetProp(实例ID, \"属性名\", 值)",
	"VM_SetRowFeature": "VM_SetRowFeature(行, \"属性\")",
	"VM_SetShovelFlameRate": "VM_SetShovelFlameRate(系数)",
	"VM_SetTerrain": "VM_SetTerrain(列, 行, \"类型\")",
	"VM_SetTimeLimit": "VM_SetTimeLimit(帧数)",
	"VM_SetWave": "VM_SetWave(波次,子波次)",
	"VM_SetWaveAuto": "VM_SetWaveAuto(0或1)",
	"VM_ShellPrint": "VM_ShellPrint(...)",
	"VM_ShowNotice": "VM_ShowNotice(...)",
	"VM_ShowNoticeDur": "VM_ShowNoticeDur(\"消息\", ..., 帧数)",
	"VM_Slot_add": "VM_Slot_add(\"卡名\")",
	"VM_SpawnBatMouse": "VM_SpawnBatMouse(列,行)",
	"VM_SpawnBoss": "VM_SpawnBoss(\"BOSS类型\",行,血量)",
	"VM_SpawnCats": "VM_SpawnCats(0或1)",
	"VM_SpawnEnemy": "VM_SpawnEnemy(\"敌人类型\",行,血量)",
	"VM_SpawnObject": "VM_SpawnObject(\"物件名\",列,行)",
	"VM_SpawnPlant": "VM_SpawnPlant(\"卡名\",列,行,外形,星级,技能)",
	"VM_SpawnPlantsRandom": "VM_SpawnPlantsRandom(x,y,w,h, shape,level,skill, card1,...,card9)",
	"VM_SpriteExists": "VM_SpriteExists(\"贴图名\")",
	"VM_SwapPlantRects": "VM_SwapPlantRects(x1,y1,w,h, x2,y2)",
	"VM_SwapPlants": "VM_SwapPlants(列1,行1,列2,行2)",
	"VM_WakePlants": "VM_WakePlants(列,行)",
	"VM_conveyor_belt_able": "VM_conveyor_belt_able(0或1)",
}


// _VM_FRAME 每帧执行（help.md 事件块表 ⚠️ 注）：这些重函数在帧回调中使用时仅提示
var frameHeavy = set(
	"VM_SpawnObject", "VM_SpawnPlant", "VM_SpawnEnemy", "VM_SpawnBoss",
	"VM_SpawnPlantsRandom", "VM_CreatePlatform", "VM_CreateButton",
)

// blockHints 事件块提示（help.md 事件块表的触发时机 + 该块中可调用的函数）
type blockHint struct {
	Desc  string   `json:"desc"`  // 触发时机
	Funcs []string `json:"funcs"` // 该块中可调用的函数
}

var blockHints = map[string]blockHint{
	"_VM_ROOM_READY_ENTRY":    {"进入准备室（设规则）", nil},
	"_VM_BATTLE_START":        {"战斗开始（造地图）", nil},
	"_VM_WAVE_START":          {"新一波开始", []string{"VM_GetWave()"}},
	"_VM_WAVE_END":            {"当前波结束", []string{"VM_GetWave()"}},
	"_VM_SUBWAVE_START":       {"新子波开始", []string{"VM_GetSubwave()"}},
	"_VM_SUBWAVE_END":         {"子波结束", []string{"VM_GetSubwave()"}},
	"_VM_CARD_CREATED":        {"卡片被种下", []string{"VM_GetLastCreatedCard()"}},
	"_VM_CARD_DESTROYED":      {"卡片被销毁", []string{"VM_GetLastDestroyedCard()", "VM_GetKilledProp()"}},
	"_VM_CARD_DAMAGED":        {"卡片受伤", nil},
	"_VM_CARD_PREVIEW_PICKED": {"卡片被选取到手槽", []string{"VM_GetPreviewCard()"}},
	"_VM_ENEMY_SPAWNED":       {"敌人出现", []string{"VM_GetLastCreatedEnemy()"}},
	"_VM_ENEMY_KILLED":        {"敌人死亡", []string{"VM_GetLastKilledEnemy()", "VM_GetKilledProp()"}},
	"_VM_ENEMY_DAMAGED":       {"敌人受伤", nil},
	"_VM_BOSS_STATE_CHANGE":   {"BOSS 状态改变", []string{"VM_GetLastBossStateChangeId()", "VM_GetLastBossOldState()", "VM_GetLastBossNewState()"}},
	"_VM_PLAYER_DAMAGED":      {"玩家受伤", nil},
	"_VM_PLATFORM_IDLE_END":   {"平台空闲结束", []string{"VM_GetLastIdlePlatform()"}},
	"_VM_MOUSE_LEFT":          {"鼠标左键按下（单帧）", nil},
	"_VM_MOUSE_RIGHT":         {"鼠标右键按下（单帧）", nil},
	"_VM_KEY_PRESSED":         {"键盘按键按下（单帧）", []string{"VM_GetKeyPressed()"}},
	"_VM_BUTTON_CLICKED":      {"按钮被点击", []string{"VM_GetLastClickedButton()"}},
	"_VM_FRAME":               {"每帧执行，禁止写复杂逻辑", nil},
	"_VM_TIMER_5f":            {"每 5 帧", nil},
	"_VM_TIMER_10f":           {"每 10 帧", nil},
	"_VM_TIMER_15f":           {"每 15 帧", nil},
	"_VM_TIMER_30f":           {"每 30 帧", nil},
	"_VM_TIMER_60f":           {"每 60 帧", nil},
	"_OBJECT_CFG":             {"对象配置块：加载贴图等，bin 加载时执行一次（写 mod 卡/武器/子弹/特效/敌人时用）", []string{"VM_LoadSpritePerm_Ex()"}},
	"_OBJECT_CREATE":          {"对象创建时执行一次", []string{"VM_GetCurCard()", "VM_SetProp()"}},
	"_OBJECT_STEP":            {"对象每帧执行", []string{"VM_GetProp()", "VM_SetProp()", "VM_RunStep()"}},
	"_OBJECT_DRAW":            {"对象绘制时执行（只有这里能画东西）", []string{"VM_DrawSpriteExt()"}},
	"_OBJECT_DESTROY":         {"对象销毁时执行", []string{"VM_ShellPrint()"}},
	"_OBJECT_MOUSE_ENTER":     {"鼠标移入该实例", nil},
	"_OBJECT_MOUSE_LEAVE":     {"鼠标移出该实例", nil},
	"_OBJECT_CLICK":           {"鼠标左键点击该实例（每实例判定，和全局的 _VM_MOUSE_LEFT 不同）", nil},
}

// ---- 解析器 ----

type blockKind int

const (
	blockEvent  blockKind = iota // _VM_* / _OBJECT_* 事件块
	blockChain                   // if / elif 块
	blockElse                    // else 块
	blockWhile                   // while 循环块
	blockCustom                  // _DEFINE_BLOCK_* 自定义块
)

// isExecBlock 块内可以写语句（赋值 / 函数调用）的块
func isExecBlock(k blockKind) bool { return k == blockEvent || k == blockCustom }

type blockEnt struct {
	name string
	line int
	kind blockKind
}

type linter struct {
	diags        []Diagnostic
	stack        []blockEnt
	pendingChain bool // 最近闭合的是 if/elif 块，可接 elif/else
	loopDepth    int  // while 嵌套层级（break/continue 必须 > 0）
	customDefs   map[string]int
}

func (l *linter) errorf(line int, format string, args ...any) {
	l.diags = append(l.diags, Diagnostic{Line: line, Severity: "error", Message: fmt.Sprintf(format, args...)})
}

func (l *linter) warnf(line int, format string, args ...any) {
	l.diags = append(l.diags, Diagnostic{Line: line, Severity: "warning", Message: fmt.Sprintf(format, args...)})
}

// lintScript 对整段脚本做语法检查
func lintScript(code string) []Diagnostic {
	lines := splitLines(code)
	l := &linter{diags: []Diagnostic{}, customDefs: collectCustomDefs(lines)}
	for i, line := range lines {
		l.checkLine(i+1, line)
	}
	// 收尾：未闭合的块
	for i := len(l.stack) - 1; i >= 0; i-- {
		blk := l.stack[i]
		l.errorf(blk.line, "块 %s 缺少右花括号 }", blk.name)
	}
	return l.diags
}

// collectCustomDefs 先扫一遍全文记下自定义块定义（允许先调用后定义）
func collectCustomDefs(lines []string) map[string]int {
	defs := map[string]int{}
	for i, line := range lines {
		s := strings.TrimSpace(line)
		if n := identifierLen(s); n > 0 && isCustomBlockName(s[:n]) &&
			strings.HasPrefix(strings.TrimSpace(s[n:]), "{") {
			if _, dup := defs[s[:n]]; !dup {
				defs[s[:n]] = i + 1
			}
		}
	}
	return defs
}

func splitLines(code string) []string {
	code = strings.ReplaceAll(code, "\r\n", "\n")
	code = strings.ReplaceAll(code, "\r", "\n")
	return strings.Split(code, "\n")
}

// enclosingEventAt 返回第 line 行（1 起）所在的事件块；不在块内返回 nil。
// 光标所在行从第一个 } 处截断，光标停在收尾花括号上时仍视为块内。
func enclosingEventAt(code string, line int) *blockEnt {
	lines := splitLines(code)
	if line < 1 || line > len(lines) {
		return nil
	}
	l := &linter{diags: nil}
	for i := 1; i < line; i++ {
		l.checkLine(i, lines[i-1])
	}
	cur := lines[line-1]
	if idx := findBraceInExpr(cur); idx >= 0 {
		cur = cur[:idx]
	}
	l.checkLine(line, cur)
	for i := len(l.stack) - 1; i >= 0; i-- {
		if isExecBlock(l.stack[i].kind) {
			return &l.stack[i]
		}
	}
	return nil
}

// checkLine 处理一行：按 ; 分句（; 视作换行），字符串内的 ; 与 // 不受影响
func (l *linter) checkLine(lineno int, line string) {
	start := 0
	inStr := false
	for i := 0; i < len(line); i++ {
		c := line[i]
		if c == '"' {
			inStr = !inStr
			continue
		}
		if inStr {
			continue
		}
		if c == '/' && i+1 < len(line) && line[i+1] == '/' {
			// 注释开始，行剩余部分忽略
			l.checkSegment(lineno, line[start:i])
			return
		}
		if c == ';' {
			l.checkSegment(lineno, line[start:i])
			start = i + 1
		}
	}
	if inStr {
		l.errorf(lineno, "字符串缺少结束引号\"（表达式不允许跨行）")
	}
	l.checkSegment(lineno, line[start:])
}

// checkSegment 处理一个语句段（可含行内 { 语句 } 与收尾 }）
func (l *linter) checkSegment(lineno int, s string) {
	for {
		s = strings.TrimSpace(s)
		if s == "" {
			return
		}
		if strings.HasPrefix(s, "}") {
			s = strings.TrimSpace(s[1:])
			l.closeBlock(lineno)
			continue
		}
		rest := l.parseStmt(lineno, s)
		if rest == s {
			l.errorf(lineno, "无法识别的语句: %s", truncate(s, 40))
			return
		}
		s = rest
	}
}

func (l *linter) closeBlock(lineno int) {
	if len(l.stack) == 0 {
		l.errorf(lineno, "多余的右花括号 }")
		l.pendingChain = false
		return
	}
	top := l.stack[len(l.stack)-1]
	l.stack = l.stack[:len(l.stack)-1]
	switch top.kind {
	case blockEvent:
		l.pendingChain = false
	case blockCustom:
		l.pendingChain = false
	case blockChain:
		l.pendingChain = true
	case blockElse:
		l.pendingChain = false // else 之后链条终止，再出现 elif/else 视为游离
	case blockWhile:
		l.loopDepth--
		l.pendingChain = false // while 之后不能接 elif/else
	}
}

// enclosingEvent 返回最近的外层执行块（事件块 / 自定义块），无则 nil
func (l *linter) enclosingEvent() *blockEnt {
	for i := len(l.stack) - 1; i >= 0; i-- {
		if isExecBlock(l.stack[i].kind) {
			return &l.stack[i]
		}
	}
	return nil
}

// needInsideBlock 语句必须位于事件块内
func (l *linter) needInsideBlock(lineno int, what string) bool {
	if l.enclosingEvent() == nil {
		l.errorf(lineno, "%s必须写在事件块（_VM_*）内", what)
		return false
	}
	return true
}

// checkChainBind elif/else 必须绑定最近刚闭合的 if
func (l *linter) checkChainBind(lineno int, kw string) {
	if !l.pendingChain {
		l.errorf(lineno, "游离的 %s：必须紧跟刚闭合的 if/elif 块", kw)
	}
}

// parseStmt 解析一条语句，返回行内剩余未解析内容；无法识别时原样返回
func (l *linter) parseStmt(lineno int, s string) string {
	// if / elif / else：仅当后面跟 ( 或 { 等合法结构时才按条件语句解析；
	// 否则按普通标识符处理，如 `if = 1` 会命中保留字变量名检查
	if kw, ok := leadingKeyword(s); ok {
		rest := strings.TrimSpace(s[len(kw):])
		switch kw {
		case "if", "elif":
			if strings.HasPrefix(rest, "(") {
				return l.parseIfElif(lineno, s, kw)
			}
		case "else":
			if strings.HasPrefix(rest, "{") || strings.HasPrefix(rest, "if") {
				return l.parseElse(lineno, s)
			}
		case "while":
			if strings.HasPrefix(rest, "(") {
				return l.parseWhile(lineno, s)
			}
		}
	}
	// 事件块：_VM_XXX {
	if i := identifierLen(s); i > 0 {
		name := s[:i]
		t := strings.TrimSpace(s[i:])
		if strings.HasPrefix(t, "{") {
			if isCustomBlockName(name) {
				l.pendingChain = false
				if len(l.stack) > 0 {
					l.errorf(lineno, "自定义块 %s 必须定义在最外层", name)
				}
				l.stack = append(l.stack, blockEnt{name: name, line: lineno, kind: blockCustom})
				return t[1:]
			}
			if !eventBlocks[name] {
				if strings.HasPrefix(name, "_VM_") {
					l.errorf(lineno, "未知事件块 %s（见帮助→事件块表）", name)
				} else {
					l.errorf(lineno, "无法识别的语句: %s（代码必须写在 _VM_* 事件块中）", name)
				}
				return ""
			}
			l.pendingChain = false
			l.stack = append(l.stack, blockEnt{name: name, line: lineno, kind: blockEvent})
			return t[1:]
		}
		// 赋值（表达式内不含花括号：行内 `}` 属于块收尾，截断后留给外层处理）
		if strings.HasPrefix(t, "=") {
			l.pendingChain = false
			if !l.needInsideBlock(lineno, "赋值语句") {
				return ""
			}
			if reservedWords[name] {
				l.errorf(lineno, "%s 是保留字，不能用作变量名", name)
			}
			rhs := strings.TrimSpace(t[1:])
			if i := findBraceInExpr(rhs); i >= 0 {
				if strings.TrimSpace(rhs[:i]) == "" {
					l.errorf(lineno, "赋值语句缺少右侧值")
				} else {
					l.checkExprCalls(lineno, rhs[:i])
				}
				return rhs[i:] // 从 } 开始的剩余部分
			}
			if rhs == "" {
				l.errorf(lineno, "赋值语句缺少右侧值")
			} else {
				l.checkExprCalls(lineno, rhs)
			}
			return ""
		}
		// 函数调用
		if strings.HasPrefix(t, "(") {
			l.pendingChain = false
			if !l.needInsideBlock(lineno, "函数调用") {
				return ""
			}
			closeIdx := findMatchingParen(t, 0)
			if closeIdx < 0 {
				l.errorf(lineno, "函数 %s 调用缺少右括号 )", name)
				return ""
			}
			l.checkCustomOrCall(lineno, name, strings.TrimSpace(t[1:closeIdx]))
			return t[closeIdx+1:]
		}
	}
	// halt / exit（可跟行内收尾 }；`halt = 1` 走赋值分支命中保留字检查）
	if kw, ok := haltingKeyword(s); ok {
		l.pendingChain = false
		l.needInsideBlock(lineno, "halt/exit ")
		return strings.TrimSpace(s[len(kw):])
	}
	// break / continue（必须位于 while 循环内；`break = 1` 走赋值分支命中保留字检查）
	if kw, ok := loopKeyword(s); ok {
		l.pendingChain = false
		l.needInsideBlock(lineno, kw+" ")
		if l.loopDepth == 0 {
			l.errorf(lineno, "%s 必须写在 while 循环内", kw)
		}
		return strings.TrimSpace(s[len(kw):])
	}
	return s
}

// haltingKeyword 若 s 以 halt/exit 开头且后接空白/}/行尾（非赋值），返回关键字
func haltingKeyword(s string) (string, bool) {
	for _, kw := range []string{"halt", "exit"} {
		if strings.HasPrefix(s, kw) {
			if len(s) == len(kw) {
				return kw, true
			}
			c := s[len(kw)]
			if c == ' ' || c == '\t' || c == '}' {
				if strings.HasPrefix(strings.TrimSpace(s[len(kw):]), "=") {
					return "", false // 赋值，交给保留字检查
				}
				return kw, true
			}
			return "", false
		}
	}
	return "", false
}

// parseIfElif 解析 `if (条件) {` / `elif (条件) {`，返回 { 之后的剩余内容
func (l *linter) parseIfElif(lineno int, s, kw string) string {
	t := strings.TrimSpace(s[len(kw):])
	if t == "" || t[0] != '(' {
		l.errorf(lineno, "%s 后必须跟 (条件)", kw)
		return ""
	}
	closeIdx := findMatchingParen(t, 0)
	if closeIdx < 0 {
		l.errorf(lineno, "%s 条件缺少右括号 )", kw)
		return ""
	}
	if strings.TrimSpace(t[1:closeIdx]) == "" {
		l.errorf(lineno, "%s 条件不能为空", kw)
	} else {
		l.checkExprCalls(lineno, t[1:closeIdx])
	}
	after := strings.TrimSpace(t[closeIdx+1:])
	if !strings.HasPrefix(after, "{") {
		l.errorf(lineno, "%s 后必须跟 {", kw)
		return ""
	}
	if !l.needInsideBlock(lineno, kw+" ") {
		return ""
	}
	if kw == "elif" {
		l.checkChainBind(lineno, kw)
	}
	l.pendingChain = false
	l.stack = append(l.stack, blockEnt{name: kw, line: lineno, kind: blockChain})
	return after[1:]
}

// loopKeyword 若 s 以 break/continue 开头且后接空白/}/行尾（非赋值），返回关键字
func loopKeyword(s string) (string, bool) {
	for _, kw := range []string{"break", "continue"} {
		if strings.HasPrefix(s, kw) {
			if len(s) == len(kw) {
				return kw, true
			}
			c := s[len(kw)]
			if c == ' ' || c == '\t' || c == '}' {
				if strings.HasPrefix(strings.TrimSpace(s[len(kw):]), "=") {
					return "", false // 赋值，交给保留字检查
				}
				return kw, true
			}
			return "", false
		}
	}
	return "", false
}

// parseWhile 解析 `while (条件) {`，返回 { 之后的剩余内容。
// 与 parseIfElif 的区别：while 块闭合后不能接 elif/else。
func (l *linter) parseWhile(lineno int, s string) string {
	t := strings.TrimSpace(s[len("while"):])
	if t == "" || t[0] != '(' {
		l.errorf(lineno, "while 后必须跟 (条件)")
		return ""
	}
	closeIdx := findMatchingParen(t, 0)
	if closeIdx < 0 {
		l.errorf(lineno, "while 条件缺少右括号 )")
		return ""
	}
	if strings.TrimSpace(t[1:closeIdx]) == "" {
		l.errorf(lineno, "while 条件不能为空")
	} else {
		l.checkExprCalls(lineno, t[1:closeIdx])
	}
	after := strings.TrimSpace(t[closeIdx+1:])
	if !strings.HasPrefix(after, "{") {
		l.errorf(lineno, "while 后必须跟 {")
		return ""
	}
	if !l.needInsideBlock(lineno, "while ") {
		return ""
	}
	l.pendingChain = false
	l.loopDepth++
	l.stack = append(l.stack, blockEnt{name: "while", line: lineno, kind: blockWhile})
	return after[1:]
}

// parseElse 解析 `else {` 或 `else if (条件) {`。
// 注意：`else if` 与 elif 等价（编译器实测），其块闭合后可继续接 elif/else；
// 只有纯 `else` 块闭合后链条才终结。
func (l *linter) parseElse(lineno int, s string) string {
	t := strings.TrimSpace(s[4:])
	isElseIf := false
	if strings.HasPrefix(t, "if") && (len(t) == 2 || isSpaceOrParen(t[2])) {
		t2 := strings.TrimSpace(t[2:])
		if t2 == "" || t2[0] != '(' {
			l.errorf(lineno, "else if 后必须跟 (条件)")
			return ""
		}
		closeIdx := findMatchingParen(t2, 0)
		if closeIdx < 0 {
			l.errorf(lineno, "else if 条件缺少右括号 )")
			return ""
		}
		if strings.TrimSpace(t2[1:closeIdx]) == "" {
			l.errorf(lineno, "else if 条件不能为空")
		} else {
			l.checkExprCalls(lineno, t2[1:closeIdx])
		}
		t = strings.TrimSpace(t2[closeIdx+1:])
		isElseIf = true
	}
	if !strings.HasPrefix(t, "{") {
		l.errorf(lineno, "else 后必须跟 {")
		return ""
	}
	if !l.needInsideBlock(lineno, "else ") {
		return ""
	}
	l.checkChainBind(lineno, "else")
	l.pendingChain = false
	if isElseIf {
		// else if 块等价 elif：闭合后可继续绑定 elif/else
		l.stack = append(l.stack, blockEnt{name: "else if", line: lineno, kind: blockChain})
	} else {
		l.stack = append(l.stack, blockEnt{name: "else", line: lineno, kind: blockElse})
	}
	return t[1:]
}

// checkExprCalls 扫描表达式中的函数调用（含嵌套），逐个校验
func (l *linter) checkExprCalls(lineno int, expr string) {
	inStr := false
	for i := 0; i < len(expr); i++ {
		c := expr[i]
		if c == '"' {
			inStr = !inStr
			continue
		}
		if inStr {
			continue
		}
		// 编译器支持 && ||（2026-09-22 起），但单个 & | 仍是错误
		if c == '&' || c == '|' {
			if i+1 < len(expr) && expr[i+1] == c {
				i++ // 跳过 && / || 的第二个字符
				continue
			}
			if c == '&' {
				l.errorf(lineno, "不支持单个 &（逻辑与请用 &&）")
			} else {
				l.errorf(lineno, "不支持单个 |（逻辑或请用 ||）")
			}
			return
		}
		if c == '_' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' {
			j := i + 1
			for j < len(expr) {
				d := expr[j]
				if d == '_' || d >= 'a' && d <= 'z' || d >= 'A' && d <= 'Z' || d >= '0' && d <= '9' {
					j++
					continue
				}
				break
			}
			k := j
			for k < len(expr) && (expr[k] == ' ' || expr[k] == '\t') {
				k++
			}
			if k < len(expr) && expr[k] == '(' {
				name := expr[i:j]
				closeIdx := findMatchingParen(expr, k)
				if closeIdx < 0 {
					l.errorf(lineno, "函数 %s 调用缺少右括号 )", name)
					return
				}
				argsStr := expr[k+1 : closeIdx]
				l.checkCall(lineno, name, argsStr)
				l.checkExprCalls(lineno, argsStr)
				i = closeIdx
				continue
			}
			i = j - 1
		}
	}
}

// checkCustomOrCall 自定义块名走自定义块规则，其余走函数表
func (l *linter) checkCustomOrCall(lineno int, name, args string) {
	if isCustomBlockName(name) {
		l.checkCustomCall(lineno, name, args)
		return
	}
	l.checkCall(lineno, name, args)
}

// checkCustomCall 自定义块调用：不能带参数、不能在自定义块里再调块、必须已定义
func (l *linter) checkCustomCall(lineno int, name, args string) {
	if args != "" {
		l.errorf(lineno, "自定义块 %s 不能带参数", name)
	}
	if l.inCustomBlock() {
		l.errorf(lineno, "自定义块里不能再调块（%s）", name)
	}
	if _, ok := l.customDefs[name]; !ok {
		l.errorf(lineno, "自定义块 %s 未定义（写成 %s { ... }）", name, name)
	}
}

// inCustomBlock 当前是否写在自定义块里
func (l *linter) inCustomBlock() bool {
	for i := len(l.stack) - 1; i >= 0; i-- {
		switch l.stack[i].kind {
		case blockCustom:
			return true
		case blockEvent:
			return false
		}
	}
	return false
}

// checkCall 校验函数名、参数个数、字符串枚举、放置要求
func (l *linter) checkCall(lineno int, name, argsStr string) {
	ev := l.enclosingEvent()
	if ev == nil {
		l.errorf(lineno, "函数调用必须写在事件块（_VM_*）内")
		return
	}
	spec, ok := funcTable[name]
	if !ok {
		l.errorf(lineno, "未知函数 %s（见帮助→函数表）", name)
		return
	}
	// _VM_FRAME 每帧执行（help.md 事件块表 ⚠️ 注）
	if ev.name == "_VM_FRAME" && frameHeavy[name] {
		l.warnf(lineno, "_VM_FRAME 每帧执行，禁止写复杂逻辑（%s）", name)
	}
	// 参数个数
	args := splitArgs(argsStr)
	n := len(args)
	if argsStr == "" {
		n = 0
	}
	// 参数中的嵌套函数调用
	l.checkExprCalls(lineno, argsStr)
	if n < spec.min {
		l.errorf(lineno, "函数 %s 至少需要 %d 个参数（当前 %d）", name, spec.min, n)
		return
	}
	if spec.max >= 0 && n > spec.max {
		l.errorf(lineno, "函数 %s 最多 %d 个参数（当前 %d）", name, spec.max, n)
		return
	}
	// 参数类型检查（编译器强制；仅检查字面量，变量/表达式跳过）
	for i := 0; i < len(args) && i < len(spec.types); i++ {
		want := spec.types[i]
		if want == ptAny {
			continue
		}
		got, known := literalType(args[i])
		if !known {
			continue // 变量/表达式，无法静态判断
		}
		ok := want == got || (want == ptFloat && got == ptInt) // int→float 自动提升
		if !ok {
			l.errorf(lineno, "函数 %s 第 %d 个参数应为 %s，得到 %s", name, i+1, want.name(), got.name())
		}
	}
	// 字符串枚举校验（编译器强制的为 error，仅文档约定的为 warning）
	for idx, e := range spec.enums {
		if idx >= len(args) {
			continue
		}
		v, isStr := strLit(args[idx])
		if !isStr {
			continue // 非字符串字面量（变量/表达式），跳过
		}
		check := v
		if e.ci {
			check = strings.ToLower(v)
		}
		if !e.allow[check] {
			msg := fmt.Sprintf("未知%s \"%s\"（见帮助→参数校验附录）", e.label, v)
			if e.sev == "warning" {
				l.warnf(lineno, "%s", msg)
			} else {
				l.errorf(lineno, "%s", msg)
			}
		}
	}
}

// ---- 参数类型识别 ----

var intRe = regexp.MustCompile(`^-?\d+$`)
var floatRe = regexp.MustCompile(`^-?(\d+\.\d*|\.\d+)$`)

// literalType 识别字面量参数的类型；表达式/变量返回 known=false
func literalType(s string) (paramType, bool) {
	if _, isStr := strLit(s); isStr {
		return ptString, true
	}
	if intRe.MatchString(s) {
		return ptInt, true
	}
	if floatRe.MatchString(s) {
		return ptFloat, true
	}
	return ptAny, false
}

// ---- 词法辅助 ----

func isSpaceOrParen(c byte) bool {
	return c == ' ' || c == '\t' || c == '('
}

// leadingKeyword 若 s 以 if/elif/else/while 开头且后接边界（空白/括号/花括号/行尾），返回关键字
func leadingKeyword(s string) (string, bool) {
	for _, kw := range []string{"if", "elif", "else", "while"} {
		if strings.HasPrefix(s, kw) {
			if len(s) == len(kw) {
				return kw, true
			}
			c := s[len(kw)]
			if c == ' ' || c == '\t' || c == '(' || c == '{' {
				return kw, true
			}
			return "", false
		}
	}
	return "", false
}

// identifierLen 返回开头的标识符长度（字母/下划线开头，后接字母数字下划线），不是标识符返回 0
func identifierLen(s string) int {
	if s == "" {
		return 0
	}
	c := s[0]
	if !(c == '_' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') {
		return 0
	}
	i := 1
	for i < len(s) {
		c := s[i]
		if c == '_' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' {
			i++
			continue
		}
		break
	}
	return i
}

// findBraceInExpr 找表达式中第一个顶层 `}`（字符串内的忽略），用于截断行内块收尾；无则返回 -1
func findBraceInExpr(s string) int {
	inStr := false
	for i := 0; i < len(s); i++ {
		if s[i] == '"' {
			inStr = !inStr
			continue
		}
		if !inStr && s[i] == '}' {
			return i
		}
	}
	return -1
}

// findMatchingParen 从 s[openIdx]（应为 '('）找配对的 ')'，忽略字符串内的括号；找不到返回 -1
func findMatchingParen(s string, openIdx int) int {
	depth := 0
	inStr := false
	for i := openIdx; i < len(s); i++ {
		c := s[i]
		if c == '"' {
			inStr = !inStr
			continue
		}
		if inStr {
			continue
		}
		if c == '(' {
			depth++
		} else if c == ')' {
			depth--
			if depth == 0 {
				return i
			}
		}
	}
	return -1
}

// splitArgs 按顶层逗号切分参数（忽略字符串与嵌套括号内的逗号）
func splitArgs(s string) []string {
	var out []string
	depth := 0
	inStr := false
	start := 0
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '"':
			inStr = !inStr
		case !inStr && c == '(':
			depth++
		case !inStr && c == ')':
			depth--
		case !inStr && depth == 0 && c == ',':
			out = append(out, strings.TrimSpace(s[start:i]))
			start = i + 1
		}
	}
	if last := strings.TrimSpace(s[start:]); last != "" {
		out = append(out, last)
	}
	return out
}

// strLit 参数是否为字符串字面量，返回内容
func strLit(s string) (string, bool) {
	if len(s) >= 2 && s[0] == '"' && s[len(s)-1] == '"' {
		return s[1 : len(s)-1], true
	}
	return "", false
}

func truncate(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n] + "..."
}
