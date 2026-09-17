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
//检测自身右方是否有敌人
var has_enemy = false
/*
with(obj_enemy_parent){
	if (grid_row >= other.grid_row-1&&grid_row <= other.grid_row+1 && grid_col >= other.grid_col && grid_col <= (global.grid_cols + 1) && can_target_on(other.target_type,target_type)){
		has_enemy = true
		break
	}
}
*/

var stride = global.grid_cols + 2;
var col_end = stride - 1;
var left_idx = grid_col > 0 ? grid_col - 1 : -1;

var enemy_up = 0;
var enemy_mid = 0;
var enemy_down = 0;
if (grid_row >= 0 && grid_row < global.grid_rows) {
	if(grid_row > 0)
	{
		var r = (grid_row - 1) * stride;
		var sum_n = global.has_enemy_normal[r + col_end] - (left_idx >=0 ? global.has_enemy_normal[r + left_idx] : 0);
		var sum_o = global.has_enemy_obstacle[r + col_end] - (left_idx >=0 ? global.has_enemy_obstacle[r + left_idx] : 0);
		enemy_up = sum_n + sum_o;
	}
	{
		var r = grid_row * stride;
		var sum_n = global.has_enemy_normal[r + col_end] - (left_idx >=0 ? global.has_enemy_normal[r + left_idx] : 0);
		var sum_o = global.has_enemy_obstacle[r + col_end] - (left_idx >=0 ? global.has_enemy_obstacle[r + left_idx] : 0);
		enemy_mid = sum_n + sum_o;
	}
	if(grid_row + 1 < global.grid_rows)
	{
		var r = (grid_row + 1) * stride;
		var sum_n = global.has_enemy_normal[r + col_end] - (left_idx >=0 ? global.has_enemy_normal[r + left_idx] : 0);
		var sum_o = global.has_enemy_obstacle[r + col_end] - (left_idx >=0 ? global.has_enemy_obstacle[r + left_idx] : 0);
		enemy_down = sum_n + sum_o;
	}
}

if(enemy_up+enemy_mid+enemy_down>0){
	has_enemy = true
}

//攻击逻辑
if (has_enemy) {
    if (attack_timer <= cycle - attack_anim * current_flash_speed) {
        attack_timer++;
    } else if (attack_timer <= cycle) {
        attack_timer++;
        state = CARD_STATE.ATTACK;
    } else {
        //event_user(1); // 发射子弹
        attack_timer = 0;
        state = CARD_STATE.IDLE;
    }
	if (attack_timer == cycle - 3*flash_speed){
		event_user(1);
	}
	if shape >= 2 && (attack_timer == cycle - 1*flash_speed){
		event_user(1)
	}
} else {
    // 没有符合条件的敌人，重置状态
    attack_timer = 0;
    state = CARD_STATE.IDLE;
}


