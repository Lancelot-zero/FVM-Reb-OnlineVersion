function find_priority_enemy() {
    var priority_enemy = noone;
    var closest_left_enemy = noone;
	var air_enemy = noone
    var min_x = room_width; // 初始化为房间宽度
    var max_hp = 0;
    
    // 检查右边一格内是否有敌人（假设一格为80像素）
    var right_range = 150;
	
	if (!variable_global_exists("_takoyaki_scan_frame")) {
	    global._takoyaki_scan_frame = -1;
	    global._takoyaki_air_enemy = noone;
	    global._takoyaki_closest_left_enemy = noone;
	}
	if (global._takoyaki_scan_frame != obj_battle.battle_time) {
	    global._takoyaki_scan_frame = obj_battle.battle_time;
	    global._takoyaki_air_enemy = noone;
	    global._takoyaki_closest_left_enemy = noone;
		with (obj_enemy_parent) {
	        if (hp > 0 && can_hit(other.target_type,target_type) && y > 0) { // 只考虑存活的敌人
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
	            if (x < min_x || (x == min_x && hp > max_hp)) {
	                min_x = x;
	                max_hp = hp;
	                global._takoyaki_closest_left_enemy = id;
	            }
	        }
	    }
	}

	
	
	for(var _grid_id=0;_grid_id<2;_grid_id++){
		var arr_size = array_length(global.enemy_array[ grid_row*(global.grid_cols + 2)+(grid_col + _grid_id)])
		for(var _k=0;_k<arr_size;_k++){
			var _enemy= global.enemy_array[ grid_row*(global.grid_cols + 2)+(grid_col + _grid_id)][_k]
			with(_enemy){
	            if (x >= other.x && x <= other.x + right_range && grid_row == other.grid_row) {
	                if (priority_enemy == noone || hp > priority_enemy.hp) {
	                    priority_enemy = id;
	                }
	            }
			}
		}
	}
	if (priority_enemy != noone) {
        return priority_enemy;
    }
	if (global._takoyaki_air_enemy != noone){
		return global._takoyaki_air_enemy
	}
    return global._takoyaki_closest_left_enemy;
	
	/*
    with (obj_enemy_parent) {
        if (hp > 0 && can_hit(other.target_type,target_type) && y > 0) { // 只考虑存活的敌人
            // 检查是否在右边一格内
            if (x >= other.x && x <= other.x + right_range && grid_row == other.grid_row) {
                if (priority_enemy == noone || hp > priority_enemy.hp) {
                    priority_enemy = id;
                }
            }
            //检查是否有空中敌人
			if target_type == "air"{
				if air_enemy != noone && instance_exists(air_enemy){
					if x < air_enemy.x{
						air_enemy = id
					}
				}
				else{
					air_enemy = id
				}
			}
			
            // 同时寻找最左侧且生命值最高的敌人
            if (x < min_x || (x == min_x && hp > max_hp)) {
                min_x = x;
                max_hp = hp;
                closest_left_enemy = id;
            }
        }
    }
    
    // 优先返回右边一格内的敌人，如果没有则返回最左侧敌人
    if (priority_enemy != noone) {
        return priority_enemy;
    }
	if (air_enemy != noone){
		return air_enemy
	}
	
    return closest_left_enemy;
	*/
}
// 追踪弹统一走【弹幕管理器 obj_Homing_Bullet_Management】（结构体弹幕，不占实例）：
//   模式 1 = 先打发射卡前方（本行、正前方 150px 内血最高），前方没人再退回"全场最左"
//   —— 这就是原版那颗 obj_takoyaki_bullet 的行为，现在由管理器负责，那个对象不再需要。
//   目标不用卡片算了：锚点（发射卡）由管理器自动认，索敌它每帧自己做。
//   贴图按服饰换：takoyaki_cancer 服饰 → spr_takoyaki_cancer_bullet(_1)。
_net_un = (variable_instance_get(id, "_net_card_equipped_attire_id")  == noone)

var _bspr = spr_takoyaki_bullet;
if _net_un&&card_equipped_attire_id(plant_id) == "takoyaki_cancer"||variable_instance_get(id, "_net_card_equipped_attire_id")=="takoyaki_cancer"{
	_bspr = spr_takoyaki_cancer_bullet
	if shape >= 2{
		_bspr = spr_takoyaki_cancer_bullet_1
	}
}

homing_bullet_add(_bspr, 1.8, x, y - 55, 10, atk, "track", "obj_takoyaki_bullet_effect", "", "", 1);
/*
if shape == 2{
	var inst2 = instance_create_depth(x+40,y-55,depth-500,obj_takoyaki_bullet)
	inst2.damage = atk
	inst2.move_speed = 10
	inst2.target_enemy = target
	inst2.timer = -15
	inst2.banding_card_obj = id
	inst2.row = grid_row
	if _net_un&&card_equipped_attire_id(plant_id) == "takoyaki_cancer"||variable_instance_get(id, "_net_card_equipped_attire_id")=="takoyaki_cancer"{
	inst2.sprite_index = spr_takoyaki_cancer_bullet
	if shape >= 2{
		inst2.sprite_index = spr_takoyaki_cancer_bullet_1
	}
}
}*/
audio_play_sound(snd_throw,0,0)