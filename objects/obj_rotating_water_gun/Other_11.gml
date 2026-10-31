var _x = x;
var _y = y;
var _range = 320
var _row_range = 2

with (obj_enemy_parent) {
	// 友化敌人不算敌人：跳过，不结算伤害
	if (variable_instance_exists(id, "is_friend") && is_friend) continue
	if (abs(x - other.x) < _range && abs(grid_row-other.grid_row) <= _row_range){
		hp -= other.atk;
		event_user(0)
		
		if get_gem_index("ice_gem")!= -1{
			var f_value = get_gem_info("ice_gem").ice_time[get_gem_level("ice_gem")] * 60
			if ice_timer < f_value{
				ice_timer = f_value
			}
		}
	}
}

var inst = instance_create_depth(x,y+45,-800,obj_coke_bomb_explode)
inst.sprite_index = spr_rotating_water_gun_bullet
audio_play_sound(snd_coffee_pot_attack,0,0)