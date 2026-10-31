/// 

self.state = {
    scale: 1.9,
    left: 0,
    top: 0,
    /// @type {Struct.CustomStage} 
    custom_stage: undefined,
    height: 0,
    width: 0,
    /// @type {function} 
    on_close_clicked: undefined,
    /// @type {Asset.GMObject.Button} 
    close_button: undefined,
    /// @type {Asset.GMObject.Button} 
    start_button: undefined,

}


/// @param {Struct.CustomStage} _custom_stage 
function init(_custom_stage) {
    self.state.custom_stage = _custom_stage
    return self
}

function set_position(_left, _top) {
    self.state.left = _left
    self.state.top = _top

    if (!is_undefined(self.state.close_button)) {
        self.state.close_button.set_position(_left + self.state.width - 50, _top + 44)
    }
    if (!is_undefined(self.state.start_button)) {
        self.state.start_button.set_position(self.state.left + self.state.width - 240, self.state.top + self.state.height - 110)
    }

    return self
}

function get_width() {
    return self.state.width
}

function get_height() {
    return self.state.height
}

function destroy_widgets() {
    if (!is_undefined(self.state.close_button) && instance_exists(self.state.close_button)) {
        instance_destroy(self.state.close_button)
    }
    self.state.close_button = undefined

    if (!is_undefined(self.state.start_button) && instance_exists(self.state.start_button)) {
        instance_destroy(self.state.start_button)
    }
    self.state.start_button = undefined
}

function on_close() {
    if (!is_undefined(self.state.on_close_clicked)) {
        self.state.on_close_clicked()
    }
    destroy_widgets()
    instance_destroy()
}

function on_create_room() {
    if (is_undefined(self.state.custom_stage)) {
        return
    }
    var _parse_result = global.laboratory_manager.file_util.load_json_from_path(self.state.custom_stage.json_path)
    if (_parse_result.is_failed()) {
        throw _parse_result.get_error_stack()
    }
    var _level_data = level_entry_from_stage_metadata(self.state.custom_stage)
    global.level_data = _level_data
    global.level_id = self.state.custom_stage.id
    global.level_file = _parse_result.data

    var _json_dir = self.state.custom_stage.json_path
    global._file_cache_json_path = _json_dir

    // VM 初始化：读取同名 .bin 文件（如果存在）
    var _bin_buf = undefined;
    var _bin_path = string_replace(_json_dir, ".json", ".bin");
    // VM 随机种子：创建房间时生成，随 bin 一起同步给客户端，保证两端 VM_Random 结果一致
    global._sync_vm_seed = irandom(2147483646) + 1;
    global.__vm.rng_state = global._sync_vm_seed;
    if (file_exists(_bin_path)) {
        _bin_buf = buffer_load(_bin_path);
        VM_InitRoomEntry(_bin_buf);
        // ⚠️ 不能在这里 buffer_delete：mode=="server" 时下面要把这份 bin 发给客机同步。
        //    VM_InitRoomEntry 内部已逐字节拷贝、不留原 buffer 引用，但同步发送还要用它。
        //    删掉后 buffer id 会被后续 buffer_create 复用（send_message 开包就会复用）→
        //    buffer_exists 判活成真 → buffer_copy 自拷/拷错对象 → 原生访问违例闪退。
        //    这份 buffer 的清理在 VM_InitRoomEntry 开头：进下一个房间时删上一份。
    } else {
        VM_InitRoomEntry(undefined);
    }
    // 单机：加载完就地释放这份地图 bin（服务器端还要拼包/发送，见下面的发送后释放）
    if (global.network.mode != "server" && buffer_exists(_bin_buf)) buffer_delete(_bin_buf);

    // 本图 mod bin：<同名>.<类型>.<名称>.bin，扫出来逐个装载（单机/房主都走这里；
    // 卸载在进下一张图时 VM_InitRoomEntry 开头按清单自动做）
    var _lab_mods = lab_mod_scan(_json_dir);
    for (var _lm = 0; _lm < array_length(_lab_mods); _lm++) {
        if (file_exists(_lab_mods[_lm].path)) {
            lab_mod_load_bin(buffer_load(_lab_mods[_lm].path), _lab_mods[_lm].type, _lab_mods[_lm].name);
        }
    }

    if (global.network.mode == "server") {
        var _json_struct = {
            target_level_id: global.level_id,
            level_index: 0,
            map_id: global.map_id,
            level_data: global.level_data,
            level_file: global.level_file,
            json_path: global._file_cache_json_path
        }
        // 自定义资源：发送文件名和指纹，客户端指纹一致则跳过传输
        // 只发原始相对路径，不泄露房主本地完整路径
        var _raw_json = _parse_result.data
        var _resource_fingerprints = {}

        var _sprite_rel = ""
        var _sprite_path_raw = variable_struct_get(_raw_json, "map_sprite")
        if (!is_undefined(_sprite_path_raw) && string_length(string(_sprite_path_raw)) > 0) {
            if (laboratory_path_is_relative(string(_sprite_path_raw))) {
                _sprite_rel = string(_sprite_path_raw)
                variable_struct_set(_json_struct, "map_sprite_name", _sprite_rel)
                _resource_fingerprints[$ "map_sprite"] = file_cache_compute_fingerprint(
                    laboratory_resolve_datafile_path(_sprite_rel, _json_dir))
            }
        }

        var _check_custom_music = function(_raw_json,_field) {
            var _path = variable_struct_get(_raw_json, _field)
            return !is_undefined(_path) && laboratory_path_is_relative(string(_path))
        }
        var _custom_music_names = {}
        if (_check_custom_music(_raw_json,"pre_music") || _check_custom_music(_raw_json,"elite_music") || _check_custom_music(_raw_json,"boss_music")) {
            var _ld = _json_struct[$ "level_data"]
            _ld[$ "pre_music"]   = mus_delicious_island_daytime_pre
            _ld[$ "elite_music"] = mus_delicious_island_daytime_elite
            _ld[$ "boss_music"]  = mus_delicious_island_daytime_boss
            if (_check_custom_music(_raw_json,"pre_music")) {
                var _rel = string(variable_struct_get(_raw_json, "pre_music"))
                _custom_music_names[$ "pre_music"] = _rel
                _resource_fingerprints[$ "pre_music"] = file_cache_compute_fingerprint(
                    laboratory_resolve_datafile_path(_rel, _json_dir))
            }
            if (_check_custom_music(_raw_json,"elite_music")) {
                var _rel = string(variable_struct_get(_raw_json, "elite_music"))
                _custom_music_names[$ "elite_music"] = _rel
                _resource_fingerprints[$ "elite_music"] = file_cache_compute_fingerprint(
                    laboratory_resolve_datafile_path(_rel, _json_dir))
            }
            if (_check_custom_music(_raw_json,"boss_music")) {
                var _rel = string(variable_struct_get(_raw_json, "boss_music"))
                _custom_music_names[$ "boss_music"] = _rel
                _resource_fingerprints[$ "boss_music"] = file_cache_compute_fingerprint(
                    laboratory_resolve_datafile_path(_rel, _json_dir))
            }
            variable_struct_set(_json_struct, "custom_music_names", _custom_music_names)
        }
        variable_struct_set(_json_struct, "resource_fingerprints", _resource_fingerprints)

        // 存全局，供 obj_shell 新客户端重发同步
        global._sync_map_sprite_name = _sprite_rel;
        global._sync_custom_music_names = _custom_music_names;
        global._sync_resource_fingerprints = _resource_fingerprints;

        // 本图 mod bin 清单 + 拼包：地图 bin 在前、各 mod bin 按清单顺序跟后，
        // 新客户端按 vm_bins 的 size 切段；没有 mod bin 时 _send_buf 就是地图 bin 本身、
        // json 不带 vm_bins → 老客户端照旧把整段当一个 bin 读
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
            _json_struct[$ "vm_bins"] = _vm_bins;
            // 地图 bin 已拼进 _send_buf，原 buffer 不再被引用；不删就漏一个
            //（_send_buf 那份由下一间房 VM_InitRoomEntry 开头删）
            if (buffer_exists(_bin_buf)) buffer_delete(_bin_buf);
        }
        global._sync_vm_bins = (array_length(_vm_bins) > 0) ? _vm_bins : undefined;
        global._sync_vm_bin_buf = _send_buf;

        // level_data 中的音频 ID 和精灵索引转为字符串名，兼容懒加载
        {
            var _ld = _json_struct[$ "level_data"];
            var _rev = global._audio_reverse;
            var _mf = ["pre_music", "elite_music", "boss_music"];
            for (var _fi = 0; _fi < 3; _fi++) {
                var _f = _mf[_fi];
                var _v = _ld[$ _f];
                if (!is_string(_v) && ds_map_exists(_rev, _v)) {
                    _ld[$ _f] = _rev[? _v];
                }
            }
            var _spr_rev = global._pid_reverse;
            var _spr_v = _ld[$ "level_sprite"];
            if (!is_string(_spr_v) && ds_map_exists(_spr_rev, _spr_v)) {
                _ld[$ "level_sprite"] = _spr_rev[? _spr_v];
            }
        }

        _json_struct[$ "vm_seed"] = global._sync_vm_seed;
        var _json = json_stringify(_json_struct)
		
		
        var _list = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_list); _i++) {
            send_message(_list[_i], MSG_ENTER_ROOM_READY, _json, _send_buf);
        }
    }
    global.gui_stack.to(room_ready)
    window_set_cursor(cr_arrow)
}

/// @type {function} _on_close_clicked
function set_on_close_clicked(_on_close_clicked) {
    self.state.on_close_clicked = _on_close_clicked
    return self
}

function create_widgets() {
    /// @type {Asset.GMObject.Button} 
    var _close_button = instance_create_layer(0, 0, "Float", Button)
    _close_button.set_auto_draw(false)
        .set_sprite(spr_closemenu_btn)
        .set_scale(1.9)
        .set_frames(0, 1, 2)
        .set_on_click(method(self, on_close))
    self.state.close_button = _close_button

    /// @type {Asset.GMObject.Button} 
    var start_button = instance_create_layer(0, 0, "Float", Button)
    start_button.set_auto_draw(false)
        .set_sprite(spr_create_room)
        .set_scale(2)
        .set_on_click(method(self, on_create_room))
        .set_should_correspond(method({owner: id}, function() {
            return instance_exists(owner)
        }))
    self.state.start_button = start_button
}

/// @description Events
function on_create() {
    self.state.height = sprite_get_height(spr_stage_detail) * self.state.scale
    self.state.width = sprite_get_width(spr_stage_detail) * self.state.scale
    create_widgets()
}

function on_step() {

}

function on_cleanup() {
    destroy_widgets()
}

function on_draw() {
    draw_sprite_ext(
        spr_stage_detail, 0, 
        self.state.left, self.state.top, 
        self.state.scale, self.state.scale, 
        0, c_white, 1)

    if( !is_undefined(self.state.close_button)) {
        self.state.close_button.on_draw()
    }
    if( !is_undefined(self.state.start_button)) {
        self.state.start_button.on_draw()
    }
    if (!is_undefined(self.state.custom_stage)) {
        scribble(self.state.custom_stage.name, "stage_detail_name")
            .align(fa_center, fa_center)
            .scale(1.2)
            .starting_format("font_hei_outline_4dir_black")
            .draw(self.state.left + (self.state.width / 2),  self.state.top + 114)

        scribble(self.state.custom_stage.id)
            .draw(self.state.left + 160, self.state.top + 170)

        scribble(self.state.custom_stage.author)
            .draw(self.state.left + 130, self.state.top + 228)
        scribble(self.state.custom_stage.description, "stage_detail_desc")
            .wrap(650, 360)
            .line_spacing("90%")
            .scale(1.0)
            .draw(self.state.left + 50, self.state.top + 300)

        draw_sprite_ext(spr_level_progress_icon, 0, 
            self.state.left + 60, self.state.top + self.state.height - 90, 
            0.7, 0.7, 
            0, c_white, 1)
        scribble("Lv." +  string(self.state.custom_stage.mouse_level))
            .draw(self.state.left + 90, self.state.top+self.state.height-108)
        
        draw_sprite_ext(spr_flame_small, 0, 
            self.state.left + 200, self.state.top + self.state.height - 90, 
            0.95, 0.95, 
            0, c_white, 1)
        scribble(string(self.state.custom_stage.initial_energy))
            .draw(self.state.left + 230, self.state.top+self.state.height-106)
        
        draw_sprite_ext(spr_level_progress_flag, 0, 
            self.state.left + 340, self.state.top + self.state.height - 80, 
            0.3, 0.3, 
            0, c_white, 1)
        scribble(string(self.state.custom_stage.total_waves))
            .draw(self.state.left + 370, self.state.top+self.state.height-106)
        
        draw_sprite_ext(spr_clock, 0, 
            self.state.left + 38, self.state.top + self.state.height - 70, 
            0.5, 0.5, 
            0, c_white, 1)
        scribble(string(self.state.custom_stage.time_limit) + "\"")
            .draw(self.state.left + 90, self.state.top+self.state.height-65)

        draw_sprite_ext(spr_first_wave_time, 0, 
            self.state.left + 178, self.state.top + self.state.height - 70, 
            0.5, 0.5, 
            0, c_white, 1)
        scribble(string(self.state.custom_stage.prepare_time) + "\"")
            .draw(self.state.left + 230, self.state.top+self.state.height-65)
      
    }
}

///
on_create()

