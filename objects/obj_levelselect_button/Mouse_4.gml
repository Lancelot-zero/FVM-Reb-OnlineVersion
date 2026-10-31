if on_click && obj_player_info_ui.menu_type == 0 && unlock{
	pressed = true
    global.audio.play(snd_button, 0, 0);
	texture_prefetch("cards")
	if target_level_id == "tower_cake"{
		global.gui_stack.to(room_tower_cake)
	}
	else if target_level_id == "camp_shop"{
		instance_create_depth(room_width/2,room_height/2,-5,obj_camp_shop_bg)
		obj_player_info_ui.menu_type = 4
		obj_world_map_button.world_map = 2
	}
	else{
	    global.gui_stack.to(room_ready)
	
		// 这是文件在 datafiles 目录下的相对路径
		global.level_id = target_level_id;
		var _file_path = "level_data/" + target_level_file; 
		if global.difficulty >= 2{
			_file_path = "level_data/" + target_level_file_hard; 
		}


		//使用 load_buffer 和 buffer_read 加载文件内容
		var _buffer = buffer_load(_file_path);
		if (!buffer_exists(_buffer)) {
		    show_debug_message("错误：无法加载关卡文件到缓冲区: " + _file_path);
		} else {
		    var _json_string = buffer_read(_buffer, buffer_string);
		    buffer_delete(_buffer); // 释放缓冲区内存

		    // 解析JSON字符串
		    global.level_file = json_parse(_json_string);
    
		    // 检查解析是否成功
		    if (global.level_file == -1) {
		        show_debug_message("错误：JSON 解析失败！");
		    } else {
		        show_debug_message("关卡文件加载并解析成功！");
		        show_debug_message(global.level_file);
		    }
		}
		var map_button_array = struct_get(ds_map_find_value(global.maps_map,global.map_id),"levels_data")
		global.level_data = map_button_array[level_index]
		global.level_data_index = level_index
		show_debug_message(global.level_data)
		
		// ── 本图 bin：与实验室地图同一套规则 ──────────────────────────
		//   ① 同名 .bin（地图 bin）有就读
		//   ② 同目录 <json名>.<类型>.<名称>.bin 逐个装载（lab_mod_scan）
		var _json_path = _file_path;
		global._file_cache_json_path = _json_path;
		var _bin_path = string_replace(_json_path, ".json", ".bin");
		var _bin_buf = undefined;
		if (file_exists(_bin_path)) {
			_bin_buf = buffer_load(_bin_path);
			VM_InitRoomEntry(_bin_buf);
		} else {
			VM_InitRoomEntry(undefined);
		}

		var _lab_mods = lab_mod_scan(_json_path);
		for (var _lm = 0; _lm < array_length(_lab_mods); _lm++) {
			if (file_exists(_lab_mods[_lm].path)) {
				lab_mod_load_bin(buffer_load(_lab_mods[_lm].path), _lab_mods[_lm].type, _lab_mods[_lm].name);
			}
		}
		if(global.network.mode == "server"){
		var _ld_send = variable_clone(global.level_data);
		var _rev = global._audio_reverse;
		var _mf = ["pre_music", "elite_music", "boss_music"];
		for (var _fi = 0; _fi < 3; _fi++) {
			var _f = _mf[_fi];
			var _v = _ld_send[$ _f];
			if (!is_string(_v) && ds_map_exists(_rev, _v)) {
				_ld_send[$ _f] = _rev[? _v];
			}
		}
		{
			var _spr_rev = global._pid_reverse;
			var _spr_v = _ld_send[$ "level_sprite"];
			if (!is_string(_spr_v) && ds_map_exists(_spr_rev, _spr_v)) {
				_ld_send[$ "level_sprite"] = _spr_rev[? _spr_v];
			}
		}
			var json_struct = {
			    "target_level_id":target_level_id,
			    "target_level_file": target_level_file,
			    "target_level_file_hard": target_level_file_hard,
			    "level_index": level_index,
				"map_id":global.map_id,
				"level_data": _ld_send,
				"level_file": global.level_file
			};

			// 拼包：地图 bin 在前、mod bin 按装载顺序跟后；json 带 vm_bins 清单
			//   （与实验室地图完全同一套协议；没有 mod bin 时 _send_buf 就是地图 bin 本身）
			var _send_buf = _bin_buf;
			var _vm_bins = [];
			var _lab_lst = lab_mod_vm_list();
			var _pack_n = 0;
			for (var _lp0 = 0; _lp0 < array_length(_lab_lst); _lp0++) {
				if (buffer_exists(_lab_lst[_lp0].buf)) _pack_n++;
			}
			if (_pack_n > 0) {
				var _map_size = buffer_exists(_bin_buf) ? buffer_get_size(_bin_buf) : 0;
				var _total = _map_size;
				for (var _lp2 = 0; _lp2 < array_length(_lab_lst); _lp2++) {
					if (buffer_exists(_lab_lst[_lp2].buf)) _total += buffer_get_size(_lab_lst[_lp2].buf);
				}
				_send_buf = buffer_create(_total, buffer_fixed, 1);
				var _off = 0;
				if (_map_size > 0) {
					buffer_copy(_bin_buf, 0, _map_size, _send_buf, 0);
					_off = _map_size;
					_vm_bins[array_length(_vm_bins)] = { type: "map", name: "", size: _map_size };
				}
				for (var _lp3 = 0; _lp3 < array_length(_lab_lst); _lp3++) {
					var _b3 = _lab_lst[_lp3].buf;
					if (!buffer_exists(_b3)) continue;
					var _sz3 = buffer_get_size(_b3);
					buffer_seek(_b3, buffer_seek_start, 0);
					buffer_copy(_b3, 0, _sz3, _send_buf, _off);
					_off += _sz3;
					_vm_bins[array_length(_vm_bins)] = { type: _lab_lst[_lp3].type, name: _lab_lst[_lp3].name, size: _sz3 };
				}
				json_struct[$ "vm_bins"] = _vm_bins;
				if (buffer_exists(_bin_buf)) buffer_delete(_bin_buf);
			}
			global._sync_vm_bins = (array_length(_vm_bins) > 0) ? _vm_bins : undefined;
			global._sync_vm_bin_buf = _send_buf;

			var json_str = json_stringify(json_struct);
			var _list = global.network.connected_clients;
			var _size = array_length(_list);
			for (var i = 0; i < _size; i++) {
				var _socket = _list[i];
				send_message(_socket, MSG_ENTER_ROOM_READY, json_str, _send_buf);
			}
		}
		
	}
}
if !unlock && obj_player_info_ui.menu_type == 0{
	if player_level_require > 80{
		show_notice("暂未开放，敬请期待！",60)
	}
	else{
		if global.save_data.player.level < player_level_require{
			show_notice("达到"+string(player_level_require)+"级以解锁此关",60)
		}
		else{
			show_notice("完成前置关卡以解锁此关",60)
		}
	}
}





