/// @function save_file(file_slot)
/// @desc 将全局变量global.save_data保存到存档文件中
/// @param {real} file_slot 存档槽位
function save_file(file_slot) {
	var file_path = src_mod_save_path("save" + string(file_slot) + ".json")

	// 未注册的卡（load_file 摘出来放在内存的 global.save_orphan_cards）写盘时并回卡表，
	// 这样存档结构和以前完全一样；写完再把内存里的卡表恢复成"不含未注册卡"的状态，
	// 免得它们在本局游戏里又被界面画成空框幽灵。
	var _orphan = variable_global_exists("save_orphan_cards") ? global.save_orphan_cards : undefined;
	var _live   = undefined;
	if (is_array(_orphan) && array_length(_orphan) > 0 && is_array(global.save_data.unlocked_cards)) {
		_live = global.save_data.unlocked_cards;
		var _merged = [];
		for (var _i = 0; _i < array_length(_live);   _i++) array_push(_merged, _live[_i]);
		for (var _j = 0; _j < array_length(_orphan); _j++) array_push(_merged, _orphan[_j]);
		global.save_data.unlocked_cards = _merged;
	}

    // 未注册的宝石同理：宝石表 + 装备栏 gems 写盘时并回，写完恢复内存状态
	var _og  = variable_global_exists("save_orphan_gems") ? global.save_orphan_gems : undefined;
	var _oeg = variable_global_exists("save_orphan_equipped_gems") ? global.save_orphan_equipped_gems : undefined;
	var _liveg  = undefined;   // 宝石表（干净版）
	var _liveeg = {};          // 槽位 → gems（干净版）
	var _slots  = ["main_weapon", "secondary_weapon", "super_weapon"];
	if (is_array(_og) && array_length(_og) > 0 && is_array(global.save_data.unlocked_gems)) {
		_liveg = global.save_data.unlocked_gems;
		var _mg = [];
		for (var _i = 0; _i < array_length(_liveg); _i++) array_push(_mg, _liveg[_i]);
		for (var _j = 0; _j < array_length(_og);    _j++) array_push(_mg, _og[_j]);
		global.save_data.unlocked_gems = _mg;
	}
	if (is_struct(_oeg) && is_struct(global.save_data.equipped_items)) {
		for (var _s = 0; _s < array_length(_slots); _s++) {
			var _slot = _slots[_s];
			if (!variable_struct_exists(_oeg, _slot)) continue;
			var _w = global.save_data.equipped_items[$ _slot];
			if (!is_struct(_w)) continue;
			var _gl = _w[$ "gems"];
			if (!is_array(_gl)) continue;
			_liveeg[$ _slot] = _gl;
			var _mg2 = [];
			for (var _i = 0; _i < array_length(_gl); _i++) array_push(_mg2, _gl[_i]);
			var _add = _oeg[$ _slot];
			for (var _j = 0; _j < array_length(_add); _j++) array_push(_mg2, _add[_j]);
			_w[$ "gems"] = _mg2;
		}
	}

	// 未注册的武器同理：武器表 + 装备栏里未注册的武器 id，写盘时都并回【武器表（仓库）】，
	// 不写回装备槽；写完恢复内存状态
	var _ow  = variable_global_exists("save_orphan_weapons") ? global.save_orphan_weapons : undefined;
	var _oew = variable_global_exists("save_orphan_equipped_weapons") ? global.save_orphan_equipped_weapons : undefined;
	var _livew  = undefined;   // 武器表（干净版）
	var _oew_ids = [];         // 装备栏摘下来的孤儿武器 id，写盘时并进武器表
	if (is_struct(_oew)) {
		for (var _s = 0; _s < array_length(_slots); _s++) {
			var _slot = _slots[_s];
			if (variable_struct_exists(_oew, _slot)) array_push(_oew_ids, _oew[$ _slot]);
		}
	}
	if ((is_array(_ow) && array_length(_ow) > 0 || array_length(_oew_ids) > 0)
	 && is_array(global.save_data.unlocked_weapons)) {
		_livew = global.save_data.unlocked_weapons;
		var _mw = [];
		for (var _i = 0; _i < array_length(_livew); _i++) array_push(_mw, _livew[_i]);
		if (is_array(_ow)) {
			for (var _j = 0; _j < array_length(_ow); _j++) array_push(_mw, _ow[_j]);
		}
		for (var _k = 0; _k < array_length(_oew_ids); _k++) {
			var _oid = _oew_ids[_k];
			var _dup = false;
			for (var _q = 0; _q < array_length(_mw); _q++) {
				if (_mw[_q].id == _oid) { _dup = true; break; }
			}
			if (!_dup) array_push(_mw, { id: _oid });
		}
		global.save_data.unlocked_weapons = _mw;
	}

	// 未注册的时装同理：时装表写盘时并回，写完恢复内存状态
	var _oa = variable_global_exists("save_orphan_attires") ? global.save_orphan_attires : undefined;
	var _livea = undefined;
	if (is_array(_oa) && array_length(_oa) > 0 && is_array(global.save_data.attires)) {
		_livea = global.save_data.attires;
		var _ma = [];
		for (var _i = 0; _i < array_length(_livea); _i++) array_push(_ma, _livea[_i]);
		for (var _j = 0; _j < array_length(_oa);    _j++) array_push(_ma, _oa[_j]);
		global.save_data.attires = _ma;
	}

    // 将数据转换为JSON字符串
    var json_string = json_stringify(global.save_data);

	// 恢复内存状态
	if (!is_undefined(_live)) global.save_data.unlocked_cards = _live;
	if (!is_undefined(_liveg)) global.save_data.unlocked_gems = _liveg;
	if (!is_undefined(_livew)) global.save_data.unlocked_weapons = _livew;
	if (!is_undefined(_livea)) global.save_data.attires = _livea;
	if (is_struct(global.save_data.equipped_items)) {
		for (var _s = 0; _s < array_length(_slots); _s++) {
			var _slot = _slots[_s];
			if (!variable_struct_exists(_liveeg, _slot)) continue;
			var _w = global.save_data.equipped_items[$ _slot];
			if (is_struct(_w)) _w[$ "gems"] = _liveeg[$ _slot];
		}
	}
    
    // 打开文件进行写入
    var file = file_text_open_write(file_path);
    if (file == -1) {
		show_notice("存档保存失败，请重试",60)
        show_debug_message("无法创建存档文件!");
        return false;
    }
    
    // 写入数据
    file_text_write_string(file, json_string);
    file_text_close(file);
    
    show_debug_message("存档保存成功!");
    return true;
}