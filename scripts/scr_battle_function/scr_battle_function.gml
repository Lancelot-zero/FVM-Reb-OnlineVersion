

global.__stop_msg_loop = false;

// ============================
// 网络消息ID定义
// ============================

#macro MSG_UNIT_REQUEST         1   // C→S: 请求放置我方单位（含类型、等级、网格坐标）
#macro MSG_REMOVE_UNIT_REQUEST  2   // C→S: 请求移除/铲除指定我方单位（携带网络ID）
#macro MSG_CHAT                 3   // C→S→C: 发送聊天消息（携带文本内容）
#macro MSG_ACTIVE_SKILL_REQUEST 4   // C→S: 请求释放主动技能


#macro MSG_SPAWN_UNIT           5   // S→C: 广播创建我方单位（网络ID、类型、等级、坐标、血量等）
#macro MSG_SPAWN_ENEMY          6   // S→C: 广播创建敌人（网络ID、类型、行、列偏移、血量等）
#macro MSG_ENEMY_HP             7   // S→C: 广播敌人血量变化（网络ID、当前血量、最大血量）
#macro MSG_ENEMY_CALIBRATE      8   // S→C: 敌人位置校准（网络ID、世界X、世界Y）
#macro MSG_UNIT_HP              9   // S→C: 广播我方单位血量变化（网络ID、当前血量、最大血量）
#macro MSG_GAME_OVER            10  // S→C: 广播游戏结束（结果：0失败/1胜利）
#macro MSG_SYNC_INITIAL_STATE   11  // S→C: 全量状态同步（新客户端加入时发送所有单位、敌人及全局状态）
#macro MSG_START_BATTLE			12	// S→C: 开始战斗
#macro MSG_ENTER_ROOM_READY		13	// S→C: 进入房间
#macro MSG_SPAWN_BOSS           14  // S→C: 广播产生BOSS
#macro MSG_REMOVE_UNIT          15  // S→C: 广播移除/铲除指定我方单位
#macro MSG_ENEMY_STEAL          16  // S→C: 广播敌方偷取植物
#macro MSG_PROGRESS_SYNC        17  // S→C: 广播进度条同步
#macro MSG_ACTIVE_SKILL         18  // S→C: 广播释放主动技能
#macro MSG_SERVER_ACTION        19  // S→C: 0正常/1加速/2暂停/3继续/4回房/5重开
#macro MSG_EVENT_ACTIONS        20  // S→C: event_manager 操作日志(JSON)
#macro MSG_DESTROY              21  // S→C: 广播销毁实例
#macro MSG_CAT_ATTACK           22  // S→C: 猫撞击敌人(row, sprite_name, image_index)
#macro MSG_PUB_INFO             23  // M→A: 中继器到所有人发消息
#macro MSG_MUSIC_SYNC           24  // S→C: 广播音乐切换
#macro MSG_MODIFY_PROP          25  // S→C: 修改实例属性(net_id, json)
#macro MSG_REQUEST_FILE         26  // C→S: 请求文件(文件名,用途)
#macro MSG_TRANSFER_FILE        27  // S→C: 传输文件(文件名, 用途, 大小, 字节流)
#macro MSG_BUN_ABSORB           28  // S→C: 国王包子吸收 (king_net_id, bun_amount, bullet_name, damage)
#macro MSG_PLATFORM_TICK        29  // S→C: 平台帧同步拍子，客户端收到后驱动所有平台step一次
#macro MSG_VM_NOTIFY            30  // S→C: 虚拟机通知(JSON)



global._net_before_plant_shape = noone;
global._net_before_plant_skill = noone;
global._net_before_plant_current_level = noone;
global._net_card_equipped_attire_id = noone;

/// @function add_net_id(ins_id, net_id)
/// @description 为实例产生一个 net_id
function add_net_id(ins_id){
	ds_map_add(global.network.map_net_id_instance_id,global.network.net_instance_count,ins_id);
	ds_map_add(global.network.map_instance_id_net_id,ins_id,global.network.net_instance_count);
	global.network.net_instance_count++;
}

/// @function set_net_id(ins_id, net_id)
/// @description 用服务端分配的 net_id 注册实例，保证两端一致
function set_net_id(ins_id, net_id){
	ds_map_add(global.network.map_net_id_instance_id, net_id, ins_id);
	ds_map_add(global.network.map_instance_id_net_id, ins_id, net_id);
	if (net_id >= global.network.net_instance_count) {
		global.network.net_instance_count = net_id + 1;
	}
}


/// @function shell_log_write(_msg)
/// @desc sudo 认证通过后，shell 里的每条输出都追加到本地 mod/shell.log（时间 + 内容），
///       方便退出游戏后直接看文件调试。没认证 / 没 mod 目录时什么都不做。
///       每 200 条裁剪一次，只留最近 2000 行，防止无限增长。
/// @param {string} _msg 要记录的一行
function shell_log_write(_msg) {
    if (!variable_global_exists("sudo_authed") || !global.sudo_authed) return;

    var _dir = working_directory + "mod/";
    if (!directory_exists(_dir)) return;
    var _path = _dir + "shell.log";

    var _w = file_text_open_append(_path);
    if (_w == -1) return;
    file_text_write_string(_w, date_datetime_string(date_current_datetime()) + "  " + string(_msg));
    file_text_writeln(_w);
    file_text_close(_w);

    if (!variable_global_exists("_shell_log_writes")) global._shell_log_writes = 0;
    global._shell_log_writes += 1;
    if ((global._shell_log_writes mod 200) != 0) return;

    // 定期裁剪：读出来只留最后 2000 行再整体写回
    var _lines = [];
    var _r = file_text_open_read(_path);
    if (_r == -1) return;
    while (!file_text_eof(_r)) {
        array_push(_lines, file_text_read_string(_r));
        file_text_readln(_r);
    }
    file_text_close(_r);
    var _n = array_length(_lines);
    if (_n <= 2000) return;
    var _wt = file_text_open_write(_path);
    if (_wt == -1) return;
    for (var _i = _n - 2000; _i < _n; _i++) {
        file_text_write_string(_wt, _lines[_i]);
        file_text_writeln(_wt);
    }
    file_text_close(_wt);
}


/// @function shell_print(msg)
/// @param {string} msg  打印消息到shell中
function shell_print(msg) {
	shell_log_write(msg);   // sudo 认证后同时落盘到 mod/shell.log
	inst_id = inst_224771E1; 
    // 检查实例是否存在
    if (!instance_exists(inst_id)) {
        show_debug_message("[shell_push_output] 实例 " + string(inst_id) + " 不存在");
        return;
    }
    
    // 通过 with 操作该实例
    with (inst_id) {
        // 检查 output 数组是否存在
        if (!variable_struct_exists(self, "output")) {
            show_debug_message("[shell_push_output] 实例 " + string(inst_id) + " 没有 output 数组");
            return;
        }
        // 压入消息
        array_push(output, msg);
    }
}


/// @function parse_network_message(buffer, socket)
/// @param {buffer} buf  完整的消息体缓冲区（已剥去长度头）
/// @param {real} _sock  消息来源的 socket
/// @description 读取消息ID并分发到对应的处理函数
function parse_network_message(buf, _sock) {
    // 确保从缓冲区开头读取
    buffer_seek(buf, buffer_seek_start, 0);
    
    // 1. 读取消息ID（4字节整数）
    var msg_id = buffer_read(buf, buffer_s32);
    
    // 2. 根据消息ID分发
    switch (msg_id) {
        // ========== 客户端请求 ==========
        case MSG_UNIT_REQUEST:
        {
            // 字段：level(u8), col(u8), row(u8), skill(u8), shape(u8), obj_name(string), meta_info(string)
            var level = buffer_read(buf, buffer_u8);
            var col = buffer_read(buf, buffer_u8);
            var row = buffer_read(buf, buffer_u8);
            var skill = buffer_read(buf, buffer_u8);
            var shape = buffer_read(buf, buffer_u8);
            var object_name = buffer_read(buf, buffer_string);
            var meta = buffer_read(buf, buffer_string);
			var _sprite_name = buffer_read(buf, buffer_string);


			var _props = { "current_level": level, "skill": skill, "shape": shape };
			if (meta != "") {
				var _st = json_parse(meta);
				var _keys = variable_struct_get_names(_st);
				for (var i = 0; i < array_length(_keys); i++) {
					var _key = _keys[i];
					_props[$ _key] = _st[$ _key];
				}
			}
			
			// sprite_list 字符串名还原为本地ID
			if (variable_struct_exists(_props, "sprite_list")) {
			    var _sl = _props[$ "sprite_list"];
			    for (var _si = 0; _si < array_length(_sl); _si++) {
			        if (is_string(_sl[_si])) {
			           _sl[_si] = get_load_sprite(_sl[_si]);		            
			        }
			    }
				_props[$ "sprite_list"] = _sl;
			}
			if (_sprite_name != "") {
				_props[$ "sprite_index"] =get_load_sprite( _sprite_name);
			}
			
			// ── mod 卡校验：客户端要种的这张 mod 卡，服务器没装 → 取消这次种植 ──
			//    （plant_id 只有 mod 卡会带；对象名是共用的 obj_card_mod，所以必须靠 plant_id 判）
			var _req_pid = variable_struct_exists(_props, "plant_id") ? _props[$ "plant_id"] : "";
			if (is_string(_req_pid) && _req_pid != ""
				&& (!variable_global_exists("mod_card_vms") || !ds_map_exists(global.mod_card_vms, _req_pid))) {
				var _warn = "服务器没有这张 mod 卡，已取消种植：" + _req_pid;
				show_notice(_warn, 150);                                   // 房主自己也提示
				var _wn = json_stringify({hook: "notice", text: _warn, dur: 150});
				var _wcl = global.network.connected_clients;               // 广播给所有人
				for (var _wc = 0; _wc < array_length(_wcl); _wc++) {
					send_message(_wcl[_wc], MSG_VM_NOTIFY, _wn);
				}
				show_debug_message("[解析] MSG_UNIT_REQUEST: 拒绝未注册的 mod 卡 " + _req_pid);
				break;
			}
			
			var obj_index = asset_get_index(object_name);
			if (obj_index==-1)obj_index = obj_xiao_long_bao;

			// ====== 国王吸收预检 ======
			var _card_id = string_delete(object_name, 1, 4);  // "obj_xiao_long_bao" → "xiao_long_bao"
			var _plant_list = ds_grid_get(global.grid_plants, col, row);
			var _absorbed = false;

			for (var _i = 0; _i < ds_list_size(_plant_list); _i++) {
				var _king = ds_list_find_value(_plant_list, _i);
				if (!instance_exists(_king)) continue;
				if (_king.feature_type != "king_bun" && _king.feature_type != "king_tbun") continue;

				// 国王已满
				if (_king.bun_count >= _king.max_bun) continue;

				// 匹配国王的 bun_card_info
				var _info = _king.bun_card_info;
				var _match_j = -1;
				for (var _j = 0; _j < array_length(_info); _j++) {
					if (_card_id == _info[_j].card_id) { _match_j = _j; break; }
				}
				if (_match_j == -1) continue;

				// 查表获取 atk（不创建临时实例）
				var _upgrade_data = get_plant_data_with_skill(_card_id, shape, level, skill);
				var _atk = (_upgrade_data != undefined) ? _upgrade_data[? "atk"] : 10;

				// 执行吸收
				var _added = 0;
				for (var _k = 0; _k < _info[_match_j].bun_amount; _k++) {
					if (_king.bun_count < _king.max_bun) {
						_king.bun_count++;
						array_push(_king.bullet_list,
							{"bullet_type": _info[_match_j].bullet_type, "damage": _atk});
						_added++;
					}
				}
				// 黑心三向国王额外多吸收1发
				if (_king.feature_type == "king_tbun" && _king.shape >= 2) {
					if (_king.bun_count < _king.max_bun) {
						_king.bun_count++;
						_added++;
						array_push(_king.bullet_list,
							{"bullet_type": _info[_match_j].bullet_type, "damage": _atk});
					}
				}
				_king.state = CARD_STATE.GROW;
				_king.upgrade_timer = 0;
				_absorbed = true;

				// 广播 MSG_BUN_ABSORB 给所有客户端
				if (ds_map_exists(global.network.map_instance_id_net_id, _king.id)) {
					var _nid = global.network.map_instance_id_net_id[? _king.id];
					var _bullet_name = object_get_name(_info[_match_j].bullet_type);
					var _cl = global.network.connected_clients;
					for (var _c = 0; _c < array_length(_cl); _c++) {
						send_message(_cl[_c], MSG_BUN_ABSORB, _nid, _added, _bullet_name, _atk);
					}
				}
				break;
			}

			if (!_absorbed) {
				var _plant = spawn_plant(col, row, obj_index, _props);
				network_apply_plant_level(_plant);
				add_net_id(_plant.id);
			}
            show_debug_message("[解析] MSG_UNIT_REQUEST: type=" + object_name + " Lv=" + string(level) + " shape=" + string(shape) + " skill=" + string(skill) + " sprite=" + _sprite_name + " meta="+meta);
            break;
        }
        
        case MSG_REMOVE_UNIT_REQUEST:
        {
            // 字段：net_id(s32), col(u8), row(u8), flame_amount(s32)
            var net_id = buffer_read(buf, buffer_s32);
            var col = buffer_read(buf, buffer_u8);
            var row = buffer_read(buf, buffer_u8);
            var flame_amount = buffer_read(buf, buffer_s32);

            network_shovel_remove(col, row, net_id, flame_amount);
            network_broadcast_shovel_remove(col, row, net_id, flame_amount);

            show_debug_message("[解析] 收到 MSG_REMOVE_UNIT_REQUEST: ID=" + string(net_id) + " col=" + string(col) + " row=" + string(row) + " flame=" + string(flame_amount));
            break;
        }

        case MSG_REMOVE_UNIT:
        {
            // 字段：net_id(s32), col(u8), row(u8), flame_amount(s32)
            var net_id = buffer_read(buf, buffer_s32);
            var col = buffer_read(buf, buffer_u8);
            var row = buffer_read(buf, buffer_u8);
            var flame_amount = buffer_read(buf, buffer_s32);

            // 客户端执行铲除（放行 instance_destroy）
            global.network.client_able = true;
            network_shovel_remove(col, row, net_id, flame_amount);
            global.network.client_able = false;

            show_debug_message("[解析] 收到 MSG_REMOVE_UNIT: ID=" + string(net_id) + " col=" + string(col) + " row=" + string(row));
            break;
        }

        
        
        case MSG_CHAT:
        {
			var _str = "";
			for (var i = 0; i < buffer_get_size(buf); i++) _str += string(buffer_peek(buf, i, buffer_u8)) + " ";
            var chat_text = buffer_read(buf, buffer_string);
            show_debug_message("[解析] 收到 MSG_CHAT: " + chat_text);
			shell_print(chat_text);
			if global.network.mode == "server"{
				sh_say(chat_text);
			}
            break;
        }
        
        // ========== 服务器广播 ==========
        
        
        case MSG_SPAWN_UNIT:
        {
            // 字段：net_id(s32), level(u8), col(u8), row(u8), skill(u8), shape(u8), object_name(string), meta_info(string)
            var net_id = buffer_read(buf, buffer_s32);
            var level = buffer_read(buf, buffer_u8);
            var col = buffer_read(buf, buffer_u8);
            var row = buffer_read(buf, buffer_u8);
            var skill = buffer_read(buf, buffer_u8);
            var shape = buffer_read(buf, buffer_u8);
            var _sprite_name = buffer_read(buf, buffer_string);
            var object_name = buffer_read(buf, buffer_string);
            var meta = buffer_read(buf, buffer_string);

			var _props = { "current_level": level, "skill": skill, "shape": shape };
			if (meta != "") {
			    var _st = json_parse(meta);
			    var _keys = variable_struct_get_names(_st);
			    for (var i = 0; i < array_length(_keys); i++) {
			        var _key = _keys[i];
			        _props[$ _key] = _st[$ _key];
			    }
			}
			
			// sprite_list 字符串名还原为本地ID
			if (variable_struct_exists(_props, "sprite_list")) {
			    var _sl = _props[$ "sprite_list"];
			    for (var _si = 0; _si < array_length(_sl); _si++) {
			        if (is_string(_sl[_si])) {
			           _sl[_si] = get_load_sprite(_sl[_si]);		            
			        }
			    }
				_props[$ "sprite_list"] = _sl;
			}
			if (_sprite_name != "") {
				_props[$ "sprite_index"] = get_load_sprite(_sprite_name);
			}

            global.network.client_able = true;
			var obj_name =  asset_get_index(object_name);
			// 非法卡片（本机没这个对象）→ 用土司面包兜底，别拿 -1 去建实例
			if (obj_name == -1) {
				show_debug_message("[解析] MSG_SPAWN_UNIT: 未注册对象 " + object_name + " → 用 obj_toast_bread 兜底");
				obj_name = obj_toast_bread;
			}
            var _plant = spawn_plant(col, row, obj_name, _props);
            global.network.client_able = false;
			
			// 种植音效：服务器确认建出来了才响（host 那边在自己放置时播，不会重复）。
			// 关卡自摆的卡 meta 里 _place_sfx=0，不响，免得开局响一串。
			if (variable_struct_exists(_props, "_place_sfx") && _props[$ "_place_sfx"] == 1) {
				if (global.grid_terrains[row][col].type == "normal") {
					audio_play_sound(snd_place1, 0, 0);
				} else if (global.grid_terrains[row][col].type == "water") {
					audio_play_sound(snd_enter_water, 0, 0);
				}
			}
			
			network_apply_plant_level(_plant);
            set_net_id(_plant.id, net_id);

            show_debug_message("[解析] MSG_SPAWN_UNIT: ID=" + string(net_id) + " type=" + object_name + " Lv=" + string(level) + " shape=" + string(shape) + " skill=" + string(skill) + " sprite=" + _sprite_name + " meta="+meta);
            break;

        }
        
        case MSG_SPAWN_ENEMY:
        {
            // 字段：网络ID(s32), x(f32), y(f32), object_name(string)
            var net_id = buffer_read(buf, buffer_s32);
            var px = buffer_read(buf, buffer_f32);
            var py = buffer_read(buf, buffer_f32);
            var object_name = buffer_read(buf, buffer_string);
			
			global.network.client_able = true;
			var obj_index = asset_get_index(object_name);
			if (obj_index==-1){obj_index=obj_iron_pan_mouse;}
            var new_enemy = instance_create_depth(px, py, 0, asset_get_index(object_name));
			global.network.client_able = false;
            set_net_id(new_enemy.id, net_id);

            show_debug_message("[解析] MSG_SPAWN_ENEMY: ID=" + string(net_id) + " type=" + object_name);
            break;
        }

        case MSG_SPAWN_BOSS:
        {
            // 字段：net_id(s32), x(f32), y(f32), object_name(string), hp(s32), maxhp(s32), row(u8), random_seed(u32)
            var net_id = buffer_read(buf, buffer_s32);
            var px = buffer_read(buf, buffer_f32);
            var py = buffer_read(buf, buffer_f32);
            var object_name = buffer_read(buf, buffer_string);
            var hp_val = buffer_read(buf, buffer_s32);
            var maxhp_val = buffer_read(buf, buffer_s32);
            var boss_row = buffer_read(buf, buffer_u8);
            var random_seed = buffer_read(buf, buffer_s32);
	
            global.network.client_able = true;
			var boss_index = asset_get_index(object_name);
			if(boss_index==-1)boss_index = obj_abyss_pharaoh;
            var new_boss = instance_create_depth(px, py, -200, boss_index);
            global.network.client_able = false;
            set_net_id(new_boss.id, net_id);
            new_boss.hp = hp_val;
            new_boss.maxhp = maxhp_val;
            new_boss.grid_row = boss_row;
            new_boss.random_seed = random_seed;
            obj_battle.boss_count++;
            show_debug_message("[解析] MSG_SPAWN_BOSS: ID=" + string(net_id) + " type=" + object_name + " HP=" + string(hp_val) + "/" + string(maxhp_val));
            break;
        }


        
        case MSG_ENEMY_HP:
        {
            // 字段：网络ID(s32), 当前血量(s32), 最大血量(s32)
            var net_id = buffer_read(buf, buffer_s32);
            var hp_val = buffer_read(buf, buffer_s32);
            var max_hp = buffer_read(buf, buffer_s32);
            var _inst = global.network.map_net_id_instance_id[? net_id];
            if (instance_exists(_inst)) {
                // Boss：HP 完全由服务端权威，避免客户端本地攻击提前把 boss 打进 DEATH
                var _is_boss = (variable_instance_exists(_inst, "is_boss") && _inst.is_boss);
                if (_is_boss) {
                    _inst.hp = hp_val;
                    _inst.maxhp = max_hp;
                    // 服务端说没死但客户端 boss 已进入死亡状态：复活
                    if (hp_val > 0 && _inst.state == BOSS_STATE.DEATH) {
                        _inst.state = BOSS_STATE.IDLE;
                        _inst.timer = 0;
                        _inst.image_alpha = 1;
                    }
                }
                // 普通敌人：服务端权威，血量>0 即存活，客户端已判定死亡也复活
                else {
                    _inst.hp = hp_val;
                    _inst.maxhp = max_hp;
                    if (hp_val > 0 && _inst.state == ENEMY_STATE.DEAD) {
                        _inst.state = ENEMY_STATE.NORMAL;
                        _inst.timer = 0;
                        _inst.image_alpha = 1;
                    }
                }
            }
            //show_debug_message("[解析] 收到 MSG_ENEMY_HP: ID=" + string(net_id) + " HP=" + string(hp_val) + "/" + string(max_hp));
            break;
        }
        case MSG_ENEMY_CALIBRATE:
        {
            // 字段：网络ID(s32), 世界X(f32), 世界Y(f32)
            var net_id = buffer_read(buf, buffer_s32);
            var wx = buffer_read(buf, buffer_f32);
            var wy = buffer_read(buf, buffer_f32);
            var _inst = global.network.map_net_id_instance_id[? net_id];
            if (instance_exists(_inst)) {
                _inst.x = wx;
                _inst.y = wy;
            }
            //show_debug_message("[解析] 收到 MSG_ENEMY_CALIBRATE: ID=" + string(net_id) + " 位置(" + string(wx) + "," + string(wy) + ")");
            break;
        }
        
        case MSG_UNIT_HP:
        {
            // 字段：net_id(s32), hp(s32), max_hp(s32)
            var net_id = buffer_read(buf, buffer_s32);
            var hp_val = buffer_read(buf, buffer_s32);
            var max_hp = buffer_read(buf, buffer_s32);
            var _inst = global.network.map_net_id_instance_id[? net_id];
            if (instance_exists(_inst)) {
                _inst.hp = hp_val;
                _inst.max_hp = max_hp;
            }
            break;
		}


        case MSG_DESTROY:
        {
            var _net_id = buffer_read(buf, buffer_s32);
            var _inst = global.network.map_net_id_instance_id[? _net_id];
			// 炸弹类/幻幻鸡：立即销毁，动画与服务端同步（不进延迟队列）
			if (instance_exists(_inst)) {
				if array_contains([obj_wine_bottle_bomb,obj_aquarius_elve,obj_whisky_bomb,obj_kettle_bomb,obj_coke_bomb,obj_bull_firework,obj_magic_chicken,
				obj_coffee_grounds,obj_firework_dragon,obj_chili_powder,obj_ice_cream,obj_delicacy_firework,obj_rabbit_lantern],_inst.object_index){
					global.network.client_able = true;
					instance_destroy(_inst);
					global.network.client_able = false;
				}
				else{
					// 延迟销毁，避免销毁后野指针访问
					_inst.pending_destroy = true;
					_inst.visible = false;
					ds_list_add(global._destroy_queue, {inst: _inst, timer: 60});
				}
			}
            break;
		}
        case MSG_CAT_ATTACK:
        {
            var _row = buffer_read(buf, buffer_u8);
            with (obj_cat) {
                if (self.row == _row) {
                    self.state ="awake"
                }
            }
            break;
        }
        

        case MSG_ENEMY_STEAL:
        {
            // 字段：net_id(s32), col(u8), row(u8)
            var net_id = buffer_read(buf, buffer_s32);
            var col = buffer_read(buf, buffer_u8);
            var row = buffer_read(buf, buffer_u8);

            // 执行偷取
            network_enemy_steal(col, row, net_id);


            show_debug_message("[解析] 收到 MSG_ENEMY_STEAL: ID=" + string(net_id) + " col=" + string(col) + " row=" + string(row));
            break;

        }
        case MSG_PROGRESS_SYNC:
        {
            var wave = buffer_read(buf, buffer_u8);
            var subwave = buffer_read(buf, buffer_u8);
            obj_battle.current_wave = wave;
            obj_battle.current_subwave = subwave;
            break;
        }

        case MSG_MUSIC_SYNC:
        {
            var _flag = buffer_read(buf, buffer_s32);
            // flag=1 → elite_music  flag=2 → boss_music
            // global.level_data 已在 MSG_ENTER_ROOM_READY 时解析为本地音频 ID
            // （标准音乐走 get_load_audio，实验室音乐走 file_cache）
            var _music = undefined;
            switch (_flag) {
                case 1: _music = global.level_data[$ "elite_music"]; break;
                case 2: _music = global.level_data[$ "boss_music"]; break;
            }
            if (!is_undefined(_music)) {
                with (obj_battle_music_controller) {
                    new_battle_music = _music;
                    event_user(0);
                }
            }
            break;
        }

        case MSG_MODIFY_PROP:
        {
            var _net_id = buffer_read(buf, buffer_s32);
            var _json = buffer_read(buf, buffer_string);
            var _inst = global.network.map_net_id_instance_id[? _net_id];
            if (instance_exists(_inst)) {
                var _props = json_parse(_json);
                // 服务器 timer 领先时快进追上，确保 target_coord 等依赖 timer==N 初始化的变量被正确生成
                if (variable_struct_exists(_props, "timer")) {
                    var _sv_t = _props[$ "timer"];
                    with (_inst) step_ready = true;
					bak_is_paused = global.is_paused
					global.is_paused = false;
				    try {
				        while (_inst.timer < _sv_t) {
							var _timer = _inst.timer; 
				            with (_inst) event_perform(ev_step, ev_step_normal);
							if(_timer==_inst.timer)break;//死循环
				        }
				    } finally {
				        global.is_paused = bak_is_paused;
				        with (_inst) step_ready = false;
				    }
                }
                if (variable_struct_exists(_props, "skill_timer") && variable_instance_exists(_inst, "skill_timer")) {
                    var _sv_st = _props[$ "skill_timer"];
                    with (_inst) step_ready = true;
					bak_is_paused = global.is_paused
					global.is_paused = false;
				      try {
				          while (_inst.skill_timer < _sv_st) {
							  var _timer = _inst.skill_timer; 
				              with (_inst) event_perform(ev_step, ev_step_normal);
							  if(_timer==_inst.skill_timer)break;//死循环
				          }
				      } finally {
				          global.is_paused = bak_is_paused;
				          with (_inst) step_ready = false;
				      }
                }
                var _keys = struct_get_names(_props);
                for (var _k = 0; _k < array_length(_keys); _k++) {
                    var _key = _keys[_k];
                    var _val = _props[$ _key];
                    if (_key == "_VM_id") {
                        ds_map_add(global._VM_id_to_real, _val, _inst);
                        ds_map_add(global._VM_real_to_vm_id, _inst, _val);
                    }
                    if (_key == "sprite_index" && is_string(_val)) {
                        _val = get_load_sprite(_val);
                    }
                    variable_instance_set(_inst, _key, _val);
                }
            }
            break;
        }

        case MSG_REQUEST_FILE:
        {
            var _filename = buffer_read(buf, buffer_string);
            var _purpose = buffer_read(buf, buffer_string);
            show_debug_message("[解析] 收到 MSG_REQUEST_FILE: " + _filename + " 用途:" + _purpose);
            file_cache_handle_request(_filename, _purpose, _sock);
            break;
        }

        case MSG_TRANSFER_FILE:
        {
            var _filename = buffer_read(buf, buffer_string);
            var _purpose = buffer_read(buf, buffer_string);
            var _size = buffer_read(buf, buffer_s32);
            var _remaining = buffer_get_size(buf) - buffer_tell(buf);
            if (_remaining <= 0) break;
            var _data = buffer_create(_size, buffer_fixed, 1);
            buffer_copy(buf, buffer_tell(buf), _size, _data, 0);
            buffer_seek(buf, buffer_seek_relative, _size);
            show_debug_message("[解析] 收到 MSG_TRANSFER_FILE: " + _filename + " 用途:" + _purpose + " (" + string(_size) + " bytes)");
            if (_purpose == "VM_sprite") {
                var _safe = string_replace_all(string_replace_all(_filename, "/", "_"), "\\", "_");
                var _path = "cache/" + _safe;
                buffer_save(_data, _path);
                var _real = sprite_add(_path, 1, false, false, 0, 0);
                if (_real != -1 && ds_map_exists(global._VM_sprite_temp_cache, _filename)) {
                    var _old = global._VM_sprite_temp_cache[? _filename];
                    if (sprite_exists(_old)) sprite_delete(_old);
				    global._VM_sprite_temp_cache[? _filename] = _real;
				    global._pid_reverse[? _real] = _filename;
				    global._pid_reverse[? _old] = _filename;
                }
                buffer_delete(_data);
            } else {
                file_cache_handle_receive(_filename, _purpose, _size, _data);
            }
            break;
        }
		
        case MSG_BUN_ABSORB:
        {
            var _net_id = buffer_read(buf, buffer_s32);
            var _bun_amount = buffer_read(buf, buffer_u8);
            var _bullet_name = buffer_read(buf, buffer_string);
            var _damage = buffer_read(buf, buffer_s32);
            var _king = global.network.map_net_id_instance_id[? _net_id];
            if (instance_exists(_king)) {
                var _bullet_type = asset_get_index(_bullet_name);
                if (_bullet_type < 0) _bullet_type = obj_xiaolongbao_bullet;
                for (var _j = 0; _j < _bun_amount; _j++) {
                    if (_king.bun_count < _king.max_bun) {
                        _king.bun_count++;
                        array_push(_king.bullet_list, {"bullet_type": _bullet_type, "damage": _damage});
                    }
                }
                				_king.state = CARD_STATE.GROW;
				_king.upgrade_timer = 0;
            }
            break;
        }

        case MSG_ACTIVE_SKILL_REQUEST:
        {
            var _type = buffer_read(buf, buffer_string);
            var _col = buffer_read(buf, buffer_u8);
            var _row = buffer_read(buf, buffer_u8);
            var _level = buffer_read(buf, buffer_u8);
            var _coords = buffer_read(buf, buffer_string);
            _coords = network_active_skill(_type, _col, _row, _level, _coords);
            network_broadcast_active_skill(_type, _col, _row, _level, _coords);
            break;
        }

        case MSG_ACTIVE_SKILL:
        {
            var _type = buffer_read(buf, buffer_string);
            var _col = buffer_read(buf, buffer_u8);
            var _row = buffer_read(buf, buffer_u8);
            var _level = buffer_read(buf, buffer_u8);
            var _coords = buffer_read(buf, buffer_string);
            network_active_skill(_type, _col, _row, _level, _coords);
            break;
		}

        case MSG_SERVER_ACTION:
        {
            var _act = buffer_read(buf, buffer_u8);
            switch (_act) {
                case 0: game_set_speed(60, gamespeed_fps); obj_battle.speed_up = false; break;
                case 1: game_set_speed(120, gamespeed_fps); obj_battle.speed_up = true; break;
                case 2: global.is_paused = true; break;
                case 3: global.is_paused = false; break;
                case 4: global.gui_stack.to(room_ready); break;
                case 5: global.gui_stack.pop(); room_goto(room_battle); break;
            }
            break;
        }

        case MSG_EVENT_ACTIONS:
        {
			global.network.client_able = true;
            var _json = buffer_read(buf, buffer_string);
            var _actions = json_parse(_json);
            for (var _i = 0; _i < array_length(_actions); _i++) {
                var _act = _actions[_i];
                switch (_act.op) {
                    case "spawn":
						var obj_index = asset_get_index(_act.obj);
						if(obj_index==-1){obj_index=obj_paratrooper_mouse_shield;}
						// mod 敌人：身份要赶在 Create 之前塞进 pending 全局（props 里带 enemy_id）
						var _bak_pending_eid = variable_global_exists("_mod_pending_enemy_id") ? global._mod_pending_enemy_id : "";
						if (is_struct(_act.props) && variable_struct_exists(_act.props, "enemy_id")) {
							global._mod_pending_enemy_id = _act.props[$ "enemy_id"];
						}
                        var _inst = instance_create_depth(_act.x, _act.y, _act.depth, obj_index);
						global._mod_pending_enemy_id = _bak_pending_eid;
                        set_net_id(_inst.id, _act.net_id);
						
                        with (_inst) {
                            var _props = _act.props;
                            var _keys = struct_get_names(_props);
                            for (var _k = 0; _k < array_length(_keys); _k++) {
                                var _key = _keys[_k];
                                var _val = _props[$ _key];
                                if (_key == "sprite_index" && is_string(_val)) {
                                    _val = get_load_sprite(_val);
                                }
                                if (_key == "map_spr" && is_string(_val)) {
                                    _val = get_load_sprite(_val);
                                }
                                variable_instance_set(id, _key, _val);
                            }
                            if (variable_struct_exists(_act, "target_ids")) {
                                var _target_ids = _act.target_ids;
                                var _ids_keys = variable_struct_get_names(_target_ids);
                                for (var _k = 0; _k < array_length(_ids_keys); _k++) {
                                    var _key = _ids_keys[_k];
									if ds_map_exists(global.network.map_net_id_instance_id,_target_ids[$ _key]){
										var _val = global.network.map_net_id_instance_id[? _target_ids[$ _key] ];
										variable_instance_set(id, _key, _val);
									}
                                }
                            }
                        }
                        break;
                   case "modify":
						_inst = global.network.map_net_id_instance_id[? _act.net_id];
						if (!instance_exists(_inst)) break;
						_inst.visible=true;
						with (_inst) {
							var _props = _act.props;
							var _keys = struct_get_names(_props);
							for (var _k = 0; _k < array_length(_keys); _k++) {
							    var _key = _keys[_k];
							    var _val = _props[$ _key];
							    if (_key == "sprite_index" && is_string(_val)) {
							        _val = get_load_sprite(_val);
							    }
							    if (_key == "map_spr" && is_string(_val)) {
							        _val = get_load_sprite(_val);
							    }
							    variable_instance_set(id, _key, _val);
							}
							if (variable_struct_exists(_act, "target_ids")) {
							    var _target_ids = _act.target_ids;
							    var _ids_keys = variable_struct_get_names(_target_ids);
							    for (var _k = 0; _k < array_length(_ids_keys); _k++) {
							        var _key = _ids_keys[_k];
									if ds_map_exists(global.network.map_net_id_instance_id,_target_ids[$ _key]){
										var _val = global.network.map_net_id_instance_id[? _target_ids[$ _key] ];
										variable_instance_set(id, _key, _val);
									}
							    }
							}
						}
                        break;
                    case "destroy":
                        if (instance_exists(_act.id)) instance_destroy(_act.id);
                        break;
					case "state":
						if (_act.key == "grid_terrains") global.grid_terrains = json_parse(_act.val);
						if (_act.key == "row_feature")   global.row_feature   = json_parse(_act.val);
						if (_act.key == "map_spr_index") obj_battle.map_spr_index = real(_act.val);
						break;
					}
            }
			global.network.client_able = false;
			
            break;
        }
        

        case MSG_GAME_OVER:
        {
            // 字段：结果(u8) 0失败 1胜利
            var result = buffer_read(buf, buffer_u8);
            // TODO: 调用处理函数
            // handle_game_over(result);
			global.is_paused = true;
			global.game_over = true;
			if result==1{
				var inst = instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over);
				inst.sprite_index = spr_win;
				audio_play_sound(snd_win,0,0);
				// 客户端自动领取关卡奖励
				if (global.network.mode == "client" && !global.laboretory_room) {
					with (obj_task_manager) {
						refresh_task_progress();
					}
					if (array_get_index(global.save_data.completed_levels, global.level_data.id) == -1) {
						if (global.level_file.rewards[1].player_level >= global.save_data.player.level) {
							global.save_data.player.level = global.level_file.rewards[1].player_level;
						}
						complete_level(global.level_data.id);
						var _bpm = instance_find(obj_battle_pause_manager, 0);
						if (_bpm != noone && array_get_index(_bpm.slot_unlock_level_id_list, global.level_data.id) != -1) {
							if (global.save_data.unlocked_items.max_slot < 21) {
								global.save_data.unlocked_items.max_slot += 1;
								show_notice("你解锁了一个新的卡槽", 60);
							}
						}
						if (global.level_data.id == "champagne_island_water") {
							global.save_data.unlocked_items.elite_unlocked = true;
						}
						if (global.level_data.id == "abyss") {
							global.save_data.unlocked_items.shovel = "copper";
						}
						if (global.level_data.id == "macchiato_port") {
							global.save_data.unlocked_items.shovel = "silver";
						}
						if (global.level_data.id == "snowcap_volcano") {
							global.save_data.unlocked_items.shovel = "gold";
						}
						if (global.level_file.rewards[1].skill_level >= global.save_data.unlocked_items.max_skill_level) {
							global.save_data.unlocked_items.max_skill_level = global.level_file.rewards[1].skill_level;
							var _len = array_length(global.save_data.unlocked_cards);
							for (var _j = 0; _j < _len; _j++) {
								global.save_data.unlocked_cards[_j].skill = global.save_data.unlocked_items.max_skill_level;
							}
						}
						global.save_data.player.gold += global.level_file.rewards[1].gold;
						var _item_list = global.level_file.rewards[1].items;
						for (var _k = 0; _k < array_length(_item_list); _k++) {
							add_material_amount(_item_list[_k].id, real(_item_list[_k].amount));
						}
						var _card_list = global.level_file.rewards[1].card_unlock;
						for (var _c = 0; _c < array_length(_card_list); _c++) {
							unlock_card(_card_list[_c], 0, 0, global.save_data.unlocked_items.max_skill_level);
						}
						var _wep_list = global.level_file.rewards[1].weapon_unlock;
						for (var _w = 0; _w < array_length(_wep_list); _w++) {
							unlock_weapon(_wep_list[_w]);
						}
						var _gem_list = global.level_file.rewards[1].gem_unlock;
						for (var _g = 0; _g < array_length(_gem_list); _g++) {
							unlock_gem(_gem_list[_g]);
						}
					} else {
						global.save_data.player.gold += global.level_file.rewards[0].gold;
						var _ritems = global.level_file.rewards[0].items;
						for (var _ri = 0; _ri < array_length(_ritems); _ri++) {
							add_material_amount(_ritems[_ri].id, _ritems[_ri].amount);
						}
					}
					save_file(global.save_slot);
					var _pm = instance_find(obj_battle_pause_manager, 0);
					if (_pm != noone) {
						_pm.settlement = true;
					}
				}
			}else{
				instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over);
				audio_play_sound(snd_lose,0,0);
			}
			
            show_debug_message("[解析] 收到 MSG_GAME_OVER: 结果=" + string(result));
            break;
        }
        

        
        case MSG_SYNC_INITIAL_STATE:
        {
            // 注意：此消息结构复杂，需按顺序读取
            // 我们先读植物列表
            var unit_count = buffer_read(buf, buffer_u16);
            for (var i = 0; i < unit_count; i++) {
                var net_id = buffer_read(buf, buffer_s32);
                var unit_type = buffer_read(buf, buffer_u8);
                var level = buffer_read(buf, buffer_u8);
                var col = buffer_read(buf, buffer_u8);
                var row = buffer_read(buf, buffer_u8);
                var hp = buffer_read(buf, buffer_s32);
                var max_hp = buffer_read(buf, buffer_s32);
                // TODO: 将信息保存到临时列表，待全量解析完成后统一处理
                // 或者直接调用 add_unit_from_sync(...)
            }
            // 再读敌人列表
            var enemy_count = buffer_read(buf, buffer_u16);
            for (var i = 0; i < enemy_count; i++) {
                var net_id = buffer_read(buf, buffer_s32);
                var enemy_type = buffer_read(buf, buffer_u8);
                var row = buffer_read(buf, buffer_u8);
                var wx = buffer_read(buf, buffer_f32);
                var wy = buffer_read(buf, buffer_f32);
                var hp = buffer_read(buf, buffer_s32);
                var max_hp = buffer_read(buf, buffer_s32);
                var state = buffer_read(buf, buffer_u8);
                //var speed = buffer_read(buf, buffer_f32);
                // TODO: 同样保存或直接创建
            }
            // 最后读全局状态
            var sunlight = buffer_read(buf, buffer_s32);
            var time_left = buffer_read(buf, buffer_s16);
            // TODO: 调用 handle_initial_sync(units, enemies, sunlight, time_left);
            show_debug_message("[解析] 收到 MSG_SYNC_INITIAL_STATE: 植物数=" + string(unit_count) + " 敌人数=" + string(enemy_count) + " 阳光=" + string(sunlight) + " 时间=" + string(time_left));
            break;
        }
		
		case MSG_START_BATTLE:
		{
            show_debug_message("[解析] 收到 MSG_START_BATTLE: 开始战斗");
			ds_map_clear(global.network.map_instance_id_net_id)
			ds_map_clear(global.network.map_net_id_instance_id)
			global.network.net_instance_count=0
			texture_prefetch("bullet")
			texture_prefetch("effects")
			if instance_exists(obj_readyroom_manager) {
				var _needed = obj_readyroom_manager._get_needed_sprites();
				sprite_manager_load_battle(_needed);
			}
			global.wait_sprite_load = true;
			global.gui_stack.to(room_battle)
			global.__stop_msg_loop = true;
			

			
			break;
		}
		
		case MSG_ENTER_ROOM_READY:
		{
		    show_debug_message("[解析] 收到 MSG_ENTER_ROOM_READY: 进入房间");

			var json_text = buffer_read(buf, buffer_string);
			var json_data;
			try
			{
			    json_data = json_parse(json_text);
			}
			catch(_)
			{
			    show_debug_message("JSON损坏：" + json_text);
			    return;
			}

			global.map_id      = json_data[$ "map_id"] ?? "";
			global.level_index = json_data[$ "level_index"] ?? 0;
			global.level_id    = json_data[$ "target_level_id"] ?? "";
			global.level_data  = json_data[$ "level_data"];
			global.level_file  = json_data[$ "level_file"];

			// json 路径：服务端下发，客户端本地查找贴图/音乐用（必须在 VM_InitRoomEntry 之前设置）
			var _json_path = variable_struct_get(json_data, "json_path");
			if (!is_undefined(_json_path) && _json_path != "") {
				global._file_cache_json_path = _json_path;
			}

			// 从 target_level_id 提取 JSON 相对子目录，供 file_cache 本地查找
			{
			    var _lid = string_replace_all(global.level_id, "\\", "/");
			    var _lab = string_pos("laboratory/", _lid);
			    if (_lab > 0) {
			        var _rel = string_copy(_lid, _lab, string_length(_lid) - _lab + 1);
			        var _last = 0;
			        for (var _j = string_length(_rel); _j > 0; _j--) {
			            if (string_char_at(_rel, _j) == "/") { _last = _j; break; }
			        }
			        global._lab_json_subdir = (_last > 0) ? string_copy(_rel, 1, _last) : "";  // "laboratory/myset"
			    } else {
			        global._lab_json_subdir = "";
			    }
			}

			// VM 随机种子：服务端随房间同步下发，必须在 VM_InitRoomEntry 之前设置
			//（_VM_CONST_INIT / _VM_ROOM_READY_ENTRY 会在加载时立即执行）
			var _vm_seed = variable_struct_get(json_data, "vm_seed");
			if (!is_undefined(_vm_seed)) {
				global.__vm.rng_state = _vm_seed;
			} else {
				global.__vm.rng_state = 0x9E3779B9;   // 无种子时重置为默认，避免残留上一局的旧状态
			}

			// 读 .bin 数据：JSON 后剩余的全部内容
			var _remain = buffer_get_size(buf) - buffer_tell(buf);
			if (_remain > 0) {
			    var _bin_buf = buffer_create(_remain, buffer_fixed, 1);
			    buffer_copy(buf, buffer_tell(buf), _remain, _bin_buf, 0);
			    buffer_seek(_bin_buf, buffer_seek_start, 0);
			    VM_InitRoomEntry(_bin_buf);
			} else {
			    VM_InitRoomEntry(undefined);
			}

			// 存到全局，供 file_cache 内部使用
			global._expected_fps = variable_struct_get(json_data, "resource_fingerprints")
			if (is_undefined(global._expected_fps)) global._expected_fps = {};

			// 自定义地图精灵：指纹一致用本地文件，否则请求服务端
			var _sprite_name = variable_struct_get(json_data, "map_sprite_name")
			if (!is_undefined(_sprite_name) && string_length(_sprite_name) > 0) {
				file_cache_set_expected_sprite(_sprite_name);
				var _fp = variable_struct_get(global._expected_fps, "map_sprite");
				global.level_data[$ "level_sprite"] = file_cache_load_sprite(
					_sprite_name,
					get_load_sprite("spr_cookie_island"),
					is_undefined(_fp) ? "" : _fp
				);
			}
			// level_sprite 如果还是 name 字符串，转回本地精灵索引（标准关/魔塔）
			{
				var _spr_v = global.level_data[$ "level_sprite"];
				if (is_string(_spr_v)) {
					global.level_data[$ "level_sprite"] = get_load_sprite(_spr_v);
				}
			}

			// 自定义音乐：指纹一致用本地文件，否则请求服务端
			var _music_names = variable_struct_get(json_data, "custom_music_names")
			if (!is_undefined(_music_names)) {
				var __fp;

				var _pre = variable_struct_get(_music_names, "pre_music");
				if (!is_undefined(_pre) && string_length(_pre) > 0) {
					file_cache_set_expected_audio(_pre, "pre_music");
					__fp = variable_struct_get(global._expected_fps, "pre_music");
					global.level_data[$ "pre_music"] = file_cache_load_audio(
					    _pre, "pre_music",
					    get_load_audio("mus_delicious_island_daytime_pre"),
					    is_undefined(__fp) ? "" : __fp
					);
				}

				var _elite = variable_struct_get(_music_names, "elite_music");
				if (!is_undefined(_elite) && string_length(_elite) > 0) {
					file_cache_set_expected_audio(_elite, "elite_music");
					__fp = variable_struct_get(global._expected_fps, "elite_music");
					global.level_data[$ "elite_music"] = file_cache_load_audio(
					    _elite, "elite_music",
					    get_load_audio("mus_delicious_island_daytime_elite"),
					    is_undefined(__fp) ? "" : __fp
					);
				}

				var _boss = variable_struct_get(_music_names, "boss_music");
				if (!is_undefined(_boss) && string_length(_boss) > 0) {
					file_cache_set_expected_audio(_boss, "boss_music");
					__fp = variable_struct_get(global._expected_fps, "boss_music");
					global.level_data[$ "boss_music"] = file_cache_load_audio(
					    _boss, "boss_music",
					    get_load_audio("mus_delicious_island_daytime_boss"),
					    is_undefined(__fp) ? "" : __fp
					);
				}
			}
			// 将 level_data 中的音频名字符串转为 audio ID
			{
				var _pre = global.level_data[$ "pre_music"];
				if (is_string(_pre)) {
					global.level_data[$ "pre_music"] = get_load_audio(_pre);
				}
				var _elite = global.level_data[$ "elite_music"];
				if (is_string(_elite)) {
					global.level_data[$ "elite_music"] = get_load_audio(_elite);
				}
				var _boss = global.level_data[$ "boss_music"];
				if (is_string(_boss)) {
					global.level_data[$ "boss_music"] = get_load_audio(_boss);
				}
			}
			if( global.level_file!= undefined&& global.level_file.total_waves!= undefined){
				audio_play_sound(snd_button, 0, 0);
				texture_prefetch("cards");
				
				if global.gui_stack.get_top() == room_ready{

					//统计敌人和BOSS类型
					with (obj_readyroom_manager){
						enemy_type_list = []
						boss_type_list = []
						var _wave_count = min(global.level_file.total_waves, array_length(global.level_file.waves));
						for(var i = 0;i < _wave_count;i ++){
							if global.level_file.waves[i].boss_wave{
								if array_get_index(boss_type_list,global.level_file.waves[i].boss) == -1{
									array_push(boss_type_list,global.level_file.waves[i].boss)
								}
								if is_real(global.level_file.version){
									if array_get_index(boss_type_list,global.level_file.waves[i].boss2) == -1 && global.level_file.waves[i].boss2 != ""{
										array_push(boss_type_list,global.level_file.waves[i].boss2)
									}
								}
							}
							var subwave = global.level_file.waves[i].subwaves
							for(var j = 0 ; j <array_length(subwave);j++){
								var enemy_list = subwave[j].enemy_list
								for(var k = 0 ; k < array_length(enemy_list);k++){
									if array_get_index(enemy_type_list,enemy_list[k].type) == -1{
										array_push(enemy_type_list,enemy_list[k].type)
									}
								}
							}
						}
					}
					
				}else{
					global.gui_stack.to(room_ready);
				}
			}

			break;
		}
		
		
		case MSG_PUB_INFO:
		{
			var chat_text = buffer_read(buf, buffer_string);
			show_debug_message("[解析] 收到 MSG_PUB_INFO: " + chat_text);

	
			// 首词当命令、其余当参数：这样带参数的指令（\setglobal 变量 值）也能写成 case
			var _cmd = chat_text;
			var _arg = "";
			var _csp = string_pos(" ", chat_text);
			if (_csp > 0) { _cmd = string_copy(chat_text, 1, _csp - 1); _arg = string_delete(chat_text, 1, _csp); }

			switch (_cmd) {
				case "\\modserver":
				    global.network.mode = "server";
				    global.network.connected_clients = [global.network.server_socket];
				    global.network.server_port = global.network.target_port;
				    break;
				case "\\modclient":
				    break;
				case "\\host_left":
				    shell_print("[系统] 房主已离开，房间关闭");
				    sh_disconnect();
				    break;
				case "\\kicked":
				    shell_print("[系统] 你被房主踢出了房间");
				    sh_disconnect();
				    break;
				case "\\join":
				    // 有人加入房间（\join [名字]）→ 屏幕公告
				    show_notice(_arg != "" ? (_arg + " 加入了房间") : "有人加入了房间", 150);
				    break;
				case "\\left":
				    // 有人离开房间（\left [名字]）→ 屏幕公告
				    show_notice(_arg != "" ? (_arg + " 离开了房间") : "有人离开了房间", 150);
				    break;
				case "\\setglobal":
				    // 批量改全局变量 —— \setglobal {"name":"x","hp":100,"on":true}
				    // 参数是 JSON 字典，逐项赋值，**类型跟着 JSON 走**（数字=real、true/false=bool、数组/结构原样）
				    if (_arg != "") {
				        var _gobj = undefined;
				        try { _gobj = json_parse(_arg); } catch (_ge) { _gobj = undefined; }
				        if (is_struct(_gobj)) {
				            var _gkeys = variable_struct_get_names(_gobj);
				            for (var _gk = 0; _gk < array_length(_gkeys); _gk++) {
				                variable_global_set(_gkeys[_gk], _gobj[$ _gkeys[_gk]]);
				            }
				            show_debug_message("[解析] MSG_PUB_INFO setglobal: " + string(array_length(_gkeys)) + " 项 ← " + _arg);
				        } else {
				            show_debug_message("[解析] MSG_PUB_INFO setglobal 参数不是合法 JSON 字典: " + _arg);
				        }
				    }
				    break;
				case "\\setnet":
				    // 改 global.network 的字段 —— \setnet 字段名 值（值可带空格；类型跟着原值走）
				    if (_arg != "") {
				        var _nsp = string_pos(" ", _arg);
				        var _fname = (_nsp > 0) ? string_copy(_arg, 1, _nsp - 1) : _arg;
				        var _fval  = (_nsp > 0) ? string_delete(_arg, 1, _nsp) : "";
				        if (!variable_struct_exists(global.network, _fname)) {
				            show_debug_message("[解析] MSG_PUB_INFO setnet 未知字段: " + _fname);
				        } else {
				            var _cur = global.network[$ _fname];
				            var _newval = _fval;
				            if (is_real(_cur))      _newval = real(_fval);                       // 数值字段
				            else if (is_bool(_cur)) _newval = (_fval == "1" || _fval == "true"); // 开关字段
				            global.network[$ _fname] = _newval;
				            show_debug_message("[解析] MSG_PUB_INFO setnet: " + _fname + " = " + string(_newval));
				        }
				    }
				    break;
				default:
					if (string_starts_with(chat_text, "\\roominfo ")) {
					    try {
					        var _json_str = string_copy(chat_text, 11, string_length(chat_text) - 10);
					        var _info = json_parse(_json_str);
					        global.room_name = _info[$ "room"];
					        if (!variable_global_exists("room_members")) {
					            global.room_members = ds_map_create();
					        }
					        ds_map_clear(global.room_members);
					        var _members = _info[$ "members"];
					        var _keys = struct_get_names(_members);
					        for (var _i = 0; _i < array_length(_keys); _i++) {
					            ds_map_add(global.room_members, _keys[_i], _members[$ _keys[_i]]);
					        }
					    } catch (_) {}
					    break;
					}
					shell_print(chat_text);
			}
	
			break;
		}
		
        case MSG_PLATFORM_TICK:            // 参数: json(string)
        {
            var _json = buffer_read(buf, buffer_string);
            var _list = json_parse(_json);
            for (var _i = 0; _i < array_length(_list); _i++) {
                var _entry = _list[_i];
                var _inst = global.network.map_net_id_instance_id[? _entry[$ "n"]];
                if (!instance_exists(_inst)) continue;
                var _gap = _entry[$ "fc"] - _inst.frame_count;
                if (_gap <= 0) continue;
                with (_inst) {
                    step_ready = true;
                    if (object_is_ancestor(object_index, obj_enemy_parent))
                        repeat (_gap) event_perform(ev_step, ev_step_normal);  // boss: Step_0
                    else
                        repeat (_gap) event_perform(ev_step, 2);               // 平台: Step_2
                    if (global.network.mode == "client") step_ready = false;
                }
            }
            break;
        }

        case MSG_VM_NOTIFY:            // 参数: json(string)
        {
            var _json = buffer_read(buf, buffer_string);
            show_debug_message("[解析] MSG_VM_NOTIFY: " + _json);
            VM_HandleNotify(_json);
            break;
        }

        default:
        {
            show_debug_message("[解析] 警告：未知消息ID " + string(msg_id) + "，跳过该消息");
            break;
        }
    }
}

/// @param {real} msg_id  消息ID常量
/// @param {...} 可变参数，按协议顺序传入字段值
/// @description 自动根据 msg_id 打包并发送，支持所有消息类型
function send_message(socket, msg_id) {
    // 创建缓冲区（grow模式会自动扩容）
	
	show_debug_message("发送数据");
	
    var buf = buffer_create(1024, buffer_grow, 1);
    
    // 根据消息ID写入对应的字段
    switch (msg_id) {
        // ======== 客户端请求 ========
        case MSG_UNIT_REQUEST:          // 参数: level(u8), col(u8), row(u8), skill(u8), shape(u8), obj_name(string), meta_info(string), sprite_name(string)
            buffer_write(buf, buffer_u8, argument[2]);
            buffer_write(buf, buffer_u8, argument[3]);
            buffer_write(buf, buffer_u8, argument[4]);
            buffer_write(buf, buffer_u8, argument[5]);
            buffer_write(buf, buffer_u8, argument[6]);
            buffer_write(buf, buffer_string, argument[7]);
            buffer_write(buf, buffer_string, argument[8]);
            buffer_write(buf, buffer_string, argument[9]);
            break;


            
		case MSG_REMOVE_UNIT_REQUEST:      // 参数: net_id(s32), col(u8), row(u8), flame_amount(s32)
			buffer_write(buf, buffer_s32, argument[2]);
			buffer_write(buf, buffer_u8, argument[3]);
			buffer_write(buf, buffer_u8, argument[4]);
			buffer_write(buf, buffer_s32, argument[5]);
			break;

		case MSG_REMOVE_UNIT:           // 参数: net_id(s32), col(u8), row(u8), flame_amount(s32)
			buffer_write(buf, buffer_s32, argument[2]);
			buffer_write(buf, buffer_u8, argument[3]);
			buffer_write(buf, buffer_u8, argument[4]);
			buffer_write(buf, buffer_s32, argument[5]);
		case MSG_ENEMY_STEAL:           // 参数: net_id(s32), col(u8), row(u8)
			buffer_write(buf, buffer_s32, argument[2]);
			buffer_write(buf, buffer_u8, argument[3]);
			buffer_write(buf, buffer_u8, argument[4]);
			break;

		case MSG_PROGRESS_SYNC:         // 参数: current_wave(u8), current_subwave(u8)
			buffer_write(buf, buffer_u8, argument[2]);
			buffer_write(buf, buffer_u8, argument[3]);
			break;

		case MSG_ACTIVE_SKILL_REQUEST:  // 参数: skill_type(string), col(u8), row(u8), level(u8), coords_json(string)
		case MSG_ACTIVE_SKILL:          // 参数: skill_type(string), col(u8), row(u8), level(u8), coords_json(string)
			buffer_write(buf, buffer_string, argument[2]);
			buffer_write(buf, buffer_u8, argument[3]);
			buffer_write(buf, buffer_u8, argument[4]);
			buffer_write(buf, buffer_u8, argument[5]);
			buffer_write(buf, buffer_string, argument_count > 6 ? argument[6] : "");
			break;

		case MSG_SERVER_ACTION:         // 参数: action(u8)
			buffer_write(buf, buffer_u8, argument[2]);
			break;

        case MSG_CHAT:                  // 参数: text(string)
            buffer_write(buf, buffer_string, argument[2]);
            break;
            
        // ======== 服务器广播 ========
            
        case MSG_SPAWN_UNIT:            // 参数: 网络ID(s32), level(u8), col(u8), row(u8), skill(u8), shape(u8), sprite_name(string), object_name(string), meta_info(string)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_u8, argument[3]);
            buffer_write(buf, buffer_u8, argument[4]);
            buffer_write(buf, buffer_u8, argument[5]);
            buffer_write(buf, buffer_u8, argument[6]);
            buffer_write(buf, buffer_u8, argument[7]);
            buffer_write(buf, buffer_string, argument[8]);
            buffer_write(buf, buffer_string, argument[9]);
            buffer_write(buf, buffer_string, argument[10]);
            break;
            
        case MSG_SPAWN_ENEMY:           // 参数: net_id(s32), x(f32), y(f32), object_name(string)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_f32, argument[3]);
            buffer_write(buf, buffer_f32, argument[4]);
            buffer_write(buf, buffer_string, argument[5]);
            break;

        case MSG_SPAWN_BOSS:            // 参数: net_id(s32), x(f32), y(f32), object_name(string), hp(s32), maxhp(s32), row(u8), random_seed(s32)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_f32, argument[3]);
            buffer_write(buf, buffer_f32, argument[4]);
            buffer_write(buf, buffer_string, argument[5]);
            buffer_write(buf, buffer_s32, argument[6]);
            buffer_write(buf, buffer_s32, argument[7]);
            buffer_write(buf, buffer_u8, argument[8]);
            buffer_write(buf, buffer_s32, argument[9]);
            break;

            
        case MSG_ENEMY_HP:              // 参数: net_id(s32), hp(s32), max_hp(s32)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_s32, argument[3]);
            buffer_write(buf, buffer_s32, argument[4]);
            break;
            
        case MSG_ENEMY_CALIBRATE:       // 参数: net_id(s32), wx(f32), wy(f32)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_f32, argument[3]);
            buffer_write(buf, buffer_f32, argument[4]);
            break;
            

        case MSG_DESTROY:              // 参数: net_id(s32)
            buffer_write(buf, buffer_s32, argument[2]);
            break;
        case MSG_CAT_ATTACK:           // 参数: row(u8)
            buffer_write(buf, buffer_u8, argument[2]);
            break;

        case MSG_UNIT_HP:               // 参数: net_id(s32), hp(s32), max_hp(s32), x(f32), y(f32)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_s32, argument[3]);
            buffer_write(buf, buffer_s32, argument[4]);
            break;

        case MSG_GAME_OVER:             // 参数: result(u8)
            buffer_write(buf, buffer_u8, argument[2]);
            break;
            
            
            
        case MSG_SYNC_INITIAL_STATE:    // 特殊：这个比较复杂，单独写一个专门函数
            // 略（因为涉及数组循环）
            break;
        case MSG_START_BATTLE:
			break;
		case MSG_ENTER_ROOM_READY:                  // 参数: text(string), bin_buf(optional)
            buffer_write(buf, buffer_string, argument[2]);
            if (argument_count > 2 && buffer_exists(argument[3])) {
                var _bin_size = buffer_get_size(argument[3]);
                buffer_seek(argument[3], buffer_seek_start, 0);
                buffer_copy(argument[3], 0, _bin_size, buf, buffer_tell(buf));
                buffer_seek(buf, buffer_seek_relative, _bin_size);
            }
            break;
        case MSG_EVENT_ACTIONS:         // 参数: json(string)
            buffer_write(buf, buffer_string, argument[2]);
            break;
        case MSG_MUSIC_SYNC:               // 参数: music_asset(s32)
            buffer_write(buf, buffer_s32, argument[2]);
            break;
        case MSG_MODIFY_PROP:              // 参数: net_id(s32), json(string)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_string, argument[3]);
            break;
        case MSG_REQUEST_FILE:             // 参数: filename(string)
            buffer_write(buf, buffer_string, argument[2]);
            buffer_write(buf, buffer_string, argument[3]);
            break;
        case MSG_TRANSFER_FILE:            // 参数: filename(string), purpose(string), size(s32), data(buffer)
            buffer_write(buf, buffer_string, argument[2]);
            buffer_write(buf, buffer_string, argument[3]);
            buffer_write(buf, buffer_s32, argument[4]);
            buffer_copy(argument[5], 0, buffer_get_size(argument[5]), buf, buffer_tell(buf));
            buffer_seek(buf, buffer_seek_relative, buffer_get_size(argument[5]));
            break;
        case MSG_BUN_ABSORB:              // 参数: king_net_id(s32), bun_amount(u8), bullet_name(string), damage(s32)
            buffer_write(buf, buffer_s32, argument[2]);
            buffer_write(buf, buffer_u8, argument[3]);
            buffer_write(buf, buffer_string, argument[4]);
            buffer_write(buf, buffer_s32, argument[5]);
            break;
        case MSG_PLATFORM_TICK:            // 参数: json(string)
            buffer_write(buf, buffer_string, argument[2]);
            break;
        case MSG_VM_NOTIFY:                // 参数: json(string)
            buffer_write(buf, buffer_string, argument[2]);
            break;

        default:
            show_debug_message("[警告] 未知消息ID: " + string(msg_id));
            buffer_delete(buf);
            return;
    }
    
    // ---- 发送数据包（长度头 + 消息ID + 负载） ----
    var payload_size = buffer_tell(buf);
    var body_size = 4 + payload_size;   // msg_id占4字节
    // 限制最大4MB-4，超出截断
    var max_body = 16777212;
    if (body_size > max_body) { body_size = max_body; }
    var packet = buffer_create(body_size + 4, buffer_grow, 1);
	
    buffer_write(packet, buffer_u32, 0);          // 占位
    buffer_write(packet, buffer_s32, msg_id);     // 写入ID
	
    buffer_copy(buf, 0, payload_size, packet, buffer_tell(packet));
    
    buffer_poke(packet, 0, buffer_u32, body_size);
	
	var total_size = buffer_get_size(packet);
	
    network_send_raw(socket, packet, 4+body_size);

    buffer_delete(buf);
    buffer_delete(packet);
}
