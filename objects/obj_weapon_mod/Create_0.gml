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
