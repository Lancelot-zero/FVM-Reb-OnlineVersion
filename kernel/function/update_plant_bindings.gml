function update_plant_bindings(_p) {
	if (!instance_exists(_p)) return;
	if (variable_struct_exists(_p, "banding_star_obj")&&instance_exists(_p.banding_star_obj)) {
		_p.banding_star_obj.x = _p.x;
		_p.banding_star_obj.y = _p.y - 5;
		_p.banding_star_obj.depth = _p.depth - 1;
	}
	if (variable_struct_exists(_p, "banding_sleep_obj")&&instance_exists(_p.banding_sleep_obj)) {
		_p.banding_sleep_obj.x = _p.x - 15;
		_p.banding_sleep_obj.y = _p.y - 20;
		_p.banding_sleep_obj.depth = _p.depth - 1;
	}
	if (variable_struct_exists(_p, "banding_water_obj")&&instance_exists(_p.banding_water_obj)) {
		_p.banding_water_obj.x = _p.x;
		_p.banding_water_obj.y = _p.y;
		_p.banding_water_obj.depth = _p.depth + 5;
	}
	// 玩家绑定的武器/护盾跟着走
	if (ds_map_exists(global._move_instance_map, _p.id)) {
		var _list = global._move_instance_map[? _p.id];
		for (var _i = ds_list_size(_list) - 1; _i >= 0; _i--) {
			var _ins = _list[| _i];
			if (!instance_exists(_ins)) {
				ds_list_delete(_list, _i);
				continue;
			}
			if (!variable_instance_exists(_ins, "parent_player") || _ins.parent_player != _p) continue;
			var grid_pos = get_grid_position_from_world(_p.x,_p.y)
			_p.grid_col = grid_pos.col
			_p.grid_row = grid_pos.row
			_ins.x = _p.x;
			_ins.y = _p.y;
			if (_ins.object_index == obj_player_shield) {
				_ins.depth = _p.depth;
			} else {
				_ins.x -= 10;
				_ins.y -= 100;
				_ins.depth = _p.depth - 1;
			}
			_ins.grid_col = _p.grid_col;
			_ins.grid_row = _p.grid_row;
		}
	}
}
