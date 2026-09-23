// 通用 mod 植物对象：src_mod 注册的所有卡共用此对象
// 卡 id 由放置逻辑在创建前写入 global._mod_pending_card_id
event_inherited();
plant_id = variable_global_exists("_mod_pending_card_id") ? global._mod_pending_card_id : ""
event_user(0)

// ══════════════════════════════════════════════════════════════════════════
// mod 卡可设置（在卡的 _OBJECT_CREATE 里 VM_SetProp；数组用 VM_InstArray*）：
//   mod_tick_max             一轮多少帧；不设 = 用卡的 cycle
//   mod_enemy_check          1 = 让 obj 帮这张卡索敌（"norm_attack" 自动开）
//   mod_step_enemy_area      索敌区域，**每 4 个值一组** [上, 下, 左, 右]（以自己那格为中心各扩几格）
//                            4 个 = 1 个区域，8 个 = 2 个区域，12 个 = 3 个……任一组有敌人就算有
//                            不设 / 凑不满 4 个 = 默认一组 [1,1,0,99]（本行 ±1、自己那格往右到边界）
//                            写：VM_InstArrayClear(self,"mod_step_enemy_area")
//                                再按组依次 VM_InstArrayAdd(self,"mod_step_enemy_area", 值)
//                            例：只打本行 [0,0,0,99] / 全场 [99,99,0,99] / 只看前方一格 [0,0,0,1]
//                                两组（本行往右 + 上一行往左）[0,0,0,99, 1,0,99,0]
//   mod_enemy_types          索敌只看哪几类敌人 = 数组，元素写类型名；空数组 = 任意类型
//                            取值："normal" / "obstacle" / "diver" / "air" / "dance" / "underground"
//                            写：VM_InstArrayClear(self,"mod_enemy_types")
//                                再 VM_InstArrayAdd(self,"mod_enemy_types", "air") 多次
//                            ⚠️ 它只管"放不放卡进 VM"，不管子弹打谁（那看子弹的 target_type）
//   mod_step_enter_condition "" 每帧 | "mod" 每 N 帧 | "norm" 命中窗口表 |
//                            "norm_attack" 有敌人且命中窗口表 | "wait" 减到 0
//   mod_step_enter_var       "mod" = 间隔帧数；"wait" = 剩余帧数
//   mod_step_enter_arr       开火窗口表 = 数组；负数 = 从一轮末尾倒数（-35 → 一轮长度-35）
//                            写：VM_InstArrayClear(self,"mod_step_enter_arr")
//                                再 VM_InstArrayAdd(self,"mod_step_enter_arr", 0-7*fs) 多次
//                            "norm"/"norm_attack" 用；下标就是进来后读到的 mod_step_enter_index
//
//   —— 批量改属性（两套，行为一样，各自独立；每套 = 三个数组 + 一个开关）——
//   mod_set_on /  mod_set_id /  mod_set_prop /  mod_set_val      第一套
//   mod_set2_on / mod_set2_id / mod_set2_prop / mod_set2_val     第二套
//   开关开着时每帧遍历 id 数组：把第 i 个 id 的 属性 设成 值（三个数组按最短长度配对）
// obj 每帧回写（卡只读）：
//   mod_tick_cycle      每帧 +1，到一轮长度归零
//   mod_tick_enemy      有敌人才 +1，没敌人清 0
//   mod_has_enemy       1 = 索敌区域里有敌人
//   mod_step_enter_index 这一帧命中的窗口下标（0 起）；不是窗口 = -1
// 判定实现见 Step_0.gml
// ══════════════════════════════════════════════════════════════════════════

// 计时
mod_tick_cycle = 0   // 【只读】每帧 +1，到一轮长度归零
mod_tick_max   = 0   // 一轮多长；0 = 用这张卡自己的 cycle
mod_tick_enemy = 0   // 【只读】有敌人才 +1

// 索敌
mod_has_enemy       = 0   // 【只读】1 = 索敌区域里有敌人
mod_enemy_check     = 0   // 1 = 让 obj 每帧帮这张卡索敌（"norm_attack" 自动开）
mod_step_enemy_area = []  // 数组 [上,下,左,右]；空 = [1,1,0,99]
mod_enemy_types     = []  // 数组，元素是类型名；空 = 任意类型

// 什么时候进 VM
mod_step_enter_condition = ""   // "" / "mod" / "norm" / "norm_attack" / "wait"
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧
mod_step_enter_arr       = []   // 数组，开火窗口表（负数 = 从一轮末尾倒数）
mod_step_enter_index     = -1   // 【只读】命中的窗口下标；不是窗口 = -1

// ── 批量改属性：两套，每套 = 三个数组（id / 属性 / 值）+ 一个开关 ──
//   开关开着时，每帧遍历 id 数组，把第 i 个 id 的 mod_set_prop[i] 属性设成 mod_set_val[i]
//   （id 用 VM 拿到的实例 id；属性和值一一对应，三个数组长度以最短的为准）
mod_set_on   = 0   // 开关，1 = 开
mod_set_id   = []  // 实例 id
mod_set_prop = []  // 属性名（字符串）
mod_set_val  = []  // 要设的值

mod_set2_on   = 0  // 第二套开关
mod_set2_id   = []
mod_set2_prop = []
mod_set2_val  = []

// 创建时加入该卡的实例容器，并执行该卡虚拟机里的创建块
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	var _cd = _vm[$ "card_data"];
	if (is_struct(_cd)) {
		// 底座类型来自卡 JSON，供范围收集按类型筛选
		if (variable_struct_exists(_cd, "plant_type")) plant_type = _cd[$ "plant_type"];
	}
	ds_list_add(_vm.instances, id);
	if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
		var _bak_last = global._VM_last_created_card;
		global._VM_last_created_card = id;      // 块内 VM_GetLastCreatedCard() 取到本实例
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;               // 块内 VM_GetCurCard() 取到本实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;                      // API 函数按全局 VM 读参，执行期间切到该卡的 VM
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_created_card = _bak_last;
	}
}
