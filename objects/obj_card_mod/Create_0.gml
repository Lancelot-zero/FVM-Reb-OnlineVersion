// 通用 mod 植物对象：src_mod 注册的所有卡共用此对象
// 卡 id 由放置逻辑在创建前写入 global._mod_pending_card_id
event_inherited();
plant_id = global._mod_pending_card_id
event_user(0)

// 创建时加入该卡的实例容器，并执行该卡虚拟机里的创建块
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	var _cd = _vm[$ "card_data"];
	if (is_struct(_cd)) {
		// 底座类型来自卡 JSON，供范围收集按类型筛选
		if (variable_struct_exists(_cd, "plant_type")) plant_type = _cd[$ "plant_type"];
	}
	ds_list_add(_vm.instances, id);
	if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
		var _bak_last = global._VM_last_created_card;
		global._VM_last_created_card = id;      // 块内 VM_GetLastCreatedCard() 取到本实例
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;               // 块内 VM_GetCurCard() 取到本实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;                      // API 函数按全局 VM 读参，执行期间切到该卡的 VM
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_created_card = _bak_last;
	}
}
