// 销毁时执行该宝石虚拟机里的销毁块，并把实例移出该宝石的实例容器
//  ⚠️ 原来宝石根本没有 Destroy 事件：_OBJECT_DESTROY 从不执行、instances 只增不减
//     （老实例的 id 一直躺在列表里）。这里补上，和 obj_card_mod / obj_enemy_mod 同一套写法。
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
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
	mod_inst_del(_vm, id);   // 计数 -1 + 移出 instances 列表
}
