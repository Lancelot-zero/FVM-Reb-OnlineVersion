if global.is_paused{
	exit
}

// 每个实例每帧执行该卡虚拟机里的步进块（单点调用）
event_inherited();

// 父对象 Step 里 hp<=0 会销毁本实例。实例一旦没了，
// 块内 VM_GetProp(self, ...) 会返回 undefined，接下去做算术就是 DoAdd 报错。
// 所以实例不在 / 已经该死了，就别再跑 mod 的 _OBJECT_STEP。
if (!instance_exists(id)) exit;
if (hp <= 0) exit;

if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
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
