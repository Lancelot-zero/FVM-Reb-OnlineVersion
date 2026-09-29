if global.is_paused{
	exit
}
if global.debug{
	image_alpha = 0.5
}
var grid_pos = get_world_position_from_grid(col,row)
timer++


has_bubble = false
non_undersea_card = false
/*
with obj_card_parent{
	var is_in_front = false
	is_in_front = grid_row == other.row && grid_col == other.col
	if is_in_front{
		if plant_id == "soda_bubble"{
			on_lava = true
			other.has_bubble = true
		}
	}
}

with obj_card_parent{
	var is_in_front = false
	is_in_front = grid_row == other.row && grid_col == other.col
	if is_in_front{
		if (plant_type != "coffee" && !invincible && array_get_index(other.ignore_list,plant_id) == -1 && !(plant_id == "player" && hp <= 0.05*max_hp)){
			other.non_undersea_card = true
			if !other.has_bubble && other.timer mod 60 == 0{ 
				hp -= 0.05*max_hp
				event_user(2)
			}
		}
	}
}
*/

var card_list = ds_grid_get(global.grid_plants, col, row);
if(ds_exists(card_list,ds_type_list)){
    for(var i=0;i<ds_list_size(card_list);i++){
        var it = card_list[|i];
		if(instance_exists(it))
        {
            if(variable_instance_exists(it, "plant_id") && it.plant_id == "soda_bubble"){
                it.on_lava = true;
				has_bubble = true;
			}
		}
    }
    for(var i=0;i<ds_list_size(card_list);i++){
        var it = card_list[|i];
		if(instance_exists(it))
        {
			with(it){
				if (plant_type != "coffee" && !invincible && array_get_index(other.ignore_list,plant_id) == -1 && !(plant_id == "player" && hp <= 0.05*max_hp)){
					other.non_undersea_card = true
					if !other.has_bubble && other.timer mod 60 == 0{ 
						hp -= 0.05*max_hp
						event_user(2)
					}
				}
			}
		}
    }
}
