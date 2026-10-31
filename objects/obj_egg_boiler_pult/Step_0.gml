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
// 检测自身右方是否有敌人，并获取最近的敌人
var has_enemy = false
var target_enemy = noone
var min_distance = 10000 // 设置一个足够大的初始值

// 差分查询索敌：遍历网格数组单元格找最近敌人（友化敌人不在 enemy_array 中，自动排除）
var _stride = global.grid_cols + 2
var _col_end = _stride - 1
for(var _c = grid_col; _c <= _col_end; _c++){
    var _cell = global.enemy_array[grid_row * _stride + _c]
    for(var _i = 0; _i < array_length(_cell); _i++){
        var _e = _cell[_i]
        if (instance_exists(_e) && can_target_on(target_type, _e.target_type)){
            var _dist = _e.grid_col - grid_col
            if (_dist < min_distance) {
                min_distance = _dist
                target_enemy = _e
                has_enemy = true
            }
        }
    }
}

// 存储目标敌人信息
if (has_enemy) {
    target_instance = target_enemy
} else {
    target_instance = noone
}
//攻击逻辑
if (has_enemy) {
    if (attack_timer <= cycle - attack_anim * current_flash_speed) {
        attack_timer++;
    }  else if (attack_timer == cycle - 30) {
        event_user(1); // 发射子弹
		attack_timer++;
    }else if (attack_timer <= cycle) {
		if shape == 2 && attack_timer == cycle - 20{
			event_user(1); // 发射子弹
		}
        attack_timer++;
        state = CARD_STATE.ATTACK;
    }else {
        attack_timer = 0;
        state = CARD_STATE.IDLE;
    }
} else {
    // 没有符合条件的敌人，重置状态
    attack_timer = 0;
    state = CARD_STATE.IDLE;
}


