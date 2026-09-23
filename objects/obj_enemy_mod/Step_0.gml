if global.is_paused{
	exit
}

// enemy_id 晚到（联机：服务器建完后补发 MSG_MODIFY_PROP）：补跑一次初始化
if (enemy_id != "" && mod_enemy_inited_id != enemy_id) mod_enemy_init();

// 父类处理移动/攻击/死亡等基础行为
event_inherited();

// 每个实例每帧执行该敌人虚拟机里的步进块（单点调用），特殊逻辑全部在 .bin 里
if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _vm = global.mod_enemy_vms[? enemy_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_STEP")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;      // 块内 API 取当前实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}
