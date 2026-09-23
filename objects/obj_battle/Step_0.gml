if(global.wait_sprite_load && global.network.mode != "client"){
	global.is_paused=true;
}

if global.is_paused{
	exit
}

if global.lose_focus_pause && global.network.mode != "client"{
	if !window_has_focus() && !global.is_paused{
		global.is_paused = true
		if (global.network.mode == "server") {
			var _cl = global.network.connected_clients;
			for (var i = 0; i < array_length(_cl); i++) {
				send_message(_cl[i], MSG_SERVER_ACTION, 2);
			}
		}
	}
}

battle_time ++
// obj_controller STEP 事件
if global.debug{
	if keyboard_check_pressed(ord("M")){
		var grid_pos = get_grid_position_from_world(mouse_x,mouse_y)
		var inst = instance_create_depth(grid_pos.x,grid_pos.y+38,0,obj_machine_beehive_mouse)
		inst.grid_row = grid_pos.row
		inst.grid_col = grid_pos.col
		inst.frozen_timer = 0000
	}
	if keyboard_check_pressed(ord("N")){
		var enemy_row = irandom_range(0,global.grid_rows-1)
		var enemy_pos = get_world_position_from_grid(10,enemy_row)
		instance_create_depth(enemy_pos.x-80,enemy_pos.y+33,-200,obj_blonde_mary)
		boss_count++
		//var grid_pos = get_grid_position_from_world(mouse_x,mouse_y)
		//var inst = instance_create_depth(grid_pos.x,grid_pos.y+38,0,obj_mario_mouse)
		//inst.grid_row = grid_pos.row
		//inst.grid_col = grid_pos.col
		//inst.frozen_timer = 0000
	}
	if keyboard_check_pressed(ord("L")){
		var grid_pos = get_grid_position_from_world(mouse_x,mouse_y)
		var inst = instance_create_depth(grid_pos.x,grid_pos.y+38,0,obj_war_god_soldier)
		inst.grid_row = grid_pos.row
		inst.grid_col = grid_pos.col
		inst.frozen_timer = 0000
	}
	if keyboard_check_pressed(ord("K")){
		var grid_pos = get_grid_position_from_world(mouse_x,mouse_y)
		var inst = instance_create_depth(grid_pos.x,grid_pos.y+38,0,obj_war_god_summon)
		inst.grid_row = grid_pos.row
		inst.grid_col = grid_pos.col
		inst.frozen_timer = 0000
	}
	if keyboard_check_pressed(ord("B")){
		var grid_pos = get_grid_position_from_world(mouse_x,mouse_y)
		var inst = instance_create_depth(grid_pos.x,grid_pos.y+38,0,obj_snail_mouse)
		inst.grid_row = grid_pos.row
		inst.grid_col = grid_pos.col
		inst.frozen_timer = 0000
	}
	if keyboard_check_pressed(ord("J")){
		if (global.network.mode == "server") {
				var _clients = global.network.connected_clients;
				for (var i = 0; i < array_length(_clients); i++) {
					send_message(_clients[i], MSG_GAME_OVER, 1);
				}
		}
		if (global.network.mode != "client"){
			global.is_paused = true
			global.game_over = true
			var inst = instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over)
			inst.sprite_index = spr_win
			audio_play_sound(snd_win,0,0)
		}
	}

	if keyboard_check_pressed(ord("R")){
		var grid_pos = get_grid_position_from_world(mouse_x, mouse_y);
		if (grid_pos.col >= 0 && grid_pos.col < global.grid_cols &&
			grid_pos.row >= 0 && grid_pos.row < global.grid_rows) {

			var new_plant = instance_create_depth(grid_pos.x, grid_pos.y, 0,obj_small_fire);
			var depth_value = calculate_plant_depth(grid_pos.col, grid_pos.row, new_plant.plant_type);
			card_created(new_plant, grid_pos.col, grid_pos.row);
			new_plant.depth = depth_value
			new_plant.flame_produce = 15000
			new_plant.ice_timer = 600
			instance_create_depth(grid_pos.x,grid_pos.y,-2,obj_place_effect)
			audio_play_sound(snd_place1,0,0)
		}
	}

	if keyboard_check_pressed(ord("A")){
		var grid_pos = get_grid_position_from_world(mouse_x, mouse_y);
		if (grid_pos.col >= 0 && grid_pos.col < global.grid_cols &&
			grid_pos.row >= 0 && grid_pos.row < global.grid_rows) {

			var new_plant = instance_create_depth(grid_pos.x, grid_pos.y, 0,obj_xiao_long_bao);
			var depth_value = calculate_plant_depth(grid_pos.col, grid_pos.row, new_plant.plant_type);
			card_created(new_plant, grid_pos.col, grid_pos.row);
			new_plant.depth = depth_value
			new_plant.atk = 90
			new_plant.ice_timer = 600
			new_plant.frozen_timer = 240
			instance_create_depth(grid_pos.x,grid_pos.y,-2,obj_place_effect)
			audio_play_sound(snd_place1,0,0)
		}
	}
}

//计时器逻辑
if global.level_file.time_limit != 0 && time_limit == -1{
	time_limit = global.level_file.time_limit * 60
}
if time_limit > 0{
	if !timer_pause{
		time_limit --
	}
	if time_limit <= 0{
		if global.network.mode == "server"{
			var _clients = global.network.connected_clients;
			for (var i = 0; i < array_length(_clients); i++) {
				send_message(_clients[i], MSG_GAME_OVER, 0);
			}
		}
		if global.network.mode != "client"{
			global.is_paused = true
			global.game_over = true
			instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over)
			audio_play_sound(snd_lose,0,0)
		}
	}
}



if keyboard_check_pressed(vk_shift) || keyboard_check_pressed(vk_lshift){
	if (global.network.mode == "server") {
		var _cl = global.network.connected_clients;
		for (var i = 0; i < array_length(_cl); i++) {
			send_message(_cl[i], MSG_SERVER_ACTION, speed_up ? 0 : 1);
		}
	}
	if (global.network.mode != "client") 
		speed_up = not speed_up
	if speed_up{
		game_set_speed(120,gamespeed_fps)
	}
	else{
		game_set_speed(60,gamespeed_fps)
	}
}

// game over 后服务端快捷键
if (global.game_over && global.network.mode == "server") {
	if (keyboard_check_pressed(vk_space)) {
		var _cl = global.network.connected_clients;
		for (var i = 0; i < array_length(_cl); i++) {
			send_message(_cl[i], MSG_SERVER_ACTION, 4);
		}
		global.gui_stack.to(room_ready);
	}
	if (keyboard_check_pressed(ord("R"))) {
		var _cl = global.network.connected_clients;
		for (var i = 0; i < array_length(_cl); i++) {
			send_message(_cl[i], MSG_SERVER_ACTION, 5);
		}
		global.gui_stack.pop();
		room_goto(room_battle);
	}
}

if speed_up{
	game_set_speed(120,gamespeed_fps)
}
else{
	game_set_speed(60,gamespeed_fps)
}



if (global.network.mode!="client"){

if(global._VM_battle_start_done){

if (global._VM_prev_subwave != current_subwave||global._VM_prev_wave != current_wave) {
	if (global.network.mode == "server") {
		var _cl = global.network.connected_clients;
		for (var _j = 0; _j < array_length(_cl); _j++) {
			if (buffer_exists(global._VM_SUBWAVE_END)&&current_subwave!=0)
				send_message(_cl[_j], MSG_VM_NOTIFY, json_stringify({hook: "subwave_end", wave: global._VM_prev_wave, subwave: global._VM_prev_subwave}));
			send_message(_cl[_j], MSG_PROGRESS_SYNC, current_wave, current_subwave);
			if (buffer_exists(global._VM_SUBWAVE_START)) 
				send_message(_cl[_j], MSG_VM_NOTIFY, json_stringify({hook: "subwave_start", wave: current_wave, subwave: current_subwave}));
		}
	}
    if (buffer_exists(global._VM_SUBWAVE_END)&&current_subwave!=0) VM_Execute(global.__vm, global._VM_SUBWAVE_END, "_VM_SUBWAVE_END");
	global._VM_prev_subwave = current_subwave;
    if (buffer_exists(global._VM_SUBWAVE_START)) VM_Execute(global.__vm, global._VM_SUBWAVE_START, "_VM_SUBWAVE_START");
    
}


if (global._VM_prev_wave != current_wave) {
	if (global.network.mode == "server") {
		var _cl = global.network.connected_clients;
		for (var _j = 0; _j < array_length(_cl); _j++) {
			if (buffer_exists(global._VM_WAVE_END)&&current_subwave!=0)
				send_message(_cl[_j], MSG_VM_NOTIFY, json_stringify({hook: "wave_end", wave: global._VM_prev_wave, subwave: global._VM_prev_subwave}));
			send_message(_cl[_j], MSG_PROGRESS_SYNC, current_wave, current_subwave);
			if (buffer_exists(global._VM_WAVE_START)) 
				send_message(_cl[_j], MSG_VM_NOTIFY, json_stringify({hook: "wave_start", wave: current_wave, subwave: current_subwave}));
		}
	}
    if (buffer_exists(global._VM_WAVE_END)&&current_wave!=0) VM_Execute(global.__vm, global._VM_WAVE_END, "_VM_WAVE_END");
	global._VM_prev_wave = current_wave;
    if (buffer_exists(global._VM_WAVE_START)) VM_Execute(global.__vm, global._VM_WAVE_START, "_VM_WAVE_START");
}

}

if global._VM_wave_auto && battle_time >= (global.level_file.first_wave_delay * 60) && level_stage == "ready" {

	level_stage = "pre"
	audio_play_sound(snd_mouse_wave_attack, 0, 0)

	enemy_subwave_summon()

	current_subwave += 1;
}
var current_total_subwaves = 0
var wave_data = {}
if current_wave < total_wave{
	current_total_subwaves = array_length(global.level_file.waves[current_wave].subwaves)
	wave_data = global.level_file.waves[current_wave]
}
else{
	if(current_wave>0){
		current_total_subwaves = array_length(global.level_file.waves[current_wave-1].subwaves)
		wave_data = global.level_file.waves[current_wave-1]
	}
}

if wave_data.boss_wave && level_stage != "boss" && global.save_data.unlocked_items.elite_unlocked && wave_timer == 1{
	level_stage = "boss"
	var enemy_row = irandom_range(0,global.grid_rows-1)
	var enemy_pos = get_world_position_from_grid(10,enemy_row)
	global._mod_pending_enemy_id = wave_data.boss  // mod 敌人创建前写入 pending id
	var boss_inst = instance_create_depth(enemy_pos.x-80,enemy_pos.y+30,-200,global.enemy_map[? wave_data.boss]._obj)
	var boss_2_inst = undefined;
	var enemy_row_2 = undefined;
	var enemy_pos_2 = undefined;
	boss_count ++
	if is_real(global.level_file.version){
		boss_inst.hp *= wave_data.boss_1_hp_modify
		boss_inst.maxhp *= wave_data.boss_1_hp_modify
		if wave_data.boss2 != ""{
			enemy_row_2 = irandom_range(0,global.grid_rows-1)
			enemy_pos_2 = get_world_position_from_grid(10,enemy_row_2)
			global._mod_pending_enemy_id = wave_data.boss2  // mod 敌人创建前写入 pending id
			boss_2_inst = instance_create_depth(enemy_pos_2.x-80,enemy_pos_2.y+30,-200,global.enemy_map[? wave_data.boss2]._obj)
			boss_2_inst.hp *= wave_data.boss_2_hp_modify
			boss_2_inst.maxhp *= wave_data.boss_2_hp_modify
			boss_count ++
		}
	}
	
	with obj_battle_music_controller{
		new_battle_music = global.level_data.boss_music
		event_user(0)
	}
	// 服务端同步boss音乐给客户端
	if (global.network.mode == "server") {
		var _cl = global.network.connected_clients;
		for (var _j = 0; _j < array_length(_cl); _j++) {
			send_message(_cl[_j], MSG_MUSIC_SYNC, 2);
		}
	}

	// 服务端广播boss生成给所有客户端
	if (global.network.mode == "server") {
		add_net_id(boss_inst.id);
		var _boss1_net = global.network.map_instance_id_net_id[? boss_inst.id];
		var _boss2_net = undefined;
		if (is_real(global.level_file.version) && wave_data.boss2 != "") {
			add_net_id(boss_2_inst.id);
			_boss2_net = global.network.map_instance_id_net_id[? boss_2_inst.id];
		}
		var _list = global.network.connected_clients;
		var _size = array_length(_list);
		for (var _i = 0; _i < _size; _i++) {
			var _socket = _list[_i];
			send_message(_socket, MSG_SPAWN_BOSS, _boss1_net, enemy_pos.x-80, enemy_pos.y+30, object_get_name(global.enemy_map[? wave_data.boss]._obj), boss_inst.hp, boss_inst.maxhp, enemy_row, boss_inst.random_seed);
		}
		// mod boss：创建后补发身份（enemy_id）
		var _eid1 = variable_instance_exists(boss_inst, "enemy_id") ? boss_inst.enemy_id : "";
		if (_eid1 != "") {
			var _eid1_json = json_stringify({enemy_id: _eid1});
			for (var _k = 0; _k < _size; _k++) {
				send_message(_list[_k], MSG_MODIFY_PROP, _boss1_net, _eid1_json);
			}
		}
		if (is_real(global.level_file.version) && wave_data.boss2 != "") {
			for (var _i = 0; _i < _size; _i++) {
				var _socket = _list[_i];
				send_message(_socket, MSG_SPAWN_BOSS, _boss2_net, enemy_pos_2.x-80, enemy_pos_2.y+30, object_get_name(global.enemy_map[? wave_data.boss2]._obj), boss_2_inst.hp, boss_2_inst.maxhp, enemy_row_2, boss_2_inst.random_seed);
			}
			// mod boss2：同上
			var _eid2 = variable_instance_exists(boss_2_inst, "enemy_id") ? boss_2_inst.enemy_id : "";
			if (_eid2 != "") {
				var _eid2_json = json_stringify({enemy_id: _eid2});
				for (var _k = 0; _k < _size; _k++) {
					send_message(_list[_k], MSG_MODIFY_PROP, _boss2_net, _eid2_json);
				}
			}
		}
	}
}
if wave_timer <= 0 && level_stage == "pre" && global._VM_wave_auto{
	if(global.save_data.unlocked_items.elite_unlocked) || !global.save_data.unlocked_items.elite_unlocked && current_wave < global.level_file.elite_wave{
		if current_wave < total_wave{
			enemy_subwave_summon()
		}
		if current_subwave < current_total_subwaves-1{
			current_subwave+=1
		}
		else if current_wave < total_wave{
			current_wave += 1
			current_subwave = 0
			audio_play_sound(snd_mouse_wave_attack,0,0)
			instance_create_depth(room_width/2,room_height/2,-300,obj_huge_wave_text)
		}
	}
}
if wave_timer <= 0 && level_stage == "boss" && global._VM_wave_auto{
	enemy_subwave_summon()
	if current_subwave < current_total_subwaves-1{
		current_subwave+=1
	}
	else{
		current_subwave = 0
	}
}


// 服务端每20步同步一次 HP 和位置
if (global.network.mode == "server" && battle_time mod 20 == 0) {
	var _list = global.network.connected_clients;
	var _size = array_length(_list);

	with (obj_enemy_parent) {
		if (hp > 0 && state != ENEMY_STATE.DEAD) {
			var _net_id = (ds_map_exists(global.network.map_instance_id_net_id, id)) ? global.network.map_instance_id_net_id[? id] : -1;
			if (_net_id != -1) {
				for (var _i = 0; _i < _size; _i++) {
					send_message(_list[_i], MSG_ENEMY_HP, _net_id, hp, maxhp);
					send_message(_list[_i], MSG_ENEMY_CALIBRATE, _net_id, x, y);
				}
			}
		}
	}

	with (obj_card_parent) {
		if (hp > 0) {
			var _net_id = (ds_map_exists(global.network.map_instance_id_net_id, id)) ? global.network.map_instance_id_net_id[? id] : -1;
			if (_net_id != -1) {
				for (var _i = 0; _i < _size; _i++) {
					send_message(_list[_i], MSG_UNIT_HP, _net_id, hp, max_hp);
				}
			}
		}
	}
}



if global.debug{
	if keyboard_check_pressed(ord("V")){
		if level_stage == "ready"{
			battle_time = (global.level_file.first_wave_delay * 60)
		}
		if current_subwave < current_total_subwaves{
			current_subwave+=1
		}
		else if  current_wave == total_wave-1 {
			if (global.network.mode == "server") {
				var _clients = global.network.connected_clients;
				for (var i = 0; i < array_length(_clients); i++) {
					send_message(_clients[i], MSG_GAME_OVER, 1);
				}
			}
			if (global.network.mode != "client") {
				global.is_paused = true
				global.game_over = true
				var inst = instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over)
				inst.sprite_index = spr_win
				audio_play_sound(snd_win,0,0)
			}
		}
		else{
			current_wave += 1
			current_subwave = 0
		}
	}
}
}




// 帧 VM 块：每帧执行
if (buffer_exists(global._VM_FRAME)) {
    VM_Execute(global.__vm, global._VM_FRAME, "_VM_FRAME");
}
// 鼠标/键盘输入 VM 块
if (buffer_exists(global._VM_MOUSE_LEFT) && mouse_check_button_pressed(mb_left)) {
    VM_Execute(global.__vm, global._VM_MOUSE_LEFT, "_VM_MOUSE_LEFT");
}
if (buffer_exists(global._VM_MOUSE_RIGHT) && mouse_check_button_pressed(mb_right)) {
    VM_Execute(global.__vm, global._VM_MOUSE_RIGHT, "_VM_MOUSE_RIGHT");
}
if (buffer_exists(global._VM_KEY_PRESSED) && keyboard_check_pressed(vk_anykey)) {
    global._VM_last_key = keyboard_lastkey;
    VM_Execute(global.__vm, global._VM_KEY_PRESSED, "_VM_KEY_PRESSED");
}
if (battle_time mod 5 == 0 && buffer_exists(global._VM_TIMER_5f)) {
    VM_Execute(global.__vm, global._VM_TIMER_5f, "_VM_TIMER_5f");
}
if (battle_time mod 10 == 0 && buffer_exists(global._VM_TIMER_10f)) {
    VM_Execute(global.__vm, global._VM_TIMER_10f, "_VM_TIMER_10f");
}
if (battle_time mod 15 == 0 && buffer_exists(global._VM_TIMER_15f)) {
    VM_Execute(global.__vm, global._VM_TIMER_15f, "_VM_TIMER_15f");
}
if (battle_time mod 30 == 0 && buffer_exists(global._VM_TIMER_30f)) {
    VM_Execute(global.__vm, global._VM_TIMER_30f, "_VM_TIMER_30f");
}
if (battle_time mod 60 == 0 && buffer_exists(global._VM_TIMER_60f)) {
    VM_Execute(global.__vm, global._VM_TIMER_60f, "_VM_TIMER_60f");
}


var size = global.grid_rows * (global.grid_cols + 2);
if (!variable_global_exists("has_enemy_normal") || array_length(global.has_enemy_normal) != size) {
	global.has_enemy_normal      = array_create(size, 0);
	global.has_enemy_obstacle    = array_create(size, 0);
	global.has_enemy_diver       = array_create(size, 0);
	global.has_enemy_air         = array_create(size, 0);
	global.has_enemy_dance       = array_create(size, 0);
	global.has_enemy_underground = array_create(size, 0);
	global.enemy_array = array_create(size);
	for(var _e = 0; _e < size; _e++){
		global.enemy_array[_e] = [];
	}
}
if (!variable_global_exists("enemy_array_left") || array_length(global.enemy_array_left) != global.grid_rows) {
	global.enemy_array_left = array_create(global.grid_rows);
	for(var _e = 0; _e < global.grid_rows; _e++){
		global.enemy_array_left[_e] = [];
	}
}

for(var i = 0; i < size; i++){
	global.has_enemy_normal[i]      = 0;
	global.has_enemy_obstacle[i]    = 0;
	global.has_enemy_diver[i]       = 0;
	global.has_enemy_air[i]         = 0;
	global.has_enemy_dance[i]       = 0;
	global.has_enemy_underground[i] = 0;
	array_resize(global.enemy_array[i], 0);
}
for(var _e = 0; _e < global.grid_rows; _e++){
	array_resize(global.enemy_array_left[_e], 0);
}
var stride = global.grid_cols + 2;

with obj_enemy_parent{
	// 命中位掩码：子弹判定只用一次位与（省掉每帧每弹每敌的 can_hit 字符串比较）
	// 位定义在 scr_command_VM 的 bullet_type_mask 上面，两边必须一致
	switch(target_type)
	{
		case "normal":      tbit = HIT_NORMAL;      break;
		case "air":         tbit = HIT_AIR;         break;
		case "dance":       tbit = HIT_DANCE;       break;
		case "obstacle":    tbit = HIT_OBSTACLE;    break;
		case "diver":       tbit = HIT_DIVER;       break;
		case "underground": tbit = HIT_UNDERGROUND; break;
		default:            tbit = HIT_OTHER;       break;
	}
	// 冰冻：顺手记住这个敌人吃不吃冰冻，子弹那边就不用每颗弹每帧再 variable_instance_exists
	ice_ok = variable_instance_exists(self, "ice_timer");
	if(grid_row >= 0 && grid_row < global.grid_rows && grid_col >= 0 && grid_col < stride)
	{
		var idx = grid_row * stride + grid_col;
		switch(target_type)
		{
			case "normal":
				global.has_enemy_normal[idx] += 1;
				break;
			case "obstacle":
				global.has_enemy_obstacle[idx] += 1;
				break;
			case "diver":
				global.has_enemy_diver[idx] += 1;
				break;
			case "air":
				global.has_enemy_air[idx] += 1;
				break;
			case "dance":
				global.has_enemy_dance[idx] += 1;
				break;
			case "underground":
				global.has_enemy_underground[idx] += 1;
				break;
		}
		array_push(global.enemy_array[idx],id)
	}
	if(grid_row >= 0 && grid_row < global.grid_rows && grid_col < 0 ){
		array_push(global.enemy_array_left[grid_row],id)
	}
	
	if(is_boss==false)continue
	if(pre_state==state)continue;
	if (buffer_exists(global._VM_BOSS_STATE_CHANGE)) {
		global._VM_last_boss_state_change_id = id
		global._VM_last_boss_old_state = pre_state
		global._VM_last_boss_new_state = state
		VM_Execute(global.__vm, global._VM_BOSS_STATE_CHANGE, "_VM_BOSS_STATE_CHANGE");
	}
	pre_state = state
}

// ============================================================
// 子弹格子表（供 VM 的 VM_ArrayExists / VM_CellCount / VM_CellItem
//                / VM_CellContains / VM_ArrayContains 查询）
//   下标和平面的敌人表同一套：idx = row * stride + col
//   ⚠️ 行**不能用 y 反算**：子弹出生在卡片 y-75 之类的位置，而卡片 y 在格子顶边附近，
//      按 y 算会整体偏一整行（第 0 行的子弹还会算成 -1 行被丢掉）。
//      原版火盆/布丁的判定是 `row == other.grid_row`，考的是子弹自己的 row，所以这里也用它；
//      只有没 row 的（垂直弹之类）才退回按 y 反算。
//   列按 x 反算，并且**只收落在格子长宽中间 50% 的**（四周各留 25% 三不管带，
//      免得贴着格边飞的子弹被相邻两格的卡片同时触发）。
//   三张表分开放：
//     global.bullet_array_special —— 带卡片侧碰撞事件（火盆点燃 / 布丁反弹）的 10 个原版子弹
//     global.bullet_array_normal  —— 其余原版子弹
//     global.bullet_array_mod     —— mod 子弹（obj_bullet_mod）
// ============================================================
if (!variable_global_exists("bullet_array_special")
    || array_length(global.bullet_array_special) != size) {
    global.bullet_array_special = array_create(size);
    global.bullet_array_normal  = array_create(size);
    global.bullet_array_mod     = array_create(size);
    for (var _bi = 0; _bi < size; _bi++) {
        global.bullet_array_special[_bi] = [];
        global.bullet_array_normal[_bi]  = [];
        global.bullet_array_mod[_bi]     = [];
    }
    // 运行时没有「这颗子弹带不带那个碰撞事件」的标记，只能把对象列出来
    global.special_bullet_objs = [
        obj_waterpipe_bullet, obj_icegun_bullet, obj_icelongbao_bullet,
        obj_icelongbao_bullet_vertical, obj_mightygun_bullet, obj_triplewinerack_bullet,
        obj_xiaolongbao_bullet, obj_xiaolongbao_bullet_vertical,
        obj_coalstarfish_bullet, obj_tarsprayer_bullet
    ];
}
for (var _bi = 0; _bi < size; _bi++) {
    array_resize(global.bullet_array_special[_bi], 0);
    array_resize(global.bullet_array_normal[_bi], 0);
    array_resize(global.bullet_array_mod[_bi], 0);
}

var _b_ox = global.grid_offset_x;
var _b_oy = global.grid_offset_y;
var _b_cw = global.grid_cell_size_x;
var _b_ch = global.grid_cell_size_y;

with (obj_bullet_parent) {
    // 行：优先用子弹自己的 row（原版就是这么判的）；没有才按 y 反算
    var _brow = -1;
    if (variable_instance_exists(id, "row")) _brow = row;
    else _brow = floor((y - _b_oy) / _b_ch);

    // 列：按 x 反算，只收格子中间 50%
    var _fgx = (x - _b_ox) / _b_cw;
    var _bcol = floor(_fgx);
    if (_bcol >= 0 && _bcol < global.grid_cols
        && (_fgx - _bcol) >= 0.25 && (_fgx - _bcol) <= 0.75
        && _brow >= 0 && _brow < global.grid_rows) {
        var _bidx = _brow * stride + _bcol;
        if (array_contains(global.special_bullet_objs, object_index)) {
            array_push(global.bullet_array_special[_bidx], id);
        } else {
            array_push(global.bullet_array_normal[_bidx], id);
        }
    }
}

// 焦油喷雾的子弹没挂 obj_bullet_parent，单独收一趟（它属于 special 那一类）
with (obj_tarsprayer_bullet) {
    var _brow = -1;
    if (variable_instance_exists(id, "row")) _brow = row;
    else _brow = floor((y - _b_oy) / _b_ch);

    var _fgx = (x - _b_ox) / _b_cw;
    var _bcol = floor(_fgx);
    if (_bcol >= 0 && _bcol < global.grid_cols
        && (_fgx - _bcol) >= 0.25 && (_fgx - _bcol) <= 0.75
        && _brow >= 0 && _brow < global.grid_rows) {
        array_push(global.bullet_array_special[_brow * stride + _bcol], id);
    }
}

// mod 子弹也是独立对象，不挂在 obj_bullet_parent 下
with (obj_bullet_mod) {
    var _brow = -1;
    if (variable_instance_exists(id, "row")) _brow = row;
    else _brow = floor((y - _b_oy) / _b_ch);

    var _fgx = (x - _b_ox) / _b_cw;
    var _bcol = floor(_fgx);
    if (_bcol >= 0 && _bcol < global.grid_cols
        && (_fgx - _bcol) >= 0.25 && (_fgx - _bcol) <= 0.75
        && _brow >= 0 && _brow < global.grid_rows) {
        array_push(global.bullet_array_mod[_brow * stride + _bcol], id);
    }
}

for(var i = 0; i < global.grid_rows; i++){
	var row_base = i * stride;
	for(var j = 1; j < stride; j++){
		global.has_enemy_normal[row_base + j] += global.has_enemy_normal[row_base + j - 1];
		global.has_enemy_obstacle[row_base + j] += global.has_enemy_obstacle[row_base + j - 1];
		global.has_enemy_diver[row_base + j] += global.has_enemy_diver[row_base + j - 1];
		global.has_enemy_air[row_base + j] += global.has_enemy_air[row_base + j - 1];
		global.has_enemy_dance[row_base + j] += global.has_enemy_dance[row_base + j - 1];
		global.has_enemy_underground[row_base + j] += global.has_enemy_underground[row_base + j - 1];
	}
}