if global.is_paused {
	exit
}

// 预留属性 cooldown_timer：每帧自减（点击时由 Mouse_4 设回 cooldown；插件触发时也可以自己重写）
if (cooldown_timer > 0) cooldown_timer--;

// 动画自动播放：与 obj_card_parent 同款，配置属性由 .bin 通过 VM_SetProp 设置
if (timer < flash_speed - 1) {
	timer++;
} else {
	switch (state) {
		case CARD_STATE.IDLE:
			if (image_index < idle_anim) {
				image_index++
			} else {
				image_index = 0;
			}
			break;
		case CARD_STATE.ATTACK:
			if (image_index >= (idle_anim + 1) && image_index <= (idle_anim + attack_anim)) image_index++;
			else image_index = (idle_anim + 1);
			break;
	}
	timer = 0;
}

// ══════════════════════════════════════════════════════════════════════
// 进 VM 的入口闸门
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 | "cool" 冷却结束（cooldown_timer <= 0）
// ══════════════════════════════════════════════════════════════════════
var _enter = true;
var _cond  = mod_step_enter_condition;

if (_cond == "mod") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0 && instance_exists(obj_battle)) {
		_enter = ((obj_battle.battle_time mod mod_step_enter_var) == 0);
	}

} else if (_cond == "wait") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0) {
		mod_step_enter_var -= 1;
		_enter = (mod_step_enter_var <= 0);
	} else {
		mod_step_enter_condition = "";        // 不合法：进一次 + 清空
	}

} else if (_cond == "cool") {
	_enter = (cooldown_timer <= 0);
}

// 每个实例每帧执行该宝石虚拟机里的步进块（单点调用），逻辑全部在 .bin 里
if (_enter && gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_STEP")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}
