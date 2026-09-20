// 通用 mod 宝石对象：src_mod 注册的所有宝石共用此对象
// 宝石 id 由装备放置逻辑在创建前写入 global._mod_pending_gem_id
// 只负责初始化和分发 VM 块，效果逻辑全部在 .bin 里
gem_id = variable_global_exists("_mod_pending_gem_id") ? global._mod_pending_gem_id : ""
gem_info = get_gem_info(gem_id)
cooldown = 60
parent_player = noone
image_speed = 0
// 动画配置（与 obj_card_parent 同款）：.bin 可在 _OBJECT_CREATE 用 VM_SetProp 覆盖
timer = 0
flash_speed = 5
idle_anim = 0
attack_anim = 0
state = CARD_STATE.IDLE;
if (is_struct(gem_info)) {
	if (!is_undefined(gem_info.icon) && gem_info.icon != noone) {
		sprite_index = gem_info.icon
	}
	// 冷却取自宝石数据（原版宝石都是 cooldown 数组，按等级取）
	if (is_array(gem_info.cooldown)) {
		var _lv = get_gem_level(gem_id)
		cooldown = gem_info.cooldown[min(_lv, array_length(gem_info.cooldown) - 1)]
	} else if (!is_undefined(gem_info.first_cooldown)) {
		cooldown = gem_info.first_cooldown
	}
}

// 创建时加入该宝石的实例容器，并执行该宝石虚拟机里的创建块
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
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
