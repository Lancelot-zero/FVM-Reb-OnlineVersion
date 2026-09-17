image_xscale = 1.8
image_yscale = 1.8
image_speed = 0

state = "start"
target_col = -1
target_row = -1
b_shape = 0
target_enemy = noone

damage = 70
timer = 0

function find_priority_enemy() {
    // 与 obj_curry_lobster_cannon 共享每帧缓存
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
        if (hp > 0 && can_hit("track",target_type) && y > 0) { // 只考虑存活的敌人
			
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