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
//检测自身右方是否有敌人
var has_enemy = false
// 差分查询索敌：本行右侧用前缀和查询（友化敌人不进 has_enemy_*，自动排除）
var _stride = global.grid_cols + 2
var _col_end = min(grid_col + (shape < 2 ? 3 : _stride - 1), _stride - 1)
var _left_idx = grid_col > 0 ? grid_col - 1 : -1
var _rb = grid_row * _stride
var _sn = global.has_enemy_normal[_rb + _col_end] - (_left_idx >= 0 ? global.has_enemy_normal[_rb + _left_idx] : 0)
var _so = global.has_enemy_obstacle[_rb + _col_end] - (_left_idx >= 0 ? global.has_enemy_obstacle[_rb + _left_idx] : 0)
if (_sn + _so > 0){
	has_enemy = true
}
//攻击逻辑
if state != CARD_STATE.SLEEP{
	if (has_enemy){
	    if (attack_timer <= cycle - attack_anim * current_flash_speed) {
	        attack_timer++;
	    } else if (attack_timer <= cycle) {
	        attack_timer++;
	        state = CARD_STATE.ATTACK;
	    } else {
	        event_user(1); // 发射子弹
	        attack_timer = 0;
	        state = CARD_STATE.IDLE;
	    }
	} else {
	    // 没有符合条件的敌人，重置状态
	    attack_timer = 0;
	    state = CARD_STATE.IDLE;
	}
}


