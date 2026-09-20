/// @function src_mod_read_text(_path)
/// @desc 读取整个文本文件
function src_mod_read_text(_path) {
	if (!file_exists(_path)) return "";
	var _buf = buffer_load(_path);
	var _txt = buffer_read(_buf, buffer_text);
	buffer_delete(_buf);
	return _txt;
}

/// @function src_mod_save_path(_rel)
/// @desc 存档路径：mod 开启时存到 mod/saves/ 与正常存档隔离，否则存到 saves/
/// @param _rel 相对文件名
function src_mod_save_path(_rel) {
	if (global.mod_enabled) {
		directory_create("mod/saves");
		return "mod/saves/" + _rel;
	}
	return "saves/" + _rel;
}

/// @function src_mod_stat_17(_arr)
/// @desc 把数值数组补齐/截断到 17 个等级（缺的用最后一个补；非数组按定值展开）
function src_mod_stat_17(_arr) {
	if (!is_array(_arr)) { _arr = [(_arr == undefined ? 0 : _arr)]; }
	var _out = array_create(17, 0);
	for (var _i = 0; _i < 17; _i++) {
		_out[_i] = (_i < array_length(_arr)) ? _arr[_i] : _arr[array_length(_arr) - 1];
	}
	return _out;
}

/// @function src_mod_register_card(_id, _c)
/// @desc 用一份卡配置注册一张卡：依次调用 register_card(卡池) / register_plant_lite(植物数据) /
///       register_card_skill(技能) / register_goods(商店) / register_card_info_island(图鉴)。
///       植物对象统一用 obj_card_mod（继承 obj_card_parent）。
/// @param _id 卡名
/// @param _c 卡配置结构体：
/// {
///   "name": "卡名标题",
///   "shapes": [
///     {
///       "shape": 0,
///       "name": "形态名",
///       "description": "形态描述",
///       "sprite": "spr_small_fire",
///       "plant_type": "normal",
///       "feature_type": "normal",
///       "target_card": "none",
///       "hp": [50, 50, ...],       // 17 档
///       "cost": [50, 50, ...],
///       "atk": [0, 0, ...],
///       "range": [0, 0, ...],
///       "cooldown": [420, 420, ...],
///       "cycle": [1500, 1440, ...],
///       "flame_produce": [25, 27, ...]   // 可选
///     }
///   ],
///   "skill": { "attr": "cycle", "values": [78, 75, 72, 69, 66, 63, 60, 57, 54] },  // 可选
///   "shop": { "cost": "50000", "description": "商店描述" },                        // 可选
///   "info_island": "图鉴说明文本"                                                  // 可选
/// }
/// @return true=注册成功
function src_mod_register_card(_id, _c) {
	if (!is_struct(_c)) return false;
	var _shapes = _c[$ "shapes"];
	if (is_undefined(_id) || !is_array(_shapes)) return false;
	if (!variable_global_exists("mod_cards")) { global.mod_cards = ds_map_create(); }
	global.mod_cards[? _id] = _c;

	var _deck_shapes = [];   // 1 register_card 用
	var _plant_shapes = [];  // 2 register_plant_lite 用
	for (var _s = 0; _s < array_length(_shapes); _s++) {
		var _sh = _shapes[_s];
		if (!is_struct(_sh)) continue;

		// 贴图：JSON 里写字符串名，与原版 register_card 一样走 get_load_sprite
		// （项目精灵大部分是运行时加载的，asset_get_index 拿不到）
		var _spr = noone;
		var _spr_name = _sh[$ "sprite"];
		if (is_string(_spr_name) && _spr_name != "") {
			_spr = get_load_sprite(_spr_name);
		}

		var _cost_arr  = src_mod_stat_17(_sh[$ "cost"]);
		var _cd_arr    = src_mod_stat_17(_sh[$ "cooldown"]);

		array_push(_deck_shapes, {
			shape: _sh[$ "shape"] ?? _s,
			sprite: _spr,
			cost: _cost_arr[0],
			cooldown: _cd_arr[0],
			description: _sh[$ "description"] ?? _c[$ "name"] ?? _id,
			plant_type: _sh[$ "plant_type"] ?? "normal",
			feature_type: _sh[$ "feature_type"] ?? "normal",
			target_card: _sh[$ "target_card"] ?? "none",
			place_preview: _spr,
		});

		var _ps = {
			name: _sh[$ "name"] ?? _c[$ "name"] ?? _id,
			shape: _sh[$ "shape"] ?? _s,
			description: _sh[$ "description"] ?? _c[$ "name"] ?? _id,
			hp: src_mod_stat_17(_sh[$ "hp"]),
			cost: _cost_arr,
			atk: src_mod_stat_17(_sh[$ "atk"]),
			range: src_mod_stat_17(_sh[$ "range"]),
			cooldown: _cd_arr,
			cycle: src_mod_stat_17(_sh[$ "cycle"]),
		};
		if (variable_struct_exists(_sh, "flame_produce")) {
			_ps[$ "flame_produce"] = src_mod_stat_17(_sh[$ "flame_produce"]);
		}
		array_push(_plant_shapes, _ps);
	}
	if (array_length(_deck_shapes) == 0) return false;

	register_card(_id, obj_card_mod, _deck_shapes);   // 1 卡池
	register_plant_lite(_id, _plant_shapes);          // 2 植物数据/标题/升级数值

	if (variable_struct_exists(_c, "skill")) {
		var _sk = _c[$ "skill"];
		register_card_skill(_id, _sk[$ "attr"], _sk[$ "values"]);  // 3 技能
	}
	if (variable_struct_exists(_c, "shop")) {
		var _sp = _c[$ "shop"];
		register_goods(_id, {                               // 5 商店
			type: "card",
			cost: string(_sp[$ "cost"] ?? 0),
			unlock_item_id: _id,
			description: _sp[$ "description"] ?? "",
			display_name: _c[$ "name"] ?? _id,
		});
	}
	if (variable_struct_exists(_c, "info_island")) {
		register_card_info_island(_id, _c[$ "info_island"]);  // 5 图鉴
	}
	return true;
}

/// @function src_mod_card_vm_load(_buf, _dir)
/// @desc 创建一张 mod 卡自己的虚拟机：函数表沿用全局 VM 注册表，
///       bin 的字符串池读入 vm，各块的字节码存进字典 vm.blocks（块名 → buffer）
/// @param _buf 该卡 bin 文件的 buffer，可为 undefined（只建空 VM）
/// @param _dir 卡所在目录（mod/cards/），执行 _VM_CONST_INIT/_OBJECT_CFG 前先写入
///             mod_dir，供块内加载贴图时作为候选路径
function src_mod_card_vm_load(_buf = undefined, _dir = "") {
	var _vm = VM_Create();
	_vm.functions = global.__vm.functions;
	_vm.func_ret_types = global.__vm.func_ret_types;
	_vm[$ "blocks"] = ds_map_create();   // 二进制逻辑字典：块名 → 字节码 buffer
	_vm[$ "instances"] = ds_list_create();  // 该卡所有实例的容器：创建时加入、消耗时移除
	if (_dir != "") { _vm[$ "mod_dir"] = _dir; }
	if (!buffer_exists(_buf)) return _vm;

	buffer_seek(_buf, buffer_seek_start, 0);
	var _buf_size = buffer_get_size(_buf);

	// 读字符串池
	var _str_count = buffer_read(_buf, buffer_s32);
	for (var _i = 0; _i < _str_count; _i++) {
		var _len = buffer_read(_buf, buffer_s32);
		var _tmp = buffer_create(_len + 1, buffer_fixed, 1);
		for (var _j = 0; _j < _len; _j++) {
			buffer_write(_tmp, buffer_u8, buffer_read(_buf, buffer_u8));
		}
		buffer_write(_tmp, buffer_u8, 0);
		buffer_seek(_tmp, buffer_seek_start, 0);
		array_push(_vm.strings, buffer_read(_tmp, buffer_string));
		buffer_delete(_tmp);
	}
	for (var _i = 0; _i < array_length(_vm.strings); _i++) {
		var _str = _vm.strings[_i];
		if (!ds_map_exists(_vm.str_map, _str)) _vm.str_map[? _str] = _i;
	}

	// 读块数据
	while (buffer_tell(_buf) < _buf_size) {
		var _block_len = buffer_read(_buf, buffer_s32);
		var _name_idx  = buffer_read(_buf, buffer_s32);
		var _block_name = "";
		if (_name_idx >= 0 && _name_idx < array_length(_vm.strings)) {
			_block_name = _vm.strings[_name_idx];
		}
		var _bc_buf = buffer_create(_block_len, buffer_fixed, 1);
		for (var _j = 0; _j < _block_len; _j++) {
			buffer_write(_bc_buf, buffer_u8, buffer_read(_buf, buffer_u8));
		}
		buffer_seek(_bc_buf, buffer_seek_start, 0);
		_vm.blocks[? _block_name] = _bc_buf;
	}
	// _VM_CONST_INIT：编译器把字面量池（所有字符串字面量）初始化放在这个块里，
	// 主 VM 加载时会立即执行；卡 VM 也必须执行，否则块内字符串槽全是 undefined
	if (ds_map_exists(_vm.blocks, "_VM_CONST_INIT")) {
		var _init_buf = _vm.blocks[? "_VM_CONST_INIT"];
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _init_buf, "_VM_CONST_INIT");
		global.__vm = _bak_vm;
		ds_map_delete(_vm.blocks, "_VM_CONST_INIT");
		buffer_delete(_init_buf);
	}
	// _OBJECT_CFG：配置块，紧跟 _VM_CONST_INIT 之后立即执行一次，执行完移除
	if (ds_map_exists(_vm.blocks, "_OBJECT_CFG")) {
		var _cfg_buf = _vm.blocks[? "_OBJECT_CFG"];
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _cfg_buf, "_OBJECT_CFG");
		global.__vm = _bak_vm;
		ds_map_delete(_vm.blocks, "_OBJECT_CFG");
		buffer_delete(_cfg_buf);
	}
	return _vm;
}

/// @function src_mod_card_vm_fill(_vm, _buf, _dir)
/// @desc 把 bin 的字符串池与块字节码灌进已有 VM（先清掉旧的 strings/str_map/blocks），
///       并立即执行 _VM_CONST_INIT；mem 内存/arrays/instances 保持不变，
///       供 reloadmod 热重载用（场上已放置实例的内存状态不丢）
/// @param _dir 卡所在目录（mod/cards/），执行块前先刷新 mod_dir
function src_mod_card_vm_fill(_vm, _buf, _dir = "") {
	if (_dir != "") { _vm[$ "mod_dir"] = _dir; }
	// 清掉旧的字节码与字符串池
	if (ds_exists(_vm[$ "blocks"], ds_type_map)) {
		var _bnames = ds_map_keys_to_array(_vm[$ "blocks"]);
		for (var _b = 0; _b < array_length(_bnames); _b++) {
			var _old_buf = _vm[$ "blocks"][? _bnames[_b]];
			if (buffer_exists(_old_buf)) buffer_delete(_old_buf);
		}
		ds_map_clear(_vm[$ "blocks"]);
	}
	array_resize(_vm.strings, 0);
	ds_map_clear(_vm.str_map);

	if (!buffer_exists(_buf)) return _vm;
	buffer_seek(_buf, buffer_seek_start, 0);
	var _buf_size = buffer_get_size(_buf);

	// 读字符串池
	var _str_count = buffer_read(_buf, buffer_s32);
	for (var _i = 0; _i < _str_count; _i++) {
		var _len = buffer_read(_buf, buffer_s32);
		var _tmp = buffer_create(_len + 1, buffer_fixed, 1);
		for (var _j = 0; _j < _len; _j++) {
			buffer_write(_tmp, buffer_u8, buffer_read(_buf, buffer_u8));
		}
		buffer_write(_tmp, buffer_u8, 0);
		buffer_seek(_tmp, buffer_seek_start, 0);
		array_push(_vm.strings, buffer_read(_tmp, buffer_string));
		buffer_delete(_tmp);
	}
	for (var _i = 0; _i < array_length(_vm.strings); _i++) {
		var _str = _vm.strings[_i];
		if (!ds_map_exists(_vm.str_map, _str)) _vm.str_map[? _str] = _i;
	}

	// 读块数据
	while (buffer_tell(_buf) < _buf_size) {
		var _block_len = buffer_read(_buf, buffer_s32);
		var _name_idx  = buffer_read(_buf, buffer_s32);
		var _block_name = "";
		if (_name_idx >= 0 && _name_idx < array_length(_vm.strings)) {
			_block_name = _vm.strings[_name_idx];
		}
		var _bc_buf = buffer_create(_block_len, buffer_fixed, 1);
		for (var _j = 0; _j < _block_len; _j++) {
			buffer_write(_bc_buf, buffer_u8, buffer_read(_buf, buffer_u8));
		}
		buffer_seek(_bc_buf, buffer_seek_start, 0);
		_vm.blocks[? _block_name] = _bc_buf;
	}

	// 常量初始化
	if (ds_map_exists(_vm.blocks, "_VM_CONST_INIT")) {
		var _init_buf = _vm.blocks[? "_VM_CONST_INIT"];
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _init_buf, "_VM_CONST_INIT");
		global.__vm = _bak_vm;
		ds_map_delete(_vm.blocks, "_VM_CONST_INIT");
		buffer_delete(_init_buf);
	}
	// _OBJECT_CFG：配置块，紧跟 _VM_CONST_INIT 之后立即执行一次，执行完移除
	if (ds_map_exists(_vm.blocks, "_OBJECT_CFG")) {
		var _cfg_buf = _vm.blocks[? "_OBJECT_CFG"];
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _cfg_buf, "_OBJECT_CFG");
		global.__vm = _bak_vm;
		ds_map_delete(_vm.blocks, "_OBJECT_CFG");
		buffer_delete(_cfg_buf);
	}
	return _vm;
}

/// @function src_mod_init(_dir)
/// @desc 扫描 mod/cards/ 目录下的所有 .json 文件，每个文件注册一张卡，
///       文件名（不含 .json）即卡名；同名 .bin 解析为该卡自己的虚拟机
///       （每张卡一个 VM，块字节码存字典），统一放进 global.mod_card_vms。
/// @param _dir 目录路径，默认 working_directory + "mod/cards/"
/// @return 成功注册的卡数量；-1=目录不存在
function src_mod_init(_dir = "") {
	if (_dir == "") { _dir = working_directory + "mod/cards/"; }
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_card_vms")) { global.mod_card_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		// file_find_first 只返回文件名，补全目录
		var _path = _dir + _file;
		if (file_exists(_file)) { _path = _file; }
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			// 同名 .bin：先加载为该卡自己的虚拟机（_OBJECT_CFG 可能加载永久贴图，
			// 必须先执行，下方注册时 get_load_sprite 才能命中永久缓存）
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			global.mod_card_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir);
			global.mod_card_vms[? _id][$ "card_data"] = _json;   // 保留原始 JSON，创建实例时取 plant_type/feature_type
			global.mod_card_vms[? _id][$ "mod_dir"] = _dir;      // 记录 JSON/bin 所在目录，贴图加载时作为候选路径
			if (buffer_exists(_bin_buf)) {
				show_debug_message("src_mod: 加载卡片虚拟机 " + _id + "（块数 " + string(ds_map_size(global.mod_card_vms[? _id].blocks)) + "）");
			}
			if (src_mod_register_card(_id, _json)) {
				show_debug_message("src_mod: 注册卡片 " + _id);
				_count++;
			}
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_reload()
/// @desc 仅重载所有 mod 的 bin 代码：按 mod/cards/ 目录重建每张卡的 VM，
///       不触碰卡池/植物注册表/商店/图鉴，场上已放置的 mod 卡实例保持有效
/// @return 重载了 bin 的卡数量；-1=mod 目录不存在
function src_mod_reload() {
	var _dir = working_directory + "mod/cards/";
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_card_vms")) { global.mod_card_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		if (file_exists(_file)) { _path = _file; }
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;

			var _vm;
			if (ds_map_exists(global.mod_card_vms, _id)) {
				// 已有 VM：把新 bin 灌进同一个 VM（保留 mem/instances，场上实例状态不丢）
				_vm = src_mod_card_vm_fill(global.mod_card_vms[? _id], _bin_buf, _dir);
			} else {
				_vm = src_mod_card_vm_load(_bin_buf, _dir);
			}
			_vm[$ "card_data"] = _json;
			_vm[$ "mod_dir"] = _dir;
			if (buffer_exists(_bin_buf)) {
				show_debug_message("src_mod: 重载卡片虚拟机 " + _id + "（块数 " + string(ds_map_size(_vm[$ "blocks"])) + "）");
			}
			_count++;
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}
