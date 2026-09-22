// ============================================================
// VM — 内存型虚拟机（无寄存器，全内存寻址）
// 内存单元: { type, value }   type: 0=int 1=float 2=字符串池索引
// ============================================================

// 操作码 (u8)
#macro VM_OP_ASSIGN  1    // dst(u32) type(u8) value(s32)
#macro VM_OP_COPY    2    // dst(u32) src(u32)
#macro VM_OP_ADD     3    // dst(u32) a(u32) b(u32)
#macro VM_OP_SUB     4
#macro VM_OP_MUL     5
#macro VM_OP_DIV     6
#macro VM_OP_MOD     7
#macro VM_OP_EQ      8    // dst = (a == b) ? 1 : 0 (type=int)
#macro VM_OP_NEQ     9
#macro VM_OP_GT      10
#macro VM_OP_GTE     11
#macro VM_OP_LT      12
#macro VM_OP_LTE     13
#macro VM_OP_CALL    14   // func_id(u16) arg_count(u8) dst(s32) [addr(s32)*]
#macro VM_OP_IF      15   // cond_addr(u32) true_ip(s32) false_ip(s32)
#macro VM_OP_JMP     16   // ip(s32)
#macro VM_OP_HALT    17

// 内存类型 (u8)
#macro VM_TYPE_INT    0
#macro VM_TYPE_FLOAT  1
#macro VM_TYPE_STRING 2

// CALL dst 特殊值
#macro VM_DST_VOID    -1

/// @function VM_Create(mem_size)
function VM_Create(mem_size = 32768) {
    var vm = {
        mem_type: array_create(mem_size, VM_TYPE_INT),
        mem_val:  array_create(mem_size, 0),
        functions: [],
        func_ret_types: [],
        strings: [],
        str_map: ds_map_create(),
        arrays: ds_map_create()   // 命名数组: 数组名 → GML 数组
    };
    return vm;
}

/// @function VM_RegisterFunction(vm, script_func, ret_type)
/// @param ret_type 返回类型，默认 VM_TYPE_INT
/// @return 函数ID (u16)
function VM_RegisterFunction(vm, script_func, ret_type = VM_TYPE_INT) {
    var func_id = array_length(vm.functions);
    array_push(vm.functions, script_func);
    array_push(vm.func_ret_types, ret_type);
    return func_id;
}


// ============================================================
// 游戏 API 函数
// ============================================================
function VM_BanCard(card_id_addr) {
    var card_id = vm_read_mem(global.__vm, card_id_addr);
    global.banned_cards_online[? card_id] = true;
}

function VM_BanGem(gem_name_addr) {
    var gem_name = vm_read_mem(global.__vm, gem_name_addr);
    if (array_get_index(global.banned_gems_online, gem_name) == -1) {
        array_push(global.banned_gems_online, gem_name);
    }
}

/// @function VM_BanAllCard()
/// @desc 禁用全部卡片（遍历全游戏卡片注册表）
function VM_BanAllCard() {
    if (!variable_global_exists("plant_registry")) return;
    var _keys = ds_map_keys_to_array(global.plant_registry);
    for (var _i = 0; _i < array_length(_keys); _i++) {
        global.banned_cards_online[? _keys[_i]] = true;
    }
}

/// @function VM_CannelBanCard(card_id)
/// @desc 解除指定卡片的禁用
function VM_CannelBanCard(card_id_addr) {
    var card_id = vm_read_mem(global.__vm, card_id_addr);
    ds_map_delete(global.banned_cards_online, card_id);
}

/// @function VM_BanWeapon()
/// @desc 禁止角色武器使用
function VM_BanWeapon() {
    global._VM_ban_weapon = true;
}

/// @function VM_BanSuperWeapon()
/// @desc 禁止角色超级武器使用
function VM_BanSuperWeapon() {
    global._VM_ban_super_weapon = true;
}

/// @function VM_BanShield()
/// @desc 禁止角色盾牌使用
function VM_BanShield() {
    global._VM_ban_shield = true;
}
/// @function VM_SetRowFeature(row, feature)
/// @param row     行，-1=所有行
/// @param feature 行属性，如 "land" / "water"
function VM_SetRowFeature(row_addr, feature_addr) {
    var row = vm_read_mem(global.__vm, row_addr);
    var feature = vm_read_mem(global.__vm, feature_addr);
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    for (var _r = _r1; _r <= _r2; _r++) {
        global.row_feature[_r] = feature;
    }
}
/// @function VM_SetEventEnabled(enabled)
/// @param enabled 0=关闭 1=开启 事件系统
function VM_SetEventEnabled(val_addr) {
    var val = vm_read_mem(global.__vm, val_addr);
    global._VM_event_enabled = (val != 0);
}
function VM_SetCardLevelCap(level_addr) {
    global._VM_card_level_cap = vm_read_mem(global.__vm, level_addr);
}
/// @function VM_SetCardShapeCap(shape)
/// @desc 设置最大转职（shape）等级限制，-1=不限制
function VM_SetCardShapeCap(shape_addr) {
    global._VM_card_shape_cap = vm_read_mem(global.__vm, shape_addr);
}
/// @function VM_SetCardSkillCap(skill)
/// @desc 设置最大技能等级限制，-1=不限制
function VM_SetCardSkillCap(skill_addr) {
    global._VM_card_skill_cap = vm_read_mem(global.__vm, skill_addr);
}
function VM_SetMaxSlots(n_addr) {
    global._VM_max_slots = vm_read_mem(global.__vm, n_addr);
}
function VM_ShellPrint(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    var _addrs = [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p];
    var _msg = "";
    for (var _n = 0; _n < 16; _n++) {
        if (!is_undefined(_addrs[_n])) _msg += string(vm_read_mem(global.__vm, _addrs[_n]));
    }
    shell_print(_msg);
}
function VM_ShowNotice(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    var _addrs = [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p];
    var _msg = "";
    for (var _n = 0; _n < 16; _n++) {
        if (!is_undefined(_addrs[_n])) _msg += string(vm_read_mem(global.__vm, _addrs[_n]));
    }
    show_notice(_msg, 120);
}
function VM_ShowNoticeDur(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    var _addrs = [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p];
    var _msg = "";
    var _dur = 120;
    for (var _n = 0; _n < 16; _n++) {
        if (!is_undefined(_addrs[_n])) {
            if (_n == 15) { _dur = vm_read_mem(global.__vm, _addrs[_n]); }
            else { _msg += string(vm_read_mem(global.__vm, _addrs[_n])); }
        }
    }
    show_notice(_msg, _dur);
}
/// @function VM_SetNoticeStyle(scale, r, g, b)
/// @param scale 公告缩放比例，-1=默认
/// @param r     公告颜色 R，-1=默认
/// @param g     公告颜色 G，-1=默认
/// @param b     公告颜色 B，-1=默认
function VM_SetNoticeStyle(scale_addr, r_addr, g_addr, b_addr) {
    global._VM_notice_scale   = vm_read_mem(global.__vm, scale_addr);
    global._VM_notice_color_r = vm_read_mem(global.__vm, r_addr);
    global._VM_notice_color_g = vm_read_mem(global.__vm, g_addr);
    global._VM_notice_color_b = vm_read_mem(global.__vm, b_addr);
}
function VM_CreatePlatform(col_addr, row_addr, width_addr, length_addr, axis_addr, distance_addr, idle_addr, spr_addr) {
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var width = vm_read_mem(global.__vm, width_addr);
    var length = vm_read_mem(global.__vm, length_addr);
    var axis = vm_read_mem(global.__vm, axis_addr);
    var distance = vm_read_mem(global.__vm, distance_addr);
    var idle = vm_read_mem(global.__vm, idle_addr);
    var spr = vm_read_mem(global.__vm, spr_addr);
    var _VM_id = ++global._VM_create_counter;
    if (global.network.mode == "client") return -_VM_id;
    var _pos = get_world_position_from_grid(col, row);
    var _bak_log = global._evt_log_enabled;   // 记录日志开关
    global._evt_log_enabled = false;          // 创建不进帧尾日志（本函数手动广播）
    var _plat = instance_create_depth(
        _pos.x - global.grid_cell_size_x / 2,
        _pos.y - global.grid_cell_size_y / 2 - 35,
        800, obj_platform
    );
    _plat.start_col = col;
    _plat.start_row = row;
    _plat.width = width;
    _plat.length = length;
    _plat.move_axis = (axis == 0) ? "y" : "x";
    _plat.move_distance = distance;
    _plat.initial_offset = 0;
    _plat.initial_idle_duration = 0;
    _plat.boundary_idle_duration = idle;
    _plat._VM_id = _VM_id;
	
    var _spr_cache;
    if (is_string(spr) && ds_map_exists(global._VM_sprite_temp_cache, spr)) {
        _spr_cache = global._VM_sprite_temp_cache[? spr];
    } else {
        _spr_cache = get_load_sprite(spr);
    }
    _plat.sprite_index = _spr_cache;
    global._evt_log_enabled = _bak_log;       // 还原

    // 网络同步
    if (global.network.mode == "server") {
        ds_map_add(global._VM_id_to_real, _VM_id, _plat.id);
        add_net_id(_plat.id);
        // 广播平台创建给所有客户端
        var _nid = global.network.map_instance_id_net_id[? _plat.id];
        var _props = {};
        // 采集 _sync_keys 白名单中的属性
        for (var _k = 0; _k < array_length(global._sync_keys); _k++) {
            var _key = global._sync_keys[_k];
            if (variable_instance_exists(_plat.id, _key)) {
                _props[$ _key] = variable_instance_get(_plat.id, _key);
            }
        }
        // sprite_index 转为字符串名，跨客户端兼容懒加载
        if (variable_struct_exists(_props, "sprite_index")) {
            var _sid = _props[$ "sprite_index"];
            if (ds_map_exists(global._pid_reverse, _sid)) {
                _props[$ "sprite_index"] = global._pid_reverse[? _sid];
            } else {
                _props[$ "sprite_index"] = sprite_get_name(_sid);
            }
        }
        var _action = [{
            op: "spawn",
            obj: "obj_platform",
            x: _plat.x,
            y: _plat.y,
            depth: _plat.depth,
            net_id: _nid,
            props: _props
        }];
        var _json = json_stringify(_action);
        var _cl = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_cl); _i++) {
            send_message(_cl[_i], MSG_EVENT_ACTIONS, _json);
        }
        // 通过 MSG_MODIFY_PROP 同步 VM_id
        var _vm_prop = {};
        _vm_prop[$ "_VM_id"] = _VM_id;
        var _vm_json = json_stringify(_vm_prop);
        for (var _i = 0; _i < array_length(_cl); _i++) {
            send_message(_cl[_i], MSG_MODIFY_PROP, _nid, _vm_json);
        }
    }
    return real(_plat.id);
}

/// @function _VM_RequestSprite(name)
/// @desc 客户端：创建空占位精灵 → 缓存 → 请求服务端发送文件 → 返回占位 ID
function _VM_RequestSprite(name) {
    var _placeholder = sprite_add(working_directory + "_ph_empty.png", 1, false, false, 0, 0);
    ds_map_add(global._VM_sprite_temp_cache, name, _placeholder);
    ds_map_add(global._pid_reverse, _placeholder, name);
    send_message(global.network.server_socket, MSG_REQUEST_FILE, name, "VM_sprite");
    return _placeholder;
}


/// @function _VM_LoadSpriteFile(name)
function _VM_LoadSpriteFile(name) {
    if (ds_map_exists(global._VM_sprite_temp_cache, name))
        return global._VM_sprite_temp_cache[? name];
    var _paths = [];
    if (variable_global_exists("_file_cache_json_path")) {
        var _prefix = global._file_cache_json_path;
        if (!string_ends_with(_prefix, "/") && !string_ends_with(_prefix, "\\")) _prefix += "/";
        array_push(_paths, _prefix + "../" + name);
    }
    // mod 卡 JSON 所在目录（mod_dir）也是候选
    var _cur_vm = global.__vm;
    if (is_struct(_cur_vm) && variable_struct_exists(_cur_vm, "mod_dir")) {
        var _md = _cur_vm[$ "mod_dir"];
        if (!string_ends_with(_md, "/") && !string_ends_with(_md, "\\")) _md += "/";
        array_push(_paths, _md + name);
    }
    show_debug_message("[VM_LoadSprite] 查找贴图: " + name + "  json_path=" + (variable_global_exists("_file_cache_json_path") ? string(global._file_cache_json_path) : "(未设置)"));
    for (var _i = 0; _i < array_length(_paths); _i++) {
        show_debug_message("[VM_LoadSprite]   尝试: " + _paths[_i] + "  exist=" + string(file_exists(_paths[_i])));
        if (file_exists(_paths[_i])) {
            var _spr = sprite_add(_paths[_i], 1, false, false, 0, 0);
            if (_spr != -1) {
                ds_map_add(global._VM_sprite_temp_cache, name, _spr);
				ds_map_add(global._pid_reverse, _spr, name); 
                return _spr;
            }
        }
    }
    // 本地未找到，客户端请求服务端发送文件
    if (variable_global_exists("network") && global.network.mode == "client")
        return _VM_RequestSprite(name);
    return -1;
}


/// @function _VM_LoadSpriteFileEx(name, frames)
function _VM_LoadSpriteFileEx(name, frames) {
    if (ds_map_exists(global._VM_sprite_temp_cache, name))
        return global._VM_sprite_temp_cache[? name];
    var _paths = [];
    if (variable_global_exists("_file_cache_json_path")) {
        var _prefix = global._file_cache_json_path;
        if (!string_ends_with(_prefix, "/") && !string_ends_with(_prefix, "\\")) _prefix += "/";
        array_push(_paths, _prefix + "../" + name);
    }
    // mod 卡 JSON 所在目录（mod_dir）也是候选
    var _cur_vm = global.__vm;
    if (is_struct(_cur_vm) && variable_struct_exists(_cur_vm, "mod_dir")) {
        var _md = _cur_vm[$ "mod_dir"];
        if (!string_ends_with(_md, "/") && !string_ends_with(_md, "\\")) _md += "/";
        array_push(_paths, _md + name);
    }
    for (var _i = 0; _i < array_length(_paths); _i++) {
        if (file_exists(_paths[_i])) {
            var _spr = sprite_add(_paths[_i], frames, false, false, 0, 0);
            if (_spr != -1) {
                ds_map_add(global._VM_sprite_temp_cache, name, _spr);
				ds_map_add(global._pid_reverse, _spr, name); 
                return _spr;
            }
        }
    }
    // 本地未找到，客户端请求服务端发送文件
    if (variable_global_exists("network") && global.network.mode == "client")
        return _VM_RequestSprite(name);
    return -1;
}

/// @function _VM_LoadSpriteFile_Ex(name, frames, xorigin, yorigin)
function _VM_LoadSpriteFile_Ex(name, frames, xorigin, yorigin) {
    if (ds_map_exists(global._VM_sprite_temp_cache, name))
        return global._VM_sprite_temp_cache[? name];
    var _paths = [];
    if (variable_global_exists("_file_cache_json_path")) {
        var _prefix = global._file_cache_json_path;
        if (!string_ends_with(_prefix, "/") && !string_ends_with(_prefix, "\\")) _prefix += "/";
        array_push(_paths, _prefix + "../" + name);
    }
    // mod 卡 JSON 所在目录（mod_dir）也是候选
    var _cur_vm = global.__vm;
    if (is_struct(_cur_vm) && variable_struct_exists(_cur_vm, "mod_dir")) {
        var _md = _cur_vm[$ "mod_dir"];
        if (!string_ends_with(_md, "/") && !string_ends_with(_md, "\\")) _md += "/";
        array_push(_paths, _md + name);
    }
    for (var _i = 0; _i < array_length(_paths); _i++) {
        if (file_exists(_paths[_i])) {
            var _spr = sprite_add(_paths[_i], frames, false, false, xorigin, yorigin);
            if (_spr != -1) {
                ds_map_add(global._VM_sprite_temp_cache, name, _spr);
                ds_map_add(global._pid_reverse, _spr, name);
                return _spr;
            }
        }
    }
    if (variable_global_exists("network") && global.network.mode == "client")
        return _VM_RequestSprite(name);
    return -1;
}

/// @function VM_LoadSprite(name_addr)
/// @return sprite index
function VM_LoadSprite(name_addr) {
    var name = vm_read_mem(global.__vm, name_addr);
    array_push(global._VM_loaded_sprite_indices, name);
    return _VM_LoadSpriteFile(name);
}

/// @function VM_LoadSpriteFrames(name, frames)
/// @param name   贴图文件名
/// @param frames 帧数（自动均分切割）
/// @return sprite index
function VM_LoadSpriteFrames(name_addr, frames_addr) {
	var _name = vm_read_mem(global.__vm, name_addr);
	array_push(global._VM_loaded_sprite_indices, _name);
	var _frames = vm_read_mem(global.__vm, frames_addr);
	if (is_undefined(_frames) || _frames <= 0) _frames = 1;
	return _VM_LoadSpriteFileEx(_name, _frames);
}

/// @function VM_LoadSpriteFrames_Ex(name, frames, xorigin, yorigin)
/// @param name     贴图文件名
/// @param frames   帧数（自动均分切割）
/// @param xorigin  原点 X，-1=默认0
/// @param yorigin  原点 Y，-1=默认0
/// @return sprite index
function VM_LoadSpriteFrames_Ex(name_addr, frames_addr, xorigin_addr, yorigin_addr) {
	var _name = vm_read_mem(global.__vm, name_addr);
	array_push(global._VM_loaded_sprite_indices, _name);
	var _frames = vm_read_mem(global.__vm, frames_addr);
	var _x = vm_read_mem(global.__vm, xorigin_addr);
	var _y = vm_read_mem(global.__vm, yorigin_addr);
	if (is_undefined(_frames) || _frames <= 0) _frames = 1;
	if (is_undefined(_x) || _x == -1) _x = 0;
	if (is_undefined(_y) || _y == -1) _y = 0;
	return _VM_LoadSpriteFile_Ex(_name, _frames, _x, _y);
}

/// @function VM_LoadSpritePerm(name, frames)
/// @desc 加载贴图到永久缓存，不被 bin 重载清理
/// @param name   贴图文件名
/// @param frames 帧数 (-1=默认1)
/// @return sprite index
function VM_LoadSpritePerm(name_addr, frames_addr) {
	var _name = vm_read_mem(global.__vm, name_addr);
	var _frames = vm_read_mem(global.__vm, frames_addr);
	if (is_undefined(_frames) || _frames <= 0) _frames = 1;
	if (ds_map_exists(global._VM_sprite_cache, _name))
		return global._VM_sprite_cache[? _name];
	var _spr = sprite_add(_name, _frames, false, false, 0, 0);
	if (_spr == -1) return -1;
	ds_map_add(global._VM_sprite_cache, _name, _spr);
	return _spr;
}

/// @function VM_FreeSpritePerm(name)
/// @desc 释放永久缓存中的贴图
/// @param name 贴图文件名
function VM_FreeSpritePerm(name_addr) {
	var _name = vm_read_mem(global.__vm, name_addr);
	if (!ds_map_exists(global._VM_sprite_cache, _name)) return;
	var _spr = global._VM_sprite_cache[? _name];
	if (sprite_exists(_spr)) sprite_delete(_spr);
	ds_map_delete(global._VM_sprite_cache, _name);
	ds_map_delete(global._pid_reverse, _spr);
}

/// @function VM_LoadSpritePerm_Ex(name, frames, xorigin, yorigin)
/// @desc 加载贴图到永久缓存并指定原点，不被 bin 重载清理
/// @param name     贴图文件名
/// @param frames   帧数 (-1=默认1)
/// @param xorigin  原点 X，-1=默认0
/// @param yorigin  原点 Y，-1=默认0
/// @return sprite index
function VM_LoadSpritePerm_Ex(name_addr, frames_addr, xorigin_addr, yorigin_addr) {
	var _name = vm_read_mem(global.__vm, name_addr);
	var _frames = vm_read_mem(global.__vm, frames_addr);
	var _x = vm_read_mem(global.__vm, xorigin_addr);
	var _y = vm_read_mem(global.__vm, yorigin_addr);
	if (is_undefined(_frames) || _frames <= 0) _frames = 1;
	if (is_undefined(_x) || _x == -1) _x = 0;
	if (is_undefined(_y) || _y == -1) _y = 0;
	if (ds_map_exists(global._VM_sprite_cache, _name))
		return global._VM_sprite_cache[? _name];
	var _paths = [];
	if (variable_global_exists("_file_cache_json_path")) {
		var _prefix = global._file_cache_json_path;
		if (!string_ends_with(_prefix, "/") && !string_ends_with(_prefix, "\\")) _prefix += "/";
		array_push(_paths, _prefix + "../" + _name);
	}
	// mod 卡 JSON 所在目录（mod_dir）也是候选
	var _cur_vm = global.__vm;
	if (is_struct(_cur_vm) && variable_struct_exists(_cur_vm, "mod_dir")) {
		var _md = _cur_vm[$ "mod_dir"];
		if (!string_ends_with(_md, "/") && !string_ends_with(_md, "\\")) _md += "/";
		array_push(_paths, _md + _name);
	}
	array_push(_paths, _name);   // 裸文件名兜底（与原 VM_LoadSpritePerm 行为一致）
	for (var _i = 0; _i < array_length(_paths); _i++) {
		if (file_exists(_paths[_i])) {
			var _spr = sprite_add(_paths[_i], _frames, false, false, _x, _y);
			if (_spr != -1) {
				ds_map_add(global._VM_sprite_cache, _name, _spr);
				ds_map_add(global._pid_reverse, _spr, _name);
				return _spr;
			}
		}
	}
	return -1;
}

/// @function VM_SetShovelFlameRate(rate)
/// @desc 设置铲子铲卡返还火苗系数：-1=用原逻辑（铲子槽 flame_rate），0~1=直接用该系数
/// @param rate 系数，-1 或 0~1
function VM_SetShovelFlameRate(rate_addr) {
	global._VM_shovel_flame_rate = vm_read_mem(global.__vm, rate_addr);
}

/// @function VM_GetLoadedSpriteName(index)
/// @param index  加载序号（0 开始）
/// @return 贴图名（字符串），越界返回 ""
function VM_GetLoadedSpriteName(index_addr) {
    var index = vm_read_mem(global.__vm, index_addr);
    if (index < 0 || index >= array_length(global._VM_loaded_sprite_indices)) return "";
    return global._VM_loaded_sprite_indices[index];
}

/// @function VM_SetDrawSlot(slot, sprite, x, y, alpha)
/// @param slot   槽位索引 0~63
/// @param sprite  贴图名(string)或精灵ID(int)，空/""/-1/noone 时清除
/// @param x      X 坐标
/// @param y      Y 坐标
/// @param alpha  透明度 0~1
/// @desc 旋转/缩放使用默认值（angle=0, xscale=1, yscale=1），需要控制请用 VM_SetDrawSlotEx
function VM_SetDrawSlot(slot_addr, sprite_addr, x_addr, y_addr, alpha_addr) {
    var slot = vm_read_mem(global.__vm, slot_addr);
    var sprite = vm_read_mem(global.__vm, sprite_addr);
    var _x = vm_read_mem(global.__vm, x_addr);
    var _y = vm_read_mem(global.__vm, y_addr);
    var alpha = vm_read_mem(global.__vm, alpha_addr);
    _vm_set_draw_slot(global.map_draw_slots, slot, sprite, _x, _y, alpha, 0, 1, 1);
}

/// @function VM_SetDrawSlotEx(slot, sprite, x, y, alpha, angle, xscale, yscale)
/// @param slot   槽位索引 0~63
/// @param sprite  贴图名(string)或精灵ID(int)，空/""/-1/noone 时清除
/// @param x      X 坐标
/// @param y      Y 坐标
/// @param alpha  透明度 0~1
/// @param angle  旋转角度（度），undefined/-1=默认0
/// @param xscale 横向缩放，undefined/-1=默认1
/// @param yscale 纵向缩放，undefined/-1=默认1
function VM_SetDrawSlotEx(slot_addr, sprite_addr, x_addr, y_addr, alpha_addr, angle_addr, xscale_addr, yscale_addr) {
    var slot = vm_read_mem(global.__vm, slot_addr);
    var sprite = vm_read_mem(global.__vm, sprite_addr);
    var _x = vm_read_mem(global.__vm, x_addr);
    var _y = vm_read_mem(global.__vm, y_addr);
    var alpha = vm_read_mem(global.__vm, alpha_addr);
    var _angle = vm_read_mem(global.__vm, angle_addr);
    var _xscale = vm_read_mem(global.__vm, xscale_addr);
    var _yscale = vm_read_mem(global.__vm, yscale_addr);
    if (is_undefined(_angle) || _angle == -1) _angle = 0;
    if (is_undefined(_xscale) || _xscale == -1) _xscale = 1;
    if (is_undefined(_yscale) || _yscale == -1) _yscale = 1;
    _vm_set_draw_slot(global.map_draw_slots, slot, sprite, _x, _y, alpha, _angle, _xscale, _yscale);
}

/// @function VM_SetDrawSlot_front(slot, sprite, x, y, alpha)
/// @param slot   槽位索引 0~63
/// @param sprite  贴图名(string)或精灵ID(int)，空/""/-1/noone 时清除
/// @param x      X 坐标
/// @param y      Y 坐标
/// @param alpha  透明度 0~1
/// @desc 和 VM_SetDrawSlot 一致，但绘制在 obj_flame_manager（depth=-900）上，显示在火焰UI后面；旋转/缩放用默认值，需要控制请用 VM_SetDrawSlotEx_front
function VM_SetDrawSlot_front(slot_addr, sprite_addr, x_addr, y_addr, alpha_addr) {
    var slot = vm_read_mem(global.__vm, slot_addr);
    var sprite = vm_read_mem(global.__vm, sprite_addr);
    var _x = vm_read_mem(global.__vm, x_addr);
    var _y = vm_read_mem(global.__vm, y_addr);
    var alpha = vm_read_mem(global.__vm, alpha_addr);
    _vm_set_draw_slot(global.map_draw_slots_front, slot, sprite, _x, _y, alpha, 0, 1, 1);
}

/// @function VM_SetDrawSlotEx_front(slot, sprite, x, y, alpha, angle, xscale, yscale)
/// @param slot   槽位索引 0~63
/// @param sprite  贴图名(string)或精灵ID(int)，空/""/-1/noone 时清除
/// @param x      X 坐标
/// @param y      Y 坐标
/// @param alpha  透明度 0~1
/// @param angle  旋转角度（度），undefined/-1=默认0
/// @param xscale 横向缩放，undefined/-1=默认1
/// @param yscale 纵向缩放，undefined/-1=默认1
/// @desc 和 VM_SetDrawSlotEx 一致，但绘制在 obj_flame_manager（depth=-900）上，显示在火焰UI后面
function VM_SetDrawSlotEx_front(slot_addr, sprite_addr, x_addr, y_addr, alpha_addr, angle_addr, xscale_addr, yscale_addr) {
    var slot = vm_read_mem(global.__vm, slot_addr);
    var sprite = vm_read_mem(global.__vm, sprite_addr);
    var _x = vm_read_mem(global.__vm, x_addr);
    var _y = vm_read_mem(global.__vm, y_addr);
    var alpha = vm_read_mem(global.__vm, alpha_addr);
    var _angle = vm_read_mem(global.__vm, angle_addr);
    var _xscale = vm_read_mem(global.__vm, xscale_addr);
    var _yscale = vm_read_mem(global.__vm, yscale_addr);
    if (is_undefined(_angle) || _angle == -1) _angle = 0;
    if (is_undefined(_xscale) || _xscale == -1) _xscale = 1;
    if (is_undefined(_yscale) || _yscale == -1) _yscale = 1;
    _vm_set_draw_slot(global.map_draw_slots_front, slot, sprite, _x, _y, alpha, _angle, _xscale, _yscale);
}

/// @function _vm_set_draw_slot(_slots, slot, sprite, x, y, alpha, angle, xscale, yscale)
/// @desc 绘制槽写入的内部实现：设置/清除槽位，并写旋转缩放字段（清除时复位为默认）
function _vm_set_draw_slot(_slots, slot, sprite, _x, _y, alpha, _angle, _xscale, _yscale) {
    if (slot < 0 || slot >= 64) return;
    if (sprite == "" || sprite == -1 || sprite == noone) {
        _slots[slot].sprite = noone;
        _slots[slot].x = 0;
        _slots[slot].y = 0;
        _slots[slot].alpha = 1;
        _slots[slot].image_angle = 0;
        _slots[slot].image_xscale = 1;
        _slots[slot].image_yscale = 1;
        return;
    }
    var _spr;
    if (is_string(sprite)) {
        _spr = ds_map_exists(global._VM_sprite_temp_cache, sprite)
            ? global._VM_sprite_temp_cache[? sprite]
            : get_load_sprite(sprite);
    } else {
        _spr = sprite;
    }
    _slots[slot].sprite = _spr;
    _slots[slot].x = _x;
    _slots[slot].y = _y;
    _slots[slot].alpha = alpha;
    _slots[slot].image_angle = _angle;
    _slots[slot].image_xscale = _xscale;
    _slots[slot].image_yscale = _yscale;
}

/// @function VM_SetMapBackground(name, step)
/// @param name  贴图名（内置精灵名或外部文件名）
/// @param step  渐变步长，0~1 浮点数
function VM_SetMapBackground(name_addr, step_addr) {
    var name = vm_read_mem(global.__vm, name_addr);
    var step = vm_read_mem(global.__vm, step_addr);
    var _spr = -1;
    if (ds_map_exists(global._sprite_cache, name)) {
        _spr = global._sprite_cache[? name];
    } else {
        _spr = get_load_sprite(name);
    }
    global.map_sprite_target = _spr;
    global.map_fade_alpha = 0;
    if (step > 0) global.map_fade_step = step;
}

/// @function VM_GameWin()
/// @desc 触发胜利。客户端跳过，服务端弹出胜利界面并广播给客户端
function VM_GameWin() {
    if (global.network.mode == "client" && global._VM_sync_exec) return;
    if (global.network.mode == "server") {
        var _cl = global.network.connected_clients;
        for (var i = 0; i < array_length(_cl); i++)
            send_message(_cl[i], MSG_GAME_OVER, 1);
    }
    global.is_paused = true;
    global.game_over = true;
    var inst = instance_create_depth(room_width / 2, room_height / 2, -3001, obj_game_over);
    inst.sprite_index = spr_win;
    audio_play_sound(snd_win, 0, 0);
}

/// @function VM_GameLose()
/// @desc 触发失败。客户端跳过，服务端弹出失败界面并广播给客户端
function VM_GameLose() {
    if (global.network.mode == "client" && global._VM_sync_exec) return;
    if (global.network.mode == "server") {
        var _cl = global.network.connected_clients;
        for (var i = 0; i < array_length(_cl); i++)
            send_message(_cl[i], MSG_GAME_OVER, 0);
    }
    global.is_paused = true;
    global.game_over = true;
    instance_create_depth(room_width / 2, room_height / 2, -3001, obj_game_over);
    audio_play_sound(snd_lose, 0, 0);
}

/// @function VM_SpawnCats(enable)
/// @param enable  是否生成初始一排猫，1=开启，0=关闭（默认关闭）
function VM_SpawnCats(enable_addr) {
    var enable = vm_read_mem(global.__vm, enable_addr);
    global._VM_spawn_cats = (enable != 0);
}

/// @function VM_SetTerrain(col, row, type)
/// @param col  列，-1=所有列
/// @param row  行，-1=所有行
/// @param type 地形: "normal"/"water"/"obstacle"
function VM_SetTerrain(col_addr, row_addr, type_addr) {
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var type = vm_read_mem(global.__vm, type_addr);
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    for (var _r = _r1; _r <= _r2; _r++) {
        for (var _c = _c1; _c <= _c2; _c++) {
            global.grid_terrains[_r][_c].type = type;
        }
    }
}

/// @function VM_GetFlame()
/// @return 当前火苗数量
function VM_GetFlame() {
    return real(global.flame);
}

/// @function VM_ClearPlants(col, row)
/// @param col  列，-1=所有列
/// @param row  行，-1=所有行
function VM_ClearPlants(col_addr, row_addr) {
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    for (var _r = _r1; _r <= _r2; _r++) {
        for (var _c = _c1; _c <= _c2; _c++) {
            var _list = ds_grid_get(global.grid_plants, _c, _r);
            for (var _i = ds_list_size(_list) - 1; _i >= 0; _i--) {
                var _plant = ds_list_find_value(_list, _i);
                if (instance_exists(_plant) && _plant.plant_id == "player") continue;
                card_destroyed(_plant);
                instance_destroy(_plant);
            }
        }
    }
}

/// @function VM_ClearPlantsByType(name)
/// @param name 卡片 plant_id，-1=清除所有植物（跳过角色）
function VM_ClearPlantsByType(name_addr) {
    var name = vm_read_mem(global.__vm, name_addr);
    if (name == -1) {
        with (obj_card_parent) {
            if (plant_id == "player") continue;
            card_destroyed(id);
            instance_destroy();
        }
        return;
    }
    var _card_data = deck_get_card_data(name, 0);
    if (is_undefined(_card_data)) return;
    var _obj = _card_data[? "obj"];
    with (_obj) {
        if (plant_id == "player") continue;
        card_destroyed(id);
        instance_destroy();
    }
}

/// @function VM_SetCardProp(col, row, name, prop, value)
/// @param col   列，-1=所有列
/// @param row   行，-1=所有行
/// @param name  卡片 plant_id，"all"=所有卡片（跳过角色）
/// @param prop  属性名
/// @param value 值
function VM_SetCardProp(col_addr, row_addr, name_addr, prop_addr, value_addr) {
    var col = vm_arg(col_addr);
    var row = vm_arg(row_addr);
    var name = vm_arg(name_addr);
    var prop = vm_arg(prop_addr);
    var value = vm_arg(value_addr);
    if (global.network.mode == "client" && global._VM_sync_exec) return;
    var _all = (name == "all");
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    for (var _r = _r1; _r <= _r2; _r++) {
        for (var _c = _c1; _c <= _c2; _c++) {
            var _list = ds_grid_get(global.grid_plants, _c, _r);
            for (var _i = ds_list_size(_list) - 1; _i >= 0; _i--) {
                var _plant = ds_list_find_value(_list, _i);
                if (!instance_exists(_plant)) continue;
                if (_plant.plant_id == "player") continue;
                if (!_all && _plant.plant_id != name) continue;
                variable_instance_set(_plant, prop, value);
            }
        }
    }
    if (global.network.mode == "server") {
        var _msg = json_stringify({hook: "call", func: "VM_SetCardProp", args: [col, row, name, prop, value]});
        var _cl = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_cl); _i++)
            send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
    }
}

/// @function VM_SetEnemyProp(name, prop, value)
/// @param name  敌人类型（mouse_id），"all"=所有敌人（跳过BOSS）
/// @param prop  属性名
/// @param value 值
function VM_SetEnemyProp(name_addr, prop_addr, value_addr) {
    var name = vm_arg(name_addr);
    var prop = vm_arg(prop_addr);
    var value = vm_arg(value_addr);
    if (global.network.mode == "client" && global._VM_sync_exec) return;
    var _all = (name == "all");
    if (_all) {
        with (obj_enemy_parent) {
            if (is_boss) continue;
            variable_instance_set(id, prop, value);
        }
    } else {
        var _info = global.enemy_map[? name];
        if (!is_undefined(_info)) {
            var _obj = _info._obj;
            with (_obj) {
                if (is_boss) continue;
                variable_instance_set(id, prop, value);
            }
        }
    }
    if (global.network.mode == "server") {
        var _msg = json_stringify({hook: "call", func: "VM_SetEnemyProp", args: [name, prop, value]});
        var _cl = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_cl); _i++)
            send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
    }
}

/// @function VM_WakePlants(col, row)
/// @param col 列，-1=所有列
/// @param row 行，-1=所有行
/// @description 唤醒指定格子内处于睡眠的卡片
function VM_WakePlants(col_addr, row_addr) {
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    for (var _r = _r1; _r <= _r2; _r++) {
        for (var _c = _c1; _c <= _c2; _c++) {
            var _list = ds_grid_get(global.grid_plants, _c, _r);
            for (var _i = ds_list_size(_list) - 1; _i >= 0; _i--) {
                var _plant = ds_list_find_value(_list, _i);
                if (!instance_exists(_plant)) continue;
                if (_plant.plant_id == "player") continue;
                if (_plant.state == CARD_STATE.SLEEP) {
                    _plant.awake_buff_timer = 1;
                }
            }
        }
    }
}

/// @function VM_ClearMapObjects(col, row, obj_name)
/// @param col      列，-1=所有列
/// @param row      行，-1=所有行
/// @param obj_name 对象名(字符串)，"all"=删除全部三种地图对象
function VM_ClearMapObjects(col_addr, row_addr, obj_name_addr) {
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var obj_name = vm_read_mem(global.__vm, obj_name_addr);
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    // 收集要删除的对象索引
    var _targets;
    if (obj_name == "all") {
        _targets = [obj_obstacle, obj_wind_tunnel, obj_lava, obj_barrier, obj_fog, obj_cloud];
    } else {
        if (!string_starts_with(obj_name, "obj_"))
            obj_name = "obj_" + obj_name;
        _targets = [asset_get_index(obj_name)];
    }
	
	var _all = (col == -1 && row == -1);
	for (var _t = 0; _t < array_length(_targets); _t++) {
		var _obj = _targets[_t];
		if (_obj < 0) continue;
		with (_obj) {
		    if (_all || (_r1 <= row && row <= _r2 && _c1 <= col && col <= _c2)) {
		        instance_destroy();
		    }
		}
	}
}

/// @function VM_SetFlame(amount)
/// @param amount 设置的火苗数
function VM_SetFlame(amount_addr) {
    global.flame = vm_read_mem(global.__vm, amount_addr);
}

/// @function VM_Random(min, max)
/// @return [min, max] 之间随机整数（基于VM内部种子，联机两端同种子同序列）
function VM_Random(min_addr, max_addr) {
    var _min = vm_read_mem(global.__vm, min_addr);
    var _max = vm_read_mem(global.__vm, max_addr);
    var _range = _max - _min + 1;
    if (_range <= 0) return _min;
    // xorshift32：状态只属于本VM，与游戏全局随机互不影响
    var _vm = global.__vm;
    var _s = _vm.rng_state;
    if (_s == 0) _s = 0x9E3779B9;
    _s ^= (_s << 13) & 0xFFFFFFFF;
    _s &= 0xFFFFFFFF;
    _s ^= _s >> 17;
    _s &= 0xFFFFFFFF;
    _s ^= (_s << 5) & 0xFFFFFFFF;
    _s &= 0xFFFFFFFF;
    _vm.rng_state = _s;
    return _min + (_s mod _range);
}

/// @function VM_ClientWrapId(real_id)
/// @description 客户端将真实实例 ID 反转为 -VM_id，保证 VM 内一致
function VM_ClientWrapId(real_id) {
    if (global.network.mode != "client" || real_id < 0) return real_id;
    var _vmid = ds_map_find_value(global._VM_real_to_vm_id, real_id);
    if (is_undefined(_vmid)) return real_id;
    return -_vmid;
}

/// @function VM_GetLastIdlePlatform()
/// @return 刚结束 idle 的平台实例 ID
function VM_GetLastIdlePlatform() {
    return VM_ClientWrapId(real(global._VM_last_idle_platform));
}

/// @function VM_SetPlatformParams(plat_id, axis, distance, idle, direction)
/// @param plat_id   平台实例 ID
/// @param axis      移动轴: 0=上下 1=左右
/// @param distance  移动距离（格）
/// @param idle      边界停顿时间（帧）
/// @param direction 移动方向: 1=正向 -1=反向
/// @description 以当前位置为新起点重新设定移动参数
function VM_SetPlatformParams(plat_id_addr, axis_addr, distance_addr, idle_addr, direction_addr) {
    var plat_id = vm_read_mem(global.__vm, plat_id_addr);
    var axis = vm_read_mem(global.__vm, axis_addr);
    var distance = vm_read_mem(global.__vm, distance_addr);
    var idle = vm_read_mem(global.__vm, idle_addr);
    var _direction = vm_read_mem(global.__vm, direction_addr);
    if (plat_id < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -plat_id);
        if (is_undefined(_real)) return;
        plat_id = _real;
    }
    var _plat = plat_id;
    if (!instance_exists(_plat) || _plat.object_index != obj_platform) return;
    // 以当前位置为新起点
    var _is_x = (_plat.move_axis == "x");
    _plat.start_col += (_is_x ? _plat.current_offset : 0);
    _plat.start_row += (!_is_x ? _plat.current_offset : 0);
    _plat.current_offset = 0;
    // 设置新参数
    _plat.move_axis = (axis == 0) ? "y" : "x";
    _plat.move_distance = distance;
    _plat.boundary_idle_duration = idle;
    _plat.move_direction = _direction;
}

/// @function VM_RefreshPlatformSnapshots()
/// @return 0
/// @description 已改用引用计数，不再需要快照
function VM_RefreshPlatformSnapshots() {
	return 0;
}

/// @function VM_GetWave()
/// @return 当前波次
function VM_GetWave() {
    return real(global._VM_prev_wave);
}

/// @function VM_GetSubwave()
/// @return 当前子波
function VM_GetSubwave() {
    return real(global._VM_prev_subwave);
}

/// @function VM_GetLastBoss()
/// @return 最新创建的 BOSS 实例 ID
function VM_GetLastBoss() {
    return VM_ClientWrapId(real(global._VM_last_boss));
}

/// @function VM_GetLastCreatedEnemy()
/// @return 最新创建的敌人实例 ID
function VM_GetLastCreatedEnemy() {
    return VM_ClientWrapId(real(global._VM_last_created_enemy));
}

/// @function VM_GetLastKilledEnemy()
/// @return 最新死亡的敌人实例 ID
function VM_GetLastKilledEnemy() {
    return VM_ClientWrapId(real(global._VM_last_killed_enemy));
}

/// @function VM_GetLastCreatedCard()
/// @return 最新创建的卡片实例 ID
function VM_GetLastCreatedCard() {
    return VM_ClientWrapId(real(global._VM_last_created_card));
}

/// @function VM_GetLastDestroyedCard()
/// @return 最新销毁的卡片实例 ID
function VM_GetLastDestroyedCard() {
    return VM_ClientWrapId(real(global._VM_last_destroyed_card));
}

/// @function VM_GetLastBossStateChangeId()
/// @return 最新改变状态的 BOSS 实例 ID
function VM_GetLastBossStateChangeId() {
    return VM_ClientWrapId(real(global._VM_last_boss_state_change_id));
}

/// @function VM_GetLastBossOldState()
/// @return 最新改变状态的 BOSS 的旧状态
function VM_GetLastBossOldState() {
    return real(global._VM_last_boss_old_state);
}

/// @function VM_GetLastBossNewState()
/// @return 最新改变状态的 BOSS 的新状态
function VM_GetLastBossNewState() {
    return real(global._VM_last_boss_new_state);
}

/// @function VM_GetMouseX()
/// @return 鼠标当前 X 坐标
function VM_GetMouseX() {
	return mouse_x;
}

/// @function VM_GetMouseY()
/// @return 鼠标当前 Y 坐标
function VM_GetMouseY() {
	return mouse_y;
}

/// @function VM_GetMouseCol()
/// @return 鼠标所在网格列
function VM_GetMouseCol() {
	var _gp = get_grid_position_from_world(mouse_x, mouse_y, true);
	return _gp.col;
}

/// @function VM_GetMouseRow()
/// @return 鼠标所在网格行
function VM_GetMouseRow() {
	var _gp = get_grid_position_from_world(mouse_x, mouse_y, true);
	return _gp.row;
}

/// @function VM_GetTerrain(col, row)
/// @return 0=normal 1=water 2=obstacle -1=超出范围
function VM_GetTerrain(col_addr, row_addr) {
	var _col = vm_read_mem(global.__vm, col_addr);
	var _row = vm_read_mem(global.__vm, row_addr);
	if (_row < 0 || _row >= global.grid_rows || _col < 0 || _col >= global.grid_cols) return -1;
	var _t = global.grid_terrains[_row][_col].type;
	if (_t == "water") return 1;
	if (_t == "obstacle") return 2;
	return 0;
}

/// @function VM_GetMousePressed(button)
/// @param button 1=左键 2=右键 3=中键
/// @return 1=按下 0=未按下
function VM_GetMousePressed(btn_addr) {
	var _btn = vm_read_mem(global.__vm, btn_addr);
	var _args = [];
	if (_btn == 1) return mouse_check_button(mb_left) ? 1 : 0;
	if (_btn == 2) return mouse_check_button(mb_right) ? 1 : 0;
	if (_btn == 3) return mouse_check_button(mb_middle) ? 1 : 0;
	return 0;
}

/// @function VM_GetKeyDown(key)
/// @param key 按键名: 单字符="A".."Z"/"0".."9", 特殊="space"/"enter"/"escape"/"tab"/"shift"/"ctrl"/"alt"/"up"/"down"/"left"/"right"
/// @return 1=按住 0=松开
function VM_GetKeyDown(key_addr) {
	var _key = vm_read_mem(global.__vm, key_addr);
	var _code = _vm_key_name_to_code(_key);
	return _code >= 0 ? (keyboard_check(_code) ? 1 : 0) : 0;
}

/// @function VM_GetKeyPressed(key)
/// @param key 同上，字符串按键名
/// @return 1=刚按下(单帧) 0=未按下
function VM_GetKeyPressed(key_addr) {
	var _key = vm_read_mem(global.__vm, key_addr);
	var _code = _vm_key_name_to_code(_key);
	return _code >= 0 ? (keyboard_check_pressed(_code) ? 1 : 0) : 0;
}

function _vm_key_name_to_code(_name) {
	var _n = string_lower(_name);
	switch (_n) {
		case "space":  return vk_space;
		case "enter":  return vk_enter;
		case "escape": return vk_escape;
		case "tab":    return vk_tab;
		case "shift":  return vk_shift;
		case "ctrl":   return vk_control;
		case "alt":    return vk_alt;
		case "up":     return vk_up;
		case "down":   return vk_down;
		case "left":   return vk_left;
		case "right":  return vk_right;
		case "backspace": return vk_backspace;
		case "delete": return vk_delete;
		case "home":   return vk_home;
		case "end":    return vk_end;
		case "pageup": return vk_pageup;
		case "pagedown": return vk_pagedown;
		case "f1": return vk_f1;
		case "f2": return vk_f2;
		case "f3": return vk_f3;
		case "f4": return vk_f4;
		case "f5": return vk_f5;
		case "f6": return vk_f6;
		case "f7": return vk_f7;
		case "f8": return vk_f8;
		case "f9": return vk_f9;
		case "f10": return vk_f10;
		case "f11": return vk_f11;
		case "f12": return vk_f12;
	}
	if (string_length(_name) == 1) return ord(string_upper(_name));
	shell_print("[VM] 未知按键名: " + string(_name));
	return -1;
}

/// @function VM_GetEnemyCount()
/// @return 场上敌人数量
function VM_GetEnemyCount() {
	return instance_number(obj_enemy_parent);
}

/// @function VM_GetPlantCount()
/// @return 场上植物数量
function VM_GetPlantCount() {
	return instance_number(obj_card_parent);
}

/// @function VM_GetPlantCountAt(col, row, type)
/// @param type "all" 统计全部, 否则按 plant_id 筛选
/// @return 该格子符合条件的卡片数量
function VM_GetPlantCountAt(col_addr, row_addr, type_addr) {
	var _col = vm_read_mem(global.__vm, col_addr);
	var _row = vm_read_mem(global.__vm, row_addr);
	var _type = vm_read_mem(global.__vm, type_addr);
	var _all_cols = (_col == -1);
	var _all_rows = (_row == -1);
	// 全场统计快速通道
	if (_all_cols && _all_rows && _type == "all") return instance_number(obj_card_parent);
	if (_all_cols && _all_rows) {
		var _cnt = 0;
		with (obj_card_parent) {
			if (_type == "all" || plant_id == _type) _cnt++;
		}
		return _cnt;
	}
	var _c1 = _all_cols ? 0 : clamp(_col, 0, global.grid_cols - 1);
	var _c2 = _all_cols ? global.grid_cols - 1 : _c1;
	var _r1 = _all_rows ? 0 : clamp(_row, 0, global.grid_rows - 1);
	var _r2 = _all_rows ? global.grid_rows - 1 : _r1;
	var _count = 0;
	for (var _c = _c1; _c <= _c2; _c++) {
		for (var _r = _r1; _r <= _r2; _r++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			for (var i = 0; i < ds_list_size(_list); i++) {
				var _p = ds_list_find_value(_list, i);
				if (!instance_exists(_p)) continue;
				if (_type == "all" || _p.plant_id == _type) _count++;
			}
		}
	}
	return _count;
}


/// @function VM_GetPlantAt(col, row, layer)
/// @param col   列，-1 查所有列
/// @param row   行，-1 查所有行
/// @param layer 层级名: "normal" "shield_inner" "lilypad" "shield_outer" "coffee"
/// @return 该格子指定层第一个植物实例 ID，没找到返回 -1
function VM_GetPlantAt(col_addr, row_addr, layer_addr) {
	var _col = vm_read_mem(global.__vm, col_addr);
	var _row = vm_read_mem(global.__vm, row_addr);
	var _layer = vm_read_mem(global.__vm, layer_addr);
	var _all_cols = (_col == -1);
	var _all_rows = (_row == -1);
	var _c1 = _all_cols ? 0 : clamp(_col, 0, global.grid_cols - 1);
	var _c2 = _all_cols ? global.grid_cols - 1 : _c1;
	var _r1 = _all_rows ? 0 : clamp(_row, 0, global.grid_rows - 1);
	var _r2 = _all_rows ? global.grid_rows - 1 : _r1;
	for (var _c = _c1; _c <= _c2; _c++) {
		for (var _r = _r1; _r <= _r2; _r++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			for (var _i = 0; _i < ds_list_size(_list); _i++) {
				var _p = ds_list_find_value(_list, _i);
				if (!instance_exists(_p)) continue;
				if (_layer == "all" || _p.plant_type == _layer) {
					var _vmid = _p.id;
					if (is_undefined(_vmid)) _vmid = real(_p);
					return VM_ClientWrapId(_vmid);
				}
			}
		}
	}
	return -1;
}

/// @function update_plant_bindings(_p)
/// @description 植物移动后更新其绑定的标记对象（星星/睡眠/水花）和玩家武器位置
function update_plant_bindings(_p) {
	if (!instance_exists(_p)) return;
	if (variable_struct_exists(_p, "banding_star_obj")&&instance_exists(_p.banding_star_obj)) {
		_p.banding_star_obj.x = _p.x;
		_p.banding_star_obj.y = _p.y - 5;
		_p.banding_star_obj.depth = _p.depth - 1;
	}
	if (variable_struct_exists(_p, "banding_sleep_obj")&&instance_exists(_p.banding_sleep_obj)) {
		_p.banding_sleep_obj.x = _p.x - 15;
		_p.banding_sleep_obj.y = _p.y - 20;
		_p.banding_sleep_obj.depth = _p.depth - 1;
	}
	if (variable_struct_exists(_p, "banding_water_obj")&&instance_exists(_p.banding_water_obj)) {
		_p.banding_water_obj.x = _p.x;
		_p.banding_water_obj.y = _p.y;
		_p.banding_water_obj.depth = _p.depth + 5;
	}
	// 玩家绑定的武器/护盾跟着走
	if (ds_map_exists(global._move_instance_map, _p.id)) {
		var _list = global._move_instance_map[? _p.id];
		for (var _i = ds_list_size(_list) - 1; _i >= 0; _i--) {
			var _ins = _list[| _i];
			if (!instance_exists(_ins)) {
				ds_list_delete(_list, _i);
				continue;
			}
			if (!variable_instance_exists(_ins, "parent_player") || _ins.parent_player != _p) continue;
			var grid_pos = get_grid_position_from_world(_p.x,_p.y)
			_p.grid_col = grid_pos.col
			_p.grid_row = grid_pos.row
			_ins.x = _p.x;
			_ins.y = _p.y;
			if (_ins.object_index == obj_player_shield) {
				_ins.depth = _p.depth;
			} else {
				_ins.x -= 10;
				_ins.y -= 100;
				_ins.depth = _p.depth - 1;
			}
			_ins.grid_col = _p.grid_col;
			_ins.grid_row = _p.grid_row;
		}
	}
}

/// @function VM_SwapPlants(col1, row1, col2, row2)
/// @description 交换两个格子上所有植物的位置
/// @param col1 第一个格子的列
/// @param row1 第一个格子的行
/// @param col2 第二个格子的列
/// @param row2 第二个格子的行
function VM_SwapPlants(col1_addr, row1_addr, col2_addr, row2_addr) {
	var _c1 = vm_arg(col1_addr);
	var _r1 = vm_arg(row1_addr);
	var _c2 = vm_arg(col2_addr);
	var _r2 = vm_arg(row2_addr);
	//if (_c1 < 0 || _c1 >= global.grid_cols || _r1 < 0 || _r1 >= global.grid_rows) return;
	//if (_c2 < 0 || _c2 >= global.grid_cols || _r2 < 0 || _r2 >= global.grid_rows) return;
	
	if (_c1 < 0) _c1 += global.grid_cols + 64;
	if (_c1 >= global.grid_cols + 64) _c1 -= global.grid_cols + 64;
	if (_r1 < 0) _r1 += global.grid_rows + 64;
	if (_r1 >= global.grid_rows + 64) _r1 -= global.grid_rows + 64;
	if (_c2 < 0) _c2 += global.grid_cols + 64;
	if (_c2 >= global.grid_cols + 64) _c2 -= global.grid_cols + 64;
	if (_r2 < 0) _r2 += global.grid_rows + 64;
	if (_r2 >= global.grid_rows + 64) _r2 -= global.grid_rows + 64;
	
	if (_c1 == _c2 && _r1 == _r2) return;
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	var _list1 = ds_grid_get(global.grid_plants, _c1, _r1);
	var _list2 = ds_grid_get(global.grid_plants, _c2, _r2);
	var _plants1 = [];
	var _plants2 = [];
	for (var i = 0; i < ds_list_size(_list1); i++) {
		var _p = ds_list_find_value(_list1, i);
		if (instance_exists(_p)) array_push(_plants1, _p);
	}
	for (var i = 0; i < ds_list_size(_list2); i++) {
		var _p = ds_list_find_value(_list2, i);
		if (instance_exists(_p)) array_push(_plants2, _p);
	}
	ds_list_clear(_list1);
	ds_list_clear(_list2);
	var _pos1 = get_world_position_from_grid(_c1, _r1);
	var _pos2 = get_world_position_from_grid(_c2, _r2);
	for (var i = 0; i < array_length(_plants1); i++) {
		var _p = _plants1[i];
		_p.x = _pos2.x;
		_p.y = _pos2.y;
		_p.grid_col = _c2;
		_p.grid_row = _r2;
		_p.depth = calculate_plant_depth(_c2, _r2, _p.plant_type);
		update_plant_bindings(_p);
		ds_list_add(_list2, _p);
	}
	for (var i = 0; i < array_length(_plants2); i++) {
		var _p = _plants2[i];
		_p.x = _pos1.x;
		_p.y = _pos1.y;
		_p.grid_col = _c1;
		_p.grid_row = _r1;
		_p.depth = calculate_plant_depth(_c1, _r1, _p.plant_type);
		update_plant_bindings(_p);
		ds_list_add(_list1, _p);
	}
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_SwapPlants", args: [_c1, _r1, _c2, _r2]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
}


/// @function VM_SwapPlantRects(x1, y1, w, h, x2, y2)
/// @description 交换两个等大矩形区域内所有植物的位置（支持重叠）
/// @param x1, y1 第一个矩形的左上角 (列, 行)
/// @param w, h  矩形宽高 (格子数)
/// @param x2, y2 第二个矩形的左上角 (列, 行)
function VM_SwapPlantRects(x1_addr, y1_addr, w_addr, h_addr, x2_addr, y2_addr) {
	var _x1 = vm_arg(x1_addr);
	var _y1 = vm_arg(y1_addr);
	var _w  = vm_arg(w_addr);
	var _h  = vm_arg(h_addr);
	var _x2 = vm_arg(x2_addr);
	var _y2 = vm_arg(y2_addr);
	if (_w <= 0 || _h <= 0) return;
	if (_x1 == _x2 && _y1 == _y2) return;
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	
	// 第一趟：收集快照（两边都在界内才收）
	var _snap = ds_map_create();
	for (var _dc = 0; _dc < _w; _dc++) {
		for (var _dr = 0; _dr < _h; _dr++) {
			var _c1 = _x1 + _dc;
			var _r1 = _y1 + _dr;
			var _c2 = _x2 + _dc;
			var _r2 = _y2 + _dr;
			var _in1 = (_c1 >= 0 && _c1 < global.grid_cols && _r1 >= 0 && _r1 < global.grid_rows);
			var _in2 = (_c2 >= 0 && _c2 < global.grid_cols && _r2 >= 0 && _r2 < global.grid_rows);
			if (!_in1 || !_in2) continue;
			var _k1 = string(_c1) + "," + string(_r1);
			if (!ds_map_exists(_snap, _k1)) {
				var _arr = [];
				var _list = ds_grid_get(global.grid_plants, _c1, _r1);
				for (var i = 0; i < ds_list_size(_list); i++) {
					var _p = ds_list_find_value(_list, i);
					if (instance_exists(_p)) array_push(_arr, _p);
				}
				ds_map_add(_snap, _k1, _arr);
			}
			var _k2 = string(_c2) + "," + string(_r2);
			if (!ds_map_exists(_snap, _k2)) {
				var _arr = [];
				var _list = ds_grid_get(global.grid_plants, _c2, _r2);
				for (var i = 0; i < ds_list_size(_list); i++) {
					var _p = ds_list_find_value(_list, i);
					if (instance_exists(_p)) array_push(_arr, _p);
				}
				ds_map_add(_snap, _k2, _arr);
			}
		}
	}
	
	// 第二趟：清空（每格只清一次）
	var _cleared = ds_map_create();
	for (var _dc = 0; _dc < _w; _dc++) {
		for (var _dr = 0; _dr < _h; _dr++) {
			var _c1 = _x1 + _dc;
			var _r1 = _y1 + _dr;
			var _c2 = _x2 + _dc;
			var _r2 = _y2 + _dr;
			var _in1 = (_c1 >= 0 && _c1 < global.grid_cols && _r1 >= 0 && _r1 < global.grid_rows);
			var _in2 = (_c2 >= 0 && _c2 < global.grid_cols && _r2 >= 0 && _r2 < global.grid_rows);
			if (!_in1 || !_in2) continue;
			var _k1 = string(_c1) + "," + string(_r1);
			if (!ds_map_exists(_cleared, _k1)) {
				ds_list_clear(ds_grid_get(global.grid_plants, _c1, _r1));
				ds_map_add(_cleared, _k1, true);
			}
			var _k2 = string(_c2) + "," + string(_r2);
			if (!ds_map_exists(_cleared, _k2)) {
				ds_list_clear(ds_grid_get(global.grid_plants, _c2, _r2));
				ds_map_add(_cleared, _k2, true);
			}
		}
	}
	ds_map_destroy(_cleared);
	
	// 第三趟：放入（格A→格B，格B→格A，重叠格以A侧为准）
	for (var _dc = 0; _dc < _w; _dc++) {
		for (var _dr = 0; _dr < _h; _dr++) {
			var _c1 = _x1 + _dc;
			var _r1 = _y1 + _dr;
			var _c2 = _x2 + _dc;
			var _r2 = _y2 + _dr;
			var _in1 = (_c1 >= 0 && _c1 < global.grid_cols && _r1 >= 0 && _r1 < global.grid_rows);
			var _in2 = (_c2 >= 0 && _c2 < global.grid_cols && _r2 >= 0 && _r2 < global.grid_rows);
			if (!_in1 || !_in2) continue;
			var _k1 = string(_c1) + "," + string(_r1);
			var _k2 = string(_c2) + "," + string(_r2);
	
			// A侧 → B侧
			var _src = ds_map_find_value(_snap, _k1);
			if (!is_undefined(_src)) {
				var _pos = get_world_position_from_grid(_c2, _r2);
				var _list = ds_grid_get(global.grid_plants, _c2, _r2);
				for (var i = 0; i < array_length(_src); i++) {
					var _p = _src[i];
					_p.x = _pos.x;
					_p.y = _pos.y;
					_p.grid_col = _c2;
					_p.grid_row = _r2;
					_p.depth = calculate_plant_depth(_c2, _r2, _p.plant_type);
					update_plant_bindings(_p);
					ds_list_add(_list, _p);
				}
				ds_map_add(_snap, _k1, undefined);
			}
	
			// B侧 → A侧（未被A侧处理过的）
			var _src = ds_map_find_value(_snap, _k2);
			if (!is_undefined(_src)) {
				var _pos = get_world_position_from_grid(_c1, _r1);
				var _list = ds_grid_get(global.grid_plants, _c1, _r1);
				for (var i = 0; i < array_length(_src); i++) {
					var _p = _src[i];
					_p.x = _pos.x;
					_p.y = _pos.y;
					_p.grid_col = _c1;
					_p.grid_row = _r1;
					_p.depth = calculate_plant_depth(_c1, _r1, _p.plant_type);
					update_plant_bindings(_p);
					ds_list_add(_list, _p);
				}
				ds_map_add(_snap, _k2, undefined);
			}
		}
	}
	ds_map_destroy(_snap);
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_SwapPlantRects", args: [_x1, _y1, _w, _h, _x2, _y2]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
	}

/// @function VM_CompactColumn(col)
/// @description 压缩整列植物：从上到下遍历，有空缺就把下面的往上搬运
/// @param col 列号，-1=所有列
function VM_CompactColumn(col_addr) {
	var _col = vm_arg(col_addr);
	var _c1 = (_col == -1) ? 0 : _col;
	var _c2 = (_col == -1) ? global.grid_cols - 1 : _col;
	if (_c1 < 0 || _c2 >= global.grid_cols) return;
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	for (var _c = _c1; _c <= _c2; _c++) {
		// 收集该列所有非空格子的植物快照
		var _snap = [];
		for (var _r = 0; _r < global.grid_rows; _r++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			var _arr = [];
			for (var i = 0; i < ds_list_size(_list); i++) {
				var _p = ds_list_find_value(_list, i);
				if (instance_exists(_p)) array_push(_arr, _p);
			}
			if (array_length(_arr) > 0) array_push(_snap, _arr);
		}
		// 清空整列
		for (var _r = 0; _r < global.grid_rows; _r++) {
			ds_list_clear(ds_grid_get(global.grid_plants, _c, _r));
		}
		// 从第0行开始重新紧密放入
		var _row = 0;
		for (var _i = 0; _i < array_length(_snap); _i++) {
			var _src = _snap[_i];
			var _pos = get_world_position_from_grid(_c, _row);
			var _list = ds_grid_get(global.grid_plants, _c, _row);
			for (var j = 0; j < array_length(_src); j++) {
				var _p = _src[j];
				_p.x = _pos.x;
				_p.y = _pos.y;
				_p.grid_row = _row;
				_p.depth = calculate_plant_depth(_c, _row, _p.plant_type);
				update_plant_bindings(_p);
				ds_list_add(_list, _p);
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_CompactColumn", args: [_col]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
			}
			_row++;
		}
	}
}

/// @function VM_CompactRow(row)
/// @description 压缩整行植物：从左到右遍历，有空缺就把右边的往左搬运
/// @param row 行号，-1=所有行
function VM_CompactRow(row_addr) {
	var _row = vm_arg(row_addr);
	var _r1 = (_row == -1) ? 0 : _row;
	var _r2 = (_row == -1) ? global.grid_rows - 1 : _row;
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	if (_r1 < 0 || _r2 >= global.grid_rows) return;
	for (var _r = _r1; _r <= _r2; _r++) {
		// 收集该行所有非空格子的植物快照
		var _snap = [];
		for (var _c = 0; _c < global.grid_cols; _c++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			var _arr = [];
			for (var i = 0; i < ds_list_size(_list); i++) {
				var _p = ds_list_find_value(_list, i);
				if (instance_exists(_p)) array_push(_arr, _p);
			}
			if (array_length(_arr) > 0) array_push(_snap, _arr);
		}
		// 清空整行
		for (var _c = 0; _c < global.grid_cols; _c++) {
			ds_list_clear(ds_grid_get(global.grid_plants, _c, _r));
		}
		// 从第0列开始重新紧密放入
		var _col = 0;
		for (var _i = 0; _i < array_length(_snap); _i++) {
			var _src = _snap[_i];
			var _pos = get_world_position_from_grid(_col, _r);
			var _list = ds_grid_get(global.grid_plants, _col, _r);
			for (var j = 0; j < array_length(_src); j++) {
				var _p = _src[j];
				_p.x = _pos.x;
				_p.y = _pos.y;
				_p.grid_col = _col;
				_p.depth = calculate_plant_depth(_col, _r, _p.plant_type);
				update_plant_bindings(_p);
				ds_list_add(_list, _p);
			}
			_col++;
		}
	}
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_CompactRow", args: [_row]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
}

/// @function VM_CompactColumnRev(col)
/// @description 压缩整列植物（反向）：从上到下遍历，有空缺就把上面的往下搬运
/// @param col 列号，-1=所有列
function VM_CompactColumnRev(col_addr) {
	var _col = vm_arg(col_addr);
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	var _c1 = (_col == -1) ? 0 : _col;
	var _c2 = (_col == -1) ? global.grid_cols - 1 : _col;
	if (_c1 < 0 || _c2 >= global.grid_cols) return;
	for (var _c = _c1; _c <= _c2; _c++) {
		var _snap = [];
		for (var _r = 0; _r < global.grid_rows; _r++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			var _arr = [];
			for (var i = 0; i < ds_list_size(_list); i++) {
				var _p = ds_list_find_value(_list, i);
				if (instance_exists(_p)) array_push(_arr, _p);
			}
			if (array_length(_arr) > 0) array_push(_snap, _arr);
		}
		for (var _r = 0; _r < global.grid_rows; _r++) {
			ds_list_clear(ds_grid_get(global.grid_plants, _c, _r));
		}
		// 从底行开始往上紧密放入
		var _row = global.grid_rows - array_length(_snap);
		for (var _i = 0; _i < array_length(_snap); _i++) {
			var _src = _snap[_i];
			var _pos = get_world_position_from_grid(_c, _row);
			var _list = ds_grid_get(global.grid_plants, _c, _row);
			for (var j = 0; j < array_length(_src); j++) {
				var _p = _src[j];
				_p.x = _pos.x;
				_p.y = _pos.y;
				_p.grid_row = _row;
				_p.depth = calculate_plant_depth(_c, _row, _p.plant_type);
				update_plant_bindings(_p);
				ds_list_add(_list, _p);
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_CompactColumnRev", args: [_col]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
			}
			_row++;
		}
	}
}

/// @function VM_CompactRowRev(row)
/// @description 压缩整行植物（反向）：从左到右遍历，有空缺就把左边的往右搬运
/// @param row 行号，-1=所有行
function VM_CompactRowRev(row_addr) {
	var _row = vm_arg(row_addr);
	var _r1 = (_row == -1) ? 0 : _row;
	if (global.network.mode == "client" && global._VM_sync_exec) return;
	var _r2 = (_row == -1) ? global.grid_rows - 1 : _row;
	if (_r1 < 0 || _r2 >= global.grid_rows) return;
	for (var _r = _r1; _r <= _r2; _r++) {
		var _snap = [];
		for (var _c = 0; _c < global.grid_cols; _c++) {
			var _list = ds_grid_get(global.grid_plants, _c, _r);
			var _arr = [];
			for (var i = 0; i < ds_list_size(_list); i++) {
				var _p = ds_list_find_value(_list, i);
				if (instance_exists(_p)) array_push(_arr, _p);
			}
			if (array_length(_arr) > 0) array_push(_snap, _arr);
		}
		for (var _c = 0; _c < global.grid_cols; _c++) {
			ds_list_clear(ds_grid_get(global.grid_plants, _c, _r));
		}
		// 从最右列开始往左紧密放入
		var _col = global.grid_cols - array_length(_snap);
		for (var _i = 0; _i < array_length(_snap); _i++) {
			var _src = _snap[_i];
			var _pos = get_world_position_from_grid(_col, _r);
			var _list = ds_grid_get(global.grid_plants, _col, _r);
			for (var j = 0; j < array_length(_src); j++) {
	if (global.network.mode == "server") {
			var _msg = json_stringify({hook: "call", func: "VM_CompactRowRev", args: [_row]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
				var _p = _src[j];
				_p.x = _pos.x;
				_p.y = _pos.y;
				_p.grid_col = _col;
				_p.depth = calculate_plant_depth(_col, _r, _p.plant_type);
				update_plant_bindings(_p);
				ds_list_add(_list, _p);
			}
			_col++;
		}
	}
}
	

/// @function VM_SpawnPlantsRandom(x, y, w, h, shape, level, skill, card1, card2, ..., card9)
/// @description 在矩形区域内随机种植植物，每格从 card1..card9 中随机选一个
/// @param x,y     左上角 (列, 行)
/// @param w,h     宽高 (格子数)
/// @param shape   共享形状 (-1=默认)
/// @param level   共享等级 (-1=默认)
/// @param skill   共享技能 (-1=默认)
/// @param cardN   植物卡片名 (最多9个)，空串/-1=跳过
/// @note 客户端不可调用，服务端自动同步
function VM_SpawnPlantsRandom(x_addr, y_addr, w_addr, h_addr,
	shape_addr, level_addr, skill_addr,
	ca1, ca2, ca3, ca4, ca5, ca6, ca7, ca8, ca9) {
	if (global.network.mode == "client") return;
	var _x = vm_read_mem(global.__vm, x_addr);
	var _y = vm_read_mem(global.__vm, y_addr);
	var _w = vm_read_mem(global.__vm, w_addr);
	var _h = vm_read_mem(global.__vm, h_addr);
	if (_w <= 0 || _h <= 0) return;
	var _shape = vm_read_mem(global.__vm, shape_addr);
	var _level = vm_read_mem(global.__vm, level_addr);
	var _skill = vm_read_mem(global.__vm, skill_addr);
	if (is_undefined(_shape)) _shape = -1;
	if (is_undefined(_level)) _level = -1;
	if (is_undefined(_skill)) _skill = -1;
	// 收集有效卡片名
	var _cards = [];
	var _all = [ca1, ca2, ca3, ca4, ca5, ca6, ca7, ca8, ca9];
	for (var _n = 0; _n < 9; _n++) {
		if (is_undefined(_all[_n])) continue;
		var _c = vm_read_mem(global.__vm, _all[_n]);
		if (_c == -1 || _c == "") continue;
		array_push(_cards, _c);
	}
	if (array_length(_cards) == 0) return;
	var _c1 = clamp(_x, 0, global.grid_cols - 1);
	var _c2 = clamp(_x + _w - 1, 0, global.grid_cols - 1);
	var _r1 = clamp(_y, 0, global.grid_rows - 1);
	var _r2 = clamp(_y + _h - 1, 0, global.grid_rows - 1);
	for (var _c = _c1; _c <= _c2; _c++) {
		for (var _r = _r1; _r <= _r2; _r++) {
			// 格子已有植物则跳过
			if (ds_list_size(ds_grid_get(global.grid_plants, _c, _r)) > 0) continue;
			var _card = _cards[irandom(array_length(_cards) - 1)];
			var _card_data = deck_get_card_data(_card, _shape);
			if (is_undefined(_card_data)) continue;
			var _props = {};
			if (_level != -1) _props[$ "current_level"] = _level;
			if (_skill != -1) _props[$ "skill"] = _skill;
			if (_shape != -1) _props[$ "shape"] = _shape;
			
			
			if (_card_data[? "obj"] == obj_card_mod) { global._mod_pending_card_id = _card; }
				var _plant = spawn_plant(_c, _r, _card_data[? "obj"], _props);
				global._mod_pending_card_id = "";
			if (_plant >= 0) {
				network_apply_plant_level(_plant);
				var _VM_id = ++global._VM_create_counter;
				_plant._VM_id = _VM_id;
				if (global.network.mode == "server") {
					ds_map_add(global._VM_id_to_real, _VM_id, _plant);
					var _nid = ds_map_exists(global.network.map_instance_id_net_id, _plant) ? global.network.map_instance_id_net_id[? _plant] : -1;
					if (_nid != -1) {
						var _vm_p = {};
						_vm_p[$ "_VM_id"] = _VM_id;
						var _list = global.network.connected_clients;
						for (var _i = 0; _i < array_length(_list); _i++)
							send_message(_list[_i], MSG_MODIFY_PROP, _nid, json_stringify(_vm_p));
					}
				}
			}
		}
	}
}

/// @function VM_CreateButton(x, y, sprite, scale, idle, hover, press)
/// @desc 创建一个可点击按钮，点击时触发 _VM_BUTTON_CLICKED 块
/// @param x,y     世界坐标
/// @param sprite  贴图名（完整精灵名，如 "spr_my_btn"）
/// @param scale   缩放倍数 (-1 默认 1)
/// @param idle    空闲态子帧 (-1 默认 0)
/// @param hover   悬停态子帧 (-1 默认 1)
/// @param press   按下态子帧 (-1 默认 2)
/// @return 按钮实例 ID
function VM_CreateButton(x_addr, y_addr, sprite_addr, scale_addr, idle_addr, hover_addr, press_addr) {
	var _x = vm_read_mem(global.__vm, x_addr);
	var _y = vm_read_mem(global.__vm, y_addr);
	var _spr_name = vm_read_mem(global.__vm, sprite_addr);
	var _scale = vm_read_mem(global.__vm, scale_addr);
	var _idle = vm_read_mem(global.__vm, idle_addr);
	var _hover = vm_read_mem(global.__vm, hover_addr);
	var _press = vm_read_mem(global.__vm, press_addr);
	if (is_undefined(_scale) || _scale == -1) _scale = 1;
	if (is_undefined(_idle)  || _idle == -1) _idle = 0;
	if (is_undefined(_hover) || _hover == -1) _hover = 1;
	if (is_undefined(_press) || _press == -1) _press = 2;
	var _spr = get_load_sprite(_spr_name);
	var _btn = instance_create_depth(_x, _y, -5000, Button);
	_btn.set_sprite(_spr).set_scale(_scale).set_position(_x, _y).set_frames(_idle, _hover, _press);
	return real(_btn);
}

/// @function VM_GetLastClickedButton()
/// @return 最后被点击的 Button 实例 ID，没有返回 -1
function VM_GetLastClickedButton() {
	return real(global._VM_last_clicked_button);
}

/// @function VM_ApplyPlantLevel(inst_id)
/// @description 应用植物的星级/技能/形态属性，刷新血量攻速等实际数值
/// @param inst_id 植物实例 ID
function VM_ApplyPlantLevel(inst_id_addr) {
	var _inst = vm_arg(inst_id_addr);
	if (_inst < 0) {
		var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
		if (is_undefined(_real)) return;
		_inst = _real;
	}

	if (!instance_exists(_inst)&&_inst!=-1) return;
	if (global.network.mode == "client" && global._VM_sync_exec && ds_map_exists(global.network.map_instance_id_net_id, _inst)) return;
	network_apply_plant_level(_inst,true);
	if(_inst==-1){
		with(obj_card_parent){
			network_apply_plant_level(id,true);
		}
	}
	if (global.network.mode == "server") {
		var _nid = ds_map_exists(global.network.map_instance_id_net_id, _inst) ? global.network.map_instance_id_net_id[? _inst] : -1;
		if (_nid != -1) {
			var _msg = json_stringify({hook: "call", func: "VM_ApplyPlantLevel", args: [_inst]});
			var _cl = global.network.connected_clients;
			for (var _i = 0; _i < array_length(_cl); _i++)
				send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
		}
	}
}
/// @function VM_SetCardSlotProp(name, prop, value)
/// @param name  "all"/-1=全部, 数字=指定 slot_index, 字符串=匹配 card_id
/// @param prop  属性名，如 "cooldown" "current_cost"
/// @param value 新的值
function VM_SetCardSlotProp(name_addr, prop_addr, value_addr) {
	var name = vm_read_mem(global.__vm, name_addr);
	var prop = vm_read_mem(global.__vm, prop_addr);
	var value = vm_read_mem(global.__vm, value_addr);
	if (name == "all" || name == -1) {
		with (obj_card_slot) variable_instance_set(id, prop, value);
	} else if (is_real(name)) {
		with (obj_card_slot) { if (slot_index == name) variable_instance_set(id, prop, value); }
	} else {
		with (obj_card_slot) { 
			if (card_id == name) 
				variable_instance_set(id, prop, value);
		}
	}
}

/// @function VM_CalcCardSlotProp(name, prop, op, val)
/// @param name  "all"/-1=全部, 数字=指定 slot_index, 字符串=匹配 card_id
/// @param prop  属性名
/// @param op    0=加 1=减 2=乘 3=除
/// @param val   操作数
function VM_CalcCardSlotProp(name_addr, prop_addr, op_addr, val_addr) {
	var name = vm_read_mem(global.__vm, name_addr);
	var prop = vm_read_mem(global.__vm, prop_addr);
	var op = vm_read_mem(global.__vm, op_addr);
	var val = vm_read_mem(global.__vm, val_addr);
	if (name == "all" || name == -1) {
		with (obj_card_slot) _calc_op(id, prop, op, val);
	} else if (is_real(name)) {
		with (obj_card_slot) { if (slot_index == name) _calc_op(id, prop, op, val); }
	} else {
		with (obj_card_slot) { if (card_id == name) _calc_op(id, prop, op, val); }
	}
}

function _calc_op(_inst, _prop, _op, _val) {
	var _cur = variable_instance_get(_inst, _prop);
	if (!is_real(_cur) || !is_real(_val)) return;
	switch (_op) {
		case 0: variable_instance_set(_inst, _prop, _cur + _val); break;
		case 1: variable_instance_set(_inst, _prop, _cur - _val); break;
		case 2: variable_instance_set(_inst, _prop, _cur * _val); break;
		case 3: if (_val != 0) variable_instance_set(_inst, _prop, _cur / _val); break;
	}
}

/// @function VM_AliasSprite(new_name, exist_name)
/// @desc 将 new_name 指向 exist_name（name → name），不存在则无操作；字符串值会在绘制链里按名字解析到最新贴图
function VM_AliasSprite(new_name_addr, exist_name_addr) {
	var _new = vm_read_mem(global.__vm, new_name_addr);
	var _exist = vm_read_mem(global.__vm, exist_name_addr);
	if (!ds_map_exists(global._VM_sprite_temp_cache, _exist)) return;
	ds_map_add(global._VM_sprite_temp_cache, _new, _exist);
}

/// @function VM_GetPreviewCard()
/// @return 当前手牌的 card_id，没有返回 -1
function VM_GetPreviewCard() {
	var _prev = instance_find(obj_card_preview, 0);
	return (_prev != noone) ? _prev.card_id : -1;
}

/// @function VM_GetCardSlotCount()
/// @return 卡槽数量
function VM_GetCardSlotCount() {
	return instance_number(obj_card_slot);
}

/// @function VM_PlaySound(name)
/// @param name 内置音效名 (如 "snd_place1")，也支持 VM_LoadSound 返回的音频 ID
function VM_PlaySound(name_addr) {
	var _name = vm_arg(name_addr);
	var _snd = asset_get_index(_name);
	if (_snd == -1 && is_real(_name)) _snd = _name;   // 本地加载的音频 ID
	if (_snd != -1) audio_play_sound(_snd, 0, 0);
}

/// @function VM_LoadSound(path)
/// @param path 本地音频文件路径，只支持 wav（相对地图 json 所在目录，如 "xxx.wav"）
/// @return 音频 ID，加载失败返回 -1
function VM_LoadSound(path_addr) {
	var _path = vm_arg(path_addr);
	if (ds_map_exists(global._audio_cache, _path)) return global._audio_cache[? _path];
	// 与读贴图相同：优先 _file_cache_json_path/../ 下查找，回退原始相对路径
	var _paths = [];
	if (variable_global_exists("_file_cache_json_path")) {
		var _prefix = global._file_cache_json_path;
		if (!string_ends_with(_prefix, "/") && !string_ends_with(_prefix, "\\")) _prefix += "/";
		array_push(_paths, _prefix + "../" + _path);
	}
	array_push(_paths, _path);
	for (var _i = 0; _i < array_length(_paths); _i++) {
		if (file_exists(_paths[_i])) {
			var _aud = audio_create_stream(_paths[_i]);
			if (_aud != -1) {
				ds_map_add(global._audio_cache, _path, _aud);
				global._audio_reverse[? _aud] = _path;   // 注册名称，音量钩子按 sound 处理
				return _aud;
			}
		}
	}
	return -1;
}

/// @function VM_Floor(value)
/// @param value 数字
/// @return 向下取整后的整数；输入不是数字返回 undefined
function VM_Floor(value_addr) {
	var _v = vm_arg(value_addr);
	if (!is_real(_v)) return undefined;
	return floor(_v);
}

/// @function VM_Ceil(value)
/// @param value 数字
/// @return 向上取整后的整数；输入不是数字返回 undefined
function VM_Ceil(value_addr) {
	var _v = vm_arg(value_addr);
	if (!is_real(_v)) return undefined;
	return ceil(_v);
}

/// @function VM_SpawnBatMouse(col, row)
/// @param col 目标格列
/// @param row 目标格行
/// @return 蝙蝠鼠实例 ID
function VM_SpawnBatMouse(col_addr, row_addr) {
	var b_col = vm_read_mem(global.__vm, col_addr);
	var b_row = vm_read_mem(global.__vm, row_addr);
	var _VM_id = ++global._VM_create_counter;
	if (global.network.mode == "client") return -_VM_id;
	var target_pos = get_world_position_from_grid(b_col, b_row);
	var inst = instance_create_depth(target_pos.x + 10, target_pos.y - room_height, -500, obj_bat_mouse_target);
	inst.target_col = b_col;
	inst.target_row = b_row;
	var bat = instance_create_depth(target_pos.x + 10, target_pos.y - room_height, -500, obj_bat_mouse);
	bat.target_col = b_col;
	bat.target_row = b_row;
	bat.banding_target_inst = inst;
	bat._VM_id = _VM_id;
	return real(bat.id);
}

/// @function VM_GetTimeLimit()
/// @return 关卡倒计时剩余时间（帧）；没有倒计时返回 undefined
function VM_GetTimeLimit() {
    if (!instance_exists(obj_battle)) return undefined;
    if (obj_battle.time_limit == -1) return undefined;
    return real(obj_battle.time_limit);
}

/// @function VM_SetTimeLimit(frames)
/// @param frames 新的倒计时时间（帧）
/// @return 设置成功返回新值；没有倒计时返回 undefined
function VM_SetTimeLimit(frames_addr) {
    if (global.network.mode == "client" && global._VM_sync_exec) return undefined;
    var _frames = vm_arg(frames_addr);
    if (!is_real(_frames) || _frames < 0) return undefined;
    if (!instance_exists(obj_battle)) return undefined;
    if (obj_battle.time_limit == -1) return undefined;   // 关卡无倒计时，不能设置
    obj_battle.time_limit = _frames;
    if (global.network.mode == "server") {
        var _msg = json_stringify({hook: "call", func: "VM_SetTimeLimit", args: [_frames]});
        var _cl = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_cl); _i++)
            send_message(_cl[_i], MSG_VM_NOTIFY, _msg);
    }
    return _frames;
}

/// @function VM_SetWaveAuto(enabled)
/// @param enabled 1=波次自动推进（默认）；0=关闭自动推进，由插件脚本用 VM_SetWave 手动控制
/// @return 开关设置后的值（1/0）
/// @desc 关闭后 battle 完全不自动出怪（含开局第一波），波次推进与出怪完全交给插件脚本
function VM_SetWaveAuto(enabled_addr) {
    var _on = vm_arg(enabled_addr);
    global._VM_wave_auto = _on != 0;
    return global._VM_wave_auto ? 1 : 0;
}

/// @function VM_SetWave(wave, subwave)
/// @param wave    要设置的波次索引（0 开始）
/// @param subwave 要设置的子波次索引（0 开始）
/// @return 1=成功，0=战斗不存在
/// @desc 只修改 current_wave/current_subwave 计数，不召唤敌人；变化会被 battle 检测到并触发 _VM_WAVE_START 等钩子
function VM_SetWave(wave_addr, subwave_addr) {
    var _wave = vm_arg(wave_addr);
    var _subwave = vm_arg(subwave_addr);
    if (!instance_exists(obj_battle)) return 0;
    obj_battle.current_wave = _wave;
    obj_battle.current_subwave = _subwave;
    return 1;
}

/// @function VM_GetCurCard()
/// @return 当前正在执行块的 mod 卡实例 ID（_OBJECT_CREATE/_STEP/_DRAW/_DESTROY 块内有效，否则 noone）
function VM_GetCurCard() {
    return VM_ClientWrapId(real(global._VM_cur_card));
}

/// @function VM_EnemyInRange(r1, r2, c1, c2, type)
/// @param r1 r2 行范围（闭区间，可反序，自动夹取到场内）
/// @param c1 c2 列范围（闭区间，可反序，自动夹取到 0..grid_cols+1）
/// @param type 敌人类型："normal"/"obstacle"/"diver"/"air"/"dance"/"underground"，""或"all"=任意
/// @return 1=范围内存在该类型敌人，0=不存在
/// @desc 前缀和查询（global.has_enemy_* 系列，obj_battle 每帧重建），O(行数×类型数)
function VM_EnemyInRange(r1_addr, r2_addr, c1_addr, c2_addr, type_addr) {
    var _r1 = vm_arg(r1_addr);
    var _r2 = vm_arg(r2_addr);
    var _c1 = vm_arg(c1_addr);
    var _c2 = vm_arg(c2_addr);
    var _type = vm_arg(type_addr);
    if (!variable_global_exists("has_enemy_normal")) return 0;
    var _arrs = [];
    if (_type == "normal") _arrs = [global.has_enemy_normal];
    else if (_type == "obstacle") _arrs = [global.has_enemy_obstacle];
    else if (_type == "diver") _arrs = [global.has_enemy_diver];
    else if (_type == "air") _arrs = [global.has_enemy_air];
    else if (_type == "dance") _arrs = [global.has_enemy_dance];
    else if (_type == "underground") _arrs = [global.has_enemy_underground];
    else if (_type == "" || _type == "all") {
        _arrs = [global.has_enemy_normal, global.has_enemy_obstacle, global.has_enemy_diver,
                 global.has_enemy_air, global.has_enemy_dance, global.has_enemy_underground];
    } else return 0;
    if (_r1 > _r2) { var _t = _r1; _r1 = _r2; _r2 = _t; }
    if (_c1 > _c2) { var _t = _c1; _c1 = _c2; _c2 = _t; }
    _r1 = clamp(_r1, 0, global.grid_rows - 1);
    _r2 = clamp(_r2, 0, global.grid_rows - 1);
    _c1 = clamp(_c1, 0, global.grid_cols + 1);
    _c2 = clamp(_c2, 0, global.grid_cols + 1);
    var _stride = global.grid_cols + 2;
    for (var _r = _r1; _r <= _r2; _r++) {
        var _base = _r * _stride;
        for (var _k = 0; _k < array_length(_arrs); _k++) {
            var _p = _arrs[_k];
            if (_p[_base + _c2] - (_c1 > 0 ? _p[_base + _c1 - 1] : 0) > 0) return 1;
        }
    }
    return 0;
}

/// @function VM_GetHomingTarget(type)
/// @param type 敌人类型（"normal"/"air"/"diver"/"dance"/"obstacle"/"underground"），"" 或 "all"=任意
/// @return 最靠左（x 最小）、同 x 时血量最高的存活敌人 id；无则 -1
/// @desc 追踪索敌（与糖葫芦炮弹同规则）；全场每帧只扫描一次，同帧所有调用共享缓存
function VM_GetHomingTarget(type_addr) {
    var _type = vm_arg(type_addr);
    if (_type == "all") _type = "";
    if (!variable_global_exists("_VM_homing_cache")) global._VM_homing_cache = ds_map_create();
    var _cache = global._VM_homing_cache;
    var _frame = instance_exists(obj_battle) ? obj_battle.battle_time : -1;
    var _snap = ds_map_find_value(_cache, "snap");
    if (!is_struct(_snap) || _snap[$ "frame"] != _frame) {
        // 每帧只扫一次全场：按敌人类型各自记录 最左 x、同 x 血量最高 的实例
        var _types = ds_map_create();
        with (obj_enemy_parent) {
            if (hp > 0 && y > 0) {
                var _t = target_type;
                var _cur = ds_map_find_value(_types, _t);
                if (is_undefined(_cur) || x < _cur.x || (x == _cur.x && hp > _cur.hp)) {
                    _types[? _t] = { x: x, hp: hp, id: id };
                }
            }
        }
        _snap = { frame: _frame, types: _types };
        _cache[? "snap"] = _snap;
    }
    var _types = _snap[$ "types"];
    if (_type == "") {
        // 任意类型：取各类型最优里的全局最优
        var _best = -1;
        var _best_x = room_width;
        var _best_hp = -1;
        var _keys = ds_map_keys_to_array(_types);
        for (var _i = 0; _i < array_length(_keys); _i++) {
            var _e = _types[? _keys[_i]];
            if (_e.x < _best_x || (_e.x == _best_x && _e.hp > _best_hp)) {
                _best_x = _e.x;
                _best_hp = _e.hp;
                _best = _e.id;
            }
        }
        return VM_ClientWrapId(_best);
    }
    var _e = ds_map_find_value(_types, _type);
    if (is_undefined(_e)) return -1;
    return VM_ClientWrapId(_e.id);
}

/// @function VM_GetInstancesInRange(arr, r1, r2, c1, c2, kind, type)
/// @param arr  VM 命名数组：先清空，再存入结果
/// @param r1 r2 行范围（闭区间，可反序，自动夹取到场内）
/// @param c1 c2 列范围（闭区间，可反序）
/// @param kind "enemy"=敌人 / "card"=我方卡片
/// @param type 筛选：敌人按 target_type；卡片按 plant_id / plant_type(底座)；"" 或 "all"=不限
/// @return 存入数量
/// @desc 范围影响类卡常用：收集范围内实例 id 进数组，配合 VM_ArraySize/VM_ArrayGet 遍历
function VM_GetInstancesInRange(arr_addr, r1_addr, r2_addr, c1_addr, c2_addr, kind_addr, type_addr) {
    var _name = vm_arg(arr_addr);
    var _r1 = vm_arg(r1_addr);
    var _r2 = vm_arg(r2_addr);
    var _c1 = vm_arg(c1_addr);
    var _c2 = vm_arg(c2_addr);
    var _kind = vm_arg(kind_addr);
    var _type = vm_arg(type_addr);
    if (_type == "all") _type = "";
    if (_r1 > _r2) { var _t = _r1; _r1 = _r2; _r2 = _t; }
    if (_c1 > _c2) { var _t = _c1; _c1 = _c2; _c2 = _t; }
    _r1 = clamp(_r1, 0, global.grid_rows - 1);
    _r2 = clamp(_r2, 0, global.grid_rows - 1);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) ds_map_add(_vm.arrays, _name, []);
    var _arr = _vm.arrays[? _name];
    array_resize(_arr, 0);
    var _count = 0;
    if (_kind == "enemy") {
        if (!variable_global_exists("enemy_array")) return 0;
        _c1 = clamp(_c1, 0, global.grid_cols + 1);
        _c2 = clamp(_c2, 0, global.grid_cols + 1);
        var _stride = global.grid_cols + 2;
        for (var _r = _r1; _r <= _r2; _r++) {
            var _base = _r * _stride;
            for (var _c = _c1; _c <= _c2; _c++) {
                var _cell = global.enemy_array[_base + _c];
                for (var _i = 0; _i < array_length(_cell); _i++) {
                    var _e = _cell[_i];
                    if (!instance_exists(_e) || _e.hp <= 0) continue;
                    if (_type != "" && _e.target_type != _type) continue;
                    array_push(_arr, VM_ClientWrapId(_e));
                    _count++;
                }
            }
        }
    } else if (_kind == "card") {
        _c1 = clamp(_c1, 0, global.grid_cols - 1);
        _c2 = clamp(_c2, 0, global.grid_cols - 1);
        for (var _r = _r1; _r <= _r2; _r++) {
            for (var _c = _c1; _c <= _c2; _c++) {
                var _list = ds_grid_get(global.grid_plants, _c, _r);
                for (var _i = 0; _i < ds_list_size(_list); _i++) {
                    var _p = ds_list_find_value(_list, _i);
                    if (!instance_exists(_p)) continue;
                    if (_p.plant_id == "player") continue;   // 与 VM_SetCardProp 一致，跳过角色
                    if (_type != "") {
                        // 匹配卡 id / 底座类型
                        var _pt = variable_instance_exists(_p, "plant_type") ? _p.plant_type : "";
                        if (_p.plant_id != _type && _pt != _type) continue;
                    }
                    array_push(_arr, VM_ClientWrapId(_p));
                    _count++;
                }
            }
        }
    }
    return _count;
}

/// @function VM_CreateInstance(obj_name, x, y)
/// @param obj_name 对象名（可带或不带 obj_ 前缀）
/// @param x y 生成坐标（像素）
/// @return 实例 ID；对象不存在或创建失败返回 -1
/// @desc 通用创建实例（子弹等）：创建后用 VM_SetProp 配置属性
function VM_CreateInstance(obj_name_addr, x_addr, y_addr) {
    var obj_name = vm_arg(obj_name_addr);
    var _x = vm_arg(x_addr);
    var _y = vm_arg(y_addr);
    if (!string_starts_with(obj_name, "obj_")) obj_name = "obj_" + obj_name;
    var _obj = asset_get_index(obj_name);
    if (_obj < 0) {
        show_debug_message("[VM_CreateInstance] 对象不存在: " + obj_name);
        return -1;
    }
    var _depth = -1200;
    if (global._VM_cur_card != noone && instance_exists(global._VM_cur_card)) {
        _depth = global._VM_cur_card.depth - 500;   // 与原版卡一致：子弹浮在发射卡上方
    }
    var _inst = instance_create_depth(_x, _y, _depth, _obj);
    if (_inst < 0) return -1;
    return VM_ClientWrapId(_inst);
}

/// @function VM_GetProp(inst_id, prop)
/// @return 属性值
function VM_GetProp(inst_id_addr, prop_addr) {
    var inst_id = vm_read_mem(global.__vm, inst_id_addr);
    var prop = vm_read_mem(global.__vm, prop_addr);
    // 特殊实例 id 0：读全局变量（0 永远不是真实实例 id），插件可用 VM_GetProp(0, "名字") 读取
    if (inst_id == 0) {
        if (variable_global_exists(prop)) return variable_global_get(prop);
        return undefined;
    }
    if (inst_id < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -inst_id);
        if (is_undefined(_real)) return undefined;
        inst_id = _real;
    }
    if (!instance_exists(inst_id)) return undefined;
    // 特殊属性：对象名（带 obj_ 前缀）
    if (prop == "object_name") {
        return object_get_name(inst_id.object_index);
    }
    // 特殊属性：没有 mouse_id 变量的实例（如植物卡片），用对象名去掉 obj_ 前缀兜底
    if (prop == "mouse_id" && (!variable_instance_exists(inst_id, prop)||
		variable_instance_exists(inst_id, prop)&&variable_instance_get(inst_id, prop)=""
	)) {
        return string_delete(object_get_name(inst_id.object_index), 1, 4);
    }
    return variable_instance_get(inst_id, prop);
}

/// @function VM_GetKilledProp(prop)
/// @return 当前销毁事件对象的属性值（仅销毁类 hook 执行期间有效）
function VM_GetKilledProp(prop_addr) {
    var _snap = global._VM_cur_dead_snap;
    if (!is_struct(_snap)) return undefined;
    var prop = vm_read_mem(global.__vm, prop_addr);
    // 特殊属性：对象名（带 obj_ 前缀）
    if (prop == "object_name") return _snap[$ "_object_name"];
    var _v = _snap[$ prop];
    if (!is_undefined(_v)) return _v;
    // 特殊属性：没有 mouse_id 变量或值为空串的实例（如植物卡片、normal_mouse），用对象名去掉 obj_ 前缀兜底
    if (prop == "mouse_id" && (is_undefined(_v) || _v == "")) return string_delete(_snap[$ "_object_name"], 1, 4);
    return undefined;
}

/// @function VM_IsUndefined(value)
/// @return 1=值为 undefined，0=不是
function VM_IsUndefined(value_addr) {
    return is_undefined(vm_arg(value_addr)) ? 1 : 0;
}

/// @function VM_IsDestroyed(inst_id)
/// @return 1=实例已被销毁（不存在），0=实例仍存在
function VM_IsDestroyed(id_addr) {
    var _id = vm_arg(id_addr);
    if (_id < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_id);
        if (is_undefined(_real)) return 1;
        _id = _real;
    }
    return instance_exists(_id) ? 0 : 1;
}

/// @function VM_SetProp(inst_id, prop, value)
function VM_SetProp(inst_id_addr, prop_addr, value_addr) {
    var inst_id = vm_read_mem(global.__vm, inst_id_addr);
    var prop = vm_read_mem(global.__vm, prop_addr);
    var value = vm_read_mem(global.__vm, value_addr);
    // 特殊实例 id 0：写全局变量（0 永远不是真实实例 id），
    // 插件可用 VM_SetProp(0, "名字", 值) 定义/修改全局，不存在会自动创建
    if (inst_id == 0) {
        variable_global_set(prop, value);
        return;
    }
    if (inst_id < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -inst_id);
        if (is_undefined(_real)) return;
        inst_id = _real;
    }
    if (!instance_exists(inst_id)) return;
    // 贴图属性传字符串时解析成精灵 id（与联机属性同步的转换保持一致）
    if (prop == "sprite_index" && is_string(value)) {
        value = get_load_sprite(value);
    }
    // 客户端：有 net_id 则跳过，服务端会通过 MSG_MODIFY_PROP 同步
    if (global.network.mode == "client" && global._VM_sync_exec
        && ds_map_exists(global.network.map_instance_id_net_id, inst_id)) return;
		
		
	if(prop=="grid_col"||prop=="grid_row"){
		if( object_is_ancestor(obj_card_parent, instance_id) ){
			var _list = ds_grid_get(global.grid_plants, instance_id.grid_col, instance_id.grid_row);
			ds_list_delete(_list,ds_list_find_index(_list,inst_id))
		}
	}
    variable_instance_set(inst_id, prop, value);
	if(prop=="x"||prop=="y"){
		update_plant_bindings(inst_id);
	}
	if(prop=="grid_col"||prop=="grid_row"){
		if( object_is_ancestor(obj_card_parent, instance_id) ){
			var _list = ds_grid_get(global.grid_plants, instance_id.grid_col, instance_id.grid_row);
			ds_list_add(_list,instance_id)
		}
	}
	if(prop=="shape" || prop=="skill"|| prop=="current_level" ){
		network_apply_plant_level(_plant,true)
	}
    if (global.network.mode == "server") {
        var _nid = ds_map_exists(global.network.map_instance_id_net_id, inst_id) ? global.network.map_instance_id_net_id[? inst_id] : -1;
        if (_nid != -1) {
            var _s = {}; _s[$ prop] = value;
            var _json = json_stringify(_s);
            var _list = global.network.connected_clients;
            for (var _i = 0; _i < array_length(_list); _i++)
                send_message(_list[_i], MSG_MODIFY_PROP, _nid, _json);
        }
    }
}

/// @function VM_SpawnObject(obj_name, col, row)
/// @return 实例 ID
function VM_SpawnObject(obj_name_addr, col_addr, row_addr) {
    var obj_name = vm_read_mem(global.__vm, obj_name_addr);
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var _VM_id = ++global._VM_create_counter;
    if (global.network.mode == "client") return -_VM_id;
    if (!string_starts_with(obj_name, "obj_"))
        obj_name = "obj_" + obj_name;
    var _obj = asset_get_index(obj_name);
    if (_obj < 0) {
        show_debug_message("[VM_SpawnObject] 对象不存在: " + obj_name);
        return -1;
    }
    var _depth = -1200;
    var _y_off = -35;
    switch (obj_name) {
        case "obj_mouse_hole":   _depth = -5;   _y_off = 0;   break;
        case "obj_pharaoh_hole": _depth = -5;   _y_off = 0;   break;
        case "obj_cloud":        _depth = 10;   _y_off = -10; break;
    }
    var _c1 = (col == -1) ? 0 : col;
    var _c2 = (col == -1) ? global.grid_cols - 1 : col;
    var _r1 = (row == -1) ? 0 : row;
    var _r2 = (row == -1) ? global.grid_rows - 1 : row;
    var _last = -1;
    for (var _r = _r1; _r <= _r2; _r++) {
        for (var _c = _c1; _c <= _c2; _c++) {
            var _pos = get_world_position_from_grid(_c, _r);
            var _inst = instance_create_depth(_pos.x, _pos.y + _y_off, _depth, _obj);
            if (_inst < 0) continue;
            _inst.row = _r;
            _inst.col = _c;
            _last = _inst;
            _inst._VM_id = _VM_id;
        }
    }
    if (global.network.mode == "server" && _last != -1) {
        ds_map_add(global._VM_id_to_real, _VM_id, _last);
        // 通过 MSG_MODIFY_PROP 同步 VM_id
        var _nid = ds_map_exists(global.network.map_instance_id_net_id, _last) ? global.network.map_instance_id_net_id[? _last] : -1;
        if (_nid != -1) {
            var _vm_prop = {};
            _vm_prop[$ "_VM_id"] = _VM_id;
            var _vm_json = json_stringify(_vm_prop);
            var _list = global.network.connected_clients;
            for (var _i = 0; _i < array_length(_list); _i++) {
                send_message(_list[_i], MSG_MODIFY_PROP, _nid, _vm_json);
            }
        }
    }
    return real(_last);
}
/// @function VM_SpawnPlant(card_id, col, row, shape, level, skill)
/// @return 植物实例 ID
function VM_SpawnPlant(card_id_addr, col_addr, row_addr, shape_addr, level_addr, skill_addr) {
    var card_id = vm_read_mem(global.__vm, card_id_addr);
    var col = vm_read_mem(global.__vm, col_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var shape = vm_read_mem(global.__vm, shape_addr);
    var level = vm_read_mem(global.__vm, level_addr);
    var skill = vm_read_mem(global.__vm, skill_addr);
    if (is_undefined(shape)) shape = 0;
    if (is_undefined(level)) level = 0;
    if (is_undefined(skill)) skill = 0;
    var _card_data = deck_get_card_data(card_id, shape);
    if (is_undefined(_card_data)) return -1;
    var _obj = _card_data[? "obj"];
    var _props = {};
	if(level!=-1)_props[$ "current_level"] =level;
	if(skill!=-1)_props[$ "skill"] =skill;
	if(shape!=-1)_props[$ "shape"] =shape;
	//_props[$ "sprite_index"] = _card_data[? "sprite"];

    // 批量生成：col=-1 整列所有行，row=-1 整行所有列
    var _batch = (col == -1 || row == -1);
    var _col_start = (col == -1) ? 0 : col;
    var _col_end   = (col == -1) ? global.grid_cols-1 : col;
    var _row_start = (row == -1) ? 0 : row;
    var _row_end   = (row == -1) ? global.grid_rows-1 : row;

    var _last = -1;
    var _bak_log = global._evt_log_enabled;   // 记录日志开关
    global._evt_log_enabled = false;          // 创建不进帧尾日志（spawn_plant/card_created 手动广播）
    for (var _r = _row_start; _r <= _row_end; _r++) {
        for (var _c = _col_start; _c <= _col_end; _c++) {
            var _VM_id = ++global._VM_create_counter;
            if (global.network.mode == "client") {
                if (!_batch) return -_VM_id;
                continue;
            }
            if (_obj == obj_card_mod) { global._mod_pending_card_id = card_id; }
            var _plant = spawn_plant(_c, _r, _obj, _props);
            global._mod_pending_card_id = "";
            if (_plant < 0) continue;
			network_apply_plant_level(_plant);
            _last = _plant;
            _plant._VM_id = _VM_id;
            if (global.network.mode == "server") {
                ds_map_add(global._VM_id_to_real, _VM_id, _plant);
                var _nid = ds_map_exists(global.network.map_instance_id_net_id, _plant) ? global.network.map_instance_id_net_id[? _plant] : -1;
                if (_nid != -1) {
                    var _vm_prop = {};
                    _vm_prop[$ "_VM_id"] = _VM_id;
                    var _vm_json = json_stringify(_vm_prop);
                    var _list = global.network.connected_clients;
                    for (var _i = 0; _i < array_length(_list); _i++) {
                        send_message(_list[_i], MSG_MODIFY_PROP, _nid, _vm_json);
                    }
                }
            }
        }
    }

    global._evt_log_enabled = _bak_log;       // 还原
    global._VM_last_created_card = _last;
    if (_batch) return 0;
    return real(_last);
}

/// @function VM_SpawnEnemy(type, row, hp_override)
/// @return 敌人实例 ID
function VM_SpawnEnemy(type_addr, row_addr, hp_override_addr) {
    var type = vm_read_mem(global.__vm, type_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var hp_override = vm_read_mem(global.__vm, hp_override_addr);
    var _VM_id = ++global._VM_create_counter;
    if (global.network.mode == "client") return -_VM_id;
    var _info = global.enemy_map[? type];
    if (is_undefined(_info)) return -1;
    var _pos = get_world_position_from_grid(global.grid_cols, row);
    var _bak_log = global._evt_log_enabled;   // 记录日志开关
    global._evt_log_enabled = false;          // 创建不进帧尾日志（本函数手动广播）
    global._mod_pending_enemy_id = type;  // mod 敌人创建前写入 pending id
    var _enemy = instance_create_depth(_pos.x + 30, _pos.y + 38, 0, _info._obj);
    if (!is_undefined(hp_override) && hp_override > 0) {
        _enemy.hp = hp_override;
        _enemy.maxhp = hp_override;
    }
    _enemy._VM_id = _VM_id;
    global._evt_log_enabled = _bak_log;       // 还原
    if (global.network.mode == "server") {
        ds_map_add(global._VM_id_to_real, _VM_id, _enemy.id);
        add_net_id(_enemy.id);
        var _nid = global.network.map_instance_id_net_id[? _enemy.id];
        var _list = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_list); _i++)
            send_message(_list[_i], MSG_SPAWN_ENEMY, _nid, _pos.x + 30, _pos.y + 3, object_get_name(_info._obj));
        // 通过 MSG_MODIFY_PROP 同步 VM_id
        var _vm_prop = {};
        _vm_prop[$ "_VM_id"] = _VM_id;
        var _vm_json = json_stringify(_vm_prop);
        for (var _i = 0; _i < array_length(_list); _i++)
            send_message(_list[_i], MSG_MODIFY_PROP, _nid, _vm_json);
    }
    return real(_enemy.id);
}

/// @function VM_SpawnBoss(type, row, hp_override)
/// @return BOSS 实例 ID
function VM_SpawnBoss(type_addr, row_addr, hp_override_addr) {
    var type = vm_read_mem(global.__vm, type_addr);
    var row = vm_read_mem(global.__vm, row_addr);
    var hp_override = vm_read_mem(global.__vm, hp_override_addr);
    var _VM_id = ++global._VM_create_counter;
    if (global.network.mode == "client") return -_VM_id;
    var _info = global.enemy_map[? type];
    if (is_undefined(_info)) return -1;
    var _pos = get_world_position_from_grid(10, row);
    global._mod_pending_enemy_id = type;  // mod 敌人创建前写入 pending id
    var _boss = instance_create_depth(_pos.x - 80, _pos.y + 30, -200, _info._obj);
    if (!is_undefined(hp_override) && hp_override > 0) {
        _boss.hp = hp_override;
        _boss.maxhp = hp_override;
    }
    _boss._VM_id = _VM_id;
    obj_battle.boss_count++;
    if (global.network.mode == "server") {
        ds_map_add(global._VM_id_to_real, _VM_id, _boss.id);
        add_net_id(_boss.id);
        var _nid = global.network.map_instance_id_net_id[? _boss.id];
        var _list = global.network.connected_clients;
        for (var _i = 0; _i < array_length(_list); _i++)
            send_message(_list[_i], MSG_SPAWN_BOSS, _nid, _pos.x - 80, _pos.y + 30, object_get_name(_info._obj), _boss.hp, _boss.maxhp, row);
        // 通过 MSG_MODIFY_PROP 同步 VM_id
        var _vm_prop = {};
        _vm_prop[$ "_VM_id"] = _VM_id;
        var _vm_json = json_stringify(_vm_prop);
        for (var _i = 0; _i < array_length(_list); _i++)
            send_message(_list[_i], MSG_MODIFY_PROP, _nid, _vm_json);
    }
    global._VM_last_boss = _boss.id;
    return real(_boss.id);
}

// ============================================================
// VM 数组（脚本语言内命名数组，按名存取，跨块共享）
// ============================================================

/// @function VM_ArrayGet(ArrayName, Index)
/// @param ArrayName 数组名（字符串）
/// @param Index     下标（0 开始）
/// @return 元素值；数组不存在或下标越界返回 0
function VM_ArrayGet(name_addr, index_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _index = vm_read_mem(global.__vm, index_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) return 0;
    var _arr = _vm.arrays[? _name];
    if (_index < 0 || _index >= array_length(_arr)) return 0;
    return _arr[_index];
}

/// @function VM_ArraySet(ArrayName, Index, val)
/// @param ArrayName 数组名（字符串，不存在时自动创建）
/// @param Index     下标（0 开始，超出当前长度时自动补 0 扩容）
/// @param val       要写入的值（int / float / string）
function VM_ArraySet(name_addr, index_addr, val_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _index = vm_read_mem(global.__vm, index_addr);
    var _val = vm_read_mem(global.__vm, val_addr);
    if (_index < 0) return;
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) ds_map_add(_vm.arrays, _name, []);
    var _arr = _vm.arrays[? _name];
    while (array_length(_arr) <= _index) array_push(_arr, 0);   // 自动扩容，空位补 0
    _arr[_index] = _val;
}

/// @function VM_ArrayDel(ArrayName, Index)
/// @param ArrayName 数组名
/// @param Index     要删除的下标（越界无操作）
/// @desc 删除后后面的元素前移，长度减 1
function VM_ArrayDel(name_addr, index_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _index = vm_read_mem(global.__vm, index_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) return;
    var _arr = _vm.arrays[? _name];
    if (_index < 0 || _index >= array_length(_arr)) return;
    array_delete(_arr, _index, 1);
}

/// @function VM_ArrayADD(ArrayName, val)
/// @param ArrayName 数组名（不存在时自动创建）
/// @param val       要追加的值（int / float / string）
/// @desc 在数组末尾追加一个元素
function VM_ArrayADD(name_addr, val_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _val = vm_read_mem(global.__vm, val_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) ds_map_add(_vm.arrays, _name, []);
    var _arr = _vm.arrays[? _name];
    array_push(_arr, _val);
}

/// @function VM_ArraySize(ArrayName)
/// @param ArrayName 数组名
/// @return 数组长度；数组不存在返回 0
function VM_ArraySize(name_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) return 0;
    return array_length(_vm.arrays[? _name]);
}

/// @function VM_ArrayClear(ArrayName)
/// @param ArrayName 数组名
/// @desc 清空数组，长度归 0；数组不存在时无操作
function VM_ArrayClear(name_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) return;
    array_resize(_vm.arrays[? _name], 0);
}

/// @function VM_ArrayClearAll()
/// @desc 清空所有数组
function VM_ArrayClearAll() {
    ds_map_clear(global.__vm.arrays);
}

// ============================================================
// 辅助：从内存地址读取值（按类型转换）
// ============================================================
function vm_read_mem(vm, addr) {
    var _type = vm.mem_type[addr];
    if (_type == VM_TYPE_STRING) {
        var _idx = vm.mem_val[addr];
        if (_idx >= 0 && _idx < array_length(vm.strings))
            return vm.strings[_idx];
        return "";
    }
    return vm.mem_val[addr];
}

/// @function vm_arg(_addr)
/// @desc 通知路径参数已是值，字节码路径参数是内存地址
function vm_arg(_addr) {
    if (global._VM_notify_call) return _addr;
    return vm_read_mem(global.__vm, _addr);
}

/// @function VM_Execute(vm, buf, name)
function VM_Execute(vm, buf, name) {
    if (!buffer_exists(buf)) return 0;
    try {
        if (global._VM_debug_mode && (global._VM_debug_block == "" || name == global._VM_debug_block)) return VM_Execute_debug(vm, buf, name);
        buffer_seek(buf, buffer_seek_start, 0);
        var _size = buffer_get_size(buf);
        var _mt = vm.mem_type;
        var _mv = vm.mem_val;
        var _mlen = array_length(_mt);

        while (buffer_tell(buf) < _size) {
            var _op = buffer_read(buf, buffer_u8);

            switch (_op) {

                // ==================== ASSIGN ====================
                case VM_OP_ASSIGN: {
                    var _dst = buffer_read(buf, buffer_s32);
                    var _type = buffer_read(buf, buffer_u8);
                    if (_type == VM_TYPE_STRING) {
                        _mt[_dst] = VM_TYPE_STRING;
                        _mv[_dst] = buffer_read(buf, buffer_u16);
                    } else  if (_type == VM_TYPE_FLOAT) {
						_mt[_dst] = VM_TYPE_FLOAT
						_mv[_dst] = buffer_read(buf, buffer_f32)
					} else {
						_mt[_dst] = _type
						_mv[_dst] = buffer_read(buf, buffer_s32)
					}

                    break;
                }

                // ==================== COPY ====================
                case VM_OP_COPY: {
                    var _dst = buffer_read(buf, buffer_s32);
                    var _src = buffer_read(buf, buffer_s32);
                    _mt[_dst] = _mt[_src];
                    _mv[_dst] = _mv[_src];
                    break;
                }

                // ==================== 算术 ====================
                case VM_OP_ADD: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _res = _mv[_a] + _mv[_b];
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _res;
                    break;
                }
                case VM_OP_SUB: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _mv[_a] - _mv[_b];
                    break;
                }
                case VM_OP_MUL: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _mv[_a] * _mv[_b];
                    break;
                }
                case VM_OP_DIV: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _int_div = (_mt[_a] != VM_TYPE_FLOAT && _mt[_b] != VM_TYPE_FLOAT);
                    if (_mv[_b] == 0) {
                        _mt[_d] = _int_div ? VM_TYPE_INT : VM_TYPE_FLOAT;
                        _mv[_d] = 0;
                    } else if (_int_div) {
                        _mt[_d] = VM_TYPE_INT;
                        _mv[_d] = _mv[_a] div _mv[_b];
                    } else {
                        _mt[_d] = VM_TYPE_FLOAT;
                        _mv[_d] = _mv[_a] / _mv[_b];
                    }
                    break;
                }
                case VM_OP_MOD: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_b] != 0) ? _mv[_a] mod _mv[_b] : 0;
                    break;
                }

                // ==================== 比较 ====================
                case VM_OP_EQ: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] == _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_NEQ: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] != _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_GT: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] > _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_GTE: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] >= _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_LT: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] < _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_LTE: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] <= _mv[_b]) ? 1 : 0;
                    break;
                }

                // ==================== CALL ====================
                case VM_OP_CALL: {
                    var _func_id = buffer_read(buf, buffer_u16);
                    var _arg_count = buffer_read(buf, buffer_u8);
                    var _dst = buffer_read(buf, buffer_s32);
                    var _args = array_create(_arg_count);

                    for (var _i = 0; _i < _arg_count; _i++) {
                        var _addr = buffer_read(buf, buffer_s32);
                        _args[_i] = _addr;
                    }

                    if (_func_id < 0 || _func_id >= array_length(vm.functions)) {
                        shell_print("VM Error: 未注册函数ID " + string(_func_id));
                        return -1;
                    }

                    var _fn = vm.functions[_func_id];
                    var _result = script_execute_ext(_fn, _args);

                    if (_dst != VM_DST_VOID) {
                        if (is_string(_result)) {
                            _mt[_dst] = VM_TYPE_STRING;
                            var _idx;
                            if (ds_map_exists(vm.str_map, _result)) {
                                _idx = vm.str_map[? _result];
                            } else {
                                _idx = array_length(vm.strings);
                                array_push(vm.strings, _result);
                                vm.str_map[? _result] = _idx;
                            }
                            _mv[_dst] = _idx;
                        } else if (is_real(_result)) {
                            if (floor(_result) == _result) {
                                _mt[_dst] = VM_TYPE_INT;
                            } else {
                                _mt[_dst] = VM_TYPE_FLOAT;
                            }
                            _mv[_dst] = _result;
                        } else {
                            _mt[_dst] = VM_TYPE_INT;
                            _mv[_dst] = _result;
                        }
                    }
                    break;
                }

                // ==================== IF ====================
                case VM_OP_IF: {
                    var _cond_addr = buffer_read(buf, buffer_s32);
                    var _true_ip = buffer_read(buf, buffer_s32);
                    var _false_ip = buffer_read(buf, buffer_s32);

                    var _cond = _mv[_cond_addr];
                    if (!is_real(_cond)) _cond = 0;

                    if (_cond != 0) {
                        if (_true_ip != -1) {
                            buffer_seek(buf, buffer_seek_start, _true_ip);
                            continue;
                        }
                    } else {
                        if (_false_ip != -1) {
                            buffer_seek(buf, buffer_seek_start, _false_ip);
                            continue;
                        }
                    }
                    break;
                }

                // ==================== JMP ====================
                case VM_OP_JMP: {
                    var _ip = buffer_read(buf, buffer_s32);
                    buffer_seek(buf, buffer_seek_start, _ip);
                    continue;
                }

                // ==================== HALT ====================
                case VM_OP_HALT: {
                    return 0;
                }

                default: {
                    shell_print("VM Error: 未知操作码 " + string(_op));
                    return -1;
                }
            }
        }

        return 0;

    } catch (_err) {
        shell_print("VM Error: " + string(_err));
        return -1;
    }
}

/// @function VM_Execute_debug(vm, buf, name) [DEBUG VERSION]
function VM_Execute_debug(vm, buf, name) {
    if (!buffer_exists(buf)) return 0;
    shell_print("[VM] ==== " + name + " ====");
    try {
        buffer_seek(buf, buffer_seek_start, 0);
        var _size = buffer_get_size(buf);
        var _mt = vm.mem_type;
        var _mv = vm.mem_val;

        while (buffer_tell(buf) < _size) {
            var _pos = buffer_tell(buf);
            var _op = buffer_read(buf, buffer_u8);

            switch (_op) {

                case VM_OP_ASSIGN: {
                    var _dst = buffer_read(buf, buffer_s32);
                    var _type = buffer_read(buf, buffer_u8);
                    if (_type == VM_TYPE_STRING) {
                        var _val = buffer_read(buf, buffer_u16);
                        _mt[_dst] = VM_TYPE_STRING;
                        _mv[_dst] = _val;
                        shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = str" + string(_val) + " \"" + vm.strings[_val] + "\"");
                    } else if (_type == VM_TYPE_FLOAT) {
                        var _val = buffer_read(buf, buffer_f32);
                        _mt[_dst] = VM_TYPE_FLOAT;
                        _mv[_dst] = _val;
                        shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = " + string(_val) + "f");
                    } else {
                        var _val = buffer_read(buf, buffer_s32);
                        _mt[_dst] = _type;
                        _mv[_dst] = _val;
                        shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = " + string(_val));
                    }
                    break;
                }

                case VM_OP_COPY: {
                    var _dst = buffer_read(buf, buffer_s32);
                    var _src = buffer_read(buf, buffer_s32);
                    var _src_val = _mv[_src];
                    _mt[_dst] = _mt[_src];
                    _mv[_dst] = _src_val;
                    shell_print("[" + string(_pos) + "] COPY  var" + string(_dst) + " = var" + string(_src) + " (" + string(_src_val) + ")");
                    break;
                }

                case VM_OP_ADD: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b]; var _res = _av + _bv;
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _res;
                    shell_print("[" + string(_pos) + "] ADD   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") + var" + string(_b) + "(" + string(_bv) + ") = " + string(_res));
                    break;
                }
                case VM_OP_SUB: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _av - _bv;
                    shell_print("[" + string(_pos) + "] SUB   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") - var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_MUL: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _av * _bv;
                    shell_print("[" + string(_pos) + "] MUL   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") * var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_DIV: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    var _int_div = (_mt[_a] != VM_TYPE_FLOAT && _mt[_b] != VM_TYPE_FLOAT);
                    if (_bv == 0) {
                        _mt[_d] = _int_div ? VM_TYPE_INT : VM_TYPE_FLOAT;
                        _mv[_d] = 0;
                        shell_print("[" + string(_pos) + "] DIV   var" + string(_d) + " = var" + string(_a) + " / var" + string(_b) + " = 0 (div0)");
                    } else if (_int_div) {
                        _mt[_d] = VM_TYPE_INT;
                        _mv[_d] = _av div _bv;
                        shell_print("[" + string(_pos) + "] DIV   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") div var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    } else {
                        _mt[_d] = VM_TYPE_FLOAT;
                        _mv[_d] = _av / _bv;
                        shell_print("[" + string(_pos) + "] DIV   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") / var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    }
                    break;
                }
                case VM_OP_MOD: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_bv != 0) ? _av mod _bv : 0;
                    shell_print("[" + string(_pos) + "] MOD   var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") % var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    break;
                }

                case VM_OP_EQ: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_av == _bv) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] EQ    var" + string(_d) + " = var" + string(_a) + "(" + string(_av) + ") == var" + string(_b) + "(" + string(_bv) + ") = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_NEQ: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    var _av = _mv[_a]; var _bv = _mv[_b];
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_av != _bv) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] NEQ   var" + string(_d) + " = var" + string(_a) + " != var" + string(_b) + " = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_GT: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] > _mv[_b]) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] GT    var" + string(_d) + " = var" + string(_a) + " > var" + string(_b) + " = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_GTE: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] >= _mv[_b]) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] GTE   var" + string(_d) + " = var" + string(_a) + " >= var" + string(_b) + " = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_LT: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] < _mv[_b]) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] LT    var" + string(_d) + " = var" + string(_a) + " < var" + string(_b) + " = " + string(_mv[_d]));
                    break;
                }
                case VM_OP_LTE: {
                    var _d = buffer_read(buf, buffer_s32);
                    var _a = buffer_read(buf, buffer_s32);
                    var _b = buffer_read(buf, buffer_s32);
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] <= _mv[_b]) ? 1 : 0;
                    shell_print("[" + string(_pos) + "] LTE   var" + string(_d) + " = var" + string(_a) + " <= var" + string(_b) + " = " + string(_mv[_d]));
                    break;
                }

                case VM_OP_CALL: {
                    var _func_id = buffer_read(buf, buffer_u16);
                    var _arg_count = buffer_read(buf, buffer_u8);
                    var _dst = buffer_read(buf, buffer_s32);
                    var _args = array_create(_arg_count);
                    var _arg_str = "";

                    for (var _i = 0; _i < _arg_count; _i++) {
                        var _addr = buffer_read(buf, buffer_s32);
                        _args[_i] = _addr;
                        var _val = vm_read_mem(vm, _addr);
                        _arg_str += " var" + string(_addr) + "(" + string(_val) + ")";
                    }

                    if (_func_id < 0 || _func_id >= array_length(vm.functions)) {
                        shell_print("VM Error: bad func_id " + string(_func_id));
                        return -1;
                    }

                    var _fn = vm.functions[_func_id];
                    var _result = script_execute_ext(_fn, _args);
                    var _dst_str = (_dst == VM_DST_VOID) ? "void" : "var" + string(_dst);

                    shell_print("[" + string(_pos) + "] CALL  func" + string(_func_id) + " (" + script_get_name(_fn) + ")" + _arg_str + " -> " + _dst_str + " = " + string(_result));

                    if (_dst != VM_DST_VOID) {
                        if (is_string(_result)) {
                            _mt[_dst] = VM_TYPE_STRING;
                            var _idx;
                            if (ds_map_exists(vm.str_map, _result)) {
                                _idx = vm.str_map[? _result];
                            } else {
                                _idx = array_length(vm.strings);
                                array_push(vm.strings, _result);
                                vm.str_map[? _result] = _idx;
                            }
                            _mv[_dst] = _idx;
                        } else if (is_real(_result)) {
                            if (floor(_result) == _result) {
                                _mt[_dst] = VM_TYPE_INT;
                            } else {
                                _mt[_dst] = VM_TYPE_FLOAT;
                            }
                            _mv[_dst] = _result;
                        } else {
                            _mt[_dst] = VM_TYPE_INT;
                            _mv[_dst] = _result;
                        }
                    }
                    break;
                }

                case VM_OP_IF: {
                    var _cond_addr = buffer_read(buf, buffer_s32);
                    var _true_ip = buffer_read(buf, buffer_s32);
                    var _false_ip = buffer_read(buf, buffer_s32);

                    var _cond = _mv[_cond_addr];
                    if (!is_real(_cond)) _cond = 0;

                    shell_print("[" + string(_pos) + "] IF    var" + string(_cond_addr) + "(" + string(_cond) + ") ? goto " + string(_true_ip) + " : goto " + string(_false_ip));

                    if (_cond != 0) {
                        if (_true_ip != -1) {
                            buffer_seek(buf, buffer_seek_start, _true_ip);
                            continue;
                        }
                    } else {
                        if (_false_ip != -1) {
                            buffer_seek(buf, buffer_seek_start, _false_ip);
                            continue;
                        }
                    }
                    break;
                }

                case VM_OP_JMP: {
                    var _ip = buffer_read(buf, buffer_s32);
                    shell_print("[" + string(_pos) + "] JMP   goto " + string(_ip));
                    buffer_seek(buf, buffer_seek_start, _ip);
                    continue;
                }

                case VM_OP_HALT: {
                    shell_print("[" + string(_pos) + "] HALT");
                    return 0;
                }

                default: {
                    shell_print("VM Error: unknown op " + string(_op));
                    return -1;
                }
            }
        }
        return 0;
    } catch (_err) {
        shell_print("VM Error: " + string(_err));
        return -1;
    }
}

/// @function VM_DumpBlock(_block_name)
/// @desc 打印指定块的字节码（纯反汇编：只读取，不执行、不修改 VM 状态）
/// @param {String} _block_name  块名，如 "_VM_WAVE_START"
function VM_DumpBlock(_block_name) {
    var _buf = global[$ _block_name];
    if (!buffer_exists(_buf)) {
        shell_print("[VM] dump: block not loaded: " + _block_name);
        return;
    }
    shell_print("[VM] ==== bytecode of " + _block_name + " (" + string(buffer_get_size(_buf)) + " bytes) ====");
    buffer_seek(_buf, buffer_seek_start, 0);
    var _size = buffer_get_size(_buf);
    var _strings = global._VM_strings;

    while (buffer_tell(_buf) < _size) {
        var _pos = buffer_tell(_buf);
        var _op = buffer_read(_buf, buffer_u8);

        switch (_op) {
            case VM_OP_ASSIGN: {
                var _dst = buffer_read(_buf, buffer_s32);
                var _type = buffer_read(_buf, buffer_u8);
                if (_type == VM_TYPE_STRING) {
                    var _val = buffer_read(_buf, buffer_u16);
                    var _str = (_val >= 0 && _val < array_length(_strings)) ? _strings[_val] : "?";
                    shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = str" + string(_val) + " \"" + _str + "\"");
                } else if (_type == VM_TYPE_FLOAT) {
                    var _val = buffer_read(_buf, buffer_f32);
                    shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = " + string(_val) + "f");
                } else {
                    var _val = buffer_read(_buf, buffer_s32);
                    shell_print("[" + string(_pos) + "] ASSIGN var" + string(_dst) + " = " + string(_val) + " (type " + string(_type) + ")");
                }
                break;
            }

            case VM_OP_COPY: {
                var _dst = buffer_read(_buf, buffer_s32);
                var _src = buffer_read(_buf, buffer_s32);
                shell_print("[" + string(_pos) + "] COPY  var" + string(_dst) + " = var" + string(_src));
                break;
            }

            case VM_OP_ADD:
            case VM_OP_SUB:
            case VM_OP_MUL:
            case VM_OP_DIV:
            case VM_OP_MOD:
            case VM_OP_EQ:
            case VM_OP_NEQ:
            case VM_OP_GT:
            case VM_OP_GTE:
            case VM_OP_LT:
            case VM_OP_LTE: {
                var _op_names = ["ADD", "SUB", "MUL", "DIV", "MOD", "EQ", "NEQ", "GT", "GTE", "LT", "LTE"];
                var _d = buffer_read(_buf, buffer_s32);
                var _a = buffer_read(_buf, buffer_s32);
                var _b = buffer_read(_buf, buffer_s32);
                shell_print("[" + string(_pos) + "] " + _op_names[_op - VM_OP_ADD] + "   var" + string(_d) + " = var" + string(_a) + " op var" + string(_b));
                break;
            }

            case VM_OP_CALL: {
                var _func_id = buffer_read(_buf, buffer_u16);
                var _arg_count = buffer_read(_buf, buffer_u8);
                var _dst = buffer_read(_buf, buffer_s32);
                var _arg_str = "";
                for (var _i = 0; _i < _arg_count; _i++) {
                    var _addr = buffer_read(_buf, buffer_s32);
                    _arg_str += " var" + string(_addr);
                }
                var _dst_str = (_dst == VM_DST_VOID) ? "void" : "var" + string(_dst);
                var _fname = "";
                if (_func_id >= 0 && _func_id < array_length(global.__vm.functions)) {
                    _fname = script_get_name(global.__vm.functions[_func_id]);
                }
                shell_print("[" + string(_pos) + "] CALL  func" + string(_func_id) + (_fname != "" ? " (" + _fname + ")" : "") + _arg_str + " -> " + _dst_str);
                break;
            }

            case VM_OP_IF: {
                var _cond_addr = buffer_read(_buf, buffer_s32);
                var _true_ip = buffer_read(_buf, buffer_s32);
                var _false_ip = buffer_read(_buf, buffer_s32);
                shell_print("[" + string(_pos) + "] IF    var" + string(_cond_addr) + " ? goto " + string(_true_ip) + " : goto " + string(_false_ip));
                break;
            }

            case VM_OP_JMP: {
                var _ip = buffer_read(_buf, buffer_s32);
                shell_print("[" + string(_pos) + "] JMP   goto " + string(_ip));
                break;
            }

            case VM_OP_HALT: {
                shell_print("[" + string(_pos) + "] HALT");
                break;
            }

            default: {
                shell_print("[" + string(_pos) + "] ? unknown op " + string(_op));
                return;
            }
        }
    }
}

/// @function VM_ReadMem(_addr)
/// @desc 读取指定内存地址的值并打印（类型 + 值，字符串会展开内容）
/// @param {Real} _addr  内存地址，如 0、5
/// @returns {String} 格式化结果，供 shell 命令直接返回
function VM_ReadMem(_addr) {
    var _vm = global.__vm;
    if (_addr < 0 || _addr >= array_length(_vm.mem_val)) {
        shell_print("[VM] readmem: bad addr " + string(_addr) + " (mem size " + string(array_length(_vm.mem_val)) + ")");
        return "bad addr " + string(_addr) + " (mem size " + string(array_length(_vm.mem_val)) + ")";
    }
    var _t = _vm.mem_type[_addr];
    var _v = _vm.mem_val[_addr];
    var _line = "";
    if (_t == VM_TYPE_STRING) {
        var _s = (_v >= 0 && _v < array_length(_vm.strings)) ? _vm.strings[_v] : "?";
        _line = "mem[" + string(_addr) + "] = STRING str" + string(_v) + " \"" + string(_s) + "\"";
    } else if (_t == VM_TYPE_FLOAT) {
        _line = "mem[" + string(_addr) + "] = FLOAT " + string(_v);
    } else if (_t == VM_TYPE_INT) {
        _line = "mem[" + string(_addr) + "] = INT " + string(_v);
    } else {
        _line = "mem[" + string(_addr) + "] = type " + string(_t) + " val " + string(_v);
    }
    shell_print("[VM] " + _line);
    return _line;
}

/// @function VM_DumpStrings()
/// @desc 打印 VM 字符串池的全部内容
function VM_DumpStrings() {
    var _strings = global.__vm.strings;
    shell_print("[VM] ==== string pool (" + string(array_length(_strings)) + " entries) ====");
    for (var _i = 0; _i < array_length(_strings); _i++) {
        shell_print("[VM] str" + string(_i) + " = \"" + string(_strings[_i]) + "\"");
    }
}

/// @function VM_CallFunc(_func_id, _args)
/// @desc 从 shell 直接调用已注册的 VM 函数，参数为内存地址（与字节码 CALL 一致）
/// @param {Real|String} _func_id  函数 ID（见 VM_RegisterFunction 注册顺序）或函数名（如 "VM_SpawnEnemy"）
/// @param {Array} _args  参数数组，每个元素是内存地址
/// @returns {String} 调用结果描述
function VM_CallFunc(_func_id, _args) {
    var _vm = global.__vm;
    // 支持按函数名调用
    if (is_string(_func_id)) {
        var _target = _func_id;
        _func_id = -1;
        for (var _i = 0; _i < array_length(_vm.functions); _i++) {
            if (script_get_name(_vm.functions[_i]) == _target) { _func_id = _i; break; }
        }
        if (_func_id == -1) {
            shell_print("[VM] call: unknown func name: " + _target);
            return "unknown func name: " + _target;
        }
    }
    if (_func_id < 0 || _func_id >= array_length(_vm.functions)) {
        return "bad func_id " + string(_func_id) + " (registered " + string(array_length(_vm.functions)) + " funcs)";
    }
    var _fn = _vm.functions[_func_id];
    var _result = script_execute_ext(_fn, _args);
    var _name = script_get_name(_fn);
    shell_print("[VM] call func" + string(_func_id) + " (" + _name + ") args[" + string(array_length(_args)) + "] = " + string(_result));
    return "func" + string(_func_id) + " (" + _name + ") = " + string(_result);
}

/// @function VM_SetMem(_addr, _value, _type = "")
/// @desc 写入 VM 内存，供 vmcall 自由传参
/// @param {Real} _addr  内存地址
/// @param {Any} _value  值
/// @param {String} _type  "" 自动判断(含小数点→float，否则int) / "int" / "float" / "string"(纯数字=str池索引，否则按内容查池，查不到追加)
/// @returns {String} 写入后的内存描述
function VM_SetMem(_addr, _value, _type = "") {
    var _vm = global.__vm;
    if (_addr < 0 || _addr >= array_length(_vm.mem_val)) {
        shell_print("[VM] setmem: bad addr " + string(_addr) + " (mem size " + string(array_length(_vm.mem_val)) + ")");
        return "bad addr " + string(_addr) + " (mem size " + string(array_length(_vm.mem_val)) + ")";
    }
    if (_type == "string" || _type == "str") {
        var _idx = real(_value);
        if (is_real(_idx) && string(_idx) != "NaN" && floor(_idx) == _idx) {
            if (_idx < 0 || _idx >= array_length(_vm.strings)) return "bad str index " + string(_idx) + " (pool size " + string(array_length(_vm.strings)) + ")";
            _vm.mem_type[_addr] = VM_TYPE_STRING;
            _vm.mem_val[_addr] = _idx;
        } else {
            var _s = string(_value);
            var _found = -1;
            for (var _i = 0; _i < array_length(_vm.strings); _i++) {
                if (_vm.strings[_i] == _s) { _found = _i; break; }
            }
            if (_found == -1) {
                _found = array_length(_vm.strings);
                array_push(_vm.strings, _s);
                _vm.str_map[? _s] = _found;
            }
            _vm.mem_type[_addr] = VM_TYPE_STRING;
            _vm.mem_val[_addr] = _found;
        }
        return VM_ReadMem(_addr);
    }
    var _v = real(_value);
    if (!is_real(_v) || string(_v) == "NaN") return "bad value (不是数字): " + string(_value);
    if (_type == "float" || _type == "f" || (_type == "" && string_pos(".", string(_value)) > 0)) {
        _vm.mem_type[_addr] = VM_TYPE_FLOAT;
    } else {
        _vm.mem_type[_addr] = VM_TYPE_INT;
    }
    _vm.mem_val[_addr] = _v;
    return VM_ReadMem(_addr);
}

/// @function VM_MetaInfo()
/// @desc 打印 VM 元信息：随机种子、波次、上个卡片/敌人/平台/按钮/按键、限制、debug 状态等
/// @returns {String} 提示文案
function VM_MetaInfo() {
    var _g = function(_name) {
        return variable_global_exists(_name) ? string(global[$ _name]) : "unset";
    };
    var _vm = global.__vm;
    shell_print("[VM] ==== meta info ====");
    shell_print("[VM] rng_state = " + string(variable_struct_get(_vm, "rng_state")));
    if (instance_exists(obj_battle)) {
        shell_print("[VM] wave = " + string(obj_battle.current_wave) + " (prev " + _g("_VM_prev_wave") + ")");
        shell_print("[VM] subwave = " + string(obj_battle.current_subwave) + " (prev " + _g("_VM_prev_subwave") + ")");
    }
    shell_print("[VM] last_created_card = " + _g("_VM_last_created_card"));
    shell_print("[VM] last_destroyed_card = " + _g("_VM_last_destroyed_card"));
    shell_print("[VM] last_created_enemy = " + _g("_VM_last_created_enemy"));
    shell_print("[VM] last_killed_enemy = " + _g("_VM_last_killed_enemy"));
    shell_print("[VM] last_idle_platform = " + _g("_VM_last_idle_platform"));
    shell_print("[VM] last_clicked_button = " + _g("_VM_last_clicked_button"));
    shell_print("[VM] last_key = " + _g("_VM_last_key"));
    shell_print("[VM] card_level_cap = " + _g("_VM_card_level_cap") + ", max_slots = " + _g("_VM_max_slots"));
    if (variable_global_exists("banned_gems_online")) {
        shell_print("[VM] banned_gems = [" + string(global.banned_gems_online) + "]");
    }
    if (variable_global_exists("banned_cards_online")) {
        var _bc = ds_map_keys_to_array(global.banned_cards_online);
        shell_print("[VM] banned_cards(" + string(array_length(_bc)) + ") = [" + string(_bc) + "]");
    }
    shell_print("[VM] battle_start_done = " + _g("_VM_battle_start_done") + ", room_ready_done = " + _g("_VM_room_ready_done"));
    shell_print("[VM] event_enabled = " + _g("_VM_event_enabled") + ", sync_exec = " + _g("_VM_sync_exec"));
    shell_print("[VM] mem = " + string(array_length(_vm.mem_val)) + ", strings = " + string(array_length(_vm.strings)) + ", functions = " + string(array_length(_vm.functions)));
    shell_print("[VM] debug_mode = " + string(global._VM_debug_mode) + (global._VM_debug_block != "" ? " (block: " + global._VM_debug_block + ")" : ""));
    return "已输出到控制台";
}

/// @function VM_ShowTerrain()
/// @desc 打印当前地形网格（. = normal 陆地，W = water 水域，# = obstacle 障碍）
/// @returns {String} 提示文案
function VM_ShowTerrain() {
	try{
	    var _rows = min(global.grid_rows, array_length(global.grid_terrains));
	    var _cols = min(global.grid_cols, array_length(global.grid_terrains[0]));
	    shell_print("[VM] ==== terrain (" + string(_rows) + "x" + string(_cols) + ") ====");
	    shell_print("[VM] legend: n = normal(陆地)  w = water(水域)  o = obstacle(障碍)");
	    // 列号行（列超过 10 显示个位数）
	    var _header = "";
	    for (var _hc = 0; _hc < _cols; _hc++) {
	        _header += string(_hc mod 10);
	    }
	    shell_print("[VM]    " + _header);
	    for (var _r = 0; _r < _rows; _r++) {
	        var _line = "";
	        var _rc = min(global.grid_cols, array_length(global.grid_terrains[_r]));
	        for (var _c = 0; _c < _rc; _c++) {
	            var _t = global.grid_terrains[_r][_c].type;
	            if (_t == "water") {
	                _line += "w";
	            } else if (_t == "obstacle") {
	                _line += "o";
	            } else {
	                _line += "n";
	            }
	        }
	        shell_print("[VM] r" + string(_r) + "  " + _line);
	    }
	}
    return "已输出到控制台";
}


/// @function VM_conveyor_belt_able(enable)
/// @desc 启用/关闭传送带模式
/// @param enable 1=开启传送带 0=关闭
function VM_conveyor_belt_able(enable_addr) {
	var enable = vm_read_mem(global.__vm, enable_addr);
	global._VM_conveyor_belt = (enable != 0);
}

/// @function VM_Slot_add(card_id)
/// @desc 向传送带添加一张卡片：按 card_id 创建卡槽实例并加入传送带队列
/// @param card_id 卡片 ID（字符串），游戏内注册的任意卡片，与玩家是否解锁无关
/// @return 卡槽实例 ID，失败返回 -1
function VM_Slot_add(card_id_addr) {
	var card_id = vm_read_mem(global.__vm, card_id_addr);
	// 直接从玩家卡池取卡牌数据，不限于出战卡组
	var card_data = deck_get_card_data(card_id, 0);
	if (card_data == noone) return -1;
	var n = array_length(global._VM_conveyor_belt_arr);
	if(n>14)return noone;
	var inst = instance_create_depth(535 + 15 * 90, 90, -5, obj_card_slot);
	inst.cost = 0;
	inst.cooldown = 0;
	inst.card_obj = card_data[? "obj"];
	inst.card_spr = card_data[? "sprite"];
	inst.place_preview = card_data[? "place_preview"]
	inst.description = card_data[? "description"];
	inst.slot_index = n + 1;
	inst.card_id = card_id;
	inst.shape = card_data[? "shape"]; // 存储形态信息
	inst.depth = -2000
	//show_debug_message("植物卡槽已生成，id：" + inst.card_id)
	array_push(global._VM_conveyor_belt_arr,inst);
	return real(inst.id);
}

// ============================================================
// 全局初始化和块管理
// ============================================================
global.banned_cards_online = ds_map_create();
global.banned_gems_online = [];
global._VM_card_level_cap = -1;
global._VM_card_shape_cap = -1;
global._VM_card_skill_cap = -1;
global._VM_max_slots = -1;
global._VM_shovel_flame_rate = -1;   // 铲子返还火苗系数：-1=原逻辑，0~1=VM 指定
global._VM_strings = [];

global._VM_ROOM_READY_ENTRY = undefined;
global._VM_room_ready_done   = false;
global._VM_BATTLE_START        = undefined;
global._VM_battle_start_done    = false;
global._VM_CARD_CREATED      = undefined;
global._VM_CARD_DESTROYED    = undefined;
global._VM_CARD_DAMAGED      = undefined;
global._VM_ENEMY_SPAWNED     = undefined;
global._VM_ENEMY_KILLED      = undefined;
global._VM_ENEMY_DAMAGED     = undefined;
global._VM_WAVE_START        = undefined;
global._VM_WAVE_END          = undefined;
global._VM_SUBWAVE_START     = undefined;
global._VM_SUBWAVE_END       = undefined;
global._VM_PLAYER_DAMAGED    = undefined;
global._VM_PLATFORM_IDLE_END = undefined;
global._VM_MOUSE_LEFT        = undefined;
global._VM_MOUSE_RIGHT       = undefined;
global._VM_KEY_PRESSED       = undefined;
global._VM_BUTTON_CLICKED    = undefined;
global._VM_CARD_PREVIEW_PICKED = undefined;
global._VM_BOSS_STATE_CHANGE = undefined;
global._VM_last_clicked_button = -1;
global._VM_FRAME             = undefined;
global._VM_TIMER_5f          = undefined;
global._VM_TIMER_10f         = undefined;
global._VM_TIMER_15f         = undefined;
global._VM_TIMER_30f         = undefined;
global._VM_TIMER_60f         = undefined;
global._VM_last_idle_platform = -1;


global._VM_prev_wave = -1 
global._VM_prev_subwave = -1
global._VM_event_enabled = true
global._VM_sync_exec = true    // VM_HandleNotify 中置 false，防止客户端守卫拦截同步调用

global._VM_debug_mode = false;
global._VM_debug_block = "";    // 非空时只打印指定块的调试日志
global._VM_notify_call = false;  // VM_HandleNotify "call" 分发期间为 true，参数按值传入
global._VM_loaded_sprite_indices = [];
global._VM_hook_queue = [];       // 待执行 hook 队列，每个元素 {buf, key, id, snap}
global._VM_sprite_cache      = ds_map_create();  // VM 永久贴图 name→id
global._VM_sprite_temp_cache = ds_map_create();  // VM 临时贴图 name→id，重载 bin 时清理

/// @function VM_BuildDeadSnap(inst)
/// @description 实例销毁瞬间做全量标量快照，供销毁类 hook 通过 VM_GetKilledProp 读取属性
function VM_BuildDeadSnap(_inst) {
    if (!instance_exists(_inst)) return undefined;
    var _snap = {};
    var _names = variable_instance_get_names(_inst);
    for (var _i = 0; _i < array_length(_names); _i++) {
        var _n = _names[_i];
        var _v = variable_instance_get(_inst, _n);
        if (is_real(_v) || is_string(_v) || is_bool(_v)) {
            _snap[$ _n] = _v;
        } else if (is_array(_v)) {
            _snap[$ _n] = variable_clone(_v, true);
        }
    }
    // 内置变量不会被 variable_instance_get_names 返回，需显式补记
    var _builtins = ["x", "y", "xstart", "ystart", "depth", "image_index", "image_speed", "sprite_index", "object_index", "direction", "speed"];
    for (var _b = 0; _b < array_length(_builtins); _b++) {
        _snap[$ _builtins[_b]] = variable_instance_get(_inst, _builtins[_b]);
    }
    _snap[$ "_object_name"] = object_get_name(_inst.object_index);
    return _snap;
}

function VM_QueueHook(buf, key, id) {
    if (!buffer_exists(buf)) return;
    var _snap = undefined;
    if (key == "card_del" || key == "enemy_kill") {
        _snap = VM_BuildDeadSnap(id);
        if (_snap != undefined) ds_map_add(global._VM_dead_snaps, id, _snap);
    }
    array_push(global._VM_hook_queue, {buf: buf, key: key, id: id, snap: _snap});
}

/**
 * Function Description
 */
function VM_FlushHooks() {
    while (array_length(global._VM_hook_queue) > 0) {
        var _q = global._VM_hook_queue;
        global._VM_hook_queue = [];
        for (var _i = 0; _i < array_length(_q); _i++) {
            var _e = _q[_i];
            if (_e.key == "card")        global._VM_last_created_card   = _e.id;
            if (_e.key == "card_del")    global._VM_last_destroyed_card = _e.id;
            if (_e.key == "enemy")       global._VM_last_created_enemy  = _e.id;
            if (_e.key == "enemy_kill")  global._VM_last_killed_enemy   = _e.id;
            if (_e.key == "platform_idle") global._VM_last_idle_platform = _e.id;
            var _hook_name = "";
            switch (_e.key) {
                case "card":            _hook_name = "_VM_CARD_CREATED";      break;
                case "card_del":        _hook_name = "_VM_CARD_DESTROYED";    break;
                case "enemy":           _hook_name = "_VM_ENEMY_SPAWNED";     break;
                case "enemy_kill":      _hook_name = "_VM_ENEMY_KILLED";      break;
                case "platform_idle":   _hook_name = "_VM_PLATFORM_IDLE_END"; break;
            }
            global._VM_cur_dead_snap = _e.snap;
            VM_Execute(global.__vm, _e.buf, _hook_name);
            global._VM_cur_dead_snap = undefined;
        }
    }
    ds_map_clear(global._VM_dead_snaps);   // 销毁逻辑处理完，释放快照
}

/// @function VM_HandleNotify(json)
/// @param {string} json  服务端发来的虚拟机通知 JSON
/// @description 处理服务端通过 MSG_VM_NOTIFY 下发的虚拟机通知
function VM_HandleNotify(json) {
    global._VM_sync_exec = false;
    var _data = json_parse(json);
    if (is_undefined(_data)) {
        show_debug_message("[VM_HandleNotify] JSON 解析失败: " + json);
        global._VM_sync_exec = true;
        return;
    }

    var _hook = _data[$ "hook"];
    if (is_undefined(_hook)) {
        show_debug_message("[VM_HandleNotify] 缺少 hook 字段: " + json);
        global._VM_sync_exec = true;
        return;
    }

    show_debug_message("[VM_HandleNotify] hook=" + _hook + " wave=" + string(_data[$ "wave"]) + " subwave=" + string(_data[$ "subwave"]));

    // 先同步波次状态
    var _wave = _data[$ "wave"];
    var _subwave = _data[$ "subwave"];
    if (!is_undefined(_wave)) {
        obj_battle.current_wave = _wave;
        global._VM_prev_wave = _wave;
    }
    if (!is_undefined(_subwave)) {
        obj_battle.current_subwave = _subwave;
        global._VM_prev_subwave = _subwave;
    }

    switch (_hook) {
        case "wave_start":
            if (buffer_exists(global._VM_WAVE_START)) VM_Execute(global.__vm, global._VM_WAVE_START, "_VM_WAVE_START");
            break;
        case "wave_end":
            if (buffer_exists(global._VM_WAVE_END)) VM_Execute(global.__vm, global._VM_WAVE_END, "_VM_WAVE_END");
            break;
        case "subwave_start":
            if (buffer_exists(global._VM_SUBWAVE_START)) VM_Execute(global.__vm, global._VM_SUBWAVE_START, "_VM_SUBWAVE_START");
            break;
        case "subwave_end":
            if (buffer_exists(global._VM_SUBWAVE_END)) VM_Execute(global.__vm, global._VM_SUBWAVE_END, "_VM_SUBWAVE_END");
            break;
        case "boss_state_change":
            if (!is_undefined(_data[$ "id"]))  global._VM_last_boss_state_change_id = _data[$ "id"];
            if (!is_undefined(_data[$ "old"])) global._VM_last_boss_old_state = _data[$ "old"];
            if (!is_undefined(_data[$ "new"])) global._VM_last_boss_new_state = _data[$ "new"];
            if (buffer_exists(global._VM_BOSS_STATE_CHANGE)) VM_Execute(global.__vm, global._VM_BOSS_STATE_CHANGE, "_VM_BOSS_STATE_CHANGE");
            break;
        case "call":
        {
            var _func = ds_map_find_value(global._VM_remote_funcs, _data[$ "func"]);
            if (is_undefined(_func)) {
                show_debug_message("[VM_HandleNotify] remote func not registered: " + _data[$ "func"]);
                break;
            }
            var _args = _data[$ "args"];
            var _old_flag = global._VM_notify_call;
            global._VM_notify_call = true;
            switch (array_length(_args)) {
                case 0: _func(); break;
                case 1: _func(_args[0]); break;
                case 2: _func(_args[0], _args[1]); break;
                case 3: _func(_args[0], _args[1], _args[2]); break;
                case 4: _func(_args[0], _args[1], _args[2], _args[3]); break;
                case 5: _func(_args[0], _args[1], _args[2], _args[3], _args[4]); break;
                case 6: _func(_args[0], _args[1], _args[2], _args[3], _args[4], _args[5]); break;
                default:
                    show_debug_message("[VM_HandleNotify] unsupported arg count: " + string(array_length(_args)));
                    break;
            }
            global._VM_notify_call = _old_flag;
            break;
        }
        default:
            show_debug_message("[VM_HandleNotify] unknown hook: " + _hook);
            break;
    }
    global._VM_sync_exec = true;
}

global._VM_last_damaged_player = -1;
global._VM_last_damaged_enemy = -1;
global._VM_last_damaged_card  = -1;

global._VM_last_boss          = -1;
global._VM_last_created_enemy = -1;
global._VM_last_killed_enemy  = -1;
global._VM_last_created_card  = -1;
global._VM_last_destroyed_card = -1;
global._VM_last_boss_state_change_id = -1;
global._VM_last_boss_old_state = -1;
global._VM_last_boss_new_state = -1;
global._VM_create_counter = 100000;
global._VM_id_to_real      = ds_map_create();  // VM_id → 真实 instance id
global._VM_real_to_vm_id   = ds_map_create();  // 真实 instance id → VM_id (客户端反向)
global._VM_dead_snaps      = ds_map_create();  // 销毁实例属性快照 real id → struct，flush 后清空
global._VM_cur_dead_snap   = undefined;        // 当前 flush 中销毁事件的快照，VM_GetKilledProp 读取
global._VM_spawn_cats = true;
global._VM_remote_funcs = ds_map_create();
global._VM_notice_scale		 = -1;
global._VM_notice_color_r	 = -1;
global._VM_notice_color_g	 = -1;
global._VM_notice_color_b	 = -1;
global._VM_conveyor_belt	 = false;
global._VM_conveyor_belt_arr = [];
global._VM_ban_weapon        = false;
global._VM_ban_super_weapon  = false;
global._VM_ban_shield        = false;

/// @function VM_GetCardSaveInfo(card_id, arr_name)
/// @param card_id  卡片 id
/// @param arr_name 命名数组名（先清空再写）
/// @return 写入的数量（3=成功；-1=卡未解锁 / 名字为空，此时数组是空的）
/// @desc 把这张卡**存档里**的外形/星级/技能写进命名数组：
///         数组[0] = shape
///         数组[1] = level
///         数组[2] = skill
///       复制类卡用：先 VM_GetProp(0, "prev_place_id") 拿上一张卡的 id，
///       再用本函数读出它的存档外形/星级/技能，原样丢给 VM_SpawnPlant。
///       （原版 obj_magic_chicken 走的是 get_card_info_simple(prev_place_id).shape/.level）
function VM_GetCardSaveInfo(card_id_addr, arr_addr) {
    var _id   = vm_arg(card_id_addr);
    var _name = vm_arg(arr_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) ds_map_add(_vm.arrays, _name, []);
    var _arr = _vm.arrays[? _name];
    array_resize(_arr, 0);
    if (!is_string(_id) || _id == "") return -1;
    var _info = get_card_info_simple(_id);
    if (!is_struct(_info)) return -1;
    array_push(_arr, _info[$ "shape"] ?? 0);
    array_push(_arr, _info[$ "level"] ?? 0);
    array_push(_arr, _info[$ "skill"] ?? 0);
    return 3;
}

/// @function VM_DestroyInstance(inst)
/// @param inst  目标实例（VM 包装 id 或真实 id）；0（全局哨兵）会被拒绝
/// @return 1=已销毁；0=没销毁（id 非法 / 实例不存在 / 传了 0）
/// @desc 直接销毁一个实例，会触发它的 Destroy 事件（mod 对象的 _OBJECT_DESTROY 也会跑）。
///       ⚠️ 在 _OBJECT_STEP 里销毁自己时，本帧剩下的语句**还会继续执行**，
///          脚本自己要用 `if (VM_IsDestroyed(self)) { exit }` 或类似的判断兜底。
function VM_DestroyInstance(inst_addr) {
    var _inst = vm_arg(inst_addr);
    if (is_undefined(_inst) || _inst == 0) return 0;   // 0 是 VM_GetProp / VM_SetProp 的全局哨兵
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return 0;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return 0;
    instance_destroy(_inst);
    return 1;
}

/// @function VM_RunStep(inst, times)
/// @param inst   目标实例（VM 包装 id 或真实 id）
/// @param times  额外跑几轮，夹到 1~60
/// @return 实际跑了几轮；实例不存在 / 正在被重跑（防递归）时返回 0
/// @desc 让某个实例额外跑若干轮 Step，用来做「加速」（同一帧内多推进几帧）。
///       一轮 = Begin Step → Step → End Step，三个事件都跑（对象没写的事件会被跳过）。
///       本质是 `with (inst) event_perform(ev_step, ...)`，所以对 mod 对象来说
///       **等于把它的 _OBJECT_STEP 再执行一遍**：计时器、冷却、伤害结算都会再推进。
///       ⚠️ 防递归：同一个实例正在被重跑时，期间它再调 VM_RunStep 会直接返回 0。
///          所以脚本里写 VM_RunStep(self, 3) 的净效果是「本帧跑 4 轮」，不会失控。
///       ⚠️ 不碰 global.is_paused：游戏暂停时被重跑的 Step 自己会 exit，等于没加速。
function VM_RunStep(inst_addr, times_addr) {
    var _inst = vm_arg(inst_addr);
    var _times = vm_arg(times_addr);
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return 0;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return 0;
    if (is_undefined(_times) || _times < 1) _times = 1;
    if (_times > 60) _times = 60;

    if (!variable_global_exists("_VM_step_running")) global._VM_step_running = ds_map_create();
    var _map = global._VM_step_running;
    if (ds_map_find_value(_map, _inst) != undefined) return 0;   // 正在被重跑 → 拒绝嵌套
    _map[? _inst] = true;

    var _n = 0;
    try {
        for (var _k = 0; _k < _times; _k++) {
            if (!instance_exists(_inst)) break;
            with (_inst) event_perform(ev_step, ev_step_begin);
            if (!instance_exists(_inst)) break;
            with (_inst) event_perform(ev_step, ev_step_normal);
            if (!instance_exists(_inst)) break;
            with (_inst) event_perform(ev_step, ev_step_end);
            _n += 1;
        }
    } finally {
        ds_map_delete(_map, _inst);
    }
    return _n;
}

/// @function VM_DrawSpriteExt(sprite, subimg, x, y, xscale, yscale, rot, alpha)
/// @param sprite     贴图：sprite 索引，或资源名/文件名（字符串，走 get_load_sprite 的缓存链）
/// @param subimg     子图索引（第几帧）
/// @param x y        屏幕坐标
/// @param xscale     X 缩放
/// @param yscale     Y 缩放
/// @param rot        旋转角度（度）
/// @param alpha      透明度 0~1
/// @return 0=画了一张；-1=贴图不存在（什么都不画）
/// @desc 等价于 draw_sprite_ext(sprite, subimg, x, y, xscale, yscale, rot, c_white, alpha)。
///       只能在 _OBJECT_DRAW 块里调用——GML 的绘制函数在 Step 事件里调用是画不出东西的。
///       贴图名解析交给 get_load_sprite()（asset_get_index → VM 临时缓存 → VM 永久缓存 → _sprite_cache），
///       所以 VM_LoadSpritePerm_Ex / VM_LoadSprite 加载的 mod 贴图直接传文件名就能用。
function VM_DrawSpriteExt(spr_addr, subimg_addr, x_addr, y_addr, xs_addr, ys_addr, rot_addr, alpha_addr) {
    var _raw = vm_arg(spr_addr);
    var _spr = is_string(_raw) ? get_load_sprite(_raw) : _raw;
    if (!sprite_exists(_spr)) return -1;
    var _sub = vm_arg(subimg_addr);
    var _xs  = vm_arg(xs_addr);
    var _ys  = vm_arg(ys_addr);
    var _rot = vm_arg(rot_addr);
    var _alp = vm_arg(alpha_addr);
    if (is_undefined(_sub)) _sub = 0;
    if (is_undefined(_xs) || _xs == 0) _xs = 1;
    if (is_undefined(_ys) || _ys == 0) _ys = 1;
    if (is_undefined(_rot)) _rot = 0;
    if (is_undefined(_alp)) _alp = 1;
    draw_sprite_ext(_spr, _sub, vm_arg(x_addr), vm_arg(y_addr), _xs, _ys, _rot, c_white, _alp);
    return 0;
}

/// @function bullet_screen_add(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life, type, death_obj, death_mod)
/// @param spr        贴图：sprite 索引，或资源名/文件名（字符串，走 get_load_sprite 的缓存链，
///                   所以 VM_LoadSpritePerm_Ex / VM_LoadSprite 加载的 mod 贴图直接传文件名就能用）
/// @param frames     总帧数；<=0 或超过贴图实际帧数时取贴图自身帧数
/// @param range      可打击范围：方格子半径，以子弹所在格为中心上下左右各扩 N 格；0=只算本格
/// @param scale      缩放（横竖同值）
/// @param anim_speed 动画速度：每帧 frame 自增多少，可以是小数；1=每帧走一帧
/// @param hits       伤害计数：正数=最多命中多少个，每命中一个减 1，减到 0 移除（1=打一个就消失；
///                   同一个敌人每帧最多挨一次，按格子里敌人数组的顺序结算，不按距离排序）；
///                   -1=不限次数，范围内**所有**可命中敌人都结算
/// @param life       存活帧数：倒计时，每帧-1，减到 0 移除；<=0 表示不自动移除
/// @param type       子弹类型：决定能命中哪些敌人（can_hit 的 card_target_type 那一套）
/// @param death_obj  销毁对象：非空时在这颗子弹销毁的位置创建该对象（字符串资产名）
/// @param death_mod  销毁对象是 mod 对象时填它的 mod 名字（如 obj_effect_mod + "hades_scythe_hit"）；
///                   原生对象留空。出界销毁不生成销毁对象
/// @return 子弹在管理器数组里的下标；管理器或贴图不存在时返回 -1
/// @desc 往 obj_Bullet_Screen_Management 的数组里塞一颗子弹（该实例由 obj_battle 开局创建）
function bullet_screen_add(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life, type = "all", death_obj = "", death_mod = "") {
    var _mgr = instance_find(obj_Bullet_Screen_Management, 0);
    if (!instance_exists(_mgr)) {
        show_debug_message("[bullet_screen_add] 找不到 obj_Bullet_Screen_Management（由 obj_battle 开局创建）");
        return -1;
    }
    var _spr = is_string(spr) ? get_load_sprite(spr) : spr;
    if (!sprite_exists(_spr)) return -1;
    var _frames = frames;
    var _spr_frames = sprite_get_number(_spr);
    if (is_undefined(_frames) || _frames <= 0 || _frames > _spr_frames) _frames = _spr_frames;
    if (is_undefined(scale) || scale == 0) scale = 1;
    if (is_undefined(anim_speed)) anim_speed = 1;
    array_push(_mgr.list, {
        spr:         _spr,
        frames:      _frames,
        cell_range:  range,
        scale:       scale,
        anim_speed:  anim_speed,
        frame:       0,
        x:           x,
        y:           y,
        vx:          vx,
        vy:          vy,
        dmg:         dmg,
        hits:        hits,
        life:        life,
        target_type: type,
        death_obj:   death_obj,
        death_mod:   death_mod
    });
    return array_length(_mgr.list) - 1;
}

/// @function VM_BulletScreenAdd(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life, type, death_obj, death_mod)
/// @desc VM 侧接口，参数和返回值和 bullet_screen_add 一致
function VM_BulletScreenAdd(spr_addr, frames_addr, range_addr, scale_addr, anim_speed_addr, x_addr, y_addr, vx_addr, vy_addr, dmg_addr, hits_addr, life_addr, type_addr, death_obj_addr, death_mod_addr) {
    return bullet_screen_add(vm_arg(spr_addr), vm_arg(frames_addr), vm_arg(range_addr), vm_arg(scale_addr), vm_arg(anim_speed_addr),
                             vm_arg(x_addr), vm_arg(y_addr), vm_arg(vx_addr), vm_arg(vy_addr), vm_arg(dmg_addr),
                             vm_arg(hits_addr), vm_arg(life_addr), vm_arg(type_addr), vm_arg(death_obj_addr), vm_arg(death_mod_addr));
}

global.__vm = VM_Create();
global.__vm.rng_state = 0x9E3779B9;   // VM随机种子：服务器生成并随bin同步，客户端收到后覆盖
VM_RegisterFunction(global.__vm, VM_BanCard);         // 0
VM_RegisterFunction(global.__vm, VM_SetCardLevelCap);  // 1
VM_RegisterFunction(global.__vm, VM_SetMaxSlots);       // 2
VM_RegisterFunction(global.__vm, VM_ShellPrint);        // 3
VM_RegisterFunction(global.__vm, VM_ShowNotice);        // 4
VM_RegisterFunction(global.__vm, VM_CreatePlatform);    // 5
VM_RegisterFunction(global.__vm, VM_SpawnPlant);       // 6
VM_RegisterFunction(global.__vm, VM_SpawnEnemy);       // 7
VM_RegisterFunction(global.__vm, VM_SpawnBoss);        // 8
VM_RegisterFunction(global.__vm, VM_LoadSprite);       // 9
VM_RegisterFunction(global.__vm, VM_SpawnObject);      // 10
VM_RegisterFunction(global.__vm, VM_SetProp);          // 11
VM_RegisterFunction(global.__vm, VM_GetWave);               // 12
VM_RegisterFunction(global.__vm, VM_GetSubwave);            // 13
VM_RegisterFunction(global.__vm, VM_GetProp);               // 14
VM_RegisterFunction(global.__vm, VM_GetLastBoss);           // 15
VM_RegisterFunction(global.__vm, VM_GetLastCreatedEnemy);   // 16
VM_RegisterFunction(global.__vm, VM_GetLastKilledEnemy);    // 17
VM_RegisterFunction(global.__vm, VM_GetLastCreatedCard);    // 18
VM_RegisterFunction(global.__vm, VM_GetLastDestroyedCard);  // 19
VM_RegisterFunction(global.__vm, VM_GetFlame);              // 20
VM_RegisterFunction(global.__vm, VM_SetFlame);              // 21
VM_RegisterFunction(global.__vm, VM_SetTerrain);            // 22
VM_RegisterFunction(global.__vm, VM_ClearPlants);           // 23
VM_RegisterFunction(global.__vm, VM_Random);                // 24
VM_RegisterFunction(global.__vm, VM_GetLastIdlePlatform);   // 25
VM_RegisterFunction(global.__vm, VM_SetPlatformParams);     // 26
VM_RegisterFunction(global.__vm, VM_SetMapBackground);      // 27
VM_RegisterFunction(global.__vm, VM_SetDrawSlot);           // 28
VM_RegisterFunction(global.__vm, VM_GetLoadedSpriteName, VM_TYPE_STRING);   // 29
VM_RegisterFunction(global.__vm, VM_GameWin);   // 30
VM_RegisterFunction(global.__vm, VM_GameLose);  // 31
VM_RegisterFunction(global.__vm, VM_SetDrawSlot_front);  // 32
VM_RegisterFunction(global.__vm, VM_SpawnCats, 1);      // 33
VM_RegisterFunction(global.__vm, VM_ClearMapObjects);    // 34
VM_RegisterFunction(global.__vm, VM_BanGem);             // 35
VM_RegisterFunction(global.__vm, VM_SetRowFeature);     // 36
VM_RegisterFunction(global.__vm, VM_ClearPlantsByType); // 37
VM_RegisterFunction(global.__vm, VM_WakePlants);        // 38
VM_RegisterFunction(global.__vm, VM_SetCardProp);      // 39
VM_RegisterFunction(global.__vm, VM_SetEnemyProp);     // 40
VM_RegisterFunction(global.__vm, VM_ShowNoticeDur);    // 41
VM_RegisterFunction(global.__vm, VM_SetEventEnabled);  // 42
VM_RegisterFunction(global.__vm, VM_RefreshPlatformSnapshots);  // 43
VM_RegisterFunction(global.__vm, VM_GetMouseX);  // 44
VM_RegisterFunction(global.__vm, VM_GetMouseY);  // 45
VM_RegisterFunction(global.__vm, VM_GetMouseCol);  // 46
VM_RegisterFunction(global.__vm, VM_GetMouseRow);  // 47
VM_RegisterFunction(global.__vm, VM_GetTerrain);  // 48
VM_RegisterFunction(global.__vm, VM_GetMousePressed);  // 49
VM_RegisterFunction(global.__vm, VM_GetKeyDown);  // 50
VM_RegisterFunction(global.__vm, VM_GetKeyPressed);  // 51
VM_RegisterFunction(global.__vm, VM_GetEnemyCount);  // 52
VM_RegisterFunction(global.__vm, VM_GetPlantCount);  // 53
VM_RegisterFunction(global.__vm, VM_GetPlantCountAt);  // 54
VM_RegisterFunction(global.__vm, VM_PlaySound);  // 55
VM_RegisterFunction(global.__vm, VM_GetPlantAt);  // 56
VM_RegisterFunction(global.__vm, VM_SwapPlants);     // 57
VM_RegisterFunction(global.__vm, VM_SwapPlantRects); // 58
VM_RegisterFunction(global.__vm, VM_CompactColumn);  // 59
VM_RegisterFunction(global.__vm, VM_CompactRow);     // 60
VM_RegisterFunction(global.__vm, VM_CompactColumnRev); // 61
VM_RegisterFunction(global.__vm, VM_CompactRowRev);    // 62
VM_RegisterFunction(global.__vm, VM_SpawnPlantsRandom);  // 63
VM_RegisterFunction(global.__vm, VM_CreateButton);  // 64
VM_RegisterFunction(global.__vm, VM_GetLastClickedButton);  // 65
VM_RegisterFunction(global.__vm, VM_LoadSpriteFrames);  // 66
VM_RegisterFunction(global.__vm, VM_ApplyPlantLevel);  // 67
VM_RegisterFunction(global.__vm, VM_LoadSpritePerm);  // 68
VM_RegisterFunction(global.__vm, VM_FreeSpritePerm);   // 69
VM_RegisterFunction(global.__vm, VM_SetCardSlotProp);  // 70
VM_RegisterFunction(global.__vm, VM_CalcCardSlotProp); // 71
VM_RegisterFunction(global.__vm, VM_GetCardSlotCount); // 72
VM_RegisterFunction(global.__vm, VM_GetPreviewCard);   // 73
VM_RegisterFunction(global.__vm, VM_AliasSprite);      // 74
VM_RegisterFunction(global.__vm, VM_LoadSpriteFrames_Ex); // 75
VM_RegisterFunction(global.__vm, VM_GetLastBossStateChangeId); // 76
VM_RegisterFunction(global.__vm, VM_GetLastBossOldState);      // 77
VM_RegisterFunction(global.__vm, VM_GetLastBossNewState);      // 78
VM_RegisterFunction(global.__vm, VM_ArrayGet);   // 79
VM_RegisterFunction(global.__vm, VM_ArraySet);   // 80
VM_RegisterFunction(global.__vm, VM_ArrayDel);   // 81
VM_RegisterFunction(global.__vm, VM_ArrayADD);   // 82
VM_RegisterFunction(global.__vm, VM_ArraySize);  // 83
VM_RegisterFunction(global.__vm, VM_ArrayClear);     // 84
VM_RegisterFunction(global.__vm, VM_ArrayClearAll);  // 85
VM_RegisterFunction(global.__vm, VM_SetNoticeStyle);  // 86
VM_RegisterFunction(global.__vm, VM_conveyor_belt_able); // 87
VM_RegisterFunction(global.__vm, VM_Slot_add);           // 88
VM_RegisterFunction(global.__vm, VM_BanAllCard);         // 89
VM_RegisterFunction(global.__vm, VM_CannelBanCard);      // 90
VM_RegisterFunction(global.__vm, VM_BanWeapon);          // 91
VM_RegisterFunction(global.__vm, VM_BanSuperWeapon);     // 92
VM_RegisterFunction(global.__vm, VM_BanShield);          // 93
VM_RegisterFunction(global.__vm, VM_SetCardShapeCap);    // 94
VM_RegisterFunction(global.__vm, VM_SetCardSkillCap);    // 95
VM_RegisterFunction(global.__vm, VM_GetKilledProp);      // 96
VM_RegisterFunction(global.__vm, VM_IsUndefined);        // 97
VM_RegisterFunction(global.__vm, VM_IsDestroyed);        // 98
VM_RegisterFunction(global.__vm, VM_LoadSound);          // 99
VM_RegisterFunction(global.__vm, VM_Floor);              // 100
VM_RegisterFunction(global.__vm, VM_Ceil);              // 101
VM_RegisterFunction(global.__vm, VM_SpawnBatMouse);     // 102
VM_RegisterFunction(global.__vm, VM_GetTimeLimit);      // 103
VM_RegisterFunction(global.__vm, VM_SetTimeLimit);      // 104
VM_RegisterFunction(global.__vm, VM_SetDrawSlotEx);        // 105
VM_RegisterFunction(global.__vm, VM_SetDrawSlotEx_front);  // 106
VM_RegisterFunction(global.__vm, VM_SetWaveAuto);    // 107
VM_RegisterFunction(global.__vm, VM_SetWave);        // 108
VM_RegisterFunction(global.__vm, VM_GetCurCard);         // 109
VM_RegisterFunction(global.__vm, VM_EnemyInRange);       // 110
VM_RegisterFunction(global.__vm, VM_GetHomingTarget);    // 111
VM_RegisterFunction(global.__vm, VM_GetInstancesInRange);// 112
VM_RegisterFunction(global.__vm, VM_CreateInstance);     // 113
VM_RegisterFunction(global.__vm, VM_LoadSpritePerm_Ex);  // 114
VM_RegisterFunction(global.__vm, VM_SetShovelFlameRate); // 115
VM_RegisterFunction(global.__vm, VM_BulletScreenAdd);    // 116
VM_RegisterFunction(global.__vm, VM_DrawSpriteExt);      // 117
VM_RegisterFunction(global.__vm, VM_RunStep);            // 118
VM_RegisterFunction(global.__vm, VM_DestroyInstance);    // 119
VM_RegisterFunction(global.__vm, VM_GetCardSaveInfo);    // 120
ds_map_add(global._VM_remote_funcs, "VM_SwapPlants", VM_SwapPlants);
ds_map_add(global._VM_remote_funcs, "VM_SwapPlantRects", VM_SwapPlantRects);
ds_map_add(global._VM_remote_funcs, "VM_CompactColumn", VM_CompactColumn);
ds_map_add(global._VM_remote_funcs, "VM_CompactRow", VM_CompactRow);
ds_map_add(global._VM_remote_funcs, "VM_CompactColumnRev", VM_CompactColumnRev);
ds_map_add(global._VM_remote_funcs, "VM_CompactRowRev", VM_CompactRowRev);
ds_map_add(global._VM_remote_funcs, "VM_SetCardProp", VM_SetCardProp);
ds_map_add(global._VM_remote_funcs, "VM_SetEnemyProp", VM_SetEnemyProp);
ds_map_add(global._VM_remote_funcs, "VM_ApplyPlantLevel", VM_ApplyPlantLevel);
ds_map_add(global._VM_remote_funcs, "VM_SetTimeLimit", VM_SetTimeLimit);
global._sync_vm_bin_buf = undefined;


/// @function VM_InitRoomEntry(buf)
function VM_InitRoomEntry(buf) {
				
    ds_map_clear(global.banned_cards_online);
    global.banned_gems_online = [];
    global._VM_card_level_cap = -1;
    global._VM_card_shape_cap = -1;
    global._VM_card_skill_cap = -1;
    global._VM_max_slots = -1;
    global._VM_shovel_flame_rate = -1;   // 铲子返还火苗系数：-1=原逻辑，0~1=VM 指定
    global._VM_ROOM_READY_ENTRY = undefined;
    global._VM_room_ready_done   = false;
    global._VM_BATTLE_START      = undefined;
    global._VM_CARD_CREATED      = undefined;
    global._VM_CARD_DESTROYED    = undefined;
    global._VM_CARD_DAMAGED      = undefined;
    global._VM_ENEMY_SPAWNED     = undefined;
    global._VM_ENEMY_KILLED      = undefined;
    global._VM_ENEMY_DAMAGED     = undefined;
    global._VM_WAVE_START        = undefined;
    global._VM_WAVE_END          = undefined;
    global._VM_SUBWAVE_START     = undefined;
    global._VM_SUBWAVE_END       = undefined;
    global._VM_PLAYER_DAMAGED    = undefined;
    global._VM_PLATFORM_IDLE_END = undefined;
    global._VM_FRAME             = undefined;
	global._VM_MOUSE_LEFT        = undefined;
	global._VM_MOUSE_RIGHT       = undefined;
	global._VM_KEY_PRESSED       = undefined;
	global._VM_BUTTON_CLICKED    = undefined;
	global._VM_CARD_PREVIEW_PICKED = undefined;
	global._VM_BOSS_STATE_CHANGE = undefined;
	global._VM_last_clicked_button = -1;
    global._VM_TIMER_5f          = undefined;
    global._VM_TIMER_10f         = undefined;
    global._VM_TIMER_15f         = undefined;
    global._VM_TIMER_30f         = undefined;
    global._VM_TIMER_60f         = undefined;
    global._VM_prev_wave         = -1;
    global._VM_prev_subwave      = -1;
    global._VM_event_enabled     = true;
	global._VM_notice_scale		 = -1;
	global._VM_notice_color_r	 = -1;
	global._VM_notice_color_g	 = -1;
	global._VM_notice_color_b	 = -1;
	global._VM_conveyor_belt	 = false;
	global._VM_ban_weapon        = false;
	global._VM_ban_super_weapon  = false;
	global._VM_ban_shield        = false;
	global._VM_last_damaged_player = -1;
	global._VM_last_damaged_enemy = -1;
	global._VM_last_damaged_card  = -1;

    global._sync_vm_bin_buf = undefined;
    global._VM_strings = [];
    global.__vm.strings = global._VM_strings;
    global.__vm.mem_type = array_create(array_length(global.__vm.mem_type), VM_TYPE_INT);
    global.__vm.mem_val  = array_create(array_length(global.__vm.mem_val), 0);
    // 释放 VM 临时贴图
    var _tmp_keys = ds_map_keys_to_array(global._VM_sprite_temp_cache);
    for (var _k = 0; _k < array_length(_tmp_keys); _k++) {
        var _spr = global._VM_sprite_temp_cache[? _tmp_keys[_k]];
        if (is_string(_spr)) continue;   // 别名条目（name→name），不是精灵
        if (sprite_exists(_spr)) { sprite_delete(_spr); ds_map_delete(global._pid_reverse, _spr); }
    }
    ds_map_clear(global._VM_sprite_temp_cache);
    global._VM_loaded_sprite_indices = [];
    global._VM_hook_queue = [];
    ds_map_clear(global._VM_dead_snaps);
    global._VM_cur_dead_snap = undefined;
    global._VM_battle_start_done = false;
    global._VM_last_boss          = -1;
    global._VM_last_created_enemy = -1;
    global._VM_last_killed_enemy  = -1;
    global._VM_last_created_card  = -1;
    global._VM_last_destroyed_card = -1;
    // 最近种植的5个卡片种类（1 为最新）
    global._recent_card_type_1 = "";
    global._recent_card_type_2 = "";
    global._recent_card_type_3 = "";
    global._recent_card_type_4 = "";
    global._recent_card_type_5 = "";
    global._VM_last_idle_platform = -1;
    global._VM_last_boss_state_change_id = -1;
    global._VM_last_boss_old_state = -1;
    global._VM_last_boss_new_state = -1;
    global._VM_create_counter = 100000;
    global._VM_spawn_cats = true;
	global._VM_conveyor_belt_arr = []
    ds_map_clear(global._VM_id_to_real);
    ds_map_clear(global._VM_real_to_vm_id);
    ds_map_clear(global.__vm.str_map);
    ds_map_clear(global.__vm.arrays);
	
    if (!buffer_exists(buf)) return;

    buffer_seek(buf, buffer_seek_start, 0);
    var _buf_size = buffer_get_size(buf);

    // ---- 读字符串池 ----
    var _str_count = buffer_read(buf, buffer_s32);
    for (var _i = 0; _i < _str_count; _i++) {
        var _len = buffer_read(buf, buffer_s32);
        var _tmp = buffer_create(_len + 1, buffer_fixed, 1);
        for (var _j = 0; _j < _len; _j++) {
            buffer_write(_tmp, buffer_u8, buffer_read(buf, buffer_u8));
        }
        buffer_write(_tmp, buffer_u8, 0);
        buffer_seek(_tmp, buffer_seek_start, 0);
        array_push(global._VM_strings, buffer_read(_tmp, buffer_string));
        buffer_delete(_tmp);
    }

    // 重建字符串反向索引（供运行时 CALL 返回字符串去重用）
    for (var _i = 0; _i < array_length(global._VM_strings); _i++) {
        var _str = global._VM_strings[_i];
        if (!ds_map_exists(global.__vm.str_map, _str))
            global.__vm.str_map[? _str] = _i;
    }

    // 字符串池里包含 "debug" 则开启调试模式
    global._VM_debug_mode = false;
    global._VM_debug_block = "";
    for (var _i = 0; _i < array_length(global._VM_strings); _i++) {
        if (global._VM_strings[_i] == "debug") { global._VM_debug_mode = true; break; }
    }

    // ---- 读块数据 ----
    while (buffer_tell(buf) < _buf_size) {
        var _block_len = buffer_read(buf, buffer_s32);
        var _name_idx  = buffer_read(buf, buffer_s32);

        var _block_name = "";
        if (_name_idx >= 0 && _name_idx < array_length(global._VM_strings)) {
            _block_name = global._VM_strings[_name_idx];
        }

        var _bc_buf = buffer_create(_block_len, buffer_fixed, 1);
        for (var _j = 0; _j < _block_len; _j++) {
            buffer_write(_bc_buf, buffer_u8, buffer_read(buf, buffer_u8));
        }
        buffer_seek(_bc_buf, buffer_seek_start, 0);

        switch (_block_name) {
            case "_VM_CONST_INIT":
                VM_Execute(global.__vm, _bc_buf, "_VM_CONST_INIT");
                buffer_delete(_bc_buf);
                break;
            case "_VM_ROOM_READY_ENTRY":
                global._VM_ROOM_READY_ENTRY = _bc_buf;
                VM_Execute(global.__vm, _bc_buf, "_VM_ROOM_READY_ENTRY");
                break;
            case "_VM_BATTLE_START":
                global._VM_BATTLE_START = _bc_buf;
                break;
            case "_VM_CARD_CREATED":
                global._VM_CARD_CREATED = _bc_buf;
                break;
            case "_VM_CARD_DESTROYED":
                global._VM_CARD_DESTROYED = _bc_buf;
                break;
            case "_VM_CARD_DAMAGED":
                global._VM_CARD_DAMAGED = _bc_buf;
                break;
            case "_VM_ENEMY_SPAWNED":
                global._VM_ENEMY_SPAWNED = _bc_buf;
                break;
            case "_VM_ENEMY_KILLED":
                global._VM_ENEMY_KILLED = _bc_buf;
                break;
            case "_VM_ENEMY_DAMAGED":
                global._VM_ENEMY_DAMAGED = _bc_buf;
                break;
            case "_VM_WAVE_START":
                global._VM_WAVE_START = _bc_buf;
                break;
            case "_VM_WAVE_END":
                global._VM_WAVE_END = _bc_buf;
                break;
            case "_VM_SUBWAVE_START":
                global._VM_SUBWAVE_START = _bc_buf;
                break;
            case "_VM_SUBWAVE_END":
                global._VM_SUBWAVE_END = _bc_buf;
                break;
            case "_VM_PLAYER_DAMAGED":
                global._VM_PLAYER_DAMAGED = _bc_buf;
                break;
            case "_VM_PLATFORM_IDLE_END":
                global._VM_PLATFORM_IDLE_END = _bc_buf;
                break;
            case "_VM_MOUSE_LEFT":
                global._VM_MOUSE_LEFT = _bc_buf;
                break;
            case "_VM_MOUSE_RIGHT":
                global._VM_MOUSE_RIGHT = _bc_buf;
                break;
            case "_VM_KEY_PRESSED":
                global._VM_KEY_PRESSED = _bc_buf;
                break;
            case "_VM_FRAME":
                global._VM_FRAME = _bc_buf;
                break;
            case "_VM_TIMER_5f":
                global._VM_TIMER_5f = _bc_buf;
                break;
            case "_VM_TIMER_10f":
                global._VM_TIMER_10f = _bc_buf;
                break;
            case "_VM_TIMER_15f":
                global._VM_TIMER_15f = _bc_buf;
                break;
            case "_VM_TIMER_30f":
                global._VM_TIMER_30f = _bc_buf;
                break;
            case "_VM_TIMER_60f":
                global._VM_TIMER_60f = _bc_buf;
                break;
            case "_VM_BUTTON_CLICKED":
                global._VM_BUTTON_CLICKED = _bc_buf;
                break;
            case "_VM_CARD_PREVIEW_PICKED":
                global._VM_CARD_PREVIEW_PICKED = _bc_buf;
                break;
            case "_VM_BOSS_STATE_CHANGE":
                global._VM_BOSS_STATE_CHANGE = _bc_buf;
                break;
            default:
                buffer_delete(_bc_buf);
                break;
        }
    }
}
