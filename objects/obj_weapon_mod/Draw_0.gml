// 该武器虚拟机里的绘制块；没有则默认画 sprite_index
if (weapon_id != "" && variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, weapon_id)) {
	var _vm = global.mod_weapon_vms[? weapon_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_DRAW")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_DRAW"], "_OBJECT_DRAW");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		exit;
	}
}
draw_self();
