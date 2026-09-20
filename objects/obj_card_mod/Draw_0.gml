// 每个实例绘制时执行该卡虚拟机里的绘制块（单点调用）
event_inherited();
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_DRAW")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;      // 块内 API 取当前实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_DRAW"], "_OBJECT_DRAW");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}
