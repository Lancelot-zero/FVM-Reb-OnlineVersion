

var bak_battle_level_stage =  obj_battle.level_stage
var bak_battle_current_subwave =  obj_battle.current_subwave
var bak_battle_current_wave =  obj_battle.current_wave
var bak_battle_total_wave =  obj_battle.total_wave
if(global.network.mode == "client"){
	obj_battle.level_stage = client_obj_battle_level_stage
	obj_battle.current_subwave = client_obj_battle_current_subwave
	obj_battle.current_wave = client_obj_battle_current_wave
	obj_battle.total_wave = client_obj_battle_total_wave
	level_stage = client_level_stage // 直接覆盖
	elite_wave = client_elite_wave // 直接覆盖
	
}

if obj_battle.level_stage == "boss"{
	draw_set_colour(c_white)
	draw_set_font(font_yuan)
	draw_set_valign(fa_middle)
	draw_set_halign(fa_left)

	var level_text = global.level_data.name
	draw_text(x-250,y-42,level_text)

	var diff_text = "美味"
	if global.difficulty == 0{
		diff_text = "美味"
		draw_set_colour(c_lime)
	}
	else if global.difficulty == 1{
		diff_text = "火山"
		draw_set_colour(c_aqua)
	}
	else if global.difficulty == 2{
		diff_text = "浮空"
		draw_set_colour(c_red)
	}
	else if global.difficulty == 3{
		diff_text = "星际"
		draw_set_colour(c_purple)
	}
	draw_text(x-230+string_width(level_text),y-42,diff_text)
	exit
}

draw_set_colour(c_white)
draw_set_font(font_yuan)
draw_set_valign(fa_middle)
draw_set_halign(fa_left)

draw_sprite_ext(spr_level_progress_bg_right,0,x,y,1.8,1.8,0,c_white,1)
draw_sprite_ext(spr_level_progress_bg_middle, 0, x, y, 65, 0.9, 0, c_white, 1)
draw_sprite_ext(spr_level_progress_bg_left, 0, x-260, y, 0.9, 0.9, 0, c_white, 1)

draw_sprite_ext(spr_level_wave_text,0,x-340,y-40,1.8,1.8,0,c_white,1)

/*
var level_progress = 0
var current_total_subwaves = 0
if obj_battle.current_wave < obj_battle.total_wave{
	current_total_subwaves = array_length(global.level_file.waves[obj_battle.current_wave].subwaves)
}
else{
	current_total_subwaves = array_length(global.level_file.waves[obj_battle.current_wave-1].subwaves)
}*/

var level_progress = 0
var _wave_count = 0;
if (variable_global_exists("level_file")
    && variable_struct_exists(global.level_file, "waves")
    && is_array(global.level_file.waves)) {
    _wave_count = array_length(global.level_file.waves);
}
if (_wave_count > 0) {
    var _idx = obj_battle.current_wave;
    if (_idx >= obj_battle.total_wave) _idx -= 1;
    _idx = clamp(_idx, 0, _wave_count - 1);
    current_total_subwaves = array_length(global.level_file.waves[_idx].subwaves);
}

if obj_battle.current_wave == total_wave - 1 && obj_battle.current_subwave == current_total_subwaves{
	last_wave = true
	if total_wave < 10{
		draw_sprite_ext(spr_level_wave_number,total_wave mod 10,x-340,y-40,1.8,1.8,0,c_white,1)
	}
	else{
		draw_sprite_ext(spr_level_wave_number,total_wave div 10,x-350,y-40,1.8,1.8,0,c_white,1)
		draw_sprite_ext(spr_level_wave_number,total_wave mod 10,x-330,y-40,1.8,1.8,0,c_white,1)
	}
}
else{
	if obj_battle.current_wave < 10{
		draw_sprite_ext(spr_level_wave_number,obj_battle.current_wave mod 10,x-340,y-40,1.8,1.8,0,c_white,1)
	}
	else{
		draw_sprite_ext(spr_level_wave_number,obj_battle.current_wave div 10,x-350,y-40,1.8,1.8,0,c_white,1)
		draw_sprite_ext(spr_level_wave_number,obj_battle.current_wave mod 10,x-330,y-40,1.8,1.8,0,c_white,1)
	}
}
if level_stage == "pre"{
	level_progress = obj_battle.current_wave / elite_wave + (obj_battle.current_subwave /current_total_subwaves)/elite_wave
	draw_sprite_ext(spr_level_progress_bar,0,x+3.6,y,130*level_progress+1.8,1.8,0,c_white,1)
	for(var i = 0; i < elite_wave;i++){
		var interval = 260 / elite_wave
		if i <= obj_battle.current_wave - 1{
			draw_sprite_ext(spr_level_progress_flag, 0, x-(i+1)*interval, y, 0.45, 0.45, 0, c_white, 1)
		}
		else{
			draw_sprite_ext(spr_level_progress_flag, 0, x-(i+1)*interval, y+15, 0.45, 0.45, 0, c_white, 1)
		}
	}
}
else if level_stage == "elite"{
	level_progress = (obj_battle.current_wave-elite_wave) / (total_wave-elite_wave) + (obj_battle.current_subwave /current_total_subwaves)/(total_wave - elite_wave)
	draw_sprite_ext(spr_level_progress_bar,0,x+3.6,y,130*level_progress+1.8,1.8,0,c_white,1)
	for(var i = 0; i < (total_wave-elite_wave);i++){
		var interval = 260 / (total_wave-elite_wave)
		if i <= obj_battle.current_wave -elite_wave- 1{
			draw_sprite_ext(spr_level_progress_flag, 0, x-(i+1)*interval, y, 0.45, 0.45, 0, c_white, 1)
		}
		else{
			draw_sprite_ext(spr_level_progress_flag, 0, x-(i+1)*interval, y+15, 0.45, 0.45, 0, c_white, 1)
		}
	}
}

draw_sprite_ext(spr_level_progress_icon, 0, x-260*level_progress, y, 0.9, 0.9, 0, c_white, 1)

draw_set_colour(c_white)
draw_set_font(font_yuan)
draw_set_valign(fa_middle)
draw_set_halign(fa_left)

var level_text = global.level_data.name

var bak_difficulty = global.difficulty
if(global.network.mode == "client"){
	global.difficulty = client_difficulty
}

draw_text(x-250,y-42,level_text)
var diff_text = "美味"
if global.difficulty == 0{
	diff_text = "美味"
	draw_set_colour(c_lime)
}
else if global.difficulty == 1{
	diff_text = "火山"
	draw_set_colour(c_aqua)
}
else if global.difficulty == 2{
	diff_text = "浮空"
	draw_set_colour(c_red)
}
else if global.difficulty == 3{
	diff_text = "星际"
	draw_set_colour(c_purple)
}

draw_text(x-230+string_width(level_text),y-42,diff_text)



if(global.network.mode == "client"){
	global.difficulty = bak_difficulty
	obj_battle.level_stage = bak_battle_level_stage
	obj_battle.current_subwave = bak_battle_current_subwave
	obj_battle.current_wave = bak_battle_current_wave
	obj_battle.total_wave = bak_battle_total_wave
}

if(global.network.mode == "server"){
	var _net_id = global.network.map_instance_id_net_id[? id];
	var _ps = { 
		client_level_stage:level_stage,
		client_difficulty:global.difficulty,
		client_obj_battle_level_stage: obj_battle.level_stage, 
		client_obj_battle_current_subwave:obj_battle.current_subwave,
		client_obj_battle_current_wave:obj_battle.current_wave,
		client_obj_battle_total_wave:obj_battle.total_wave,
		client_level_stage:level_stage,
		client_elite_wave:elite_wave};
	var _props = json_stringify(_ps);
	var _cl = global.network.connected_clients;
	
    var _list = global.network.connected_clients;
	for (var _j = 0; _j < array_length(_cl); _j++) {
		send_message(_list[_j], MSG_MODIFY_PROP, _net_id, _props);
	}
}

