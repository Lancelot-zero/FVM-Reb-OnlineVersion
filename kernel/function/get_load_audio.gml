function get_load_audio(_name) {
	var _audio = global._audio_cache[? _name];
	if (!is_undefined(_audio)) {
		return _audio;
	}
	var _result = -1;
	// ① 先查 .yyp 内置资源（白名单保留的）
	var _idx = asset_get_index(_name);
	if (_idx != -1 && audio_exists(_idx)) {
		_result = _idx;
	} else {
		// ② 从 backgroundmusic 目录加载 OGG（优先 _project_root，回退相对路径）
		var _rel = "backgroundmusic/" + _name + ".ogg";
		var _ogg_path = _rel;
		if (!is_undefined(global._project_root)) {
			var _abs = global._project_root + _rel;
			if (file_exists(_abs)) {
				_ogg_path = _abs;
			}
		}
		if (file_exists(_ogg_path)) {
			_result = audio_create_stream(_ogg_path);
			show_debug_message("[audio] loaded: " + _name + ".ogg");
		}
	}
	// ③ 读不到 → 缺省音乐（防递归）
	if (_result == -1) {
		var _default = "mus_menu";
		if (_name != _default) {
			show_debug_message("[audio] not found: " + _name + " → fallback " + _default);
			_result = get_load_audio(_default);
		} else {
			show_debug_message("[audio] default also not found: " + _name);
		}
	}
	if (_result != -1) {
		global._audio_cache[? _name] = _result;
		global._audio_reverse[? _result] = _name;
	}
	return _result;
}
