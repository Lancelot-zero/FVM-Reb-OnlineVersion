if state != ENEMY_STATE.ACTING && grid_row == other.row{
	state = ENEMY_STATE.ACTING
	sprite_index = spr_suv_mouse_act
	anim_timer = 0
}