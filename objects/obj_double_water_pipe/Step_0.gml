if global.is_paused{
	exit
}
event_inherited(); 
if is_frozen || state == CARD_STATE.SLEEP{
	exit
}
var current_flash_speed = flash_speed
if is_slowdown{
	current_flash_speed *= 2
}
/*
//检测自身右方是否有敌人
var has_enemy = false
with(obj_enemy_parent){
	if (grid_row == other.grid_row && grid_col <= (global.grid_cols + 1)) && can_target_on(other.target_type,target_type){
		has_enemy = true
		break
	}
}*/


//检测自身行是否有敌人（左侧场外 + 右侧场内）
var has_enemy = false
if (grid_row >= 0 && grid_row < global.grid_rows) {
	// 左侧场外（col<0）敌人
	var _left_arr = global.enemy_array_left[grid_row]
	for(var _i = 0; _i < array_length(_left_arr); _i++){
		var _e = _left_arr[_i]
		if (instance_exists(_e) && can_target_on(target_type, _e.target_type)){
			has_enemy = true
			break
		}
	}
	// 右侧场内（col 0..cols+1）敌人用前缀和查询
	if (!has_enemy) {
		var _row_base = grid_row * (global.grid_cols + 2);
		var _col_end = global.grid_cols + 1;
		if (global.has_enemy_normal[_row_base + _col_end] > 0 || global.has_enemy_obstacle[_row_base + _col_end] > 0){
			has_enemy = true
		}
	}
}


//攻击逻辑
if (has_enemy) {
    if (attack_timer <= cycle - attack_anim * current_flash_speed) {
        attack_timer++;
    } else if (attack_timer <= cycle) {
        attack_timer++;
        state = CARD_STATE.ATTACK;
    } else {
        attack_timer = 0;
        state = CARD_STATE.IDLE;
		b_count = 0
    }
	if (attack_timer == cycle - 7*flash_speed){
		event_user(1);
	}
	if (attack_timer == cycle - 4*flash_speed){
		event_user(1)
	}
	if (attack_timer == cycle - 1*flash_speed) && shape >= 2{
		event_user(1); // 发射子弹
	}
} else {
    // 没有符合条件的敌人，重置状态
    attack_timer = 0;
    state = CARD_STATE.IDLE;
}


