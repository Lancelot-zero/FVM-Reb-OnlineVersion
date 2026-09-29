if global.is_paused{
	exit
}
timer++
if state == "appear"{
	image_index = floor(timer/5) mod 9
	if timer >= 9 * 5{
		state = "idle"
	}
}
if state == "idle"{
	image_index = 8
}

if timer mod 1800 == 0 && global.network.mode != "client"{
	var i = irandom_range(0,2)
	var _m = instance_create_depth(x,y+38,depth,enemy_list[i])
	network_spawn_enemy(_m.x, _m.y, _m)
}

var grid_pos = get_grid_position_from_world(x,y)

grid_col = grid_pos.col
grid_row = grid_pos.row

if global.grid_terrains[grid_pos.row][grid_pos.col].type != "obstacle"{
	current_grid_type = global.grid_terrains[grid_pos.row][grid_pos.col].type
	global.grid_terrains[grid_pos.row][grid_pos.col].type = "obstacle"
}