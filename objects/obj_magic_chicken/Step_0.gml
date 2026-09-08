
if global.is_paused{
	exit
}

event_inherited(); 
var current_flash_speed = flash_speed
if is_slowdown{
	current_flash_speed *= 2
}

attack_timer++

if attack_timer == 15 * current_flash_speed - 1&& global.network.mode != "client"{
	var card_save_data = get_card_info_simple(target_card)
	if card_save_data != false{
		var prev_card_info = get_plant_data_with_skill(target_card,card_save_data.shape,card_save_data.level,card_save_data.skill)
		var card_slot_data = deck_get_card_data(target_card,card_save_data.shape)
		
		var found_plat = noone;
		var platform_shift_x = 0;
		var platform_shift_y = 0;
		var logical_col = -1;
		var logical_row = -1;
		
		with (obj_platform) {
		    var is_axis_x = (variable_instance_exists(id, "move_axis") && move_axis == "x");
		    var shift_x = is_axis_x ? visual_x_shift : 0;
		    var shift_y = (!is_axis_x) ? visual_y_shift : 0;
		    var adj_x = other.x - shift_x;
		    var adj_y = other.y - shift_y;
		    var grid_pos_adj = get_grid_position_from_world(adj_x, adj_y);
		    
		    var c_off = is_axis_x ? current_offset : 0;
		    var r_off = (!is_axis_x) ? current_offset : 0;
		    var p_start_c = start_col + c_off;
		    var p_start_r = start_row + r_off;
		    
		    if (grid_pos_adj.col >= p_start_c && grid_pos_adj.col < p_start_c + width &&
		        grid_pos_adj.row >= p_start_r && grid_pos_adj.row < p_start_r + length) {
		        found_plat = id;
		        logical_col = grid_pos_adj.col;
		        logical_row = grid_pos_adj.row;
		        platform_shift_x = shift_x;
		        platform_shift_y = shift_y;
		        break;
		    }
		    
		    var grid_pos_dir = get_grid_position_from_world(other.x, other.y);
		    if (grid_pos_dir.col >= p_start_c && grid_pos_dir.col < p_start_c + width &&
		        grid_pos_dir.row >= p_start_r && grid_pos_dir.row < p_start_r + length) {
		        found_plat = id;
		        logical_col = grid_pos_dir.col;
		        logical_row = grid_pos_dir.row;
		        platform_shift_x = shift_x;
		        platform_shift_y = shift_y;
		        break;
		    }
		}
		
		if (found_plat == noone) {
		    var grid_pos_direct = get_grid_position_from_world(x, y);
		    logical_col = grid_pos_direct.col;
		    logical_row = grid_pos_direct.row;
		}
		
		var logical_world = get_world_position_from_grid(logical_col, logical_row);
		
		
		
		// 当前网络同步需要的数据，不影响offline模式 ,创建的时候就修改数据
		if (variable_instance_exists(self, "target_card_info")){
			global._net_before_plant_shape = target_card_info[$ "shape"];
			global._net_before_plant_current_level = target_card_info[$ "level"];
			global._net_before_plant_skill = target_card_info[$ "skill"];
			global._net_card_equipped_attire_id = target_card_info[$ "_net_card_equipped_attire_id"]
		}
		var new_card = instance_create_depth(logical_world.x + platform_shift_x, logical_world.y + platform_shift_y, 0, card_slot_data[? "obj"])
		
		global._net_before_plant_shape = noone;
		global._net_before_plant_skill = noone;
		global._net_before_plant_current_level = noone;
		global._net_card_equipped_attire_id = noone;
		
		if (variable_instance_exists(self, "target_card_info")){
			if (variable_struct_exists(target_card_info, "sprite_list")) {
				var _sl = target_card_info[$ "sprite_list"];
				for (var _si = 0; _si < array_length(_sl); _si++) {
				    if (is_string(_sl[_si])) {
				        _sl[_si] = get_load_sprite(_sl[_si]);		            
				    }
				}
				new_card.sprite_list = _sl;
			}
			if (variable_struct_exists(target_card_info, "sprite_index") && target_card_info[$ "sprite_index"] != "") {
				new_card.sprite_index = get_load_sprite(target_card_info[$ "sprite_index"]);
			}
			if (variable_struct_exists(target_card_info, "cycle")) {
				new_card.cycle = target_card_info[$ "cycle"];
			}
			if (variable_struct_exists(target_card_info, "hp")) {
				new_card.max_hp = target_card_info[$ "hp"];
				new_card.hp = target_card_info[$ "hp"];
			}
			if (variable_struct_exists(target_card_info, "grow_time")) {
				new_card.grow_time = target_card_info[$ "grow_time"];
			}
			if (variable_struct_exists(target_card_info, "_net_cookbook_equipped")) {
				new_card._net_cookbook_equipped = target_card_info[$ "_net_cookbook_equipped"];
			}
			
		}
		network_apply_plant_level(new_card);
		card_created(new_card, logical_col, logical_row)
		
	}
}

if attack_timer > current_flash_speed * 29 - 1{
	  x+=1000;
      instance_destroy()
}