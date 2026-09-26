// 通用 mod 武器对象：src_mod 注册的所有武器共用此对象
// 武器 id 怎么给：
//   放置逻辑现在是「建实例 → 赋值 parent_player → 加属性名 weapon_id → 调一次它的步」；
//   老写法（创建前先把 id 写进 global._mod_pending_weapon_id）也还认。
// 只负责初始化和分发 VM 块，攻击逻辑全部在 .bin 里
weapon_id = variable_global_exists("_mod_pending_weapon_id") ? global._mod_pending_weapon_id : ""
weapon_info = get_weapon_info(weapon_id)
atk = 0
cycle = 60
image_xscale = 1.6
image_yscale = 1.6
image_speed = 0
parent_player = noone
grid_col = 0
grid_row = 0
// 偏移：每帧 Step 结束时把这两个量加到 x/y 上（obj 每帧会先按 parent_player 重置 x/y）
// mod 武器脚本用 VM_SetProp(self, "mod_prestep_dx", 值) 改；0 = 不偏移
mod_prestep_dx = 0
mod_prestep_dy = 0
// 动画配置（与 obj_card_parent 同款）：.bin 可在 _OBJECT_CREATE 用 VM_SetProp 覆盖
timer = 0
flash_speed = 5
idle_anim = 0
attack_anim = 0
state = CARD_STATE.IDLE;
if (is_struct(weapon_info)) {
	atk = weapon_info.atk
	cycle = weapon_info.cycle
	sprite_index = weapon_info.sprite
	if (get_gem_index("attack_gem") != -1 && variable_struct_exists(weapon_info, "atk_impact")) {
		atk = weapon_info.atk_impact[get_gem_level("attack_gem")]
	}
}

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（和 obj_card_mod 同款，只去掉武器用不上的 hp_change —— 武器没有 hp）
//   mod_step_enter_condition
//     ""            每帧进（默认；没设 / 写错都按这个兜底）
//     "mod"         battle_time % mod_step_enter_var == 0（全场共享，同 var 的武器同帧进）
//     "norm"        mod_tick_cycle 命中窗口表（不看敌人）
//     "norm_attack" 有敌人 且 命中窗口表；窗口表空 = 有敌人就每帧进（类型看 mod_enemy_types）
//     "wait"        mod_step_enter_var 每帧自减 1，减到 0 进；值非法 → 进一次 + 清空条件
//   窗口表 mod_step_enter_arr：负数 = 从一轮末尾倒数（-35 → 一轮长度-35）
//   命中窗口 → mod_step_enter_index = 窗口下标（0 起），否则 -1
//   区域 mod_step_enemy_area：每 4 个值一组 [上,下,左,右]，可写多组，任一组命中即可
//   类型 mod_enemy_types：空 = 任意；取值 "normal"/"obstacle"/"diver"/"air"/"dance"/"underground"
// ══════════════════════════════════════════════════════════════════════════
mod_tick_cycle = 0   // 【只读】每帧 +1，到一轮长度归零
mod_tick_max   = 0   // 一轮多长；0 = 用这把武器自己的 cycle
mod_tick_enemy = 0   // 【只读】有敌人才 +1

mod_has_enemy       = 0   // 【只读】1 = 索敌区域里有敌人
mod_enemy_check     = 0   // 1 = 让 obj 每帧帮这把武器索敌（"norm_attack" 自动开）
mod_step_enemy_area = []  // 数组 [上,下,左,右]；空 = [1,1,0,99]
mod_enemy_types     = []  // 数组，元素是类型名；空 = 任意类型

mod_step_enter_condition = ""   // "" / "mod" / "norm" / "norm_attack" / "wait"
// 盯梢（实现见 Step）：插件用命名数组 mod_watch 声明"要盯的属性名"，任一属性值变了，这一帧也进 _OBJECT_STEP
mod_changed_prop = ""    // 【只读】这一帧第一个发生变化的盯梢属性名（没变化 = ""）
mod_watch_last  = []     // 【obj 内部】上一帧 mod_watch 各属性的值（和数组一一对应）
mod_watch_changed = []  // 【obj 内部】这一帧发生变化的盯梢属性名（每个各进一次 _OBJECT_STEP）
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧
mod_step_enter_arr       = []   // 数组，开火窗口表（负数 = 从一轮末尾倒数）
mod_step_enter_index     = -1   // 【只读】命中的窗口下标；不是窗口 = -1

// 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
mod_countdown = 0    // 倒计时（帧）：> 0 时每帧 -1
mod_alpha_add = 0    // 每帧加到 image_alpha 上的量（负数 = 渐隐）；只在倒计时 > 0 时加

// 登记实例 + 跑 .bin 的 _OBJECT_CREATE **不在这里做**，挪到了 Step_0 顶部：
//   Create 在 instance_create_depth 里跑，那时放置逻辑还没执行 xxx.parent_player = 角色，
//   现在跑的话插件 _OBJECT_CREATE 里清 parent_player / 摆位置会被随后那行赋值盖掉。
//   （数值 / 贴图上面已按 weapon_info 设好，不受影响。）
_mod_initialized = false
