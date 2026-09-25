// 鼠标进入本实例：执行该对象虚拟机里的 _OBJECT_MOUSE_ENTER 块
// 与 _OBJECT_STEP 同一套路：块存在才跑，执行期间切 global.__vm / _VM_cur_card
if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _vm = global.mod_enemy_vms[? enemy_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_MOUSE_ENTER")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_MOUSE_ENTER"], "_OBJECT_MOUSE_ENTER");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}
event_inherited();