if global.is_paused{
	exit
}
timer++;
// 与章鱼烧 Other_11 共享每帧索敌缓存（最左空中敌人 + 最左敌人）
if (!variable_global_exists("_takoyaki_scan_frame")) {
	global._takoyaki_scan_frame = -1;
	global._takoyaki_air_enemy = noone;
	global._takoyaki_closest_left_enemy = noone;
}
if (global._takoyaki_scan_frame != obj_battle.battle_time) {
	global._takoyaki_scan_frame = obj_battle.battle_time;
	global._takoyaki_air_enemy = noone;
	global._takoyaki_closest_left_enemy = noone;
	var _min_x = room_width;
	var _max_hp = 0;
	with (obj_enemy_parent) {
		if (hp > 0 && can_hit("track",target_type) && y > 0) { // 只考虑存活的敌人
			//检查是否有空中敌人
			if target_type == "air"{
				if global._takoyaki_air_enemy != noone && instance_exists(global._takoyaki_air_enemy){
					if x < global._takoyaki_air_enemy.x{
						global._takoyaki_air_enemy = id
					}
				}
				else{
					global._takoyaki_air_enemy = id
				}
			}

			// 同时寻找最左侧且生命值最高的敌人
			if (x < _min_x || (x == _min_x && hp > _max_hp)) {
				_min_x = x;
				_max_hp = hp;
				global._takoyaki_closest_left_enemy = id;
			}
		}
	}
}

// 卡片右一格内寻找敌人（取血最高），查本列+右列网格数组
var _find_right_cell = function(_range) {
	return noone
	var _found = noone;
	if (instance_exists(banding_card_obj)) {
		var _c0 = banding_card_obj.grid_col;
		if (_c0 >= 0) {
			var _row_base = row * (global.grid_cols + 2);
			var _c_end = min(_c0 + 1, global.grid_cols + 1);
			for(var _c = _c0; _c <= _c_end; _c++){
				var _arr = global.enemy_array[_row_base + _c];
				for(var _k = 0; _k < array_length(_arr); _k++){
					var _e = _arr[_k];
					if (instance_exists(_e) && _e.hp > 0 && _e.y > 0 && can_hit(target_type,_e.target_type) && _e.x >= banding_card_obj.x && _e.x <= banding_card_obj.x + _range) {
						if (_found == noone || _e.hp > _found.hp){
							_found = _e;
						}
					}
				}
			}
		}
	}
	return _found;
}

// 子弹追踪逻辑
if (instance_exists(target_enemy) && target_enemy.hp > 0  && can_hit(target_type,target_enemy.target_type)) {
	// 目标存在且存活，继续追踪
	var target_x = target_enemy.x;
	var target_y = target_enemy.y-75;

	// 计算朝向目标的方向
	var dir = point_direction(x, y, target_x, target_y);
	x += lengthdir_x(move_speed, dir);
	y += lengthdir_y(move_speed, dir);
	//show_debug_message(dir)

	// 检查是否需要重新选择目标（有更高优先级的目标出现）
	var new_target = _find_right_cell(150);

	// 如果找到更高优先级的目标，切换目标
	if (new_target != noone) {
		target_enemy = new_target;
	} else if (global._takoyaki_air_enemy != noone && instance_exists(global._takoyaki_air_enemy)) {
		target_enemy = global._takoyaki_air_enemy;
	} else if (global._takoyaki_closest_left_enemy != noone && instance_exists(global._takoyaki_closest_left_enemy) && global._takoyaki_closest_left_enemy.x < target_enemy.x) {
		target_enemy = global._takoyaki_closest_left_enemy;
	}

} else {
	// 目标不存在或已死亡，寻找新目标
	var new_target = _find_right_cell(80);

	// 优先选择右边一格内的敌人，如果没有则选择最左侧敌人
	if (new_target != noone) {
		target_enemy = new_target;
	} else if (global._takoyaki_air_enemy != noone && instance_exists(global._takoyaki_air_enemy)) {
		target_enemy = global._takoyaki_air_enemy;
	} else if (global._takoyaki_closest_left_enemy != noone && instance_exists(global._takoyaki_closest_left_enemy)) {
		target_enemy = global._takoyaki_closest_left_enemy;
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
}
