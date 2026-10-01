// obj_small_furnace 的 Create 事件
// 唯一标识符
event_inherited();  // 继承父对象属性
plant_id = "tang_hu_lu"; 
// 设置对象类型和精灵
obj_type = object_index;
sprite_index = spr_tang_hu_lu;
current_level = 1
event_user(0)
if shape == 1{
	sprite_index = spr_tang_hu_lu_1
}
else if shape == 2{
	sprite_index = spr_tang_hu_lu_2
}
// ========== 特定属性默认值 ==========

attack_anim = 15;
idle_anim = 15
flash_speed = 5
plant_type = "normal"
is_slowdown = false
target_type = "air_only"

function find_priority_enemy() {
    var closest_left_enemy = noone;
    var min_x = room_width; // 初始化为房间宽度
    var max_hp = 0;
/*    
    // 检查右边一格内是否有敌人（假设一格为80像素）
    with (obj_enemy_parent) {
        if (hp > 0 && can_hit(other.target_type,target_type) && y > 0) { // 只考虑存活的敌人
			
            // 同时寻找最左侧且最大生命值最高的敌人
            if (x < min_x || (x == min_x && maxhp > max_hp)) {
                min_x = x;
                max_hp = maxhp;
                closest_left_enemy = id;
            }
        }
    }
*/

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
	closest_left_enemy = global._tanghulu_best_id;
    return closest_left_enemy;
}