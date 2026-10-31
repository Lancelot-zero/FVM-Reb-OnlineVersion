// 鼠标左键点在本实例上：执行该卡虚拟机里的 _OBJECT_CLICK 块
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_CLICK")) {
		if (global._vm_click_singleton_eff) {
			// 全局单例按下模式：先不执行，只记录最上面的实例（depth 越小越靠上），帧末 obj_battle End Step 统一触发
			if (global._vm_click_pick == noone || !instance_exists(global._vm_click_pick) || global._vm_click_pick.depth > depth) {
				global._vm_click_pick = id;
			}
		} else {
			vm_mod_click_execute(id);
		}
	}
}
event_inherited();
