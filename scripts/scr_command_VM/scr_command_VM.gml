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
// ── 函数专用操作码 ──────────────────────────────────────────
// VM_Decode 在解码期把**每一条** VM_OP_CALL 换成
//     操作码 = VM_OP_FUNC_BASE + 函数id
// 排布：[操作码][arg_count(u8)][dst(s32)][addr(s32) * arg_count]
// 解释器用 global._vm_opt_run_funcs[操作码] 一次派发到 run_VM_xxx，不用再读 func_id。
// 0~144 全部注册在案（见 scr_command_VM_opt_execute 末尾的表）。
// 注意：18 即函数 id 0，整段操作码空间都让给函数，不再保留任何快捷码。
#macro VM_OP_FUNC_BASE 18
// ── 加载期专用化：VM_GetProp 的"内置变量版" ──
// VM_Decode 认出 VM_GetProp(id, "x") 这种（属性名是字面量常量字符串）时，
// 把函数 id 换成下面这 9 个之一（argc 2→1），handler 直接读 inst.x，
// 省掉 variable_instance_get 的"字符串→变量"查找。
// ⚠️ 这 9 个是**运行时专用 id，编译器表里没有**。原来接在 144 后面（145~153），
//    但 145 起已经被公开 VM 函数占用了（VM_GetInfo 等，见 compiler_defs.h），所以挪到 200 起，
//    145~199 留给公开函数。改这里的编号必须同步改 scr_command_VM_opt_execute.gml 里那 9 行派发表。
#macro VM_FID_GETPROPX 200

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
        arrays: ds_map_create(),  // 命名数组: 数组名 → GML 数组
        // 随机种子必须在这里初始化：VM_Random 直接读 _vm.rng_state，
        // 少了它就是 "Variable ...rng_state not set before reading it"，整张卡的 Step 当场断掉。
        // （mod 卡各有自己的 VM，是 VM_Create 建的，不会走 global.__vm 那句种子赋值）
        rng_state: 0x9E3779B9
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

/// @function VM_SpriteExists(name)
/// @desc 判断一张贴图现在**真的可用**吗（而不是 get_load_sprite 塞的空白占位图）。
///       查的顺序和贴图解析链一致：项目资源 → VM 临时缓存 → VM 永久缓存 → 全局贴图缓存。
///       坑：get_load_sprite 找不到时会塞一张空白图并且**永不失败**，所以光看它有没有返回
///       分不出真假；这里对全局缓存额外查 global._sprite_state 的 empty_load 标记。
///       （VM_LoadSprite 的返回值不能当存在性用：它只查临时缓存 + 本地文件，
///         永久缓存里有的、本地文件不在的贴图它照样返回 -1）
/// @return 1 = 可用，0 = 不可用（不存在 / 只是占位图 / 名字为空）
function VM_SpriteExists(name_addr) {
    var _name = vm_arg(name_addr);
    if (!is_string(_name) || _name == "") return 0;
    // ① 项目内置资源
    var _native = asset_get_index(_name);
    if (_native != -1 && sprite_exists(_native)) return 1;
    // ② VM 临时缓存（bin 专属贴图）/ ③ VM 永久缓存（VM_LoadSpritePerm_Ex）
    if (ds_map_exists(global._VM_sprite_temp_cache, _name)) {
        if (sprite_exists(global._VM_sprite_temp_cache[? _name])) return 1;
    }
    if (ds_map_exists(global._VM_sprite_cache, _name)) {
        if (sprite_exists(global._VM_sprite_cache[? _name])) return 1;
    }
    // ④ 全局贴图缓存：这里混着真图和占位图，靠 _sprite_state 区分
    if (ds_map_exists(global._sprite_cache, _name)) {
        var _state = ds_map_exists(global._sprite_state, _name) ? global._sprite_state[? _name] : empty_load;
        if (_state != empty_load && sprite_exists(global._sprite_cache[? _name])) return 1;
    }
    return 0;
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
        _targets = [obj_obstacle, obj_wind_tunnel, obj_lava, obj_barrier, obj_fog, obj_cloud, obj_seawater];
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
				vm_attach_plant_to_platform(_plant, _c, _r);   // 和手牌种植一样贴平台
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

/// @function VM_AliasSpritePerm(new_name, exist_name)
/// @desc 把 new_name **永久**指向 exist_name（写永久缓存，进房间不会被清）。
///       exist_name 必须已在永久缓存里（VM_LoadSpritePerm / VM_LoadSpritePerm_Ex 加载过）。
///       与 VM_AliasSprite 的区别：那个写临时缓存、存的是名字字符串、进房间会被清空；
///       这个写永久缓存、存的是**精灵 id**，所以 get_load_sprite 直接返回真 id，
///       可以安全地喂给 sprite_index（实例变量只能放 id，不能放名字）。
///       建过的别名会被登记，[reloadmod] 时由 VM_FreeSpritePermAlias() 释放，让新 bin 重新挂。
function VM_AliasSpritePerm(new_name_addr, exist_name_addr) {
    var _new   = vm_read_mem(global.__vm, new_name_addr);
    var _exist = vm_read_mem(global.__vm, exist_name_addr);
    if (!is_string(_new) || _new == "" || !is_string(_exist) || _exist == "") return;
    if (!ds_map_exists(global._VM_sprite_cache, _exist)) return;
    var _spr = global._VM_sprite_cache[? _exist];
    if (is_undefined(_spr) || is_string(_spr) || !sprite_exists(_spr)) return;
    ds_map_add(global._VM_sprite_cache, _new, _spr);
    if (!ds_map_exists(global._pid_reverse, _spr)) ds_map_add(global._pid_reverse, _spr, _new);
    // 登记，供 reloadmod 释放（底下的真精灵不动，还按原文件名留在永久缓存里）
    if (!variable_global_exists("_VM_perm_alias_names")) global._VM_perm_alias_names = [];
    if (array_get_index(global._VM_perm_alias_names, _new) == -1) {
        array_push(global._VM_perm_alias_names, _new);
    }
}

/// @function VM_FreeSpritePermAlias()
/// @desc 释放所有 VM_AliasSpritePerm 建过的永久别名（名字 → id 的重定向）。
///       底下的真精灵**不动**（仍按原文件名留在永久缓存里），所以重新挂一遍不会重复加载。
///       由 [reloadmod] 调用；mod 侧一般不用自己调。
/// @return 释放的条数
function VM_FreeSpritePermAlias() {
    if (!variable_global_exists("_VM_perm_alias_names")) return 0;
    var _n = array_length(global._VM_perm_alias_names);
    for (var _i = 0; _i < _n; _i++) {
        ds_map_delete(global._VM_sprite_cache, global._VM_perm_alias_names[_i]);
    }
    global._VM_perm_alias_names = [];
    return _n;
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

// ============================================================
// 表格 / 数组查询（只读，不参与联机同步）
//   · 全局二维表：参数传**全局变量名**，表是扁平的，格子下标 idx = j * (grid_cols + 2) + i
//     （i = 列，j = 行；stride 留 +2 是为了能放下 col = -1 / col = grid_cols 这两列）
//   · 实例一维数组：参数传**实例变量名**，就是挂在该实例身上的普通一维数组
//   目标不存在 / 不是数组 / 下标越界 一律返回 -1；Contains 类正常返回 1 / 0
// ============================================================

/// @function VM_ArrayExists(name)
/// @param name 全局变量名
/// @return 1=存在且是数组，0=不存在或不是数组
function VM_ArrayExists(name_addr) {
    var _name = vm_arg(name_addr);
    if (!variable_global_exists(_name)) return 0;
    return is_array(variable_global_get(_name)) ? 1 : 0;
}

/// @function VM_CellCount(name, i, j)
/// @param name 全局变量名（扁平二维表）
/// @param i 列  j 行
/// @return 该格元素个数；表不存在 / 非数组 / i,j 越界 → -1
function VM_CellCount(name_addr, i_addr, j_addr) {
    var _name = vm_arg(name_addr);
    var _i = vm_arg(i_addr);
    var _j = vm_arg(j_addr);
    if (!variable_global_exists(_name)) return -1;
    var _tbl = variable_global_get(_name);
    if (!is_array(_tbl)) return -1;
    if (_i < 0 || _i >= global.grid_cols) return -1;
    if (_j < 0 || _j >= global.grid_rows) return -1;
    var _idx = _j * (global.grid_cols + 2) + _i;
    if (_idx < 0 || _idx >= array_length(_tbl)) return -1;
    var _cell = _tbl[_idx];
    if (!is_array(_cell)) return -1;
    return array_length(_cell);
}

/// @function VM_CellItem(name, i, j, k)
/// @param name 全局变量名（扁平二维表）
/// @param i 列  j 行  k 第几个（从 0 起）
/// @return 实例 ID；表不存在 / 非数组 / 下标越界 → -1
function VM_CellItem(name_addr, i_addr, j_addr, k_addr) {
    var _name = vm_arg(name_addr);
    var _i = vm_arg(i_addr);
    var _j = vm_arg(j_addr);
    var _k = vm_arg(k_addr);
    if (!variable_global_exists(_name)) return -1;
    var _tbl = variable_global_get(_name);
    if (!is_array(_tbl)) return -1;
    if (_i < 0 || _i >= global.grid_cols) return -1;
    if (_j < 0 || _j >= global.grid_rows) return -1;
    var _idx = _j * (global.grid_cols + 2) + _i;
    if (_idx < 0 || _idx >= array_length(_tbl)) return -1;
    var _cell = _tbl[_idx];
    if (!is_array(_cell)) return -1;
    if (_k < 0 || _k >= array_length(_cell)) return -1;
    return VM_ClientWrapId(_cell[_k]);
}

/// @function VM_CellContains(name, i, j, value)
/// @param name 全局变量名（扁平二维表）
/// @param i 列  j 行
/// @return 1=这一格里有这个值，0=没有；表不存在 / 非数组 / i,j 越界 → -1
function VM_CellContains(name_addr, i_addr, j_addr, value_addr) {
    var _name = vm_arg(name_addr);
    var _i = vm_arg(i_addr);
    var _j = vm_arg(j_addr);
    var _v = vm_arg(value_addr);
    if (!variable_global_exists(_name)) return -1;
    var _tbl = variable_global_get(_name);
    if (!is_array(_tbl)) return -1;
    if (_i < 0 || _i >= global.grid_cols) return -1;
    if (_j < 0 || _j >= global.grid_rows) return -1;
    var _idx = _j * (global.grid_cols + 2) + _i;
    if (_idx < 0 || _idx >= array_length(_tbl)) return -1;
    var _cell = _tbl[_idx];
    if (!is_array(_cell)) return -1;
    return array_contains(_cell, _v) ? 1 : 0;
}

/// @function VM_ArrayContains(name, value)
/// @param name 命名数组名（和 VM_ArrayADD / VM_ArraySet / VM_ArrayGet 同一套**一维数组**）
/// @return **值第一次出现的下标**（从 0 起）；没有这个值 / 数组不存在 → -1
/// @desc 就是 GML 的 array_get_index。注意它查的是 VM 的命名一维数组，
///       不是 enemy_array 那种全局二维表（那是 VM_CellCount / CellItem / CellContains 一家）
function VM_ArrayContains(name_addr, value_addr) {
    var _name = vm_arg(name_addr);
    var _v = vm_arg(value_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) return -1;
    return array_get_index(_vm.arrays[? _name], _v);
}

/// @function VM_InstArrayExists(inst_id, name)
/// @param inst_id 实例 ID（支持 VM 的负号包装 ID）
/// @param name 实例变量名
/// @return 1=该实例身上有这个一维数组，0=没有
function VM_InstArrayExists(inst_addr, name_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    // inst = 0：和 VM_GetProp(0, …) 一个口径 —— 名字当全局变量名，操作 global.<name> 那个数组
    if (_inst == 0) {
        if (!variable_global_exists(_name)) return 0;
        return is_array(variable_global_get(_name)) ? 1 : 0;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return 0;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return 0;
    if (!variable_instance_exists(_inst, _name)) return 0;
    return is_array(variable_instance_get(_inst, _name)) ? 1 : 0;
}

/// @function VM_InstArraySize(inst_id, name)
/// @return 数组长度；实例/变量不存在或不是数组 → -1
function VM_InstArraySize(inst_addr, name_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    if (_inst == 0) {
        if (!variable_global_exists(_name)) return -1;
        var _g = variable_global_get(_name);
        return is_array(_g) ? array_length(_g) : -1;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    return array_length(_arr);
}

/// @function VM_InstArrayItem(inst_id, name, k)
/// @param k 第几个（从 0 起）
/// @return 元素值；实例/变量不存在、不是数组、k 越界 → -1
function VM_InstArrayItem(inst_addr, name_addr, k_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    var _k = vm_arg(k_addr);
    if (_inst == 0) {
        if (!variable_global_exists(_name)) return -1;
        var _g = variable_global_get(_name);
        if (!is_array(_g)) return -1;
        if (_k < 0 || _k >= array_length(_g)) return -1;
        return _g[_k];
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    if (_k < 0 || _k >= array_length(_arr)) return -1;
    var _v = _arr[_k];
    if (is_real(_v) && instance_exists(_v)) return VM_ClientWrapId(_v);
    return _v;
}

/// @function VM_InstArraySet(inst_id, name, k, value)
/// @param k 第几个（从 0 起）
/// @return 1=成功；实例/变量不存在、不是数组、k 越界 → -1
/// @desc 注意 GML 数组是值语义，改完必须 variable_instance_set 写回去
function VM_InstArraySet(inst_addr, name_addr, k_addr, value_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    var _k = vm_arg(k_addr);
    var _v = vm_arg(value_addr);
    if (_inst == 0) {
        var _g = variable_global_exists(_name) ? variable_global_get(_name) : [];
        if (!is_array(_g)) _g = [];
        if (_k < 0) return -1;
        if (_k >= array_length(_g)) array_resize(_g, _k + 1);
        _g[_k] = _v;
        variable_global_set(_name, _g);
        return 1;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    if (_k < 0 || _k >= array_length(_arr)) return -1;
    // 写进去的如果是 VM 的负号包装 id，先解回真实实例 id，免得引擎那边拿到负数
    if (is_real(_v) && _v < 0) {
        var _rv = ds_map_find_value(global._VM_id_to_real, -_v);
        if (!is_undefined(_rv)) _v = _rv;
    }
    _arr[_k] = _v;
    variable_instance_set(_inst, _name, _arr);
    return 1;
}

/// @function VM_InstArrayAdd(inst_id, name, value)
/// @return 1=成功；实例/变量不存在或不是数组 → -1（不自动建数组）
function VM_InstArrayAdd(inst_addr, name_addr, value_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    var _v = vm_arg(value_addr);
    if (_inst == 0) {
        var _g = variable_global_exists(_name) ? variable_global_get(_name) : [];
        if (!is_array(_g)) _g = [];
        array_push(_g, _v);
        variable_global_set(_name, _g);
        return 1;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    if (is_real(_v) && _v < 0) {
        var _rv = ds_map_find_value(global._VM_id_to_real, -_v);
        if (!is_undefined(_rv)) _v = _rv;
    }
    array_push(_arr, _v);
    variable_instance_set(_inst, _name, _arr);
    return 1;
}

/// @function VM_InstArrayDel(inst_id, name, k)
/// @return 1=成功；实例/变量不存在、不是数组、k 越界 → -1
function VM_InstArrayDel(inst_addr, name_addr, k_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    var _k = vm_arg(k_addr);
    if (_inst == 0) {
        if (!variable_global_exists(_name)) return -1;
        var _g = variable_global_get(_name);
        if (!is_array(_g)) return -1;
        if (_k < 0 || _k >= array_length(_g)) return -1;
        array_delete(_g, _k, 1);
        variable_global_set(_name, _g);
        return 1;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    if (_k < 0 || _k >= array_length(_arr)) return -1;
    array_delete(_arr, _k, 1);
    variable_instance_set(_inst, _name, _arr);
    return 1;
}

/// @function VM_InstArrayClear(inst_id, name)
/// @return 1=成功；实例/变量不存在或不是数组 → -1
function VM_InstArrayClear(inst_addr, name_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    if (_inst == 0) {
        variable_global_set(_name, []);
        return 1;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    if (!is_array(variable_instance_get(_inst, _name))) return -1;
    variable_instance_set(_inst, _name, []);
    return 1;
}

/// @function VM_InstArrayContains(inst_id, name, value)
/// @return 1=数组里有这个值，0=没有；实例/变量不存在或不是数组 → -1
function VM_InstArrayContains(inst_addr, name_addr, value_addr) {
    var _inst = vm_arg(inst_addr);
    var _name = vm_arg(name_addr);
    var _v = vm_arg(value_addr);
    if (_inst == 0) {
        if (!variable_global_exists(_name)) return -1;
        var _g = variable_global_get(_name);
        if (!is_array(_g)) return -1;
        return (array_get_index(_g, _v) != -1) ? 1 : 0;
    }
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return -1;
        _inst = _real;
    }
    if (!instance_exists(_inst)) return -1;
    if (!variable_instance_exists(_inst, _name)) return -1;
    var _arr = variable_instance_get(_inst, _name);
    if (!is_array(_arr)) return -1;
    return array_contains(_arr, _v) ? 1 : 0;
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
		// ⚠️ 这里原来写的是 GML 内置的 instance_id（= 跑这段 VM 的当前实例），不是被改的 inst_id：
		//    任何 mod 脚本对【别的实例】写 grid_col/grid_row，都会去「自己那格」的 plants 列表里
		//    删目标 id —— 找不到时 ds_list_find_index 返回 -1，ds_list_delete(_list, -1) 会误删
		//    列表末尾那张卡（同格的底座卡 / 莲叶）。
		if( object_is_ancestor(obj_card_parent, inst_id) ){
			var _list = ds_grid_get(global.grid_plants, inst_id.grid_col, inst_id.grid_row);
			var _idx = ds_list_find_index(_list, inst_id);
			if (_idx >= 0) ds_list_delete(_list, _idx);
		}
	}
    variable_instance_set(inst_id, prop, value);
	if(prop=="x"||prop=="y"){
		update_plant_bindings(inst_id);
	}
	if(prop=="grid_col"||prop=="grid_row"){
		if( object_is_ancestor(obj_card_parent, inst_id) ){
			var _list = ds_grid_get(global.grid_plants, inst_id.grid_col, inst_id.grid_row);
			ds_list_add(_list, inst_id)
		}
	}
	if(prop=="shape" || prop=="skill"|| prop=="current_level" ){
		network_apply_plant_level(inst_id,true)   // 原来写的 _plant 在 VM_SetProp 里没有定义，会读未赋值变量报错
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

/// @function damage_enemy(inst, dmg, dmg_type)
/// @param inst     敌人实例（真实实例 id）
/// @param dmg      伤害值
/// @param dmg_type 伤害类型（同原版子弹的 damage_type）："normal" = 有盾只打盾；
///                 "pierce" = 盾和血一起掉；其它（"explosion" 等）= 无视护盾直接扣血
/// @return 1=已结算 0=实例不存在
/// @desc 走**敌人自己的受击事件**（闪白 + 音效 + 护盾结算，含各敌人自己重写的 Other_10），
///       和原版子弹命中时做的事完全一致。
///       比 VM_SetProp(敌人, "hp", …) 正确：后者绕过受击事件，没闪白没音效、跳过护盾，
///       也跑不到那些自定义受击逻辑的敌人。
///       GML 侧（对象事件里）直接调这个；VM 脚本里调 VM_DamageEnemy。
function damage_enemy(inst, dmg, dmg_type) {
    var _type = dmg_type;
    if (is_undefined(_type) || _type == "") _type = "normal";
    if (!instance_exists(inst)) return 0;
    with (inst) {
        if (variable_instance_exists(id, "hit_sound")) audio_play_sound(hit_sound, 0, 0);
        damage_amount = dmg;
        damage_type   = _type;
        event_user(0);
    }
    return 1;
}

/// @function damage_enemy_ash(inst, dmg, dmg_type)
/// @desc **灰烬伤害**：接得下（剩余血量 > 伤害）就正常结算；接不下就**一击必杀**，
///       把敌人换成 obj_mouse_ash_death（灰烬），不再走正常死亡表现。
///       口径照抄原版 obj_power_god_bullet_1：它比的是 hp 和伤害，**不看护盾**。
///       GML 侧直接调这个；VM 脚本里调 VM_DamageEnemyAsh。
function damage_enemy_ash(inst, dmg, dmg_type) {
    var _type = dmg_type;
    if (is_undefined(_type) || _type == "") _type = "normal";
    if (!instance_exists(inst)) return 0;
    with (inst) {
        if (variable_instance_exists(id, "hit_sound")) audio_play_sound(hit_sound, 0, 0);
        if (hp > dmg) {
            damage_amount = dmg;
            damage_type   = _type;
            event_user(0);
        } else {
            var _ash = instance_create_depth(x, y - 20, depth, obj_mouse_ash_death);
            // 带了 special_ash 的敌人（灰烬类）要把外形也带给灰烬
            if (variable_instance_exists(id, "special_ash") && special_ash) {
                _ash.special_ash  = true;
                _ash.sprite_index = sprite_index;
                _ash.image_index  = image_index;
            }
            instance_destroy();
        }
    }
    return 1;
}

/// @function VM_DamageEnemy(inst_id, dmg, dmg_type)
/// @desc VM 侧接口，走 damage_enemy
function VM_DamageEnemy(inst_addr, dmg_addr, dmg_type_addr) {
    var _inst = vm_arg(inst_addr);
    if (_inst == 0) return 0;
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return 0;
        _inst = _real;
    }
    return damage_enemy(_inst, vm_arg(dmg_addr), vm_arg(dmg_type_addr));
}

/// @function VM_DamageEnemyAsh(inst_id, dmg, dmg_type)
/// @desc VM 侧接口，走 damage_enemy_ash
function VM_DamageEnemyAsh(inst_addr, dmg_addr, dmg_type_addr) {
    var _inst = vm_arg(inst_addr);
    if (_inst == 0) return 0;
    if (_inst < 0) {
        var _real = ds_map_find_value(global._VM_id_to_real, -_inst);
        if (is_undefined(_real)) return 0;
        _inst = _real;
    }
    return damage_enemy_ash(_inst, vm_arg(dmg_addr), vm_arg(dmg_type_addr));
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
/// @function vm_attach_plant_to_platform(plant, col, row)
/// @desc 让 VM 生成出来的植物像**手牌种植**一样贴在平台上。
///       手牌那条路（obj_card_slot/Step_0.gml）是「鼠标位置先减去平台的 visual_*_shift 算逻辑格
///       → 创建时再加回位移 → 平台正在移动时给植物 platform_grid_lock = true」。
///       这里植物已经由 spawn_plant 按逻辑格建好了，所以事后做同样两件事：
///         1. 把实例挪到平台的**视觉**位置（加上 visual_x_shift / visual_y_shift）；
///         2. 平台 state 是 "moving" 时打 platform_grid_lock —— 否则 obj_card_parent 的 Step
///            会用 x/y 反算 grid_col/grid_row，格子会被视觉位置带偏。
///       **不动 spawn_plant**：它里面那段 platform_id/platform_offset 是给网络传参用的
///       （单机在 props 里没有 platform_id），所以本地生成走这里补。
/// @return 是否贴在平台上
function vm_attach_plant_to_platform(plant, col, row) {
    if (!instance_exists(plant)) return false;
    var _sx = 0;
    var _sy = 0;
    var _moving = false;
    var _found = false;
    with (obj_platform) {
        var _isx  = (variable_instance_exists(id, "move_axis") && move_axis == "x");
        var _p_sc = start_col + ((_isx) ? current_offset : 0);
        var _p_sr = start_row + ((_isx) ? 0 : current_offset);
        if (col >= _p_sc && col < _p_sc + width && row >= _p_sr && row < _p_sr + length) {
            if (_isx) _sx = visual_x_shift; else _sy = visual_y_shift;
            if (variable_instance_exists(id, "state") && state == "moving") _moving = true;
            _found = true;
            break;
        }
    }
    if (!_found) return false;
    if (_sx != 0 || _sy != 0) {
        plant.x += _sx;
        plant.y += _sy;
    }
    if (_moving) plant.platform_grid_lock = true;
    return true;
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
            vm_attach_plant_to_platform(_plant, _c, _r);   // 和手牌种植一样贴平台
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

/// @function vm_store_result(vm, _mt, _mv, _dst, _result)
/// @desc 把 VM 函数返回值写回内存槽：字符串登记进字符串池、整数/浮点分类型。
///       通用 CALL 与热函数直通（操作码 18~23）共用；_dst == VM_DST_VOID 时丢弃返回值。
function vm_store_result(vm, _mt, _mv, _dst, _result) {
    if (_dst == VM_DST_VOID) return;
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
        _mt[_dst] = (floor(_result) == _result) ? VM_TYPE_INT : VM_TYPE_FLOAT;
        _mv[_dst] = _result;
    } else {
        _mt[_dst] = VM_TYPE_INT;
        _mv[_dst] = _result;
    }
}

/// @function VM_PropIdOf(_name)
/// @desc 属性名 → 加载期专用化 id（0 = 不专用化，走原来的 variable_instance_get）。
///       ⚠️ **只有内置实例变量能进这张表** —— 它们在任何实例上都存在，`inst.x` 不会报错。
///          实例变量（atk / cd / damage / state …）不能进：`inst.atk` 在没设过时会直接
///          报错，而 VM_GetProp 原本返回 undefined，mod 脚本正靠那个 undefined 兜底
///          （每帧块里 VM_IsUndefined 有 57 次），改了会让这些地方全崩。
///       频次依据：每帧块里内置变量共 148 次（占 GetProp+SetProp 661 次的 22.4%），
///          其中 x + y 就占 111 次。
function VM_PropIdOf(_name) {
    switch (_name) {
        case "x":            return 1;
        case "y":            return 2;
        case "sprite_index": return 3;
        case "image_xscale": return 4;
        case "image_yscale": return 5;
        case "depth":        return 6;
        case "image_angle":  return 7;
        case "image_alpha":  return 8;
        case "image_index":  return 9;
    }
    return 0;
}

/// @function VM_Decode(buf)
/// @desc 把一块字节码**一次性**解码成"值数组"，供 VM_Execute_code 用下标直接取指。
///       原始字节码里每条指令要 3~5 次 buffer_read（原生调用），爱神卡一次 Step 就是 647 次、
///       穿透弹 510 次，而且是每帧每实例重来 —— 这里是"少量多次"的最大浪费。
///       解码后每条指令只剩若干次数组下标读。
///
///       数组里的排布 = 解释器读的顺序，一个操作数一个元素：
///         opcode, 操作数1, 操作数2, ...      （浮点已转成真数、字符串存池下标）
///       所以解释器只要 _ip 一路往后走，不需要知道原始宽度。
///
///       ⚠️ IF / JMP 在字节码里存的是**绝对字节偏移**，数组版必须换成**数组下标**，
///          所以解码时分两趟：先记 字节偏移→数组下标 的映射，最后回填跳转目标。
///          （-1 是"不跳"；IF 的两个目标在解码期就换成落空下标，运行期不必再判 -1）
/// @return 值数组（空块返回空数组）
function VM_Decode(buf, _poslist = noone, _vm = noone) {
    var _code = [];
    if (!buffer_exists(buf)) return _code;

    // 加载期专用化要用到 vm 内存（读属性名那个槽），见 case VM_OP_CALL
    var _vm_ok = (_vm != noone && is_struct(_vm));

    var _off2idx = ds_map_create();   // 字节偏移（string）→ 数组下标
    var _fix = [];                    // 待回填：[数组位置, 原始字节偏移, 数组位置, 原始字节偏移, ...]
    buffer_seek(buf, buffer_seek_start, 0);
    var _size = buffer_get_size(buf);
    var _want_pos = (_poslist != noone && ds_exists(_poslist, ds_type_list));   // 传了 ds_list 才记位置（报错定位用）

    while (buffer_tell(buf) < _size) {
        _off2idx[? string(buffer_tell(buf))] = array_length(_code);
        if (_want_pos) {
            ds_list_add(_poslist, array_length(_code));   // 这条指令占用的【数组下标】（操作数也在数组里，不是逐条 +1）
            ds_list_add(_poslist, buffer_tell(buf));      // 它对应的【字节偏移】
        }
        var _op = buffer_read(buf, buffer_u8);
        array_push(_code, _op);

        switch (_op) {
            case VM_OP_ASSIGN: {
                array_push(_code, buffer_read(buf, buffer_s32));      // dst
                var _at = buffer_read(buf, buffer_u8);                // 类型
                array_push(_code, _at);
                if (_at == VM_TYPE_STRING) {
                    array_push(_code, buffer_read(buf, buffer_u16));  // 字符串池下标
                } else if (_at == VM_TYPE_FLOAT) {
                    array_push(_code, buffer_read(buf, buffer_f32));
                } else {
                    // ⚠️ 必须 real() 包一层：buffer_read(..., buffer_s32) 返回的是 int64，
                    //    而 is_real() 对 int64 判 false（只有 real 才算实数）。
                    //    不包的话整数常量解出来是 int64，卡里写 60 会让 is_real 判断全废，
                    //    写成 60.0（走 FLOAT 分支、返回 real）反而正常。
                    array_push(_code, real(buffer_read(buf, buffer_s32)));
                }
                break;
            }
            case VM_OP_COPY: {
                array_push(_code, buffer_read(buf, buffer_s32));
                array_push(_code, buffer_read(buf, buffer_s32));
                break;
            }
            case VM_OP_CALL: {
                var _fid = buffer_read(buf, buffer_u16);              // 函数 id
                var _argc = buffer_read(buf, buffer_u8);
                var _dst = buffer_read(buf, buffer_s32);
                var _args = array_create(_argc);
                for (var _i = 0; _i < _argc; _i++) {
                    _args[_i] = buffer_read(buf, buffer_s32);
                }

                // ── 加载期专用化：VM_GetProp(id, "内置名") → 专用函数 id（argc 2→1）──
                //    属性名在字节码里是个槽。**不用另建映射表**：每个 bin 载入/重载都先跑
                //    _VM_CONST_INIT 把字面量池写进内存，所以解别的块时直接读那个槽就是属性名。
                //    读到的不是字符串（还没初始化 / 不是字面量）就原样保留，行为一字不变。
                if (_fid == 14 && _argc == 2 && _vm_ok) {
                    // ⚠️ real() 不能省：buffer_read(..., buffer_s32) 返回 int64，
                    //    而 is_real() 对 int64 判 false（见 VM_OP_ASSIGN 那段注释），
                    //    不包的话下面两个 is_real 全部为假，专用化永远不会生效
                    var _ps = real(_args[1]);
                    if (_ps >= 0 && _ps < array_length(_vm.mem_type)
                        && _vm.mem_type[_ps] == VM_TYPE_STRING) {
                        var _si = real(_vm.mem_val[_ps]);
                        if (_si >= 0 && _si < array_length(_vm.strings)) {
                            var _pid = VM_PropIdOf(_vm.strings[_si]);
                            if (_pid > 0) {
                                _fid  = VM_FID_GETPROPX + _pid - 1;
                                _argc = 1;
                                _args = [_args[0]];
                            }
                        }
                    }
                }

                // 所有函数一视同仁：换成"专用操作码" VM_OP_FUNC_BASE + 函数id，
                // 排布 [操作码][arg_count][dst][addr...]，解释器按表直接派发。
                var _has_slot = variable_global_exists("_vm_opt_run_funcs")
                             && _fid >= 0
                             && (VM_OP_FUNC_BASE + _fid) < array_length(global._vm_opt_run_funcs);
                if (_has_slot) {
                    _code[array_length(_code) - 1] = VM_OP_FUNC_BASE + _fid;
                    array_push(_code, _argc);
                    array_push(_code, _dst);
                } else {
                    // 表里没有（函数 id 越界）：保留通用 CALL，交给 run_VM_OP_CALL 报"未注册函数ID"
                    array_push(_code, _fid);
                    array_push(_code, _argc);
                    array_push(_code, _dst);
                }
                for (var _i = 0; _i < _argc; _i++) {
                    array_push(_code, _args[_i]);
                }
                break;
            }
            case VM_OP_IF: {
                array_push(_code, buffer_read(buf, buffer_s32));      // cond
                var _fall = array_length(_code) + 2;                  // 落空 = 三个操作数之后的下标
                var _raw_t = buffer_read(buf, buffer_s32);
                if (_raw_t == -1) {
                    array_push(_code, _fall);                         // -1（不跳）→ 写落空下标，运行期不必再判
                } else {
                    array_push(_fix, array_length(_code));            // 记下 true 目标所在下标
                    array_push(_fix, _raw_t);
                    array_push(_code, _raw_t);                        // ⚠️ 必须占位，最后回填成数组下标
                }
                var _raw_f = buffer_read(buf, buffer_s32);
                if (_raw_f == -1) {
                    array_push(_code, _fall);
                } else {
                    array_push(_fix, array_length(_code));            // 记下 false 目标所在下标
                    array_push(_fix, _raw_f);
                    array_push(_code, _raw_f);                        // ⚠️ 同上
                }
                break;
            }
            case VM_OP_JMP: {
                var _raw_j = buffer_read(buf, buffer_s32);
                array_push(_fix, array_length(_code));
                array_push(_fix, _raw_j);
                array_push(_code, _raw_j);                            // ⚠️ 同上
                break;
            }
            case VM_OP_HALT: {
                break;                                               // 无操作数
            }
            default: {
                // ADD/SUB/MUL/DIV/MOD/EQ/NEQ/GT/GTE/LT/LTE：dst, a, b
                array_push(_code, buffer_read(buf, buffer_s32));
                array_push(_code, buffer_read(buf, buffer_s32));
                array_push(_code, buffer_read(buf, buffer_s32));
                break;
            }
        }
    }

    // 跳转目标：字节偏移 → 数组下标（IF 的 -1 已在上面换成落空下标，这里是普通回填）
    // ⚠️ 必须把"块末尾"(_size) 也登记进去：编译器有大量 `IF.false -> 块末尾` 的跳转
    //    （跳过剩余指令直接结束），映射成 array_length(_code) 正好让 while 退出。
    //    漏了它就会被当成 -1（= 不跳）而继续往下执行，行为就错了。
    _off2idx[? string(_size)] = array_length(_code);
    for (var _f = 0; _f < array_length(_fix); _f += 2) {
        var _pos = _fix[_f];
        var _tar = _fix[_f + 1];
        if (_tar == -1) continue;
        var _key = string(_tar);
        // 映射不到（理论上不会，结构校验过全部 bin）就跳到块末尾，宁可少执行也不乱执行
        _code[_pos] = ds_map_exists(_off2idx, _key) ? _off2idx[? _key] : array_length(_code);
    }
    ds_map_destroy(_off2idx);

    return _code;
}

/// @function mod_error_report(vm, name, ip, err)
/// @desc 把一次 mod 运行错误整理成"人能看懂"的多行块写进 mod/mod_errors.log：
///         先说清【哪个 bin、哪个块、源码第几行 + 那一行的原文】，
///         再附【GML 的原始错误】（script / message / longMessage / 完整调用栈）。
///       shell 只打一行摘要，避免控制台被刷屏。
function mod_error_report(vm, name, ip, _err, _pos_is_byte = false) {
    var _dir = (is_struct(vm) && variable_struct_exists(vm, "mod_dir")) ? string(vm[$ "mod_dir"]) : "";
    // 出错的到底是哪个 bin（src_lines 是 dir+id+".lines"，换成 .bin 就是它）
    var _binp = "";
    if (is_struct(vm) && variable_struct_exists(vm, "src_lines")) _binp = string_replace(vm[$ "src_lines"], ".lines", ".bin");
    var _nm  = "";
    if (is_struct(vm) && variable_struct_exists(vm, "card_data") && is_struct(vm.card_data)
     && variable_struct_exists(vm.card_data, "name")) _nm = string(vm.card_data.name);

    // 源码行号（VM_LineOf 返回 "err_test.txt:15"，拿不到就是 ""）
    var _loc = VM_LineOf(vm, name, ip, _pos_is_byte);
    var _ln  = -1;
    if (_loc != "") _ln = real(string_delete(_loc, 1, string_pos(":", _loc)));

    // 源码那一行的原文（.txt 就在 bin 旁边：src_lines 是 .lines，换成 .txt）
    var _srctext = "";
    if (_ln > 0 && is_struct(vm) && variable_struct_exists(vm, "src_lines")) {
        var _tp = string_replace(vm[$ "src_lines"], ".lines", ".txt");
        if (file_exists(_tp)) {
            var _fr = file_text_open_read(_tp);
            if (_fr != -1) {
                var _k = 0;
                while (!file_text_eof(_fr)) {
                    var _s = file_text_read_string(_fr);
                    file_text_readln(_fr);
                    _k++;
                    if (_k == _ln) { _srctext = _s; break; }
                }
                file_text_close(_fr);
            }
        }
    }

    // GML 的原始错误信息
    var _escript = "", _emsg = "", _elong = "", _estack = "";
    if (is_struct(_err)) {
        if (variable_struct_exists(_err, "script"))      _escript = string(_err[$ "script"]);
        if (variable_struct_exists(_err, "message"))     _emsg    = string(_err[$ "message"]);
        if (variable_struct_exists(_err, "longMessage")) _elong   = string(_err[$ "longMessage"]);
        if (variable_struct_exists(_err, "stacktrace") && is_array(_err[$ "stacktrace"])) {
            var _st = _err[$ "stacktrace"];
            for (var _i = 0; _i < array_length(_st); _i++) _estack += "      " + string(_st[_i]) + "\n";
        }
    } else {
        _emsg = string(_err);
    }

    var _o = "\n========== mod 运行出错 ==========\n";
    _o += "  文件   : " + _binp + "\n";
    _o += "  名称   : " + _nm + "\n";
    _o += "  VM块   : " + string(name) + "   指令位置 " + string(ip) + "\n";
    if (_ln > 0) _o += "  源码行 : 第 " + string(_ln) + " 行   （" + _loc + "）\n";
    else         _o += "  源码行 : 拿不到（那个 bin 里没有行号表？）\n";
    if (_srctext != "") _o += "  那一行 : " + string_trim(_srctext) + "\n";
    _o += "  ---- GML 原始错误 ----\n";
    _o += "  script : " + _escript + "\n";
    _o += "  message: " + _emsg + "\n";
    if (_elong != "") _o += "  long   : " + _elong + "\n";
    if (_estack != "") _o += "  调用栈 :\n" + _estack;
    _o += "==================================\n";
    mod_error_log(_o);

    // shell 只给一行摘要：哪个文件、哪张卡、哪个块、第几行；详细内容看日志
    shell_print("[mod 出错] " + _binp + "   块 " + string(name) + "   名称 " + _nm
              + ((_ln > 0) ? ("   第 " + string(_ln) + " 行") : "   行号未知"));
    shell_print("详情见 错误日志：mod/mod_errors.log");
    return _loc;
}

/// @function mod_error_ctx(vm, name, pos)
/// @desc 组装一条 mod 运行错误的定位信息：块名 + 位置 + 是哪个 mod（目录/名称）+ 源码行号
function mod_error_ctx(vm, name, pos) {
    var _who = "";
    if (is_struct(vm)) {
        if (variable_struct_exists(vm, "mod_dir") && is_string(vm.mod_dir)) _who += " | 目录 " + vm.mod_dir;
        if (variable_struct_exists(vm, "card_data") && is_struct(vm.card_data)
         && variable_struct_exists(vm.card_data, "name"))              _who += " | 名称 " + string(vm.card_data.name);
    }
    var _loc = VM_LineOf(vm, name, pos);   // 有行号表就带上 "xxx.txt:88"，没有就是空串
    if (_loc != "") _loc = " " + _loc;
    return "[块 " + string(name) + " @ " + string(pos) + _who + "]" + _loc;
}

/// @function VM_LineOf(vm, name, ip)
/// @desc 把数组版解释器的 _ip 翻译成源码位置（"clotho.txt:88"）。
///       行号表在加载 bin 时已读进 vm.line_tab，这里只做两次查表：
///         _ip →（报错时才解码出的位置表）→ 字节偏移 →（行号表）→ 源行号
///       没有行号表 / 查不到 → 返回 ""，调用方保持原来的"块名 + 位置"输出。
function VM_LineOf(vm, name, ip, _is_byte = false) {
    if (!is_struct(vm)) return "";
    if (!variable_struct_exists(vm, "line_tab") || !variable_struct_exists(vm, "blocks")) {
        mod_error_log("[VM_LineOf] 没有 line_tab/blocks（bin 里没打包行号表？）");
        return "";
    }
    if (!ds_map_exists(vm.line_tab, name) || !ds_map_exists(vm.blocks, name)) {
        mod_error_log("[VM_LineOf] 行号表里没有块 " + string(name));
        return "";
    }

    // _ip → 字节偏移：按块解码一次拿位置表并缓存（只在报错路径上发生）
    if (!variable_struct_exists(vm, "code_pos")) vm[$ "code_pos"] = ds_map_create();
    if (!ds_map_exists(vm.code_pos, name)) {
        var _pl = ds_list_create();
        VM_Decode(vm.blocks[? name], _pl, vm);
        vm.code_pos[? name] = _pl;
    }
    var _pos = vm.code_pos[? name];   // [指令的数组下标, 字节偏移, 下标, 偏移, …]
    var _pn = ds_list_size(_pos);
    var _byte = -1;
    if (_is_byte) {
        _byte = ip;   // 字节码版/调试版解释器传进来的本来就是字节偏移，直接用
    } else {
        for (var _pi = 0; _pi + 1 < _pn; _pi += 2) {
            if (ds_list_find_value(_pos, _pi) > ip) break;   // 找到 ip 落在哪条指令上
            _byte = ds_list_find_value(_pos, _pi + 1);
        }
    }
    if (_byte < 0) {
        mod_error_log("[VM_LineOf] ip " + string(ip) + " 落在位置表之外（共 " + string(_pn div 2) + " 条指令）");
        return "";
    }

    // 行号表里找"起始字节 ≤ _byte"的最后一条语句
    var _lst = vm.line_tab[? name];
    var _n = ds_list_size(_lst);
    var _line_no = -1;
    for (var _i = 0; _i + 1 < _n; _i += 2) {
        if (ds_list_find_value(_lst, _i) > _byte) break;
        _line_no = ds_list_find_value(_lst, _i + 1);
    }
    if (_line_no < 0) {
        mod_error_log("[VM_LineOf] 字节偏移 " + string(_byte) + " 之前没有任何记录（表长度 " + string(_n) + "）");
        return "";
    }

    var _f = "?";
    if (variable_struct_exists(vm, "src_lines")) _f = string_replace(filename_name(vm[$ "src_lines"]), ".lines", ".txt");
    return _f + ":" + string(_line_no);
}

/// @function vm_hook_register(_name, _vm)
/// @desc 把一个 VM 挂到某个挂载点上。地图 bin 与每个 mod 的 VM 加载后各挂一次。
///       ⚠️ **不检查**该 VM 此时有没有这个块 —— 热重载会换掉 vm.blocks，
///          检查放在 vm_hook_run 里做，这样重载后不需要重新注册（见那里的注释）。
///       同一个 VM 重复挂同一个点会被去重（重载/重复加载时安全）。
function vm_hook_register(_name, _vm) {
    // ⚠️ 按需建表：不能指望"注册前表一定已经建好"。开游戏时的注册跑在 VM_Create 之前，
    //    那时表还不存在 → 老写法在这里直接 return，注册全部静默丢掉，
    //    结果所有 mod 的挂载点在**正常开局**下永远是死的（只有 reloadmod 之后才活）。
    if (!variable_global_exists("_VM_hooks")) global._VM_hooks = ds_map_create();
    if (is_undefined(_vm)) return;
    if (!ds_map_exists(global._VM_hooks, _name)) {
        global._VM_hooks[? _name] = [];
    }
    var _list = global._VM_hooks[? _name];
    if (array_get_index(_list, _vm) != -1) return;
    array_push(_list, _vm);
}

/// @function vm_hook_unregister(_vm)
/// @desc 把一个 VM 从所有挂载点摘掉（丢弃 VM 时用；热重载复用的是同一个 VM 结构体，
///       所以重载**不需要**调这个）。
function vm_hook_unregister(_vm) {
    if (!variable_global_exists("_VM_hooks")) return;
    var _names = ds_map_keys_to_array(global._VM_hooks);
    for (var _n = 0; _n < array_length(_names); _n++) {
        var _list = global._VM_hooks[? _names[_n]];
        var _i = array_get_index(_list, _vm);
        if (_i != -1) array_delete(_list, _i, 1);
    }
}

/// @function vm_hook_run(_name)
/// @desc 依次执行挂在 _name 上的所有 VM 的该块。
///       没块的直接跳过（热重载换了 bin 之后，旧的挂载会自动失效，不用手动清）。
///       执行期间把 global.__vm 切到目标 VM、跑完切回 —— VM 的 API 函数只认 global.__vm。
function vm_hook_run(_name) {
    if (!variable_global_exists("_VM_hooks")) return;
    if (!ds_map_exists(global._VM_hooks, _name)) return;
    var _list = global._VM_hooks[? _name];
    if (array_length(_list) == 0) return;

    var _bak = global.__vm;
    for (var _i = 0; _i < array_length(_list); _i++) {
        var _vm = _list[_i];
        if (is_undefined(_vm)) continue;
        if (!variable_struct_exists(_vm, "blocks")) continue;
        if (!ds_map_exists(_vm.blocks, _name)) continue;   // 这个 VM 没有这个块 → 跳过
        global.__vm = _vm;
        VM_Execute(_vm, _vm.blocks[? _name], _name);
    }
    global.__vm = _bak;
}

/// @function vm_hook_register_all(_vm)
/// @desc 按 _vm 当前的 blocks 重新挂载：先摘掉它在所有挂载点上的旧注册，
///       再把 blocks 里的**每个块名**都登记一遍（不设白名单）。
///       ⚠️ 热重载换掉 blocks 之后必须再调一次，否则新加/删掉的块不会生效。
function vm_hook_register_all(_vm) {
    // ⚠️ 按需建表（同 vm_hook_register）：正常开局的注册跑在 VM_Create 之前，
    //    老写法在这里直接 return → 所有 mod 的挂载点静默失效（只有 reloadmod 之后才活）
    if (!variable_global_exists("_VM_hooks")) global._VM_hooks = ds_map_create();
    if (is_undefined(_vm)) return;
    vm_hook_unregister(_vm);
    if (!variable_struct_exists(_vm, "blocks")) return;
    if (!ds_exists(_vm.blocks, ds_type_map)) return;
    var _bnames = ds_map_keys_to_array(_vm.blocks);
    for (var _i = 0; _i < array_length(_bnames); _i++) {
        vm_hook_register(_bnames[_i], _vm);
    }
}

/// @function mod_error_log(_msg)
/// @desc 把一条 mod 运行错误追加到本地日志 mod/mod_errors.log（时间 + 内容）。
///       同一个错误连续刷屏时每 60 次才再记一条；日志只保留最近 400 行。
function mod_error_log(_msg) {
    var _m = string(_msg);

    // 连续重复的同一个错误不刷屏
    if (variable_global_exists("_mod_err_last") && _m == global._mod_err_last) {
        global._mod_err_dup += 1;
        if ((global._mod_err_dup mod 60) != 0) return;
    } else {
        global._mod_err_last = _m;
        global._mod_err_dup  = 0;
    }

    var _dir = working_directory + "mod/";
    if (!directory_exists(_dir)) return;
    var _path = _dir + "mod_errors.log";

    // 读旧内容（GM 没有追加模式，只能读出来再整体写回）
    var _lines = [];
    if (file_exists(_path)) {
        var _r = file_text_open_read(_path);
        if (_r != -1) {
            while (!file_text_eof(_r)) {
                array_push(_lines, file_text_read_string(_r));
                file_text_readln(_r);
            }
            file_text_close(_r);
        }
    }
    array_push(_lines, date_datetime_string(date_current_datetime()) + "  " + _m);
    if (array_length(_lines) > 2000) {
        var _keep = [];
        for (var _i = array_length(_lines) - 2000; _i < array_length(_lines); _i++) array_push(_keep, _lines[_i]);
        _lines = _keep;
    }

    var _w = file_text_open_write(_path);
    if (_w == -1) return;
    for (var _i = 0; _i < array_length(_lines); _i++) {
        file_text_write_string(_w, _lines[_i]);
        file_text_writeln(_w);
    }
    file_text_close(_w);
}
/*
/// @function VM_Execute_code(vm, code, name)
/// @desc 数组版解释器：逻辑和 VM_Execute 完全一致，只是取指从"buffer_read"换成"_code[_ip]"。
///       跳转目标在 VM_Decode 里已经换算成数组下标，所以 IF/JMP 直接 _ip = 目标。
/// @param code VM_Decode 出来的值数组
function VM_Execute_code(vm, code, name) {
    try {
        var _n  = array_length(code);
        var _ip = 0;
        var _mt = vm.mem_type;
        var _mv = vm.mem_val;

        while (_ip < _n) {
            var _op = code[_ip];
            _ip += 1;

            switch (_op) {
                // ==================== CALL ====================
                case VM_OP_CALL_GETPROP: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_GetProp(code[_ip], code[_ip + 1]);
                    _ip += 2;

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

                case VM_OP_CALL_SETPROP: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_SetProp(code[_ip], code[_ip + 1], code[_ip + 2]);
                    _ip += 3;

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

                // ==================== 热函数直通（解码期由 CALL 换来的） ====================
                case VM_OP_CALL_ISUNDEF: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_IsUndefined(code[_ip]); _ip += 1;
                    vm_store_result(vm, _mt, _mv, _dst, _result);
                    break;
                }

                case VM_OP_CALL_GETCURCARD: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_GetCurCard();
                    vm_store_result(vm, _mt, _mv, _dst, _result);
                    break;
                }

                case VM_OP_CALL_ARRAYGET: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_ArrayGet(code[_ip], code[_ip + 1]); _ip += 2;
                    vm_store_result(vm, _mt, _mv, _dst, _result);
                    break;
                }

                case VM_OP_CALL_CREATEINST: {
                    var _dst = code[_ip]; _ip += 1;
                    var _result = VM_CreateInstance(code[_ip], code[_ip + 1], code[_ip + 2]); _ip += 3;
                    vm_store_result(vm, _mt, _mv, _dst, _result);
                    break;
                }

                case VM_OP_CALL: {
                    var _func_id = code[_ip]; _ip += 1;
                    var _arg_count = code[_ip]; _ip += 1;
                    var _dst = code[_ip]; _ip += 1;
                    var _args = array_create(_arg_count);
                    for (var _i = 0; _i < _arg_count; _i++) {
                        _args[_i] = code[_ip];
                        _ip += 1;
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


                // ==================== ASSIGN ====================
                case VM_OP_ASSIGN: {
                    var _dst = code[_ip]; _ip += 1;
                    var _type = code[_ip]; _ip += 1;
                    if (_type == VM_TYPE_STRING) {
                        _mt[_dst] = VM_TYPE_STRING;
                        _mv[_dst] = code[_ip]; _ip += 1;
                    } else if (_type == VM_TYPE_FLOAT) {
                        _mt[_dst] = VM_TYPE_FLOAT;
                        _mv[_dst] = code[_ip]; _ip += 1;
                    } else {
                        _mt[_dst] = _type;
                        _mv[_dst] = code[_ip]; _ip += 1;
                    }
                    break;
                }

                // ==================== COPY ====================
                case VM_OP_COPY: {
                    var _dst = code[_ip]; _ip += 1;
                    var _src = code[_ip]; _ip += 1;
                    _mt[_dst] = _mt[_src];
                    _mv[_dst] = _mv[_src];
                    break;
                }

                // ==================== 算术 ====================
                case VM_OP_ADD: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    var _res = _mv[_a] + _mv[_b];
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _res;
                    break;
                }
                case VM_OP_SUB: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _mv[_a] - _mv[_b];
                    break;
                }
                case VM_OP_MUL: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = (_mt[_a] == VM_TYPE_FLOAT || _mt[_b] == VM_TYPE_FLOAT) ? VM_TYPE_FLOAT : VM_TYPE_INT;
                    _mv[_d] = _mv[_a] * _mv[_b];
                    break;
                }
                case VM_OP_DIV: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
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
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_b] != 0) ? _mv[_a] mod _mv[_b] : 0;
                    break;
                }

                // ==================== 比较 ====================
                case VM_OP_EQ: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] == _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_NEQ: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] != _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_GT: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] > _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_GTE: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] >= _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_LT: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] < _mv[_b]) ? 1 : 0;
                    break;
                }
                case VM_OP_LTE: {
                    var _d = code[_ip]; _ip += 1;
                    var _a = code[_ip]; _ip += 1;
                    var _b = code[_ip]; _ip += 1;
                    _mt[_d] = VM_TYPE_INT;
                    _mv[_d] = (_mv[_a] <= _mv[_b]) ? 1 : 0;
                    break;
                }

                // ==================== IF ====================
                case VM_OP_IF: {
                    var _cond_addr = code[_ip]; _ip += 1;
                    var _true_ip = code[_ip]; _ip += 1;
                    var _false_ip = code[_ip]; _ip += 1;

                    var _cond = _mv[_cond_addr];
                    if (!is_real(_cond)) _cond = 0;

                    if (_cond != 0) {
                        if (_true_ip != -1) _ip = _true_ip;
                    } else {
                        if (_false_ip != -1) _ip = _false_ip;
                    }
                    break;
                }

                // ==================== JMP ====================
                case VM_OP_JMP: {
                    _ip = code[_ip];
                    break;
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
        mod_error_report(vm, name, _ip, _err);
        // 严格模式：再抛一次，让 GameMaker 报出完整调用栈（开发期用；会中断当帧）
        if (variable_global_exists("_VM_strict") && global._VM_strict) throw _err;
        return -1;
    }
}
*/
/// @function VM_Execute(vm, buf, name)
function VM_Execute(vm, buf, name) {
    if (!buffer_exists(buf)) return 0;
    try {
        if (global._VM_debug_mode && (global._VM_debug_block == "" || name == global._VM_debug_block)) return VM_Execute_debug(vm, buf, name);

        // ══════════════ 新增：解码缓存命中就走数组版解释器 ══════════════
        // vm.codes = 块名 → VM_Decode 出来的值数组（惰性解码，一块只解一次）
        // 解码结果为空（解码失败/空块）就继续走下面的原版字节码路径，等于一行开关就能回退
        if (!variable_struct_exists(vm, "codes")) vm[$ "codes"] = ds_map_create();
        if (!ds_map_exists(vm.codes, name)) {
            vm.codes[? name] = VM_Decode(buf, noone, vm);
        }
        var _cached_code = vm.codes[? name];
        if (is_array(_cached_code) && array_length(_cached_code) > 0) {
			return VM_Execute_code_opt(vm, _cached_code, name)
			//return VM_Execute_code(vm, _cached_code, name);
        }
        // ══════════════ 以下为原来的字节码解释器，未改动 ══════════════
        buffer_seek(buf, buffer_seek_start, 0);
        var _size = buffer_get_size(buf);
        var _mt = vm.mem_type;
        var _mv = vm.mem_val;
        var _mlen = array_length(_mt);
        var _pos_cur = 0;   // 每次取指前记一次位置，出错时报出来

        while (buffer_tell(buf) < _size) {
            _pos_cur = buffer_tell(buf);
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
        mod_error_report(vm, name, _pos_cur, _err, true);
        // 严格模式：再抛一次，让 GameMaker 报出完整调用栈（开发期用；会中断当帧）
        if (variable_global_exists("_VM_strict") && global._VM_strict) throw _err;
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
        mod_error_report(vm, name, _pos, _err, true);
        // 严格模式：再抛一次，让 GameMaker 报出完整调用栈（开发期用；会中断当帧）
        if (variable_global_exists("_VM_strict") && global._VM_strict) throw _err;
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

/// @function VM_CallFuncRaw(_func_id, _args)
/// @desc **只给控制台 vmcall 用**：从已注册的 VM 函数表里按编号/名字调，参数为内存地址。
///       （脚本侧要按名字调用请用 VM_CallFunc，它走的是独立字典，和这里无关）
/// @param {Real|String} _func_id  函数 ID（见 VM_RegisterFunction 注册顺序）或函数名（如 "VM_SpawnEnemy"）
/// @param {Array} _args  参数数组，每个元素是内存地址
/// @returns {String} 调用结果描述
function VM_CallFuncRaw(_func_id, _args) {
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

/// @function VM_CallFunc(name, 参数1, ..., 参数15)
/// @desc 按名字调用**独立字典** global._VM_call_dict 里的函数（不是 VM 注册表，也和控制台 vmcall 无关）。
///       名字对编译器只是字符串，编译器不检查、不校验参数——所以往字典里加函数不用改编译器、
///       不用重编编译器、不用跑编辑器 sync。
///       字典条目：{ fn: 函数引用, raw: 收地址/收真值, desc: 说明 }，见本文件末尾的字典初始化。
///         raw = false → 目标是 VM 函数（收内存地址），把地址原样转发
///         raw = true  → 目标是普通 GML 函数（收真值），转发前先用 vm_arg 把地址转成值
/// @return 被调函数的返回值；名字不在字典里返回 undefined（同时打一条调试输出）
function VM_CallFunc(name_addr, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) {
    var _name = vm_arg(name_addr);
    if (!variable_global_exists("_VM_call_dict")) return undefined;
    var _dict = global._VM_call_dict;
    if (!ds_map_exists(_dict, _name)) {
        show_debug_message("[VM_CallFunc] 字典里没有这个函数: " + string(_name) + "（VM_FuncExists 可先判断）");
        return undefined;
    }
    // 收集参数：它们是内存地址，遇到 undefined 说明后面没传了（和 VM_ShellPrint 同一个套路）
    var _slots = [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o];
    var _addrs = [];
    for (var _i = 0; _i < 15; _i++) {
        if (is_undefined(_slots[_i])) break;
        array_push(_addrs, _slots[_i]);
    }
    var _ent = _dict[? _name];
    var _args = _addrs;
    if (_ent[$ "raw"]) {          // 普通 GML 函数：地址 → 值
        _args = [];
        for (var _i = 0; _i < array_length(_addrs); _i++) {
            array_push(_args, vm_arg(_addrs[_i]));
        }
    }
    return script_execute_ext(_ent[$ "fn"], _args);
}

/// @function VM_FuncExists(name)
/// @return 1 = 字典里有这个函数，0 = 没有
/// @desc VM_CallFunc 的配套：调之前先问一句
function VM_FuncExists(name_addr) {
    var _name = vm_arg(name_addr);
    if (!variable_global_exists("_VM_call_dict")) return 0;
    return ds_map_exists(global._VM_call_dict, _name) ? 1 : 0;
}

/// @function VM_FuncDesc(name)
/// @return 字典里登记的该函数说明字符串；没登记或没写说明返回 ""
/// @desc VM_CallFunc 的配套：看这个函数是干什么的、参数怎么排
function VM_FuncDesc(name_addr) {
    var _name = vm_arg(name_addr);
    if (!variable_global_exists("_VM_call_dict")) return "";
    if (!ds_map_exists(global._VM_call_dict, _name)) return "";
    var _ent = global._VM_call_dict[? _name];
    if (!is_struct(_ent) || !variable_struct_exists(_ent, "desc")) return "";
    return _ent[$ "desc"];
}

/// @function VM_CanPlace(card_id, col, row)
/// @desc 按游戏正规种植规则判断某格能不能种这张卡：地形、障碍、水域/莲叶、护盾层、底座卡、
///       替换开关全都算进去（就是玩家手牌点下去时走的那套规则）。
/// @return 1 = 能种，0 = 不能种
function VM_CanPlace(card_id_addr, col_addr, row_addr) {
    var _id  = vm_arg(card_id_addr);
    var _col = vm_arg(col_addr);
    var _row = vm_arg(row_addr);
    if (!is_string(_id) || _id == "") return 0;
    // 卡池数据：plant_type / feature_type / target_card 是判定要用的三项
    var _shape = 0;
    var _save_info = get_card_info_simple(_id);
    if (is_struct(_save_info)) _shape = _save_info[$ "shape"] ?? 0;
    var _card_data = deck_get_card_data(_id, _shape);
    if (is_undefined(_card_data) || _card_data == noone) return 0;
    var _world = get_world_position_from_grid(_col, _row);
    var _ok = can_place_at_position(_world.x, _world.y,
                                    _card_data[? "plant_type"],
                                    _card_data[? "feature_type"],
                                    _card_data[? "target_card"]);
    return _ok ? 1 : 0;
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
    var _builtins = ["x", "y", "xstart", "ystart", "depth", "image_index", "image_speed", "sprite_index", "object_index", "direction", "speed","state"];
    for (var _b = 0; _b < array_length(_builtins); _b++) {
        _snap[$ _builtins[_b]] = variable_instance_get(_inst, _builtins[_b]);
    }
    _snap[$ "_object_name"] = object_get_name(_inst.object_index);
    return _snap;
}

function VM_QueueHook(buf, key, id) {
    // ⚠️ 以前这里是 `if (!buffer_exists(buf)) return;`：全局 buffer 只有**地图脚本**定义了同名块才有值，
    //    导致「只有 mod 单位（卡/武器/宝石…）定义了这个块」时永远排不进队列 → 挂载点 vm_hook_run 也不会被调到。
    //    现在改成：全局没有 buffer，但只要挂载点上有 VM 注册过这个事件，就照样入队；
    //    两边都没有才真的跳过（保持零开销）。
    var _hook_name = "";
    switch (key) {
        case "card":          _hook_name = "_VM_CARD_CREATED";      break;
        case "card_del":      _hook_name = "_VM_CARD_DESTROYED";    break;
        case "enemy":         _hook_name = "_VM_ENEMY_SPAWNED";     break;
        case "enemy_kill":    _hook_name = "_VM_ENEMY_KILLED";      break;
        case "platform_idle": _hook_name = "_VM_PLATFORM_IDLE_END"; break;
    }
    var _has = buffer_exists(buf);
    if (!_has && _hook_name != "" && variable_global_exists("_VM_hooks")
        && ds_map_exists(global._VM_hooks, _hook_name)) {
        _has = (array_length(global._VM_hooks[? _hook_name]) > 0);
    }
    if (!_has) return;
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
            // 全局（地图脚本）没定义这个块时 _e.buf 是 undefined —— 只有 mod 单位挂了这个事件，
            // 那就跳过地图脚本的执行，直接交给 vm_hook_run 跑各 mod 的块
            if (buffer_exists(_e.buf)) VM_Execute(global.__vm, _e.buf, _hook_name);
            vm_hook_run(_hook_name);   // mod 侧：同一时机，各自查块（读上面刚设好的 _VM_last_* 全局）
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
            vm_hook_run("_VM_WAVE_START");
            break;
        case "wave_end":
            if (buffer_exists(global._VM_WAVE_END)) VM_Execute(global.__vm, global._VM_WAVE_END, "_VM_WAVE_END");
            vm_hook_run("_VM_WAVE_END");
            break;
        case "subwave_start":
            if (buffer_exists(global._VM_SUBWAVE_START)) VM_Execute(global.__vm, global._VM_SUBWAVE_START, "_VM_SUBWAVE_START");
            vm_hook_run("_VM_SUBWAVE_START");
            break;
        case "subwave_end":
            if (buffer_exists(global._VM_SUBWAVE_END)) VM_Execute(global.__vm, global._VM_SUBWAVE_END, "_VM_SUBWAVE_END");
            vm_hook_run("_VM_SUBWAVE_END");
            break;
        case "boss_state_change":
            if (!is_undefined(_data[$ "id"]))  global._VM_last_boss_state_change_id = _data[$ "id"];
            if (!is_undefined(_data[$ "old"])) global._VM_last_boss_old_state = _data[$ "old"];
            if (!is_undefined(_data[$ "new"])) global._VM_last_boss_new_state = _data[$ "new"];
            if (buffer_exists(global._VM_BOSS_STATE_CHANGE)) VM_Execute(global.__vm, global._VM_BOSS_STATE_CHANGE, "_VM_BOSS_STATE_CHANGE");
            vm_hook_run("_VM_BOSS_STATE_CHANGE");
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

/// @function VM_GetCardProp(card_id, prop)
/// @param card_id 卡片 id（字符串）
/// @param prop    属性名，见下
/// @return 属性值；卡不存在 / 属性名不认识 → undefined（脚本用 VM_IsUndefined 判断）
/// @desc 按属性名读一张卡的数据，单值返回（不再往命名数组里塞）。
///       存档类（走 get_card_info_simple，是玩家自己那张卡的进度）：
///         "shape" "level" "skill" "max_level" "max_shape"
///       卡池类（走 deck_get_card_data，是卡片本身的配置，与存档无关）：
///         "plant_type" "feature_type" "target_card" "cost" "cooldown"
///       复制类卡用：VM_GetCardProp(上一张卡id, "shape" / "level" / "skill") 读出来
///       原样丢给 VM_SpawnPlant；"plant_type" / "feature_type" 用来判格子能不能种。
function VM_GetCardProp(card_id_addr, prop_addr) {
    var _id   = vm_arg(card_id_addr);
    var _prop = vm_arg(prop_addr);
    if (!is_string(_id) || _id == "" || !is_string(_prop)) return undefined;

    // 存档类
    if (_prop == "shape" || _prop == "level" || _prop == "skill"
     || _prop == "max_level" || _prop == "max_shape") {
        var _info = get_card_info_simple(_id);
        if (!is_struct(_info)) return undefined;
        return _info[$ _prop];
    }

    // 卡池类：先拿存档里的 shape 才能定位到对应形态
    if (_prop == "plant_type" || _prop == "feature_type" || _prop == "target_card"
     || _prop == "cost" || _prop == "cooldown") {
        var _shape = 0;
        var _save_info = get_card_info_simple(_id);
        if (is_struct(_save_info)) _shape = _save_info[$ "shape"] ?? 0;
        var _card_data = deck_get_card_data(_id, _shape);
        if (is_undefined(_card_data) || _card_data == noone) return undefined;
        return _card_data[? _prop];
    }

    return undefined;
}

// ══════════════════════════════════════════════════════════════════
// 145 VM_GetInfo / 146 VM_CatInRow / 147 VM_MapObj
// 148 VM_GetInstanceCount / 149 VM_GetInstanceAt
// 变长函数用 16 个形参收地址（和 VM_ShellPrint 同一套），实际几个参数看后面续没续地址
// ══════════════════════════════════════════════════════════════════

/// @function vm_args_of_16(a..p)
/// @desc 把变长函数的 16 个地址参数转成值数组（遇到 undefined 就停，说明后面没传）
function vm_args_of_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    var _addrs = [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p];
    var _out = [];
    for (var _i = 0; _i < 16; _i++) {
        if (is_undefined(_addrs[_i])) break;
        array_push(_out, vm_read_mem(global.__vm, _addrs[_i]));
    }
    return _out;
}

/// @function vm_info_query(_args)
/// @desc VM_GetInfo 的实现体（非专用操作码和专用操作码两条路共用）。
///       _args = [类别, id, 字段1, 字段2, ...]
///       字符串段 = 取字段（struct 用 [$]、ds_map 用 [?]），数字段 = 取下标；
///       数字段落在 ds_map 上时自动转成字符串键（注册表的 shapes/upgrades 键就是 "0"/"3"）。
///       查到数组 / 结构体 / ds_map 本身 → undefined（要求继续往下写）。
///       card 读 global.plant_registry（大池子，不是存档那份）。
function vm_info_query(_args) {
    if (!is_array(_args) || array_length(_args) < 3) return undefined;
    var _kind = _args[0];
    var _id   = _args[1];
    if (!is_string(_kind) || !is_string(_id) || _id == "") return undefined;

    var _root = undefined;
    switch (_kind) {
        case "card":
            if (!variable_global_exists("plant_registry")) {
                shell_print("[GetInfo] 没有 global.plant_registry 这个全局");   // 正常不该出现；出现说明初始化顺序坏了
                break;
            }
            if (!ds_map_exists(global.plant_registry, _id)) {
                shell_print("[GetInfo] plant_registry 里没有 \"" + _id + "\"（现有 " + string(ds_map_size(global.plant_registry)) + " 个键）");
                break;
            }
            _root = global.plant_registry[? _id];
            break;
        case "enemy":
            if (variable_global_exists("enemy_map") && ds_map_exists(global.enemy_map, _id)) {
                _root = global.enemy_map[? _id];
            }
            break;
        case "weapon":
            if (variable_global_exists("weapon_pool") && ds_map_exists(global.weapon_pool, _id)) {
                _root = global.weapon_pool[? _id];
            }
            break;
        case "gem":
            if (variable_global_exists("gems_pool") && ds_map_exists(global.gems_pool, _id)) {
                _root = global.gems_pool[? _id];
            }
            break;
        default:
            return undefined;
    }
    if (is_undefined(_root)) return undefined;

    var _node = _root;
    for (var _i = 2; _i < array_length(_args); _i++) {
        var _seg = _args[_i];
        if (is_undefined(_node)) return undefined;

        // 三种取法挨个试（数组下标 / ds_map 键 / 结构体字段），谁成功用谁。
        // ⚠️ 不靠 is_real / is_struct / ds_exists 判类型：运行时里 ds id 的类型和 is_real()
        //    的判定并不一致（本仓库 div 那里就踩过 int64 的坑），直接"试着访问"最稳。
        //    VM_GetInfo 是低频查询，这点开销无所谓。
        var _next = undefined;
        var _got  = false;

        if (!_got && is_real(_seg)) {                       // ① 数组下标
            try {
                var _k = floor(_seg);
                if (is_array(_node) && _k >= 0 && _k < array_length(_node)) { _next = _node[_k]; _got = true; }
            } catch (_e1) { _got = false; }
        }
        if (!_got) {                                        // ② ds_map：数字段自动转字符串键
            var _key = is_real(_seg) ? string(floor(_seg)) : _seg;
            try {
                if (is_string(_key) && ds_map_exists(_node, _key)) { _next = _node[? _key]; _got = true; }
            } catch (_e2) { _got = false; }
        }
        if (!_got && is_string(_seg)) {                     // ③ 结构体字段
            try {
                if (variable_struct_exists(_node, _seg)) { _next = _node[$ _seg]; _got = true; }
            } catch (_e3) { _got = false; }
        }

        if (!_got) return undefined;
        _node = _next;
    }
    if (is_int64(_node)) return real(_node);                // int64 转回 real，别把 int64 丢给 VM
    if (is_real(_node) || is_string(_node)) return _node;   // 只放行数字 / 字符串
    return undefined;
}

function VM_GetInfo(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    return vm_info_query(vm_args_of_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p));
}

/// @function vm_cat_first_in_row(_row)
/// @desc 该行第一只猫（obj_cat；海底图的螃蟹是同一个对象）的实例 id，没有返回 -1
function vm_cat_first_in_row(_row) {
    if (!is_real(_row)) return -1;
    var _want = floor(_row);
    var _found = -1;
    with (obj_cat) {
        if (_found == -1 && variable_instance_exists(id, "row") && row == _want) _found = id;
    }
    if (_found == -1) return -1;
    return VM_ClientWrapId(_found);
}

function VM_CatInRow(row_addr) {
    return vm_cat_first_in_row(vm_read_mem(global.__vm, row_addr));
}

/// @function vm_mapobj_query(_args)
/// @desc VM_MapObj 的实现体。_args = [列, 行] 或 [列, 行, 名字]。
///       读 global.cell_terrain_flag[row * grid_cols + col] 的位：
///         bit0 obstacle / bit1 mucus / bit2 lava / bit3 seawater
///         bit4 barrier / bit5 fog / bit6 cloud / bit7 wind_tunnel
///       名字 "all" 或 "" = 任意一种；"list" = 返回逗号分隔名字串；其它名字不认识 → -1。
///       列或行传 -1 = 该方向不限。返回 1 / 0（"list" 时返回字符串）。
function vm_mapobj_query(_args) {
    if (!is_array(_args) || array_length(_args) < 2) return -1;
    var _col = _args[0];
    var _row = _args[1];
    if (!is_real(_col) || !is_real(_row)) return -1;
    _col = floor(_col);
    _row = floor(_row);

    var _name = (array_length(_args) >= 3) ? _args[2] : "";
    if (!is_string(_name)) _name = "";

    if (!variable_global_exists("cell_terrain_flag") || !variable_global_exists("grid_cols")) return 0;
    var _cols = global.grid_cols;
    var _rows = global.grid_rows;
    if (_cols <= 0 || _rows <= 0) return 0;
    if (array_length(global.cell_terrain_flag) != _cols * _rows) return 0;

    var _names = ["obstacle", "mucus", "lava", "seawater", "barrier", "fog", "cloud", "wind_tunnel"];

    var _c1 = (_col == -1) ? 0 : _col;
    var _c2 = (_col == -1) ? _cols - 1 : _col;
    var _r1 = (_row == -1) ? 0 : _row;
    var _r2 = (_row == -1) ? _rows - 1 : _row;

    // "list"：这一格（或这一行/列）上有哪些
    if (_name == "list") {
        var _out = "";
        for (var _b = 0; _b < 8; _b++) {
            var _hit = 0;
            for (var _rr = _r1; _rr <= _r2 && _hit == 0; _rr++) {
                if (_rr < 0 || _rr >= _rows) continue;
                for (var _cc = _c1; _cc <= _c2; _cc++) {
                    if (_cc < 0 || _cc >= _cols) continue;
                    if ((global.cell_terrain_flag[_rr * _cols + _cc] & (1 << _b)) != 0) { _hit = 1; break; }
                }
            }
            if (_hit == 1) {
                if (_out != "") _out += ",";
                _out += _names[_b];
            }
        }
        return _out;
    }

    // 掩码
    var _mask = 0;
    if (_name == "" || _name == "all") {
        _mask = 255;
    } else {
        var _idx = -1;
        for (var _b2 = 0; _b2 < 8; _b2++) { if (_names[_b2] == _name) { _idx = _b2; break; } }
        if (_idx == -1) return -1;
        _mask = 1 << _idx;
    }

    for (var _r = _r1; _r <= _r2; _r++) {
        if (_r < 0 || _r >= _rows) continue;
        for (var _c = _c1; _c <= _c2; _c++) {
            if (_c < 0 || _c >= _cols) continue;
            if ((global.cell_terrain_flag[_r * _cols + _c] & _mask) != 0) return 1;
        }
    }
    return 0;
}

function VM_MapObj(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    return vm_mapobj_query(vm_args_of_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p));
}

/// @function vm_obj_of_name(_name)
/// @desc 类名 → 对象资源索引。类名就是游戏里的 object 名（obj_xxxx），必须写全；
///       不是对象资源（写错成贴图名 / 不存在）返回 -1
function vm_obj_of_name(_name) {
    if (!is_string(_name) || _name == "") return -1;
    // 判据和 VM_CreateInstance 完全一致（那边实测能建出实例），不加别的类型检查
    var _idx = asset_get_index(_name);
    if (_idx < 0) return -1;
    return _idx;
}

function VM_GetInstanceCount(name_addr) {
    var _obj = vm_obj_of_name(vm_read_mem(global.__vm, name_addr));
    if (_obj < 0) return -1;
    return instance_number(_obj);
}

function VM_GetInstanceAt(name_addr, k_addr) {
    var _obj = vm_obj_of_name(vm_read_mem(global.__vm, name_addr));
    if (_obj < 0) return -1;
    var _k = vm_read_mem(global.__vm, k_addr);
    if (!is_real(_k)) return -1;
    _k = floor(_k);
    if (_k < 0 || _k >= instance_number(_obj)) return -1;
    return VM_ClientWrapId(instance_find(_obj, _k));
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

// 命中位掩码：等价 can_target_on，但判定变成一次位与（省掉每帧每弹每敌的字符串比较）
// 敌人按自己的 target_type 占一位（obj_battle 每帧写进 tbit），子弹按"能打哪些类型"或起来（创建时写进 ttype）
#macro HIT_NORMAL      1
#macro HIT_AIR         2
#macro HIT_DANCE       4
#macro HIT_OBSTACLE    8
#macro HIT_DIVER       16
#macro HIT_UNDERGROUND 32
#macro HIT_OTHER       64   // 其它/未知敌人类型：只有子弹类型 "all" 能打（can_target_on 的兜底行为）

/// @function enemy_tbit_of(type)
/// @desc 敌人 target_type 字符串 → 命中位掩码（唯一一份映射，别处不要再抄 switch）。
///       由 obj_enemy_parent 自己在 Create / Step_2 里写进 tbit，不再依赖 obj_battle 的每帧遍历 ——
///       否则刚生成的敌人、或排在 obj_battle 之前执行的管理器读 tbit 会"变量未定义"。
function enemy_tbit_of(_t) {
    switch (_t) {
        case "normal":      return HIT_NORMAL;
        case "air":         return HIT_AIR;
        case "dance":       return HIT_DANCE;
        case "obstacle":    return HIT_OBSTACLE;
        case "diver":       return HIT_DIVER;
        case "underground": return HIT_UNDERGROUND;
    }
    return HIT_OTHER;   // "invisible" / mod 自定义类型等
}

/// @function bullet_type_mask(type)
/// @desc 子弹 type 字符串 → 命中位掩码，和 can_target_on 一一对应
function bullet_type_mask(_t) {
    switch (_t) {
        case "normal":   return HIT_NORMAL | HIT_OBSTACLE;
        case "air":      return HIT_NORMAL | HIT_AIR;
        case "air_only": return HIT_AIR;
        case "pierce":   return HIT_NORMAL | HIT_DANCE | HIT_OBSTACLE;
        case "track":    return HIT_NORMAL | HIT_DIVER | HIT_AIR;
        case "throw":    return HIT_NORMAL | HIT_DIVER;
        case "rotate":   return HIT_NORMAL | HIT_AIR | HIT_DANCE | HIT_OBSTACLE | HIT_DIVER | HIT_UNDERGROUND;
        case "d_fruit":  return HIT_NORMAL | HIT_DANCE | HIT_OBSTACLE | HIT_DIVER | HIT_UNDERGROUND;
        case "all":      return HIT_NORMAL | HIT_AIR | HIT_DANCE | HIT_OBSTACLE | HIT_DIVER | HIT_UNDERGROUND | HIT_OTHER;
    }
    return 0;
}

/// @function bullet_merge_find(mgr, x, y, spr, scale, vx, vy, dmg, hits, life, range, type,
///                             damage_type, flag, angle, ty, lk, ignore_dist)
/// @desc 在新弹的出生格里找一颗"完全同类"的子弹来合并（表由 Step 每帧重建，每格 16 槽）
///       字段必须全等才合并（同一贴图 = 同一种子弹），只有距离要求分档：
///       ignore_dist = false（一阶段）要求 |dx|、|dy| ≤ 8；true（二/三阶段）不限距离，取最近的
/// @return 候选下标；没有可合并的返回 -1
function bullet_merge_find(mgr, x, y, spr, scale, vx, vy, dmg, hits, life, range, type,
                           damage_type, flag, angle, ty, lk, ignore_dist) {
    if (!variable_global_exists("grid_cols") || !variable_global_exists("grid_rows")) return -1;
    var _cw = global.grid_cell_size_x;
    var _ch = global.grid_cell_size_y;
    if (_cw <= 0 || _ch <= 0) return -1;
    var _gc = floor((x - global.grid_offset_x) / _cw);
    if (_gc < 0 || _gc >= global.grid_cols) return -1;
    var _gr = floor((y - global.grid_offset_y) / _ch);
    if (_gr < 0 || _gr >= global.grid_rows) return -1;
    var _k = _gr * global.grid_cols + _gc;
    if (_k >= array_length(mgr.cell_n)) return -1;
    var _n = mgr.cell_n[_k];
    if (_n <= 0) return -1;

    var _ttype  = bullet_type_mask(type);
    var _pierce = (hits < 0);
    var _base   = _k * 16;
    var _cnt    = mgr.count;
    var _best   = -1;
    var _bestd  = 999999;
    for (var _j = 0; _j < _n; _j++) {
        var _idx = mgr.bullet_grid[_base + _j];
        if (_idx < 0 || _idx >= _cnt) continue;          // 已死（交换删除会把它挪到 >= count）
        var _b = mgr.list[_idx];
        if (_b.spr != spr || _b.scale != scale) continue;
        if (_b.vx != vx || _b.vy != vy) continue;
        if (_b.dmg != dmg) continue;
        if ((_b.hits < 0) != _pierce) continue;          // 穿透 / 非穿透不能混（合并规则不同）
        if (_b.life != life || _b.cell_range != range) continue;
        if (_b.ttype != _ttype) continue;
        if (_b.damage_type != damage_type) continue;
        if (_b.flag != flag || _b.angle != angle) continue;
        if (_b.ty != ty || _b.lk != lk) continue;
        var _dx = abs(_b.x - x);
        var _dy = abs(_b.y - y);
        if (!ignore_dist && (_dx > 8 || _dy > 8)) continue;
        var _d = _dx + _dy;
        if (_d < _bestd) { _bestd = _d; _best = _idx; }
    }
    return _best;
}

/// @function bullet_merge_into(mgr, idx, dmg, hits)
/// @desc 把一发新弹并进既有子弹：穿透（hits < 0）伤害累加，非穿透命中次数累加
function bullet_merge_into(mgr, idx, dmg, hits) {
    var _b = mgr.list[idx];
    if (_b.hits < 0) {
        _b.dmg += dmg;
    } else {
        _b.hits += hits;
    }
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
function bullet_screen_add(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
                           type = "all", death_obj = "", death_mod = "",
                           damage_type = "normal", flag = 0, angle = 0, ty = -1, lk = 0.15, death_spr = "") {
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
    if (is_undefined(damage_type) || damage_type == "") damage_type = "normal";
    if (is_undefined(flag)) flag = 0;
    if (is_undefined(angle)) angle = 0;
    if (is_undefined(ty)) ty = -1;
    if (is_undefined(lk)) lk = 0.15;

    // ── 合并三阶段（按当前弹数分档，参数见 Create_0 的 merge_lv1 / merge_lv2）──
    //    一阶段：字段全等 + 距离 8px 内；二/三阶段：字段全等即可，取最近的
    //    merge_enable = false 时整段跳过 = 原逻辑（能加就加，满了就丢）
    var _mn = _mgr.count;
    var _mi = -1;
    if (_mgr.merge_enable) {
    if (_mn >= _mgr.merge_lv1) {
        _mi = bullet_merge_find(_mgr, x, y, _spr, scale, vx, vy, dmg, hits, life, range,
                                type, damage_type, flag, angle, ty, lk, true);
        if (_mi >= 0) {
            bullet_merge_into(_mgr, _mi, dmg, hits);
            _mgr.merge2_n += 1; _mgr.merge_n += 1;
            return _mi;
        }
        if (_mn > _mgr.merge_lv2) { _mgr.drop_n += 1; return -1; }   // 三阶段：合并不了就丢弃
    } else {
        _mi = bullet_merge_find(_mgr, x, y, _spr, scale, vx, vy, dmg, hits, life, range,
                                type, damage_type, flag, angle, ty, lk, false);
        if (_mi >= 0) {
            bullet_merge_into(_mgr, _mi, dmg, hits);
            _mgr.merge1_n += 1; _mgr.merge_n += 1;
            return _mi;
        }
    }
    }
    if (_mn >= _mgr.bullet_max) { _mgr.drop_n += 1; return -1; }     // 满了：丢弃（原逻辑）

    // 原地填 list[count] 那个现成的"空子弹"结构体（不新建，省掉每颗一次的分配）
    var _b = _mgr.list[_mn];
    _b.spr         = _spr;
    _b.frames      = _frames;
    _b.cell_range  = range;
    _b.scale       = scale;
    _b.angle       = angle;    // 出生角度（度）：后向子弹传 180，和原版 image_angle = 180 一致
    _b.anim_speed  = anim_speed;
    _b.frame       = 0;
    _b.x           = x;
    _b.y           = y;
    _b.vx          = vx;
    _b.vy          = vy;
    _b.dmg         = dmg;
    _b.hits        = hits;
    _b.life        = life;
    _b.target_type = type;
    _b.ttype       = bullet_type_mask(type);   // 命中掩码：命中判定只做一次位与，不再比字符串
    _b.damage_type = damage_type;              // 命中走 damage_enemy（闪白/音效/护盾 + 各敌人的 Other_10）
    _b.flag        = flag;                     // 卡片效果位：普通版本固定"什么都不吃"，只有 _Ex 加的子弹才参与
    _b.freeze      = 0;                        // 累计冰冻帧数：命中时写给敌人的 ice_timer
    _b.death_obj   = death_obj;
    _b.death_mod   = death_mod;
    _b.death_spr   = death_spr;
    // ⚠️ 槽位是回收复用的，这两个必须每次重置，否则新弹会继承上一颗的渐变目标
    _b.ty          = ty;
    _b.lk          = lk;

    _mgr.count = _mn + 1;
    _mgr.add_n += 1;
    return _mgr.count - 1;
}

/// @function VM_BulletScreenAdd(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life, type, death_obj, death_mod)
/// @desc VM 侧接口，参数和返回值和 bullet_screen_add 一致
function VM_BulletScreenAdd(spr_addr, frames_addr, range_addr, scale_addr, anim_speed_addr, x_addr, y_addr, vx_addr, vy_addr, dmg_addr, hits_addr, life_addr, type_addr, death_obj_addr, death_mod_addr) {
    return bullet_screen_add(vm_arg(spr_addr), vm_arg(frames_addr), vm_arg(range_addr), vm_arg(scale_addr), vm_arg(anim_speed_addr),
                             vm_arg(x_addr), vm_arg(y_addr), vm_arg(vx_addr), vm_arg(vy_addr), vm_arg(dmg_addr),
                             vm_arg(hits_addr), vm_arg(life_addr), vm_arg(type_addr), vm_arg(death_obj_addr), vm_arg(death_mod_addr));
}

/// @function bullet_screen_add_Ex(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
///                                type, death_obj, death_mod, damage_type, flag)
/// @desc 往 obj_Bullet_Screen_Management 里塞一颗子弹，比 bullet_screen_add 多两个可控项：
///       【伤害类型】走 damage_enemy 的护盾口径（normal / pierce / 其它）；
///       【标志数值】flag = 这颗子弹**还能接受哪些类别的卡片效果**（位掩码）：
///           bit1 = 过火类、bit2 = 解冻类、4 / 8 / 16 … = 自定义类（效果全看卡片上的属性字段）
///       子弹进到格子中心带时，与本格卡片上的 bullet_flag 做【与运算】，> 0 就应用并消位，
///       所以同一类卡片对同一颗子弹只生效一次。
///       规则和卡片字段详见 obj_Bullet_Screen_Management/Step_0.gml 的「3.5 卡片效果」
/// @return 子弹在管理器数组里的下标；管理器或贴图不存在时返回 -1
function bullet_screen_add_Ex(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
                              type = "all", death_obj = "", death_mod = "",
                              damage_type = "normal", flag = 0, angle = 0, ty = -1, lk = 0.15, death_spr = "") {
    // 全部参数一次传下去（合并判定要能看全这些字段，所以不再"先加进去再改写"）
    return bullet_screen_add(spr, frames, range, scale, anim_speed, x, y, vx, vy,
                             dmg, hits, life, type, death_obj, death_mod,
                             damage_type, flag, angle, ty, lk, death_spr);
}

/// @function VM_BulletScreenAdd_Ex(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
///                                 type, death_obj, death_mod, damage_type, flag, angle)
/// @desc VM 侧接口，参数和返回值和 bullet_screen_add_Ex 一致
function VM_BulletScreenAdd_Ex(spr_addr, frames_addr, range_addr, scale_addr, anim_speed_addr,
                               x_addr, y_addr, vx_addr, vy_addr, dmg_addr, hits_addr, life_addr,
                               type_addr, death_obj_addr, death_mod_addr,
                               damage_type_addr, flag_addr, angle_addr) {
    return bullet_screen_add_Ex(vm_arg(spr_addr), vm_arg(frames_addr), vm_arg(range_addr), vm_arg(scale_addr),
                                vm_arg(anim_speed_addr), vm_arg(x_addr), vm_arg(y_addr), vm_arg(vx_addr),
                                vm_arg(vy_addr), vm_arg(dmg_addr), vm_arg(hits_addr), vm_arg(life_addr),
                                vm_arg(type_addr), vm_arg(death_obj_addr), vm_arg(death_mod_addr),
                                vm_arg(damage_type_addr), vm_arg(flag_addr), vm_arg(angle_addr));
}

/// @function bullet_screen_add_Exs(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
///                                 type, death_obj, death_mod, damage_type, flag, angle, ty, lk)
/// @desc 在 bullet_screen_add_Ex 基础上多两个"行渐变"参数（原版水管弹那种先拐到目标行再直线飞）：
///       【ty】目标行的**世界 y**；不想渐变就传 -1（就是普通直线子弹）
///       【lk】每帧朝目标 y 靠拢的比例；原版水管弹是 0.15（传 0 会一直不动，别传 0）
///       ⚠️ VM 侧调用要写满 20 个参数（编译器按固定个数校验），不渐变时写：…, -1, 0.15
///       渐变到位（|ty - y| <= 8）会自动吸附并把 ty 清成 -1，之后这颗弹就是纯直线，不再花那笔计算。
///       渐变过程中子弹会依次经过中间几行，那几行的敌人/卡片照常结算（行是按 y 每帧反算的）。
/// @return 子弹在管理器数组里的下标；管理器或贴图不存在、表满返回 -1
function bullet_screen_add_Exs(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
                               type = "all", death_obj = "", death_mod = "",
                               damage_type = "normal", flag = 0, angle = 0, ty = -1, lk = 0.15, death_spr = "") {
    return bullet_screen_add_Ex(spr, frames, range, scale, anim_speed, x, y, vx, vy,
                                dmg, hits, life, type, death_obj, death_mod,
                                damage_type, flag, angle, ty, lk, death_spr);
}

/// @function VM_BulletScreenAdd_Exs(spr, frames, range, scale, anim_speed, x, y, vx, vy, dmg, hits, life,
///                                  type, death_obj, death_mod, damage_type, flag, angle, ty, lk)
/// @desc VM 侧接口，参数和返回值和 bullet_screen_add_Exs 一致
function VM_BulletScreenAdd_Exs(spr_addr, frames_addr, range_addr, scale_addr, anim_speed_addr,
                                x_addr, y_addr, vx_addr, vy_addr, dmg_addr, hits_addr, life_addr,
                                type_addr, death_obj_addr, death_mod_addr,
                                damage_type_addr, flag_addr, angle_addr, ty_addr, lk_addr) {
    return bullet_screen_add_Exs(vm_arg(spr_addr), vm_arg(frames_addr), vm_arg(range_addr), vm_arg(scale_addr),
                                 vm_arg(anim_speed_addr), vm_arg(x_addr), vm_arg(y_addr), vm_arg(vx_addr),
                                 vm_arg(vy_addr), vm_arg(dmg_addr), vm_arg(hits_addr), vm_arg(life_addr),
                                 vm_arg(type_addr), vm_arg(death_obj_addr), vm_arg(death_mod_addr),
                                 vm_arg(damage_type_addr), vm_arg(flag_addr), vm_arg(angle_addr),
                                 vm_arg(ty_addr), vm_arg(lk_addr));
}

/// @function homing_bullet_instant_hit(mgr, type, dmg)
/// @desc 追踪弹到数量上限时的"直接命中"：不生成子弹，直接按管理器**当帧的索敌快照**
///       挑最左的可命中敌人（和模式 0 一个口径）结算一次伤害。
///       快照由 obj_Homing_Bullet_Management 的 Step 每帧刷新，上限触发时数组必然非空，
///       所以快照一定是最新的。
/// @return 被命中的敌人 id；没有可打目标返回 noone
function homing_bullet_instant_hit(mgr, type, dmg) {
    var _best    = noone;
    var _best_x  = 999999;
    var _best_hp = -1;
    var _mask    = bullet_type_mask(type);
    for (var _qt = 0; _qt < 6; _qt++) {
        if (mgr.scan_id[_qt] == noone) continue;
        if ((_mask & mgr.scan_bit[_qt]) == 0) continue;
        if (mgr.scan_x[_qt] < _best_x || (mgr.scan_x[_qt] == _best_x && mgr.scan_hp[_qt] > _best_hp)) {
            _best_x  = mgr.scan_x[_qt];
            _best_hp = mgr.scan_hp[_qt];
            _best    = mgr.scan_id[_qt];
        }
    }
    if (instance_exists(_best)) damage_enemy(_best, dmg, "normal");
    return _best;
}

/// @function homing_bullet_add(spr, scale, x, y, spd, dmg, type, death_obj, death_mod, death_spr, mode)
/// @param spr        贴图：sprite 索引，或资源名/文件名（走 get_load_sprite 的缓存链）
/// @param scale      缩放（横竖同值）；0 或 undefined = 1
/// @param spd        速度大小（每帧像素）；追踪时 vx/vy 每帧按朝向重算
/// @param type       子弹类型（can_hit 那一套）：all/normal/air/air_only/pierce/track/throw/rotate/d_fruit
/// @param death_obj  销毁对象：非空时在这颗子弹销毁的位置创建该对象（字符串资产名）；出界不生成
/// @param death_mod  销毁对象是 mod 对象时填它的 mod 名字（创建后写进 mod_type）；原生对象留空
/// @param death_spr  销毁对象的贴图覆盖：非空时把创建出来的销毁对象贴图换成它。
///                   有些子弹的销毁对象是通用的、要靠换贴图区分
///                   （例如糖葫芦借 obj_coke_bomb_explode 再盖上 spr_tanghulu_bullet_effect）
/// @param mode       索敌模式：0（默认）= 只咬"全场最左的可命中敌人"（糖葫芦 / 大力神）；
///                   1 = **先打发射卡前方**：本行、正前方 150px 内血最高的那个，没有才退回全场最左
///                       （章鱼烧 / 月神）。锚点 = 调用这个函数的卡片实例，自动取，不用传。
/// @return 子弹在管理器数组里的下标；贴图不存在、或管理器已到上限（没生成子弹）返回 -1
/// @desc 往 obj_Homing_Bullet_Management 的数组里塞一颗追踪弹；管理器不在时会自动创建。
///       固定口径（都不是参数）：**打一下就消失**、**不自动到期**（出界才没）、帧数取贴图自身、
///       动画速度固定 1、**每帧自转 6 度**、**伤害类型固定 "normal"**（有盾只打盾）、
///       命中 = 只管自己锁定的那个目标，像素距离 <= 90x85 就算撞上（不扫网格、不穿透）。
///       出生自带朝右的初速度（免得没索到目标时原地卡死，同原版"没敌人就按原方向飞"）。
///       **数量上限** = 管理器的 max_bullets（400）：到顶后这一发不再生成子弹，
///       改为**直接命中**管理器当帧锁定的那个敌人（伤害不丢，个数硬卡在上限）。
function homing_bullet_add(spr, scale, x, y, spd, dmg, type = "all", death_obj = "", death_mod = "", death_spr = "", mode = 0) {
    var _mgr = instance_find(obj_Homing_Bullet_Management, 0);
    if (!instance_exists(_mgr)) {
        _mgr = instance_create_depth(0, 0, -700, obj_Homing_Bullet_Management);
    }
    var _spr = is_string(spr) ? get_load_sprite(spr) : spr;
    if (!sprite_exists(_spr)) return -1;
    var _frames = sprite_get_number(_spr);      // 帧数固定取贴图自身，不再当参数传
    if (is_undefined(scale) || scale == 0) scale = 1;
    if (is_undefined(spd)) spd = 0;
    if (is_undefined(mode)) mode = 0;
    // ── 数量上限：到顶就不再生成子弹，这一发直接命中（否则子弹只会在出界时才删，会无限堆积）──
    if (array_length(_mgr.list) >= _mgr.max_bullets) {
        homing_bullet_instant_hit(_mgr, type, dmg);
        return -1;
    }
    // 锚点（模式 1 的"发射卡"）：调用这个函数的实例如果是一张卡（有 grid_row），就认它。
    // 从卡片对象事件里调 → self 就是那张卡；从 VM 卡片脚本里调 → self 也是那张 mod 卡实例。
    var _anchor = noone;
    if (instance_exists(id) && variable_instance_exists(id, "grid_row")) _anchor = id;
    array_push(_mgr.list, {
        spr:         _spr,
        frames:      _frames,
        scale:       scale,
        anim_speed:  1,          // 固定 1
        frame:       0,
        x:           x,
        y:           y,
        vx:          spd,        // 出生就给朝右的初速度：这一帧没索到目标也不会原地卡死
        vy:          0,          //   （原版糖葫芦没敌人时也是"按原方向继续飞"）
        spd:         spd,
        angle:       0,
        spin:        0,          // 自转累加器：每帧 +6，绘制取 -spin
        spin_speed:  6,          // 固定 6 度/帧（同原版 image_angle = -timer*6）
        target:      noone,      // 每帧按 mode 重挑
        anchor:      _anchor,
        mode:        mode,
        dmg:         dmg,
        hits:        1,          // 固定：打一下就消失
        life:        0 - 1,      // 固定：不自动到期，出界才没
        target_type: type,
        ttype:       bullet_type_mask(type),   // 命中掩码：判定只做一次位与
        damage_type: "normal",   // 固定：伤害类型不再当参数（四个调用点本来就全是 normal）
        death_obj:   death_obj,
        death_mod:   death_mod,
        death_spr:   death_spr
    });
    return array_length(_mgr.list) - 1;
}

/// @function VM_HomingBulletAdd(spr, scale, x, y, spd, dmg, type, death_obj, death_mod, death_spr, mode)
/// @desc VM 侧接口，参数和返回值和 homing_bullet_add 一致
function VM_HomingBulletAdd(spr_addr, scale_addr, x_addr, y_addr, spd_addr, dmg_addr, type_addr, death_obj_addr, death_mod_addr, death_spr_addr, mode_addr) {
    return homing_bullet_add(vm_arg(spr_addr), vm_arg(scale_addr),
                             vm_arg(x_addr), vm_arg(y_addr), vm_arg(spd_addr),
                             vm_arg(dmg_addr), vm_arg(type_addr),
                             vm_arg(death_obj_addr), vm_arg(death_mod_addr),
                             vm_arg(death_spr_addr), vm_arg(mode_addr));
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
global._VMfn_SetProp = VM_RegisterFunction(global.__vm, VM_SetProp);   // 11
VM_RegisterFunction(global.__vm, VM_GetWave);               // 12
VM_RegisterFunction(global.__vm, VM_GetSubwave);            // 13
global._VMfn_GetProp = VM_RegisterFunction(global.__vm, VM_GetProp);   // 14
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
global._VMfn_ArrayGet = VM_RegisterFunction(global.__vm, VM_ArrayGet);   // 79
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
global._VMfn_IsUndefined = VM_RegisterFunction(global.__vm, VM_IsUndefined);        // 97
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
global._VMfn_GetCurCard = VM_RegisterFunction(global.__vm, VM_GetCurCard);         // 109
VM_RegisterFunction(global.__vm, VM_EnemyInRange);       // 110
VM_RegisterFunction(global.__vm, VM_GetHomingTarget);    // 111
VM_RegisterFunction(global.__vm, VM_GetInstancesInRange);// 112
global._VMfn_CreateInstance = VM_RegisterFunction(global.__vm, VM_CreateInstance);     // 113
VM_RegisterFunction(global.__vm, VM_LoadSpritePerm_Ex);  // 114
VM_RegisterFunction(global.__vm, VM_SetShovelFlameRate); // 115
VM_RegisterFunction(global.__vm, VM_BulletScreenAdd);    // 116
VM_RegisterFunction(global.__vm, VM_DrawSpriteExt);      // 117
VM_RegisterFunction(global.__vm, VM_RunStep);            // 118
VM_RegisterFunction(global.__vm, VM_DestroyInstance);    // 119
VM_RegisterFunction(global.__vm, VM_GetCardProp);    // 120
VM_RegisterFunction(global.__vm, VM_CanPlace);       // 121
VM_RegisterFunction(global.__vm, VM_CallFunc);       // 122
VM_RegisterFunction(global.__vm, VM_FuncExists);     // 123
VM_RegisterFunction(global.__vm, VM_FuncDesc);       // 124
VM_RegisterFunction(global.__vm, VM_SpriteExists);   // 125
VM_RegisterFunction(global.__vm, VM_AliasSpritePerm); // 126
VM_RegisterFunction(global.__vm, VM_ArrayExists);      // 127
VM_RegisterFunction(global.__vm, VM_CellCount);        // 128
VM_RegisterFunction(global.__vm, VM_CellItem);         // 129
VM_RegisterFunction(global.__vm, VM_CellContains);     // 130
VM_RegisterFunction(global.__vm, VM_ArrayContains);    // 131
VM_RegisterFunction(global.__vm, VM_InstArrayExists);  // 132
VM_RegisterFunction(global.__vm, VM_InstArraySize);    // 133
VM_RegisterFunction(global.__vm, VM_InstArrayItem);    // 134
VM_RegisterFunction(global.__vm, VM_InstArraySet);     // 135
VM_RegisterFunction(global.__vm, VM_InstArrayAdd);     // 136
VM_RegisterFunction(global.__vm, VM_InstArrayDel);     // 137
VM_RegisterFunction(global.__vm, VM_InstArrayClear);   // 138
VM_RegisterFunction(global.__vm, VM_InstArrayContains);// 139
VM_RegisterFunction(global.__vm, VM_HomingBulletAdd);  // 140
VM_RegisterFunction(global.__vm, VM_DamageEnemy);      // 141
VM_RegisterFunction(global.__vm, VM_DamageEnemyAsh);   // 142
VM_RegisterFunction(global.__vm, VM_BulletScreenAdd_Ex);   // 143
VM_RegisterFunction(global.__vm, VM_BulletScreenAdd_Exs);  // 144
VM_RegisterFunction(global.__vm, VM_GetInfo);              // 145 — 变长：按注册表逐级查（类别, id, 字段1[, 字段2, ...]）
VM_RegisterFunction(global.__vm, VM_CatInRow);             // 146 — 该行第一只猫的实例 id
VM_RegisterFunction(global.__vm, VM_MapObj);               // 147 — 变长：格子上有没有地图物品（列, 行[, 名字/"list"]）
VM_RegisterFunction(global.__vm, VM_GetInstanceCount);     // 148 — obj_xxxx 的实例个数
VM_RegisterFunction(global.__vm, VM_GetInstanceAt);        // 149 — obj_xxxx 的第 k 个实例 id
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

// ══════════ VM_CallFunc 的可调用字典（单独维护）══════════
// 和 VM 注册表**无关**——这里没有自动收录。没写进来的，VM_CallFunc 就调不到，
// 所以这是一份明确的清单，想看能调什么、直接看这几行就行。
// 加一个「能被 VM_CallFunc 按名字调用」的函数：
//     ① 写一个 GML 函数（VM_ 开头或随便叫什么都行，**不用注册、不占 VM 函数号**）
//     ② 在下面加一行
// 因为不占函数号，所以不用改 compiler_defs.h、不用重编编译器、不用跑编辑器 sync_vmfuncs.py。
// 条目格式： 名字 → { fn: 函数引用, raw: 收地址还是收真值, desc: 说明（VM_FuncDesc 会返回它） }
//     raw = false → VM 那套：函数用 vm_read_mem(global.__vm, addr) 收内存地址（VM_* 全是这种）
//     raw = true  → 普通 GML 函数：直接收真值，参数就是普通参数，写起来不用管地址
global._VM_call_dict = ds_map_create();
// 清单写在 scripts/scr_command_VM_callFunction 里（那边只管登记，重复调安全）
scr_command_VM_callFunction();

// ── 清单（想开放哪个写哪个）──
// 示例：ds_map_add(global._VM_call_dict, "VM_SpawnPlant", { fn: VM_SpawnPlant, raw: false, desc: "种下植物（卡名,列,行,形态,星级,技能）" });
// 示例：ds_map_add(global._VM_call_dict, "my_helper",    { fn: my_helper,    raw: true,  desc: "my_helper(a, b) → 返回 ..." });

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

    // ══════════════════════════════════════════════════════════════════
    // 挂载点注册表：挂载点名 → VM 引用数组
    //   地图 bin 与每个 mod 的 VM 在加载后把自己挂进来，挂载点触发时逐个跑。
    //   一个 VM 可以同时挂在多个挂载点上（在哪个数组里，取决于它有哪些块）。
    //   ⚠️ 注册不设白名单：块名第一次出现时 vm_hook_register 自动建数组。
    //      新增挂载点只需在触发处写 vm_hook_run("_VM_新名字")，不用回来登记。
    //   ⚠️ 热重载换掉 blocks 之后要重新挂一次，见 src_mod_card_vm_fill。
    //   ⚠️ 地图专属的 _VM_ROOM_READY_ENTRY / _VM_CONST_INIT 不走这套，
    //      它们在 bin 加载当场直接执行。
    // ══════════════════════════════════════════════════════════════════
    // ⚠️ 只在**第一次**建 VM 时建表，绝不能每次 VM_Create 都 destroy+重建：
    //    VM_Create 是**每个 mod 单位各调一次**的（每张卡/武器/宝石/敌人/子弹/特效一个 VM），
    //    每次重建都会把先前加载的单位注册的挂载点全清掉 →
    //    结果是"只有最后加载的那个 mod 的挂载点还在"，_VM_CARD_CREATED / _VM_ENEMY_SPAWNED /
    //    _VM_WAVE_START … 对其它所有 mod 全部失效。
    //    地图 VM 是复用 global.__vm 的（加载器只灌块不再建 VM），且 vm_hook_register 自带去重，
    //    所以不重建也不会留下旧关卡的重复注册。
    if (!variable_global_exists("_VM_hooks")) global._VM_hooks = ds_map_create();
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

    // 地图 VM 的块表：和 mod VM 统一用 vm.blocks（块名 → 字节码 buffer），
    // 挂载点执行时按同一套逻辑取块，不用再区分"地图的块在 global._VM_XXX"。
    // 旧的先释放掉，否则每次进关卡都会漏一批 buffer。
    if (variable_struct_exists(global.__vm, "blocks")
        && ds_exists(global.__vm.blocks, ds_type_map)) {
        var _old_bn = ds_map_keys_to_array(global.__vm.blocks);
        for (var _ob = 0; _ob < array_length(_old_bn); _ob++) {
            var _old_buf = global.__vm.blocks[? _old_bn[_ob]];
            if (buffer_exists(_old_buf)) buffer_delete(_old_buf);
        }
        ds_map_clear(global.__vm.blocks);
    } else {
        global.__vm[$ "blocks"] = ds_map_create();
    }
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
    // 解码缓存必须清：字符串池重建后旧的下标全部失效
    if (variable_struct_exists(global.__vm, "codes")) ds_map_clear(global.__vm.codes);
	
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

        // 块统一进 vm.blocks（挂载点执行时和 mod VM 走同一套取块逻辑）。
        // 注册不设白名单：任何块名都登记，以后新增挂载点只需要在触发处写
        // vm_hook_run("_VM_新名字")，不必回来改清单。
        // 没被 vm_hook_run 调用的块（_OBJECT_STEP 之类）挂在表里也无害，只是不会被执行。
        if (variable_struct_exists(global.__vm, "blocks")
            && !ds_map_exists(global.__vm.blocks, _block_name)) {
            global.__vm.blocks[? _block_name] = _bc_buf;
        }
        vm_hook_register(_block_name, global.__vm);
    }
}
