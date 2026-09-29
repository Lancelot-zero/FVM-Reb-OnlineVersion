// obj_small_furnace 的 Create 事件
//plant_id = "small_fire";  // 唯一标识符
event_inherited();  // 继承父对象属性
plant_id = "curry_lobster_cannon"; 
// 设置对象类型和精灵
obj_type = object_index;
current_level = 1
event_user(0)
if shape == 0{
	sprite_index = spr_curry_lobster_cannon
}
else if shape == 1{
	sprite_index = spr_curry_lobster_cannon_1
}
else if shape == 2{
	sprite_index = spr_curry_lobster_cannon_2
}

if(variable_instance_get(id, "_net_card_equipped_attire_id")==noone){
	if card_equipped_attire_id(plant_id) != -1{
		var spr_list = get_attire_info(card_equipped_attire_id(plant_id)).spr
		sprite_index = spr_list[shape]
	}
}


// ========== 特定属性默认值 ==========

attack_anim = 21;
idle_anim = 11
if card_equipped_attire_id(plant_id) == "lobster_athena"{
	idle_anim = 11
	attack_anim = 31
}
flash_speed = 5
plant_type = "normal"
is_slowdown = false
target_type = "track"
drown_timer = 0

function find_priority_enemy() {
    // 每帧缓存一次索敌结果（与调用者位置无关，炮台与炮弹共享）
    if (!variable_global_exists("_curry_cannon_scan_frame")) {
        global._curry_cannon_scan_frame = -1;
        global._curry_cannon_target = noone;
    }
    if (global._curry_cannon_scan_frame != obj_battle.battle_time) {
        global._curry_cannon_scan_frame = obj_battle.battle_time;
        global._curry_cannon_target = noone;
        var _min_x = room_width; // 初始化为房间宽度
        var _max_hp = 0;
    
	    // 检查右边一格内是否有敌人（假设一格为80像素）
	    with (obj_enemy_parent) {
	        if (hp > 0 && can_hit(other.target_type,target_type) && y > 0) { // 只考虑存活的敌人
			
	            // 同时寻找最左侧且最大生命值最高的敌人
	            if (maxhp > _max_hp || (x < _min_x && maxhp == _max_hp)) {
	                _min_x = x;
	                _max_hp = maxhp;
	                global._curry_cannon_target = id;
	            }
	        }
	    }
    }
    return global._curry_cannon_target;
}