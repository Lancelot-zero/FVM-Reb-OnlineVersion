// 通用 mod 武器对象：src_mod 注册的所有武器共用此对象
// 武器 id 由放置逻辑在创建前写入 global._mod_pending_weapon_id
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
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧
mod_step_enter_arr       = []   // 数组，开火窗口表（负数 = 从一轮末尾倒数）
mod_step_enter_index     = -1   // 【只读】命中的窗口下标；不是窗口 = -1

// 创建时加入该武器的实例容器，并执行该武器虚拟机里的创建块
if (weapon_id != "" && variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, weapon_id)) {
	var _vm = global.mod_weapon_vms[? weapon_id];
	ds_list_add(_vm.instances, id);
	if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
		var _bak_last = global._VM_last_created_card;
		global._VM_last_created_card = id;
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_created_card = _bak_last;
	}
}
