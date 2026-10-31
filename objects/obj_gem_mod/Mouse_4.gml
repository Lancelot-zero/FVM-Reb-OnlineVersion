// 主动宝石（插件在 _OBJECT_CREATE 里把 clickable 置 1）：
//   不在冷却时点一下 → 置 clicked 给插件放技能，并自动重新走冷却（同原版宝石的 Mouse_4）
// 被动宝石（clickable 保持 0，默认）：
//   点击不响应，cooldown_timer 由插件自己按自己的节奏写
if (clickable) {
	if (cooldown_timer <= 0) {
		clicked = true;
		cooldown_timer = cooldown;
	}
}

// 插件块：鼠标左键点在本实例上：执行该对象虚拟机里的 _OBJECT_CLICK 块
// 与 _OBJECT_STEP 同一套路：块存在才跑，执行期间切 global.__vm / _VM_cur_card
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
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