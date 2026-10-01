if global.is_paused{
	exit
}
event_inherited(); 

if shape < 1{
	drown_timer ++
	if drown_timer mod 60 == 0{
		//0转检测是否有气泡和海水
		var has_bubble = false
		var has_seawater = false
		with obj_card_parent{
			if plant_id == "soda_bubble" && grid_col == other.grid_col && grid_row == other.grid_row{
				has_bubble = true
			}
		}
		with obj_seawater{
			if col == other.grid_col && row == other.grid_row{
				has_seawater = true
			}
		}
		if !has_bubble && !has_seawater{
			hp -= 0.05*max_hp
			event_user(2)
		}
	}
}

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
	if ((grid_row == other.grid_row || grid_col >= other.grid_col && grid_col <= (global.grid_cols + 1))&& can_target_on(other.target_type,target_type)){
		has_enemy = true
		break
	}
}*/

//检测自身行/右方是否有敌人（原条件：同行任意列 || 右方任意行，用前缀和+左侧数组查询）
var has_enemy = false
var _stride = global.grid_cols + 2;
var _col_end = global.grid_cols + 1;
var _left = grid_col > 0 ? grid_col - 1 : -1;
// 同行左侧场外（col<0）敌人
if (grid_row >= 0 && grid_row < global.grid_rows) {
	var _left_arr = global.enemy_array_left[grid_row]
	for(var _i = 0; _i < array_length(_left_arr); _i++){
		var _e = _left_arr[_i]
		if (instance_exists(_e) && can_target_on(target_type, _e.target_type)){
			has_enemy = true
			break
		}
	}
}
// 同行 col 0..cols+1（条件前半）+ 任意行 col [自己列, cols+1]（条件后半）
for(var _r = 0; _r < global.grid_rows; _r++){
	var _row_base = _r * _stride;
	if (!has_enemy && _r == grid_row) {
		if (global.has_enemy_normal[_row_base + _col_end] > 0 || global.has_enemy_obstacle[_row_base + _col_end] > 0){
			has_enemy = true
		}
	}
	var _sn = global.has_enemy_normal[_row_base + _col_end];
	var _so = global.has_enemy_obstacle[_row_base + _col_end];
	if (_left >= 0) {
		_sn -= global.has_enemy_normal[_row_base + _left];
		_so -= global.has_enemy_obstacle[_row_base + _left];
	}
	if (_sn > 0 || _so > 0){
		has_enemy = true
		break
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
    }
	if shape < 2{
		if attack_timer == cycle - 8*current_flash_speed{
			event_user(1)
			audio_play_sound(snd_shot,0,0)
		}
	}
	else{
		if attack_timer == cycle - 5*current_flash_speed{
			event_user(1)
			audio_play_sound(snd_shot,0,0)
		}
		if attack_timer == cycle - 2*current_flash_speed{
			event_user(1)
			audio_play_sound(snd_shot,0,0)
		}
	}
} else {
    // 没有符合条件的敌人，重置状态
    attack_timer = 0;
    state = CARD_STATE.IDLE;
}


