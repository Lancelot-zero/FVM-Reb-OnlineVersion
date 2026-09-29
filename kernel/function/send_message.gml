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
