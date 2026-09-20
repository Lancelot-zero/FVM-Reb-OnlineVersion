// 通用 mod 敌人对象：src_mod 注册的所有敌人共用此对象（继承 obj_enemy_parent，
// 移动/攻击/死亡等基础行为全部复用父类），特殊逻辑在 .bin 里
// 敌人 id 由生成逻辑在创建前写入 global._mod_pending_enemy_id
event_inherited();
enemy_id = variable_global_exists("_mod_pending_enemy_id") ? global._mod_pending_enemy_id : ""

// 原版敌人是每个对象在 Create 里写死自己的数值，mod 敌人统一从注册表套用
if (enemy_id != "" && variable_global_exists("enemy_map")) {
	var _info = global.enemy_map[? enemy_id];
	if (is_struct(_info)) {
		if (!is_undefined(_info.spr) && _info.spr != noone) { sprite_index = _info.spr; }
		hp = _info.hp ?? hp;
		maxhp = hp;
		shield_hp = _info.shield ?? 0;
		shield_max_hp = shield_hp;
		move_speed = _info.speed ?? move_speed;
		atk = _info.atk ?? atk;
		atk_cycle = _info.cycle ?? atk_cycle;
		attack_range = _info.range ?? attack_range;
		immune_to_ash = _info.ash_proof ?? false;
		feature = _info.feature ?? "land";
	}
}

// 创建时加入该敌人的实例容器，并执行该敌人虚拟机里的创建块
if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _vm = global.mod_enemy_vms[? enemy_id];
	ds_list_add(_vm.instances, id);
	if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
		var _bak_last = global._VM_last_created_card;
		global._VM_last_created_card = id;      // 块内 VM_GetLastCreatedCard() 取到本实例
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;               // 块内 VM_GetCurCard() 取到本实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;                      // API 函数按全局 VM 读参，执行期间切到该敌人的 VM
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_created_card = _bak_last;
	}
}
