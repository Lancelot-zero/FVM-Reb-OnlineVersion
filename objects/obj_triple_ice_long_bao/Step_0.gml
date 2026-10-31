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
// 差分查询索敌：本行右侧用前缀和，|x|<=100 部分遍历网格数组（友化敌人不进数组，自动排除）
var _stride = global.grid_cols + 2
var _col_end = _stride - 1
var _left_idx = grid_col > 0 ? grid_col - 1 : -1
var _rb = grid_row * _stride
var _sn = global.has_enemy_normal[_rb + _col_end] - (_left_idx >= 0 ? global.has_enemy_normal[_rb + _left_idx] : 0)
var _so = global.has_enemy_obstacle[_rb + _col_end] - (_left_idx >= 0 ? global.has_enemy_obstacle[_rb + _left_idx] : 0)
if (_sn + _so > 0){
	has_enemy = true
}
// |x - other.x| <= 100：可能出现在任意行的相邻列（col<0 的场外敌人也查）
if (!has_enemy){
	for(var _r = 0; _r < global.grid_rows; _r++){
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
	}
	if (!has_enemy){
		for(var _r = 0; _r < global.grid_rows; _r++){
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
	if attack_timer == cycle - 15{
		event_user(1)
	}
} else {
    // 没有符合条件的敌人，重置状态
    attack_timer = 0;
    state = CARD_STATE.IDLE;
}


