// 鼠标离开（照原版宝石）
on_click = false;

// 插件块：鼠标离开本实例：执行该对象虚拟机里的 _OBJECT_MOUSE_LEAVE 块
// 与 _OBJECT_STEP 同一套路：块存在才跑，执行期间切 global.__vm / _VM_cur_card
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
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