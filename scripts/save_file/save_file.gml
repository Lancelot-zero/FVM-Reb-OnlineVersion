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

    // 将数据转换为JSON字符串
    var json_string = json_stringify(global.save_data);

	// 恢复内存状态
	if (!is_undefined(_live)) global.save_data.unlocked_cards = _live;
    
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