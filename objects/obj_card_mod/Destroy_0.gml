// 消耗时执行该卡虚拟机里的销毁块，并把实例移出该卡的实例容器
event_inherited();
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_DESTROY")) {
		var _bak_last = global._VM_last_destroyed_card;
		global._VM_last_destroyed_card = id;      // 块内 VM_GetLastDestroyedCard() 取到本实例
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;                 // 块内 VM_GetCurCard() 取到本实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_DESTROY"], "_OBJECT_DESTROY");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_destroyed_card = _bak_last;
	}
	var _idx = ds_list_find_index(_vm.instances, id);
	if (_idx != -1) { ds_list_delete(_vm.instances, _idx); }
}
