if global.is_paused{
	exit
}
event_inherited(); 
if is_frozen{
	exit
}
var current_flash_speed = flash_speed
if is_slowdown{
	current_flash_speed *= 2
}

if state == CARD_STATE.SLEEP{
	anim_timer++
	image_index = floor(anim_timer/current_flash_speed) mod 10
	//if anim_timer > 60{
	//	state = CARD_STATE.AWAKE
	//}
}
if state == CARD_STATE.AWAKE{
	wake_timer++
	image_index = floor(wake_timer/current_flash_speed) mod 4 + 10
	if wake_timer >= 5*current_flash_speed{
		sprite_index = get_load_sprite("spr_oden_pot");
		image_index = 0
		timer = 0
		state = CARD_STATE.IDLE
	}
}

//检测自身右方是否有敌人
var has_enemy = false
// 差分查询索敌：网格数组查询（友化敌人不进数组，自动排除）
var _stride = global.grid_cols + 2
var _col_end = _stride - 1
var _left_idx = grid_col > 0 ? grid_col - 1 : -1
// 第一部分：本行前缀和（shape<2 限 [own..own+4]，shape>=2 整行）
var _cend = (shape < 2) ? min(grid_col + 4, _col_end) : _col_end
var _rb = grid_row * _stride
var _sn = global.has_enemy_normal[_rb + _cend] - (_left_idx >= 0 ? global.has_enemy_normal[_rb + _left_idx] : 0)
var _sd = global.has_enemy_dance[_rb + _cend] - (_left_idx >= 0 ? global.has_enemy_dance[_rb + _left_idx] : 0)
var _so = global.has_enemy_obstacle[_rb + _cend] - (_left_idx >= 0 ? global.has_enemy_obstacle[_rb + _left_idx] : 0)
if (_sn + _sd + _so > 0){
	has_enemy = true
}
// 第二部分：|x|<=100（shape<2 只查行差<=4 的行，shape>=2 查所有行），列 own-1..own+1 + col<0 场外
if (!has_enemy){
	var _rmin = (shape < 2) ? max(grid_row - 4, 0) : 0
	var _rmax = (shape < 2) ? min(grid_row + 4, global.grid_rows - 1) : global.grid_rows - 1
	for(var _r = _rmin; _r <= _rmax; _r++){
		var _rbase = _r * _stride
		for(var _c = max(grid_col - 1, 0); _c <= min(grid_col + 1, _col_end); _c++){
			var _cell = global.enemy_array[_rbase + _c]
			for(var _i = 0; _i < array_length(_cell); _i++){
				var _e = _cell[_i]
				if (instance_exists(_e) && abs(_e.x - x) <= 100 && can_target_on(target_type, _e.target_type)){
					has_enemy = true
					break
				}
			}
			if (has_enemy) break
		}
		if (has_enemy) break
		var _larr = global.enemy_array_left[_r]
		for(var _i = 0; _i < array_length(_larr); _i++){
			var _e = _larr[_i]
			if (instance_exists(_e) && abs(_e.x - x) <= 100 && can_target_on(target_type, _e.target_type)){
				has_enemy = true
				break
			}
		}
		if (has_enemy) break
	}
}
// 第三部分：shape>=2 时本行 col<0 场外敌人不受距离限制，单独补查
if (!has_enemy && shape >= 2){
	var _larr = global.enemy_array_left[grid_row]
	for(var _i = 0; _i < array_length(_larr); _i++){
		var _e = _larr[_i]
		if (instance_exists(_e) && can_target_on(target_type, _e.target_type)){
			has_enemy = true
			break
		}
	}
}
//攻击逻辑
if state != CARD_STATE.SLEEP && state != CARD_STATE.AWAKE{
	if (has_enemy) {
	    if (attack_timer <= cycle - attack_anim * current_flash_speed) {
	        attack_timer++;
	    }else if (attack_timer == cycle - (attack_anim-3) * current_flash_speed) && state == CARD_STATE.ATTACK{
	        attack_timer++;
        
			event_user(1)
	    } else if (attack_timer <= cycle) {
	        attack_timer++;
	        state = CARD_STATE.ATTACK;
	    } else {
	        attack_timer = 0;
	        state = CARD_STATE.IDLE;
	    }
	} else {
	    // 没有符合条件的敌人，重置状态
	    attack_timer = 0;
	    state = CARD_STATE.IDLE;
	}
}


