if global.is_paused{
	exit
}
timer++;

// 每帧首次扫描缓存最左敌人，同帧其他炮弹复用
if (!variable_global_exists("_tanghulu_scan_frame")) {
    global._tanghulu_scan_frame = -1;
    global._tanghulu_best_id = noone;
}
if (global._tanghulu_scan_frame != obj_battle.battle_time) {
    global._tanghulu_scan_frame = obj_battle.battle_time;
    global._tanghulu_best_id = noone;
    var _best_x = room_width;
    var _best_hp = 0;
    with (obj_enemy_parent) {
        if (hp > 0 && can_hit(other.target_type,target_type) && y > 0) {
            if (x < _best_x || (x == _best_x && hp > _best_hp)) {
                _best_x = x;
                _best_hp = hp;
                global._tanghulu_best_id = id;
            }
        }
    }

}
// 从缓存取最左敌人
var _best = global._tanghulu_best_id;

// 子弹追踪逻辑
if (instance_exists(target_enemy) && target_enemy.hp > 0 && can_hit(target_type,target_enemy.target_type)) {
	
	if(global._tanghulu_buttet_number>100){

		with(target_enemy){
			audio_play_sound(hit_sound,0,0)
			damage_amount = other.damage
			damage_type = other.damage_type
			event_user(0)
		}
		instance_create_depth(x,y,depth,obj_takoyaki_bullet_effect)
		global._tanghulu_buttet_number-=1
		instance_destroy()
		exit
	}
	
    // 目标存在且存活，继续追踪
    var target_x = target_enemy.x;
    var target_y = target_enemy.y-75;
    
    // 计算朝向目标的方向
    var dir = point_direction(x, y, target_x, target_y);
    x += lengthdir_x(move_speed, dir);
    y += lengthdir_y(move_speed, dir);
	//show_debug_message(dir)
    
    // 检查是否需要重新选择目标（有更高优先级的目标出现）
    var new_target = noone;
    var closest_left_enemy = noone;
	var air_enemy = noone
    var min_x = room_width;
    var max_hp = 0;
    var right_range = 150;
    
    // 如果缓存的最左敌人比当前目标更靠左，切换目标
    if (instance_exists(_best) && _best.hp > 0 && can_hit(target_type, _best.target_type)) {
        if (_best.x < target_enemy.x) {
            closest_left_enemy = _best;
        }
    }
    
    // 如果找到更高优先级的目标，切换目标
    if (new_target != noone) {
        target_enemy = new_target;
    } else if (air_enemy != noone) {
        target_enemy = air_enemy;
    } else if (closest_left_enemy != noone) {
        target_enemy = closest_left_enemy;
    }
    
} else {
    // 目标不存在或已死亡，寻找新目标
    var new_target = noone;
    var closest_left_enemy = noone;
	var air_enemy = noone
    var min_x = room_width;
    var max_hp = 0;
    var right_range = 80;
    
    // 使用缓存的最左敌人作为新目标
    if (instance_exists(_best) && _best.hp > 0 && can_hit(target_type, _best.target_type)) {
        closest_left_enemy = _best;
    }
    
    // 优先选择右边一格内的敌人，如果没有则选择最左侧敌人
    if (new_target != noone) {
        target_enemy = new_target;
    } else if (air_enemy != noone) {
        target_enemy = air_enemy;
    }else if (closest_left_enemy != noone) {
        target_enemy = closest_left_enemy;
    } else {
        // 没有敌人，按原方向继续飞行
        var dir = point_direction(xstart, ystart, x, y);
        x += lengthdir_x(move_speed, dir);
        y += lengthdir_y(move_speed, dir);
    }
}

image_angle =- timer * 6
if x > 2200 or y > 1200 or x < -200 or y < -200{
	instance_destroy()
	global._tanghulu_buttet_number-=1
}
