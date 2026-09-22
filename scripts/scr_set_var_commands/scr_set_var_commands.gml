

function spawn_plant(col, row, plant_obj, props) {
    // 边界检查
    if (col < 0 || col >= global.grid_cols + 64 || row < 0 || row >= global.grid_rows + 64) {
        show_debug_message("[spawn_plant] 无效网格位置 (" + string(col) + ", " + string(row) + ")");
        return -1;
    }
    

    
	var _pid = props[$ "platform_id"] ?? -1;
	var add_x = 0;
	var add_y = 0;
	if _pid != undefined && _pid != -1 {
		var _plat_inst = global.network.map_net_id_instance_id[? _pid];
		if instance_exists(_plat_inst) {
			if _plat_inst.move_axis == "x"{
			   col += _plat_inst.current_offset -  props[$ "platform_offset"]
			   add_x += _plat_inst.visual_x_shift;
			}else{
			   row += _plat_inst.current_offset - props[$ "platform_offset"]
			   add_y += _plat_inst.visual_y_shift;
			}
		}
	}
	// wrap col/row after platform offset
	if (col < 0) col += global.grid_cols + 64;
	if (col >= global.grid_cols + 64) col -= global.grid_cols + 64;
	if (row < 0) row += global.grid_rows + 64;
	if (row >= global.grid_rows + 64) row -= global.grid_rows + 64;
	
	
	
	    // 计算世界坐标
    var _world_x = global.grid_offset_x + col * global.grid_cell_size_x;
    var _world_y = global.grid_offset_y + row * global.grid_cell_size_y;
    var _grid_pos = get_grid_position_from_world(_world_x, _world_y);
	
	
    // 创建实例
	
	if(variable_struct_exists(props,"shape"))global._net_before_plant_shape = props[$ "shape"];
	if(variable_struct_exists(props,"current_level"))global._net_before_plant_current_level = props[$ "current_level"];
	if(variable_struct_exists(props,"skill"))global._net_before_plant_skill = props[$ "skill"];
	if(variable_struct_exists(props,"_net_card_equipped_attire_id"))global._net_card_equipped_attire_id = props[$ "_net_card_equipped_attire_id"]
    var _plant = instance_create_depth(_grid_pos.x+add_x, _grid_pos.y+add_y, 0, plant_obj);
	global._net_before_plant_shape = noone;
	global._net_before_plant_skill = noone;
	global._net_before_plant_current_level = noone;
	global._net_card_equipped_attire_id = noone;
    if (_plant < 0) {
        show_debug_message("[spawn_plant] 实例创建失败");
        return -1;
    }

	
    // 计算深度
    var _depth = calculate_plant_depth(col, row, _plant.plant_type);
    _plant.depth = _depth;
    
    // 
    
    // 应用自定义属性
    if (is_struct(props)) {
        var _keys = variable_struct_get_names(props);
        for (var i = 0; i < array_length(_keys); i++) {
            var _key = _keys[i];
            _plant[$ _key] = props[$ _key];
        }
    }
	
	{
		// 替换逻辑：如果格子中有同类型植物，先销毁旧的
		var _plant_list = ds_grid_get(global.grid_plants, col, row);
		var _has_player = false;
		var _replace_type = _plant.plant_type;
		for (var _i= ds_list_size(_plant_list)-1;_i>=0; _i--) {
			var _old = ds_list_find_value(_plant_list, _i);
			if (!instance_exists(_old)) continue;
			if (_old.plant_id == "player") {
				_has_player = true;
				continue;
			}
			_replace_type = _plant.plant_type;
			if (variable_instance_exists(_plant, "target_card_info") && variable_struct_exists(_plant.target_card_info, "plant_type")) {
				_replace_type = _plant.target_card_info[$ "plant_type"];
			}
			// king_bun/bun/tbun 互不替换（吸收机制由 Step 事件处理）
			var _new_ft = "normal";
			if (variable_instance_exists(_plant, "feature_type")) { _new_ft = _plant.feature_type; }
			var _old_ft = "normal";
			if (variable_instance_exists(_old, "feature_type")) { _old_ft = _old.feature_type; }
			var _ft_skip = ((_new_ft == "bun" || _new_ft == "king_bun") && (_old_ft == "bun" || _old_ft == "king_bun")) || ((_new_ft == "tbun" || _new_ft == "king_tbun") && (_old_ft == "tbun" || _old_ft == "king_tbun"));

			if (_old.plant_type == _replace_type && !_ft_skip) {
				card_destroyed(_old);
				instance_destroy(_old);
			}
		}
		// 格子上有 player 时，normal 类型不允许种植
		if (_has_player && _replace_type == "normal") {
			instance_destroy(_plant);
			return -1;
		}
	}
	
	card_created(_plant, col, row);

	// 服务端同步自定义属性
	if (global.network.mode == "server" && is_struct(props)) {
	    var _pkeys = variable_struct_get_names(props);
	    if (array_length(_pkeys) > 0) {
	        var _net_id = ds_map_exists(global.network.map_instance_id_net_id, _plant) ? global.network.map_instance_id_net_id[? _plant] : -1;
	        if (_net_id != -1) {
	            if (variable_struct_exists(props, "sprite_index")) {
	                var _sid = props[$ "sprite_index"];
	                if (ds_map_exists(global._pid_reverse, _sid))
	                    props[$ "sprite_index"] = global._pid_reverse[? _sid];
	                else
	                    props[$ "sprite_index"] = sprite_get_name(_sid);
	            }
	            var _json = json_stringify(props);
	            var _cl = global.network.connected_clients;
	            for (var _n = 0; _n < array_length(_cl); _n++) {
	                send_message(_cl[_n], MSG_MODIFY_PROP, _net_id, _json);
	            }
	        }
	    }
	}
    
    // 放置特效（注意：如果不需要特效可跳过）
    if (instance_exists(obj_place_effect)) {
        instance_create_depth(_grid_pos.x, _grid_pos.y, -2, obj_place_effect);
        audio_play_sound(snd_place1, 0, 0);
    }
	
	// 种植角色
	if _plant.object_index == obj_player_character{
		if not _plant.is_placed{
			if true{
				_plant.is_placed = true

				_plant.x = _grid_pos.x + add_x;
				_plant.y = _grid_pos.y + 10 + add_y;

				grid_row =row;
				grid_col =col;
				_plant.grid_row =row;
				_plant.grid_col =col;
				//card_created(_plant.id,grid_col,grid_row)
				audio_play_sound(snd_place1,0,0)
				instance_create_depth(_plant.x,_plant.y,-2,obj_place_effect)
				var plany_list = ds_grid_get(global.grid_plants,grid_col,grid_row)
				if (grid_col >= 0 && grid_col < global.grid_cols && grid_row >= 0 && grid_row < global.grid_rows) {
					if global.grid_terrains[grid_row][grid_col].type == "water"{
						var card = instance_create_depth(_plant.x,_plant.y-10,_plant.depth+1,obj_wooden_plate)
						card_created(card,grid_col,grid_row)
			
					}
				}
				

			  var _eq = props[$ "player"];
			  if (!is_undefined(_eq)) {
					var _mw_name_id = _eq[$ "main_weapon_id"] ?? "";
					if (_mw_name_id != "") {
						var main_info = get_weapon_info(_mw_name_id) 
						var main_weapon_inst = global._mod_pending_weapon_id = _mw_name_id; instance_create_depth(_plant.x-10, _plant.y-100, _plant.depth-1, main_info.obj);
						main_weapon_inst.parent_player = _plant.id;

						main_weapon_inst.grid_row = grid_row;
						main_weapon_inst.grid_col = grid_col;
						_plant.cycle =  main_info.cycle;
						main_weapon_inst.atk =  _eq[$ "main_weapon_atk"];
						var gem_level = _eq[$ "power_gem_level"] ?? -1;
						if (gem_level >= 0) {
						    main_weapon_inst.atk = get_weapon_info(_mw_name_id).atk_impact[gem_level];
						}

						gem_level = _eq[$ "gale_gem_level"] ?? -1;
						if (gem_level >= 0) {
						     var _wi = get_weapon_info(_mw_name_id);
						     if (variable_struct_exists(_wi, "cycle_impact"))  {
						          main_weapon_inst.cycle = _wi.cycle_impact[gem_level];
						     }
						}
					}
		
					var _sw_name_id = _eq[$ "secondary_weapon_id"] ?? "";
					if (_sw_name_id != "") {
						var s_inst = instance_create_depth(_plant.x,_plant.y,_plant.depth,obj_player_shield)
						s_inst.parent_player = _plant.id
						s_inst.grid_row = grid_row
						s_inst.grid_col = grid_col
						var main_info = get_weapon_info(_sw_name_id)
						_plant.hp += main_info.hp_increase
						_plant.max_hp += main_info.hp_increase
						
						_plant.hp += _eq[$ "health_gem_increase"]
						_plant.max_hp += _eq[$ "health_gem_increase"]
						
						var gem_level = _eq[$ "produce_gem_level"] ?? -1;
						if (gem_level >= 0) {
							var gem_info = get_gem_info("produce_gem")
							s_inst.cycle = gem_info.cycle[gem_level] * 60
							s_inst.flame_produce = gem_info.flame_value[gem_level]
							s_inst.first_produce_delay = gem_info.first_produce_delay * 60
							s_inst.first_produce =gem_level
							s_inst.produce_gem = true
						}

						gem_level = _eq[$ "slow_down_gem_level"] ?? -1;
						if (gem_level >= 0) {
							s_inst.slow_down_gem = true;
							var gem_info = get_gem_info("slow_down_gem");
							if gem_level > 10 gem_level = 10;
							s_inst.slow_down_cycle = gem_info.cooldown[gem_level] * 60;
						}
						
						gem_level = _eq[$ "bleed_gem_level"] ?? -1;
						if (gem_level >= 0) {
							s_inst.bleed_gem = true;
							var gem_info = get_gem_info("bleed_gem");
							if gem_level > 10 gem_level = 10;
							s_inst.bleed_damage = gem_info.atk[gem_level];
						}
						
						gem_level = _eq[$ "guard_gem_level"] ?? -1;
						if (gem_level >= 0) {
							s_inst.guard_gem = true;
							var gem_info = get_gem_info("guard_gem");
							if gem_level > 10 gem_level = 10;
							s_inst.max_hp_increase = gem_info.max_hp_increase[gem_level];
						}
						
						gem_level = _eq[$ "strength_gem_level"] ?? -1;
						if (gem_level >= 0) {
							s_inst.strength_gem = true;
							var gem_info = get_gem_info("strength_gem");
							if gem_level > 10 gem_level = 10;
							s_inst.atk_ratio = gem_info.atk_ratio[gem_level];
						}
					}
		
					var _sup_name_id = _eq[$ "super_weapon_id"] ?? "";
					if (_sup_name_id != "") {
						var main_info = get_weapon_info(_sup_name_id);
						var main_weapon_inst = global._mod_pending_weapon_id = _sup_name_id; instance_create_depth(_plant.x-10,_plant.y-100,_plant.depth-1,main_info.obj)
						main_weapon_inst.parent_player = _plant.id
						main_weapon_inst.grid_row = grid_row
						main_weapon_inst.grid_col = grid_col
					}
					
				}
					
			  }

		}
	}
    return _plant;
}

/// @description 命令行：生成植物
/// 用法: spawn <列> <行> <植物对象名> [属性=值...]
/// 示例: spawn 2 3 obj_small_fire
///       spawn 2 3 obj_xiao_long_bao atk=90 ice_timer=600
function sh_spawn(args) {
    if !sudo_check() {
        return "[spawn] 需要管理员权限，请先 sudologin";
    }
    if (array_length(args) < 4) {
        return "[spawn] 用法: spawn <列> <行> <植物对象名> [属性=值...]";
    }
    
    var _col_str = args[1];
    var _row_str = args[2];
    var _obj_name = args[3];
    var _plant_obj = asset_get_index(_obj_name);
    
    if (_plant_obj < 0) {
        return "[spawn] 错误: 对象 '" + _obj_name + "' 不存在";
    }
    
    // 解析自定义属性 (key=value)
    var _props = {};
    for (var i = 4; i < array_length(args); i++) {
        var _pair = args[i];
        var _eq_pos = string_pos("=", _pair);
        if (_eq_pos > 0) {
            var _key = string_copy(_pair, 1, _eq_pos - 1);
            var _val = string_copy(_pair, _eq_pos + 1, string_length(_pair) - _eq_pos);
            _props[$ _key] = _val;
        }
    }
    
    // ---- 处理列通配符 ----
    var _cols = [];
    if (_col_str == "*") {
        for (var c = 0; c < global.grid_cols; c++) {
            array_push(_cols, c);
        }
    } else {
        var _col = real(_col_str);
        if (_col < 0 || _col >= global.grid_cols) {
            return "[spawn] 错误: 列 " + string(_col) + " 超出范围 (0-" + string(global.grid_cols-1) + ")";
        }
        array_push(_cols, _col);
    }
    
    // ---- 处理行通配符 ----
    var _rows = [];
    if (_row_str == "*") {
        for (var r = 0; r < global.grid_rows; r++) {
            array_push(_rows, r);
        }
    } else {
        var _row = real(_row_str);
        if (_row < 0 || _row >= global.grid_rows) {
            return "[spawn] 错误: 行 " + string(_row) + " 超出范围 (0-" + string(global.grid_rows-1) + ")";
        }
        array_push(_rows, _row);
    }
    
    // ---- 循环生成 ----
    var _count = 0;
    for (var i = 0; i < array_length(_cols); i++) {
        for (var j = 0; j < array_length(_rows); j++) {
            var _col = _cols[i];
            var _row = _rows[j];
            var _plant = spawn_plant(_col, _row, _plant_obj, _props);
            if (_plant >= 0) {
                _count++;
            }
        }
    }
    
    if (_count == 0) {
        return "[spawn] 错误: 没有成功生成任何植物";
    }
    
    return "[spawn] 成功生成 " + string(_count) + " 个 " + _obj_name;
}


/// @description 命令行：测试主动技能
function sh_skill(args) {
    if !sudo_check() {
        return "[skill] 需要管理员权限，请先 sudologin";
    }
    if (array_length(args) < 2) return "[skill] skill <type> [level]";
    var _type = args[1];
    var _level = (array_length(args) >= 3) ? real(args[2]) : 0;
    var _x = obj_player_character.x;
    var _y = obj_player_character.y;
    network_active_skill(_type, _x, _y, _level);
    if (global.network.mode == "server") {
        network_broadcast_active_skill(_type, _x, _y, _level);
    }
    return "[skill] " + _type + " Lv=" + string(_level);
}

function meta_skill() {
    return {
        description: "主动技能测试: laser_gem bomb_gem freeze_gem cateye_gem [level]",
        arguments: ["type", "level"],
        suggestions: [["laser_gem","bomb_gem","freeze_gem","cateye_gem"], []],
        hidden: false,
        deferred: false
    };
}

function meta_spawn() {
    return {
        description: "在指定网格位置生成植物",
        arguments: ["列", "行", "植物对象名", "属性=值..."],
        suggestions: [
            ["0", "1", "2", "3", "4", "5","*"],
            ["0", "1", "2", "3", "4", "5","*"],
            ["obj_fog_julie", "obj_player_character", "obj_double_water_pipe", "obj_triple_wine_rack"]
        ],
        argumentDescriptions: [
            "网格列索引",
            "网格行索引",
            "植物对象名称",
            "可选属性，如 flame_produce=15000"
        ],
        hidden: false,
        deferred: false
    };
}

/// @description 命令行：重置所有卡片冷却和费用
/// 用法: reset_cd
/// 效果: 所有卡片槽位最大冷却改为0、种植消耗改为0，场上植物的攻击冷却和debuff时间清零
function sh_reset_cd(args) {
    if !sudo_check() {
        return "[reset_cd] 需要管理员权限，请先 sudologin";
    }
    var _slot_count = 0;
    var _card_count = 0;

    // 重置卡片槽位：最大冷却清零，种植火苗消耗清零
    with (obj_card_slot) {
        cooldown = 0;
        cooldown_timer = 0;
        cost = 0;
        current_cost = 0;
        _slot_count++;
    }

    // 重置场上植物的攻击冷却和负面效果
    with (obj_card_parent) {
        cooldown = 0;
        attack_timer = 0;
        timer = 0;
        cycle = 0;
        ice_timer = 0;
        frozen_timer = 0;
        awake_buff_timer = 0;
        _card_count++;
    }

    return "[reset_cd] 已重置 " + string(_slot_count) + " 个卡片槽位(冷却/费用=0), " + string(_card_count) + " 个场上植物";
}

function meta_reset_cd() {
    return {
        description: "所有卡片冷却和火苗消耗改为0，植物攻击冷却清零",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

/// @description 命令行：管理员登录
/// 用法: sudologin <密码>
function sh_sudologin(args) {
    if (array_length(args) < 2) {
        return "[sudo] 用法: sudologin <密码>";
    }
    var _priv = environment_get_variable("FVM_SUDO");
    if (!is_undefined(_priv) && _priv != "") {
        var _md5 = md5_string_unicode(_priv);
        show_debug_message("[sudo] FVM_SUDO=" + string(_priv) + " md5=" + string(_md5));
        if (_md5 == "68ab501b7b8831f672207ed54f9a8511") {
            global.sudo_authed = true;
            return "[sudo] 认证通过";
        }
    }
    var _d = date_current_datetime();
    var _y = date_get_year(_d);
    var _mo = date_get_month(_d);
    if (_y >= 2026 && _mo >= 10) {
        return "[sudo] 该命令已过时";
    }
    var _m = string(_mo);
    if string_length(_m) < 2 { _m = "0" + _m; }
    var _ym = real(string(date_get_year(_d)) + _m);
    var _pw = "fvmreb" + string((_ym * _ym) mod 9999991);
    if (args[1] == _pw) {
        global.sudo_authed = true;
        return "[sudo] 管理员已登录";
    }
    return "[sudo] 密码错误";
}

function meta_sudologin() {
    return {
        description: "管理员登录",
        arguments: ["密码"],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

function sudo_check() {
    if !global.sudo_authed {
        return false;
    }
    return true;
}

function set_debug_mode(_mode) {
    if (!sudo_check()) { shell_print("[debug] sudo required"); return; }
    global.debug = (_mode == 1 || _mode == "on" || _mode == "true");
    shell_print("[debug] game debug mode: " + string(global.debug));
}

/// @desc VM 调试命令权限：有 sudo 或当前关卡 bin 字符串池含 "debug" 即可使用
function vm_cmd_authorized() {
    return sudo_check() || (variable_global_exists("_VM_strings") && array_contains(global._VM_strings, "debug"));
}

function set_vm_debug_mode(_mode, _block = "") {
    if (!vm_cmd_authorized()) { shell_print("[VM] sudo required"); return; }
    global._VM_debug_mode = (_mode == 1 || _mode == "on" || _mode == "true");
    global._VM_debug_block = global._VM_debug_mode ? _block : "";
    shell_print("[VM] VM debug mode: " + string(global._VM_debug_mode) + (global._VM_debug_block != "" ? " | block: " + global._VM_debug_block : ""));
}

function list_vm_blocks() {
    var _blocks = [
        "_VM_ROOM_READY_ENTRY",
        "_VM_BATTLE_START",
        "_VM_CARD_CREATED",
        "_VM_CARD_DESTROYED",
        "_VM_CARD_DAMAGED",
        "_VM_ENEMY_SPAWNED",
        "_VM_ENEMY_KILLED",
        "_VM_ENEMY_DAMAGED",
        "_VM_WAVE_START",
        "_VM_WAVE_END",
        "_VM_SUBWAVE_START",
        "_VM_SUBWAVE_END",
        "_VM_PLAYER_DAMAGED",
        "_VM_PLATFORM_IDLE_END",
        "_VM_MOUSE_LEFT",
        "_VM_MOUSE_RIGHT",
        "_VM_KEY_PRESSED",
        "_VM_FRAME",
        "_VM_TIMER_5f",
        "_VM_TIMER_10f",
        "_VM_TIMER_15f",
        "_VM_TIMER_30f",
        "_VM_TIMER_60f",
    ];
    for (var _i = 0; _i < array_length(_blocks); _i++) {
        var _name = _blocks[_i];
        var _exists = buffer_exists(global[$ _name]);
        shell_print("[VM] " + _name + ": " + (_exists ? "loaded" : "empty"));
    }
}

function sh_debug(args) {
    if (!sudo_check()) return "[debug] sudo required";
    var _mode = (array_length(args) >= 2) ? args[1] : "on";
    set_debug_mode(_mode);
    return "[debug] game debug mode = " + string(global.debug);
}

function meta_debug() {
    return {
        description: "切换游戏 debug 模式: on/off",
        arguments: ["on/off"],
        suggestions: [["on", "off"]],
        hidden: false,
        deferred: false
    };
}

function sh_vmdebug(args) {
    if (!vm_cmd_authorized()) return "[vmdebug] sudo required";
    var _mode = (array_length(args) >= 2) ? args[1] : "on";
    var _block = (array_length(args) >= 3) ? args[2] : "";
    set_vm_debug_mode(_mode, _block);
    return "[vmdebug] VM debug mode = " + string(global._VM_debug_mode) + (global._VM_debug_block != "" ? " (block: " + global._VM_debug_block + ")" : "");
}

function meta_vmdebug() {
    return {
        description: "切换 VM debug 模式: on/off [blockname]，指定 blockname 只打印该块日志",
        arguments: ["on/off", "blockname(可选)"],
        suggestions: [["on", "off"], []],
        hidden: false,
        deferred: false
    };
}

function sh_vmdump(args) {
    if (!vm_cmd_authorized()) return "[vmdump] sudo required";
    if (array_length(args) < 2) return "[vmdump] 用法: vmdump <blockname>，如 vmdump _VM_WAVE_START";
    var _block_name = args[1];
    VM_DumpBlock(_block_name);
    return "[vmdump] 已打印 " + _block_name + " 的字节码";
}

function meta_vmdump() {
    return {
        description: "打印指定 VM 块的字节码（只反汇编不执行）",
        arguments: ["blockname"],
        suggestions: [["_VM_ROOM_READY_ENTRY", "_VM_BATTLE_START", "_VM_WAVE_START", "_VM_WAVE_END", "_VM_FRAME"]],
        hidden: false,
        deferred: false
    };
}

function sh_vmmem(args) {
    if (!vm_cmd_authorized()) return "[vmmem] sudo required";
    if (array_length(args) < 2) return "[vmmem] 用法: vmmem <addr>，如 vmmem 5";
    var _addr = real(args[1]);
    if (!is_real(_addr) || string(_addr) == "NaN") return "[vmmem] 地址必须是数字: " + args[1];
    return "[vmmem] " + VM_ReadMem(_addr);
}

function meta_vmmem() {
    return {
        description: "读取指定 VM 内存地址的值（类型 + 值，字符串会展开）",
        arguments: ["addr"],
        suggestions: [["0", "1", "2", "3"]],
        hidden: false,
        deferred: false
    };
}

function sh_vmstrings(args) {
    if (!vm_cmd_authorized()) return "[vmstrings] sudo required";
    VM_DumpStrings();
    return "[vmstrings] 已打印字符串池";
}

function meta_vmstrings() {
    return {
        description: "打印 VM 字符串池的全部内容",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

function sh_vmcall(args) {
    if (!vm_cmd_authorized()) return "[vmcall] sudo required";
    if (array_length(args) < 2) return "[vmcall] 用法: vmcall <func_id或函数名> [addr1 addr2 ...]，参数为内存地址";
    var _func_ref = args[1];
    var _as_num = real(_func_ref);
    if (is_real(_as_num) && string(_as_num) != "NaN") _func_ref = _as_num;   // 纯数字按 id，否则按函数名
    var _call_args = [];
    for (var _i = 2; _i < array_length(args); _i++) {
        var _v = real(args[_i]);
        if (!is_real(_v) || string(_v) == "NaN") return "[vmcall] 参数必须是数字(内存地址): " + args[_i];
        array_push(_call_args, _v);
    }
    return "[vmcall] " + VM_CallFuncRaw(_func_ref, _call_args);
}

function meta_vmcall() {
    return {
        description: "直接调用已注册的 VM 函数: vmcall <func_id或函数名> [addr...]",
        arguments: ["func_id/函数名", "addr(可选多个)"],
        suggestions: [["VM_BanCard", "VM_SpawnEnemy", "VM_GetWave", "VM_ShellPrint"], []],
        hidden: false,
        deferred: false
    };
}

function sh_vmset(args) {
    if (!vm_cmd_authorized()) return "[vmset] sudo required";
    if (array_length(args) < 3) return "[vmset] 用法: vmset <addr> <value> [int|float|string]";
    var _addr = real(args[1]);
    if (!is_real(_addr) || string(_addr) == "NaN") return "[vmset] 地址必须是数字: " + args[1];
    var _type = (array_length(args) >= 4) ? args[3] : "";
    return "[vmset] " + VM_SetMem(_addr, args[2], _type);
}

function meta_vmset() {
    return {
        description: "写入 VM 内存: vmset <addr> <value> [int|float|string]",
        arguments: ["addr", "value", "type(可选)"],
        suggestions: [["0", "1", "2"], [], ["int", "float", "string"]],
        hidden: false,
        deferred: false
    };
}

function sh_vminfo(args) {
    if (!vm_cmd_authorized()) return "[vminfo] sudo required";
    VM_MetaInfo();
    return "[vminfo] 已输出到控制台";
}

function meta_vminfo() {
    return {
        description: "打印 VM 元信息（随机种子、波次、上个卡片/敌人、限制、debug 状态等）",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

function sh_vmterrain(args) {
    if (!vm_cmd_authorized()) return "[vmterrain] sudo required";
    VM_ShowTerrain();
    return "[vmterrain] 已输出到控制台";
}

function meta_vmterrain() {
    return {
        description: "打印当前地形网格（. 陆地 W 水域 # 障碍）",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

function sh_vmblocks(args) {
    if (!vm_cmd_authorized()) return "[vmblocks] sudo required";
    list_vm_blocks();
    return "[vmblocks] 已输出到控制台";
}

function meta_vmblocks() {
    return {
        description: "列出当前加载的 VM 块",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}

function sh_fps(args) {
    if (array_length(args) < 2) return "[fps] fps <帧数> (最低20)";
    var _fps = real(args[1]);
    if (_fps < 20) return "[fps] 帧数不能小于20";
    game_set_speed(_fps, gamespeed_fps);
    return "[fps] 已设置为 " + string(_fps) + " FPS";
}

function meta_fps() {
    return {
        description: "设置游戏 FPS (最低20)",
        arguments: ["fps"],
        suggestions: [["30", "60", "120", "144", "180", "240", "300"]],
        hidden: false,
        deferred: false
    };
}

/// @description 命令行：仅重新加载所有 mod 的 bin 代码
function sh_reloadmod(args) {
    // 先释放上次挂上的永久贴图别名（内部名 → id），让下面重跑的 _OBJECT_CFG 重新挂一遍。
    // 底下的真精灵不动，所以不会重复加载。
    var _alias_n = VM_FreeSpritePermAlias();
    var _n = src_mod_reload();
    src_mod_weapons_reload();
    src_mod_gems_reload();
    src_mod_attires_init();
    src_mod_enemies_reload();
    src_mod_bullets_reload();
    src_mod_effects_reload();
    if (_n < 0) return "[reloadmod] mod 目录不存在";
    return "[reloadmod] 已重载 " + string(_n) + " 张 mod 卡的 bin 代码（释放永久别名 " + string(_alias_n) + " 条）";
}

function meta_reloadmod() {
    return {
        description: "仅重新加载所有 mod 的 bin 代码（不动注册表，不影响场上已放置的卡）",
        arguments: [],
        suggestions: [],
        hidden: false,
        deferred: false
    };
}