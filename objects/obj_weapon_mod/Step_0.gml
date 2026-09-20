if global.is_paused {
	exit
}

// 跟随放置它的玩家
if (instance_exists(parent_player)) {
	depth = parent_player.depth - 1
	x = parent_player.x - 10
	y = parent_player.y - 100
	grid_row = parent_player.grid_row
	grid_col = parent_player.grid_col
}

// 动画自动播放：与 obj_card_parent 同款，配置属性（flash_speed/idle_anim/attack_anim/state）
// 由 .bin 在 _OBJECT_CREATE/_OBJECT_STEP 里通过 VM_SetProp 设置
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

// 每个实例每帧执行该武器虚拟机里的步进块（单点调用），逻辑全部在 .bin 里
if (weapon_id != "" && variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, weapon_id)) {
	var _vm = global.mod_weapon_vms[? weapon_id];
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
