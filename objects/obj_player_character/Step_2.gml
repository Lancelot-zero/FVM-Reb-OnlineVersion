if not is_placed{
	x = mouse_x
	y = mouse_y
}


if (pre_hp>hp&&buffer_exists(global._VM_PLAYER_DAMAGED)) {	
	global._VM_last_damaged_player = id;
    VM_Execute(global.__vm, global._VM_PLAYER_DAMAGED, "_VM_PLAYER_DAMAGED");
}
pre_hp = hp;