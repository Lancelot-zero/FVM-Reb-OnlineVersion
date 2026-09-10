	// obj_plant_parent 的 Step 事件
	if global.is_paused{
		exit
	}
	
	// 不在网格内，跳过
if (grid_row > global.grid_rows+2&& grid_row < global.grid_rows+62||
	grid_col > global.grid_cols+2&& grid_col < global.grid_cols+62){ 
	visible = false;
	exit;
}else{
	visible = true;
}
	
	if (global.network.mode == "client" && hp <= 0) { exit; }

	if ice_timer > 0{
		ice_timer--
		is_slowdown = true
	}
	else{
		is_slowdown = false
	}
	if frozen_timer > 0{
		frozen_timer--
		is_frozen = true
	}
	else{
		is_frozen = false
	}
	if hp <= 0{
		instance_destroy()
	}
	if flash_value >0{
	
	flash_value -= 10
	
}
if awake_buff_timer > 0{
	awake_buff_timer--
	if state == CARD_STATE.SLEEP{
		if awake_anim > 0{
			state = CARD_STATE.AWAKE
		}
		else{
			state = CARD_STATE.IDLE
		}
		if global.network.mode == "server" && ds_map_exists(global.network.map_instance_id_net_id, id){
			var _net_id = global.network.map_instance_id_net_id[? id];
			var _json = json_stringify({state: state});
			var _list = global.network.connected_clients;
			for (var _j = 0; _j < array_length(_list); _j++){
				send_message(_list[_j], MSG_MODIFY_PROP, _net_id, _json);
			}
		}
	}
}
if state == CARD_STATE.SLEEP && !instance_exists(banding_sleep_obj) && awake_anim == 0{
	banding_sleep_obj = instance_create_depth(x-15,y-20,depth-1,obj_sleep_effect)
	banding_sleep_obj.banding_card_obj = id
}
if instance_exists(banding_sleep_obj){
	banding_sleep_obj.x = x-15
	banding_sleep_obj.y = y-20
	banding_sleep_obj.depth = depth-1
}
if state != CARD_STATE.SLEEP && instance_exists(banding_sleep_obj){
	instance_destroy(banding_sleep_obj)
}

// 计算深度值
//var depth_value = -((y + depth_offset) * 10 + x);
//depth = depth_value - depth_group * 100;



var grid_pos = get_grid_position_from_world(x,y)

if !(variable_instance_exists(id, "platform_grid_lock") && platform_grid_lock) {
    grid_col = grid_pos.col
    grid_row = grid_pos.row
}

depth = calculate_plant_depth(grid_pos.col, grid_pos.row, plant_type)
if instance_exists(banding_star_obj){
banding_star_obj.depth = depth - 1
}

var water_define_pos_y = clamp(grid_col,0,global.grid_cols - 1)
var water_define_pos_x = clamp(grid_row,0,global.grid_rows - 1)

if !instance_exists(banding_water_obj) && global.grid_terrains[water_define_pos_x][water_define_pos_y].type == "water" {
	if plant_type == "lilypad" || (feature_type == "water" && plant_type == "normal"){
		banding_water_obj = instance_create_depth(x,y,depth+5,obj_in_water_effect)
		banding_water_obj.banding_card_obj = id
	}
}
if instance_exists(banding_water_obj) && global.grid_terrains[water_define_pos_x][water_define_pos_y].type != "water"{
	instance_destroy(banding_water_obj)
}

if is_frozen{
	exit
}
// 动画计时器
var current_flash_speed = flash_speed
if is_slowdown{
	current_flash_speed *= 2
}

if(attack_cycle==-1)attack_cycle =  origin_cycle

if is_slowdown {
    cycle = attack_cycle * 2;    
}
else{
	cycle = attack_cycle
}


if timer < current_flash_speed - 1 {
    timer++;
} else {
    switch (state) {
        case CARD_STATE.IDLE:
            if (image_index < idle_anim){ 
				image_index++
			}
            else image_index = 0;
            break;
            
        case CARD_STATE.ATTACK:
            if (image_index >= (idle_anim+1) && image_index <= (idle_anim) + attack_anim) image_index++;
            else image_index = (idle_anim+1);
            break;
		
    }
    timer = 0;
}


if(global.network.mode!="offline"&&!ds_map_exists(global.network.map_instance_id_net_id, id)){
	instance_destroy(id);
	show_debug_message("销毁卡片");
}
