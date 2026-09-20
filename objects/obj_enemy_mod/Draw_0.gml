// 父类绘制本体
event_inherited();

// 该敌人虚拟机里的绘制块；没有则不多画
if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _vm = global.mod_enemy_vms[? enemy_id];
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
