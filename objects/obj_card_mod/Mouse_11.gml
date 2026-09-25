// 鼠标离开本实例：执行该卡虚拟机里的 _OBJECT_MOUSE_LEAVE 块
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_MOUSE_LEAVE")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_MOUSE_LEAVE"], "_OBJECT_MOUSE_LEAVE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}
event_inherited();
