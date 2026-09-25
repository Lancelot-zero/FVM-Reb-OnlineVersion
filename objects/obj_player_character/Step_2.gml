if not is_placed{
	x = mouse_x
	y = mouse_y
}


if (pre_hp>hp) {
	global._VM_last_damaged_player = id;
	if (buffer_exists(global._VM_PLAYER_DAMAGED)) {
		VM_Execute(global.__vm, global._VM_PLAYER_DAMAGED, "_VM_PLAYER_DAMAGED");
	}
	vm_hook_run("_VM_PLAYER_DAMAGED");   // mod 侧：同一时机，各自查块
}
pre_hp = hp;