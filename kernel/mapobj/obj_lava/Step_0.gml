if global.is_paused{
	exit
}


_grid_pos = get_grid_position_from_world(x,y)
depth = calculate_plant_depth(_grid_pos.col,_grid_pos.row,"normal")-5
 
if global.debug{
	image_alpha = 0.5
}
var grid_pos = get_world_position_from_grid(col,row)
timer++


var card_list = ds_grid_get(global.grid_plants, col, row);
if(ds_exists(card_list,ds_type_list)){
    for(var i=0;i<ds_list_size(card_list);i++){
        var it = card_list[|i];
		if(instance_exists(it))
        {
            if(variable_instance_exists(it, "plant_id") && it.plant_id == "cotton_candy")
                it.on_lava = true;
        }
    }
}

/*
with obj_card_parent{
	if plant_id == "cotton_candy" && grid_row == other.row && grid_col == other.col{
		on_lava = true
	}
}*/


if (!variable_global_exists("_obj_enemy_parent_scan_frame")) 
{
    global._obj_enemy_parent_scan_frame = -1;
    global._obj_enemy_parent_grid = ds_grid_create(global.grid_cols, global.grid_rows);
    
    for (var _col = 0; _col < global.grid_cols; _col++) 
    for (var _row = 0; _row < global.grid_rows; _row++) 
    {
        var enemy_list = ds_list_create();
        ds_grid_set(global._obj_enemy_parent_grid, _col, _row, enemy_list);
    }

}

if (global._obj_enemy_parent_scan_frame != obj_battle.battle_time) 
{
    global._obj_enemy_parent_scan_frame = obj_battle.battle_time;
    for (var _col = 0; _col < global.grid_cols; _col++) 
    for (var _row = 0; _row < global.grid_rows; _row++) 
    {
        var enemy_list = ds_grid_get(global._obj_enemy_parent_grid,_col,_row);
        if(ds_exists(enemy_list, ds_type_list))
            ds_list_clear(enemy_list);
    }
    
    with obj_enemy_parent
    {
        var c = grid_col;
        var r = grid_row;
        if (c >= 0 && c < global.grid_cols && r >=0 && r < global.grid_rows)
        {
            var enemy_list = ds_grid_get(global._obj_enemy_parent_grid, c, r);
            if(ds_exists(enemy_list, ds_type_list))  ds_list_add(enemy_list, id);
        }
    }
}

var enemy_list = ds_grid_get(global._obj_enemy_parent_grid,col,row)
	
	

if timer mod 60 == 0{
	var plant_in_range = noone;
        
	var plant_order_list = [noone,noone,noone,noone,noone]
	
	if(ds_exists(card_list,ds_type_list)){
	    for(var _i=0;_i<ds_list_size(card_list);_i++){
	        var it = card_list[|_i];
			if(instance_exists(it))
	        {
	            with(it){
				    for (var i = 0; i < array_length(other.damage_order); i++) {
		                var tar_type = other.damage_order[i]                    
		                if (plant_type == tar_type) {
		                    plant_order_list[i] = id;
		                    break;
		                }
		            }
				}
	        }
	    }
	}
	/*
    // 使用碰撞检测查找攻击范围内的植物
    with (obj_card_parent) {
		var dx = x - other.x;
		var dy = y - other.y;
		var is_in_front = false
		is_in_front = grid_row == other.row && grid_col == other.col
				
        // 检查是否在攻击范围内
        if (is_in_front) {
            // 按铲除顺序优先选择
            for (var i = 0; i < array_length(other.damage_order); i++) {
                var tar_type = other.damage_order[i]
                    
                if (plant_type == tar_type) {
                    plant_order_list[i] = id;
                    break;
                }
            }
                
        }
    }*/
	for(var i = 0 ; i < 5 ; i++){
		if plant_order_list[i] != noone{
			with plant_order_list[i]{
				if (plant_type != "coffee" && !invincible && plant_id != "cotton_candy" && !(plant_id == "player" && hp <= 10)){
					hp -= 10
					event_user(2)
				}
			}
			break
		}
	}
	/*
	with obj_enemy_parent{
		if grid_row == other.row && grid_col == other.col &&
		(target_type == "normal" || target_type == "dance" || target_type == "obstacle"){
			hp -= 10
			event_user(0)

		}
		
	}*/
	
	
	for(var _i=0;_i<ds_list_size(enemy_list);_i++){
		var it = enemy_list[|_i];
		if(instance_exists(it))
		{
		    with(it){
				if(target_type == "normal" || target_type == "dance" || target_type == "obstacle"){
					hp -= 10
					event_user(0)
				}
			}
		}
	}
}
has_mouse = false
/*
with obj_enemy_parent{
	if grid_row == other.row && grid_col == other.col &&
	(target_type == "normal" || target_type == "dance" || target_type == "obstacle"){
		other.has_mouse = true
		ice_timer = 0
		frozen_timer = 0
	}
}
*/

for(var _i=0;_i<ds_list_size(enemy_list);_i++){
	var it = enemy_list[|_i];
	if(instance_exists(it))
	{
	    with(it){
			if(target_type == "normal" || target_type == "dance" || target_type == "obstacle"){
				other.has_mouse = true
				ice_timer = 0
				frozen_timer = 0
			}
		}
	}
}