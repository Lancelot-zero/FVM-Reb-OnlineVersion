/// @function plant_created(plant_inst, col, row)
/// @description 当植物创建时调用，更新网格数据
/// @param {instance} plant_inst 植物实例
/// @param {real} col 网格列
/// @param {real} row 网格行
function card_created(plant_inst, col, row) {
	
	// 客户端拦截种植，放送种植请求
	if(global.network.mode=="client"&&!global.network.client_able){
		var skill = variable_instance_get(plant_inst, "skill") ?? 0;
		var shape = variable_instance_get(plant_inst, "shape") ?? 0;
		var level = variable_instance_get(plant_inst, "current_level") ?? 0;
		var _meta = package_character(plant_inst);
		var _sid = plant_inst.sprite_index;
		var _sprite_name = ds_map_exists(global._pid_reverse, _sid) ? global._pid_reverse[? _sid] : sprite_get_name(_sid);

		if (plant_inst.object_index == obj_magic_chicken) {
			var _target = variable_instance_get(plant_inst, "target_card") ?? "";
			if _target == ""{
				_meta = {target_card:""}
			}else{
				var target_card_info ={
					skill:get_card_info(_target)[$ "skill"],
					shape:get_card_info(_target)[$ "shape"],
					level:get_card_info(_target)[$ "level"],
					plant_type: deck_get_card_data(_target, get_card_info(_target)[$ "shape"])[? "plant_type"]
				}
				bak = global.network.client_able;
				global.network.client_able = true
				var card_save_data = get_card_info_simple(_target)
				var card_slot_data = deck_get_card_data(_target,card_save_data.shape)
				var new_card = instance_create_depth(0, 0, 0, card_slot_data[? "obj"])
				target_card_info[$ "sprite_index"] = ds_map_exists(global._pid_reverse, new_card.sprite_index) ? global._pid_reverse[? new_card.sprite_index] : sprite_get_name(new_card.sprite_index);
				target_card_info[$ "_net_card_equipped_attire_id"] = card_equipped_attire_id(new_card.plant_id)
				target_card_info[$ "cycle"] = new_card.cycle
				target_card_info[$ "hp"] = new_card.hp
				if(object_get_name(new_card.object_index)=="obj_goblet_lamp"&&is_cookbook_equipped("grape_wine")){
					target_card_info[$ "grow_time"] =  60 * 60
				}
				if(object_get_name(new_card.object_index)=="obj_cherry_pudding"){
					if(is_cookbook_equipped("pineapple_pudding")){
						target_card_info[$ "_net_cookbook_equipped"] = "pineapple_pudding"
					}else{
						target_card_info[$ "_net_cookbook_equipped"] = ""
					}
				}
		
				if (variable_instance_exists(new_card, "sprite_list")) {
				    var _sl = new_card.sprite_list;
				    var _sl_names = [];
				    for (var _si = 0; _si < array_length(_sl); _si++) {
				        _sid = _sl[_si];
						if(!is_string(_sid)){
							if(ds_map_exists(global._pid_reverse, _sid))
								_sl_names[_si] =  global._pid_reverse[? _sid];
							else
								_sl_names[_si] = sprite_get_name(_sid);
						}
				    }
					target_card_info[$ "sprite_list"] = _sl_names;
				}
				instance_destroy(new_card);
				global.network.client_able =bak;
				_meta = {target_card:_target,target_card_info:target_card_info};
			}
		}else{
		
			// 同步 sprite_list（转为字符串名）
			if (variable_instance_exists(plant_inst, "sprite_list")) {
			    var _sl = plant_inst.sprite_list;
			    var _sl_names = [];
			    for (var _si = 0; _si < array_length(_sl); _si++) {
			        _sid = _sl[_si];
					if(!is_string(_sid)){
						if(ds_map_exists(global._pid_reverse, _sid))
							_sl_names[_si] =  global._pid_reverse[? _sid];
						else
							_sl_names[_si] = sprite_get_name(_sid);
					}
			    }
			    _meta[$ "sprite_list"] = _sl_names;
			}

		}

		var _plat = get_platform_at_grid(col, row)
		if (_plat != noone) {
		      _plat_id = ds_map_exists(global.network.map_instance_id_net_id, _plat.id) ? global.network.map_instance_id_net_id[? _plat.id] : -1;
			  if(_plat_id!=-1)
			  {
				  _meta[$ "platform_id"]  = _plat_id;
				  _meta[$ "platform_offset"]  = _plat.current_offset;
			  }
		}

		_meta[$ "_net_card_equipped_attire_id"]  = card_equipped_attire_id(plant_inst.plant_id);
		_meta[$ "cycle"] = plant_inst.cycle
		_meta[$ "hp"] = plant_inst.hp
		if(object_get_name(plant_inst.object_index)=="obj_goblet_lamp"&&is_cookbook_equipped("grape_wine")){
			_meta[$ "grow_time"] =  60 * 60
		}
		if(object_get_name(plant_inst.object_index)=="obj_cherry_pudding"){
			if(is_cookbook_equipped("pineapple_pudding")){
				_meta[$ "_net_cookbook_equipped"] = "pineapple_pudding"
			}else{
				_meta[$ "_net_cookbook_equipped"] = ""
			}
		}
		
		_meta = json_stringify(_meta);
		send_message(global.network.server_socket, MSG_UNIT_REQUEST, level, col, row, skill, shape, object_get_name(plant_inst.object_index), _meta, _sprite_name);
		return;
	}



    // 获取该网格的植物列表
    // VM 等级上限：超过则压到上限
    if (global._VM_card_level_cap >= 0) {
        var _lv = variable_instance_get(plant_inst, "current_level") ?? 0;
        if (_lv > global._VM_card_level_cap) {
            variable_instance_set(plant_inst, "current_level", global._VM_card_level_cap);
        }
    }

    var plant_list = ds_grid_get(global.grid_plants, col, row);

    // 添加新植物到列表
    ds_list_add(plant_list, plant_inst);

    // 设置植物的网格位置
    plant_inst.grid_col = col;
    plant_inst.grid_row = row;

    // 更新植物的深度偏移（根据层级）
    plant_inst.depth_offset = ds_list_size(plant_list) * 5;

    // 更新所有植物的深度偏移
	sort_plants_in_grid(col, row)

	// 服务端广播种植消息
	if(global.network.mode=="server"){
		var level = variable_instance_get(plant_inst, "current_level") ?? 0;
		var skill = variable_instance_get(plant_inst, "skill") ?? 0;
		var shape = variable_instance_get(plant_inst, "shape") ?? 0;
		var _equipped_attire = variable_instance_get(plant_inst, "_net_card_equipped_attire_id")
		if _equipped_attire==noone{
			_equipped_attire = card_equipped_attire_id(plant_inst.plant_id);
		}
		var _sid = plant_inst.sprite_index;
		var _sprite_name = ds_map_exists(global._pid_reverse, _sid) ? global._pid_reverse[? _sid] : sprite_get_name(_sid);
		
		var _target = variable_instance_get(plant_inst, "target_card") ?? "";
		var object_name = object_get_name(plant_inst.object_index);

		var _meta = {};
		if (variable_instance_exists(plant_inst, "player") && is_struct(plant_inst.player)) {
			_meta = { player: plant_inst.player };
		} else if (plant_inst.object_index == obj_magic_chicken ) {
			_meta = {target_card:""}
		} else {
			_meta = package_character(plant_inst);
		}
		_meta[$ "_net_card_equipped_attire_id"]  = _equipped_attire;
		_meta[$ "cycle"] = plant_inst.cycle
		_meta[$ "hp"] = plant_inst.hp
		if(object_get_name(plant_inst.object_index)=="obj_goblet_lamp"&&is_cookbook_equipped("grape_wine")){
			_meta[$ "grow_time"] =  60 * 60
		}
		if(object_get_name(plant_inst.object_index)=="obj_cherry_pudding"){
			if(is_cookbook_equipped("pineapple_pudding")){
				_meta[$ "_net_cookbook_equipped"] = "pineapple_pudding"
			}else{
				_meta[$ "_net_cookbook_equipped"] = ""
			}
		}
		
		var _plat = get_platform_at_grid(col, row)
		if (_plat != noone) {
		      _plat_id = ds_map_exists(global.network.map_instance_id_net_id, _plat.id) ? global.network.map_instance_id_net_id[? _plat.id] : -1;
			  if(_plat_id!=-1)
			  {
				  _meta[$ "platform_id"]  = _plat_id;
				  _meta[$ "platform_offset"]  = _plat.current_offset;
			  }
		}
		
		// 同步 sprite_list（转为字符串名）
		if (variable_instance_exists(plant_inst, "sprite_list")) {
		    var _sl = plant_inst.sprite_list;
		    var _sl_names = [];
		    for (var _si = 0; _si < array_length(_sl); _si++) {
			    _sid = _sl[_si];
				if(!is_string(_sid)){
					if(ds_map_exists(global._pid_reverse, _sid))
						_sl_names[_si] =  global._pid_reverse[? _sid];
					else
						_sl_names[_si] = sprite_get_name(_sid);
				}
		    }
		    _meta[$ "sprite_list"] = _sl_names;
		}
		
		_meta = json_stringify(_meta);
		var _list = global.network.connected_clients;
		var _size = array_length(_list);
		for (var i = 0; i < _size; i++) {
			var _socket = _list[i];
			send_message(_socket, MSG_SPAWN_UNIT, global.network.net_instance_count, level, col, row, skill, shape, _sprite_name, object_name, _meta);
		}
		add_net_id(plant_inst.id);
	}

	// VM Hook: 卡片创建
	global._VM_last_created_card = plant_inst.id;
	if (buffer_exists(global._VM_CARD_CREATED)) VM_QueueHook(global._VM_CARD_CREATED, "card", plant_inst.id);

	// 维护最近种植的5个卡片种类（1 为最新）
	var _card_type = variable_instance_exists(plant_inst, "plant_id") ? plant_inst.plant_id : "";
	if (_card_type != "" && _card_type != "player") {
		var _dup_index = 0;
		if (global._recent_card_type_1 == _card_type) _dup_index = 1;
		else if (global._recent_card_type_2 == _card_type) _dup_index = 2;
		else if (global._recent_card_type_3 == _card_type) _dup_index = 3;
		else if (global._recent_card_type_4 == _card_type) _dup_index = 4;
		else if (global._recent_card_type_5 == _card_type) _dup_index = 5;

		if (_dup_index == 0) {
			global._recent_card_type_5 = global._recent_card_type_4;
			global._recent_card_type_4 = global._recent_card_type_3;
			global._recent_card_type_3 = global._recent_card_type_2;
			global._recent_card_type_2 = global._recent_card_type_1;
			global._recent_card_type_1 = _card_type;
		} else {
			if (_dup_index >= 5) global._recent_card_type_5 = global._recent_card_type_4;
			if (_dup_index >= 4) global._recent_card_type_4 = global._recent_card_type_3;
			if (_dup_index >= 3) global._recent_card_type_3 = global._recent_card_type_2;
			if (_dup_index >= 2) global._recent_card_type_2 = global._recent_card_type_1;
			global._recent_card_type_1 = _card_type;
		}
	}

}



function package_character(plant_inst){
	if (plant_inst.object_index == obj_player_character) {
		var _eq = {};
		
		var _mw = global.save_data.equipped_items.main_weapon;
		if (_mw.id != "") {
			_eq.main_weapon_id = global.save_data.equipped_items.main_weapon.id;
			var atk =get_weapon_info(_mw.id).atk
			if get_gem_index("attack_gem")!= -1{
				atk = get_weapon_info(_mw.id).atk_impact[get_gem_level("attack_gem")]
			}
			_eq.main_weapon_atk = atk;
		}
		
		var _sw = global.save_data.equipped_items.secondary_weapon;
		if (_sw.id != "") {
			_eq.secondary_weapon_id = global.save_data.equipped_items.secondary_weapon.id;
		}
		var _sup = global.save_data.equipped_items.super_weapon;
		if (_sup.id != "") {
			_eq.super_weapon_id =  global.save_data.equipped_items.super_weapon.id;
		}
		
		
		if get_gem_index("health_gem")!= -1{
			_eq.health_gem_increase = get_gem_info("health_gem").hp_increase * (get_gem_level("health_gem")+1);
		}else{
			_eq.health_gem_increase = 0;
		}

	
		if get_gem_index("power_gem")!= -1{ _eq.power_gem_level = get_gem_level("power_gem"); }
		if get_gem_index("gale_gem")!= -1{ _eq.gale_gem_level = get_gem_level("gale_gem"); }
		if get_gem_index("produce_gem")!= -1{ _eq.produce_gem_level = get_gem_level("produce_gem"); } 
		if get_gem_index("slow_down_gem")!= -1{ _eq.slow_down_gem_level = get_gem_level("slow_down_gem"); }
		if get_gem_index("bleed_gem")!= -1{ _eq.bleed_gem_level = get_gem_level("bleed_gem"); }
		if get_gem_index("guard_gem")!= -1{ _eq.guard_gem_level = get_gem_level("guard_gem"); }
		if get_gem_index("strength_gem")!= -1{ _eq.strength_gem_level = get_gem_level("strength_gem"); }
		if get_gem_index("transform_gem")!= -1{ _eq.transform_gem_level = get_gem_level("transform_gem"); }
			
		
		return { player: _eq };
	}
	return {}
}