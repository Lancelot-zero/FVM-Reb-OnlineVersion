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

/// @function src_mod_bullets_init(_dir)
/// @desc 扫描 mod/bullets/ 下的 json，只加载 VM，不注册进任何游戏卡池/敌人池。
///       空 json 也能用来给 obj_bullet_mod 提供同名 .bin 逻辑。
function src_mod_bullets_init(_dir = "") {
  if (_dir == "") { _dir = working_directory + "mod/bullets/"; }
  if (!directory_exists(_dir)) {
    show_debug_message("src_mod: 目录不存在 " + _dir);
    return -1;
  }
  if (!variable_global_exists("mod_bullet_vms")) { global.mod_bullet_vms = ds_map_create(); }

  var _count = 0;
  var _file = file_find_first(_dir + "*.json", 0);
  while (_file != "") {
    var _path = _dir + _file;
    var _json = json_parse(src_mod_read_text(_path));
    if (!is_struct(_json)) {
      show_debug_message("src_mod: 子弹 JSON 解析失败 " + _path);
    } else {
      var _name = filename_name(_file);
      var _id = string_copy(_name, 1, string_length(_name) - 5);
      var _bin_path = string_replace(_path, ".json", ".bin");
      var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
      global.mod_bullet_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
      global.mod_bullet_vms[? _id][$ "card_data"] = _json;
      global.mod_bullet_vms[? _id][$ "mod_dir"] = _dir;
      _count++;
    }
    _file = file_find_next();
  }
  file_find_close();
  return _count;
}

/// @function src_mod_effects_init(_dir)
/// @desc 扫描 mod/effects/ 下的 json，只加载 VM，不注册进任何游戏卡池/敌人池。
function src_mod_effects_init(_dir = "") {
  if (_dir == "") { _dir = working_directory + "mod/effects/"; }
  if (!directory_exists(_dir)) {
    show_debug_message("src_mod: 目录不存在 " + _dir);
    return -1;
  }
  if (!variable_global_exists("mod_effect_vms")) { global.mod_effect_vms = ds_map_create(); }

  var _count = 0;
  var _file = file_find_first(_dir + "*.json", 0);
  while (_file != "") {
    var _path = _dir + _file;
    var _json = json_parse(src_mod_read_text(_path));
    if (!is_struct(_json)) {
      show_debug_message("src_mod: 特效 JSON 解析失败 " + _path);
    } else {
      var _name = filename_name(_file);
      var _id = string_copy(_name, 1, string_length(_name) - 5);
      var _bin_path = string_replace(_path, ".json", ".bin");
      var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
      global.mod_effect_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
      global.mod_effect_vms[? _id][$ "card_data"] = _json;
      global.mod_effect_vms[? _id][$ "mod_dir"] = _dir;
      _count++;
    }
    _file = file_find_next();
  }
  file_find_close();
  return _count;
}

/// @function src_mod_bullets_reload()
function src_mod_bullets_reload() {
  var _dir = working_directory + "mod/bullets/";
  if (!directory_exists(_dir)) {
    show_debug_message("src_mod: 目录不存在 " + _dir);
    return -1;
  }
  if (!variable_global_exists("mod_bullet_vms")) { global.mod_bullet_vms = ds_map_create(); }

  var _count = 0;
  var _file = file_find_first(_dir + "*.json", 0);
  while (_file != "") {
    var _path = _dir + _file;
    var _json = json_parse(src_mod_read_text(_path));
    if (!is_struct(_json)) {
      show_debug_message("src_mod: 子弹 JSON 解析失败 " + _path);
    } else {
      var _name = filename_name(_file);
      var _id = string_copy(_name, 1, string_length(_name) - 5);
      var _bin_path = string_replace(_path, ".json", ".bin");
      var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
      var _vm;
      if (ds_map_exists(global.mod_bullet_vms, _id)) {
        _vm = src_mod_card_vm_fill(global.mod_bullet_vms[? _id], _bin_buf, _dir);
      } else {
        _vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
        global.mod_bullet_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
      }
      _vm[$ "card_data"] = _json;
      _vm[$ "mod_dir"] = _dir;
      _count++;
    }
    _file = file_find_next();
  }
  file_find_close();
  return _count;
}

/// @function src_mod_effects_reload()
function src_mod_effects_reload() {
  var _dir = working_directory + "mod/effects/";
  if (!directory_exists(_dir)) {
    show_debug_message("src_mod: 目录不存在 " + _dir);
    return -1;
  }
  if (!variable_global_exists("mod_effect_vms")) { global.mod_effect_vms = ds_map_create(); }

  var _count = 0;
  var _file = file_find_first(_dir + "*.json", 0);
  while (_file != "") {
    var _path = _dir + _file;
    var _json = json_parse(src_mod_read_text(_path));
    if (!is_struct(_json)) {
      show_debug_message("src_mod: 特效 JSON 解析失败 " + _path);
    } else {
      var _name = filename_name(_file);
      var _id = string_copy(_name, 1, string_length(_name) - 5);
      var _bin_path = string_replace(_path, ".json", ".bin");
      var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
      var _vm;
      if (ds_map_exists(global.mod_effect_vms, _id)) {
        _vm = src_mod_card_vm_fill(global.mod_effect_vms[? _id], _bin_buf, _dir);
      } else {
        _vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
        global.mod_effect_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
      }
      _vm[$ "card_data"] = _json;
      _vm[$ "mod_dir"] = _dir;
      _count++;
    }
    _file = file_find_next();
  }
  file_find_close();
  return _count;
}
/// @function src_mod_default_map_icon()
/// @desc 默认 mod 大地图入口按钮图标：mod/maps/____shared____/default_map_icon.png
function src_mod_default_map_icon() {
  if (!variable_global_exists("mod_default_map_icon")) {
    var _path = working_directory + "mod/maps/____shared____/default_map_icon.png";
    global.mod_default_map_icon = file_exists(_path) ? sprite_add(_path, 1, false, false, 0, 0) : -1;
  }
  return global.mod_default_map_icon;
}

/// @function src_mod_default_map_bg()
/// @desc 默认 mod 大地图背景：mod/maps/____shared____/default_map_bg.png
function src_mod_default_map_bg() {
  if (!variable_global_exists("mod_default_map_bg")) {
    var _path = working_directory + "mod/maps/____shared____/default_map_bg.png";
    global.mod_default_map_bg = file_exists(_path) ? sprite_add(_path, 1, false, false, 0, 0) : -1;
  }
  return global.mod_default_map_bg;
}
/// @function src_mod_maps_init()
/// @desc 初始化 mod 大地图注册表。大地图/小地图都通过代码手动注册，不扫描。
function src_mod_maps_init() {
  if (!variable_global_exists("mod_maps")) global.mod_maps = ds_map_create();
  if (!variable_global_exists("mod_map_order")) global.mod_map_order = [];
}

/// @function src_mod_map_register_big(map_id, data)
/// @desc 注册一个 mod 大地图。
/// data: { name:"显示名", sprite: 大地图贴图, levels: [] }
function src_mod_map_register_big(_map_id, _data) {
  if (!is_string(_map_id) || _map_id == "") return false;
  src_mod_maps_init();
  global.mod_maps[? _map_id] = {
    id: _map_id,
    name: _data[$ "name"] ?? _map_id,
    sprite: _data[$ "sprite"] ?? -1,
    icon: _data[$ "icon"] ?? -1,
    share_src: _data[$ "share_src"] ?? "",
    levels: []
  };
  array_push(global.mod_map_order, _map_id);
  return true;
}

/// @function src_mod_map_register_small(map_id, data)
/// @desc 给 mod 大地图手动添加一个小地图。
/// data: {
///   name: "显示名",
///   sprite: 小地图预览贴图,
///   button_x: 按钮 X,
///   button_y: 按钮 Y,
///   stage_src: "mod/maps/xxx/",   // 源目录
///   stage_file: "level.json"      // 源 JSON 文件名
/// }
function src_mod_map_register_small(_map_id, _data) {
  src_mod_maps_init();
  if (!ds_map_exists(global.mod_maps, _map_id)) return false;
  var _big = global.mod_maps[? _map_id];
  array_push(_big[$ "levels"], _data);
  return true;
}

/// @function src_mod_map_clear_temp()
/// @desc 清空 laboratory/____mod_map____/ 临时目录。启动和打开实验室时调用。
function src_mod_map_clear_temp() {
  var _dir = "laboratory/____mod_map____";
  if (directory_exists(_dir)) {
    directory_destroy(_dir);
  }
}

/// @function src_mod_map_copy_folder(src, dst)
/// @desc 把 mod 地图源目录里的文件复制到实验室临时目录。当前实现复制一层文件，子目录暂不递归。
function src_mod_map_copy_folder(_src, _dst) {
  if (!string_ends_with(_src, "/") && !string_ends_with(_src, "\\")) _src += "/";
  if (!string_ends_with(_dst, "/") && !string_ends_with(_dst, "\\")) _dst += "/";
  if (!directory_exists(_dst)) directory_create(_dst);

  // 复制当前层文件
  var _file = file_find_first(_src + "*.*", fa_none);
  while (_file != "") {
    var _sp = _src + _file;
    var _dp = _dst + _file;
    if (file_exists(_sp)) {
      if (file_exists(_dp)) file_delete(_dp);
      file_copy(_sp, _dp);
    }
    _file = file_find_next();
  }
  file_find_close();

  // 递归复制子目录
  var _dir = file_find_first(_src + "*", fa_directory);
  while (_dir != "") {
    if (_dir != "." && _dir != "..") {
      var _src_sub = _src + _dir;
      var _dst_sub = _dst + _dir;
      if (directory_exists(_src_sub)) {
        if (!directory_exists(_dst_sub)) directory_create(_dst_sub);
        src_mod_map_copy_folder(_src_sub + "/", _dst_sub + "/");
      }
    }
    _dir = file_find_next();
  }
  file_find_close();
}
/// @function src_mod_map_copy_shared(dst)
/// @desc 把 mod/maps/____shared____/ 里的内容复制到目标临时目录。
///       mod 地图可以共用这里的贴图、音乐、脚本等资源。
function src_mod_map_copy_shared(_dst) {
  var _shared = working_directory + "mod/maps/____shared____/";
  if (directory_exists(_shared)) {
    src_mod_map_copy_folder(_shared, _dst);
  }
}

/// @function src_mod_map_enter(map_id, level_index)
/// @desc 点击 mod 小地图后：复制到 laboratory/____mod_map____/，构造 CustomStage，
///       然后完全交给实验室 StageDetail.on_create_room 处理（联机同步也自动沿用实验室）。
function src_mod_map_enter(_map_id, _level_index) {
  src_mod_maps_init();
  if (!ds_map_exists(global.mod_maps, _map_id)) return false;

  var _big = global.mod_maps[? _map_id];
  var _levels = _big[$ "levels"];
  if (_level_index < 0 || _level_index >= array_length(_levels)) return false;

  var _level = _levels[_level_index];
  var _src = _level[$ "stage_src"];
  var _file = _level[$ "stage_file"];
  if (!is_string(_src) || !is_string(_file) || _file == "") return false;

  // 1. 复制资源到实验室临时目录
  //    顺序：全局共享 → 大地图 share → 当前小地图（小地图文件最后复制，优先覆盖共享资源）
  src_mod_map_clear_temp();
  var _temp_dir = "laboratory/____mod_map____/" + _map_id + "/";

  src_mod_map_copy_shared(_temp_dir);

  var _map_share = _big[$ "share_src"] ?? "";
  if (is_string(_map_share) && _map_share != "" && directory_exists(_map_share)) {
    src_mod_map_copy_folder(_map_share, _temp_dir);
  }

  src_mod_map_copy_folder(_src, _temp_dir);

  var _json_path = _temp_dir + _file;
  var _parse_result = global.laboratory_manager.file_util.load_json_from_path(_json_path);
  if (_parse_result.is_failed()) {
    show_debug_message("mod 地图 JSON 加载失败: " + _json_path);
    return false;
  }

  // 2. 构造实验室 CustomStage
  var _stage_result = create_custom_stage(_parse_result.data, _json_path);
  if (_stage_result.is_failed()) {
    show_debug_message(_stage_result.get_error_stack());
    return false;
  }

  var _stage = _stage_result.data;
  var _verify_result = verify_stage(_stage);
  if (_verify_result.is_failed()) {
    show_debug_message(_verify_result.get_error_stack());
    return false;
  }

  // 3. 完全走实验室房间创建流程，联机同步也沿用实验室逻辑
  // 当前房间可能没有 Float 层（正常关卡界面没有），先补一个，StageDetail 内部按钮会用
  if (layer_get_id("Float") == -1) {
    layer_create(-1000, "Float");
  }

  var _detail = instance_create_layer(0, 0, "Float", StageDetail);
  if (_detail == -1) {
    show_debug_message("创建 StageDetail 失败");
    return false;
  }
  _detail.init(_stage);
  _detail.on_create_room();
  return true;
}
/// @function src_mod_image_path(path)
/// @desc 让 mod JSON 里的 PNG 路径更宽松：icon.png 会自动转成 ./icon.png。
function src_mod_image_path(_path) {
  if (!is_string(_path) || _path == "") return _path;
  var _p = string_replace_all(_path, "\\", "/");
  if (string_ends_with(_p, ".png") && !laboratory_path_is_relative(_p)) {
    _p = "./" + _p;
  }
  return _p;
}
/// @function src_mod_maps_load_dir(dir)
/// @desc 扫描 mod/maps/ 下每个子目录的 world.json，并注册成 mod 大地图/小地图。
///       world.json 示例：
///       {
///         "id": "example_island",
///         "name": "示例大地图",
///         "sprite": "spr_delicious_islands",
///         "levels": [
///           {
///             "name": "示例关卡",
///             "sprite": "spr_cookie_island",
///             "button_x": 400,
///             "button_y": 400,
///             "stage_dir": "",
///             "stage_file": "level.json"
///           }
///         ]
///       }
function src_mod_maps_load_dir(_dir = "") {
  if (_dir == "") { _dir = working_directory + "mod/maps/"; }
  if (!string_ends_with(_dir, "/") && !string_ends_with(_dir, "\\")) _dir += "/";
  if (!directory_exists(_dir)) return -1;

  src_mod_maps_init();
  var _count = 0;
  var _sub = file_find_first(_dir + "*", fa_directory);
  while (_sub != "") {
    if (_sub != "." && _sub != "..") {
      var _subdir = _dir + _sub;
      var _world = _subdir + "/world.json";
      if (file_exists(_world)) {
        var _json = json_parse(src_mod_read_text(_world));
        if (!is_struct(_json)) {
          show_debug_message("src_mod: mod 地图 world.json 解析失败: " + _world);
        } else {
          var _map_id = _json[$ "id"] ?? _sub;

          var _share_rel = _json[$ "share"] ?? "";
          var _share_src = "";
          if (is_string(_share_rel) && _share_rel != "") {
            _share_src = _subdir + "/" + _share_rel;
            if (!string_ends_with(_share_src, "/") && !string_ends_with(_share_src, "\\")) _share_src += "/";
          }

          var _big_spr = -1;
          var _big_spr_res = get_sprite_or_create(_json[$ "sprite"], _world);
          if (_big_spr_res.is_succeed()) _big_spr = _big_spr_res.data;

          var _big_icon = -1;
          var _big_icon_res = get_sprite_or_create(src_mod_image_path(_json[$ "icon"]), _world);
          if (_big_icon_res.is_succeed()) _big_icon = _big_icon_res.data;

          src_mod_map_register_big(_map_id, {
            name: _json[$ "name"] ?? _map_id,
            sprite: _big_spr,
            icon: _big_icon,
            share_src: _share_src
          });

          var _levels = _json[$ "levels"] ?? [];
          for (var _i = 0; _i < array_length(_levels); _i++) {
            var _lv = _levels[_i];
            if (!is_struct(_lv)) continue;

            var _spr = -1;
            var _spr_res = get_sprite_or_create(_lv[$ "sprite"], _world);
            if (_spr_res.is_succeed()) _spr = _spr_res.data;

            var _icon = -1;
            var _icon_res = get_sprite_or_create(src_mod_image_path(_lv[$ "icon"]), _world);
            if (_icon_res.is_succeed()) _icon = _icon_res.data;

            // 优先支持单个相对路径：stage_json: "level_1/level.json"
            // 同时兼容旧的 stage_dir + stage_file 写法
            var _stage_src = _subdir + "/";
            var _stage_file = _lv[$ "stage_file"] ?? "level.json";
            var _stage_json = _lv[$ "stage_json"] ?? "";
            if (_stage_json != "") {
              var _stage_rel = string_replace_all(_stage_json, "\\", "/");
              var _last_slash = 0;
              for (var _j = string_length(_stage_rel); _j > 0; _j--) {
                if (string_char_at(_stage_rel, _j) == "/") { _last_slash = _j; break; }
              }
              if (_last_slash > 0) {
                _stage_src = _subdir + "/" + string_copy(_stage_rel, 1, _last_slash);
                _stage_file = string_copy(_stage_rel, _last_slash + 1, string_length(_stage_rel) - _last_slash);
              } else {
                _stage_file = _stage_rel;
              }
            } else {
              var _stage_dir = _lv[$ "stage_dir"] ?? "";
              if (_stage_dir != "" && !string_ends_with(_stage_dir, "/") && !string_ends_with(_stage_dir, "\\")) {
                _stage_dir += "/";
              }
              _stage_src = _subdir + "/" + _stage_dir;
            }

            src_mod_map_register_small(_map_id, {
              name: _lv[$ "name"] ?? ("关卡 " + string(_i + 1)),
              sprite: _spr,
              icon: _icon,
              button_x: _lv[$ "button_x"] ?? 0,
              button_y: _lv[$ "button_y"] ?? 0,
              stage_src: _stage_src,
              stage_file: _stage_file
            });
          }
          _count++;
        }
      }
    }
    _sub = file_find_next();
  }
  file_find_close();
  return _count;
}
/// @function src_mod_check_json(_kind, _id, _json)
/// @desc 注册前体检 mod 的 json：缺字段 / 类型不对的补安全默认值，并把该文件的**所有问题一次性**
///       报到 shell 控制台 + GameMaker 输出 + mod/mod_errors.log。
///       目的：json 写漏字段时"这张卡不生效，但不会崩游戏、也不会静默没有任何提示"。
function src_mod_check_json(_kind, _id, _json) {
	if (!is_struct(_json)) return false;
	var _bad = "";

	// ── 通用字段 ──
	if (!variable_struct_exists(_json, "name") || !is_string(_json[$ "name"]) || _json[$ "name"] == "") {
		_json[$ "name"] = _id; _bad += "name(补 id) ";
	}
	if (!variable_struct_exists(_json, "description") && !variable_struct_exists(_json, "shop")) {
		_bad += "description(缺,可省) ";
	}

	switch (_kind) {
		case "card": {
			var _shapes = _json[$ "shapes"];
			if (!is_array(_shapes)) {
				_json[$ "shapes"] = [];
				_bad += "shapes(缺/不是数组 → 补空表, 这张卡不会注册) ";
			} else {
				for (var _i = 0; _i < array_length(_shapes); _i++) {
					var _s = _shapes[_i];
					if (!is_struct(_s)) { _bad += "shapes[" + string(_i) + "](不是结构体, 跳过) "; continue; }
					var _t = "shapes[" + string(_i) + "]";
					if (!is_real(_s[$ "shape"]))  { _s[$ "shape"] = _i;                       _bad += _t + ".shape(补 " + string(_i) + ") "; }
					if (!is_string(_s[$ "sprite"]) || _s[$ "sprite"] == "") {
						_s[$ "sprite"] = "spr_brazier";                                     _bad += _t + ".sprite(补 spr_brazier) ";
					}
					if (!is_real(_s[$ "hp"]))       _bad += _t + ".hp(缺→按 0) ";
					if (!is_real(_s[$ "atk"]))      _bad += _t + ".atk(缺→按 0) ";
					if (!is_real(_s[$ "cost"]))     _bad += _t + ".cost(缺→按 0) ";
					if (!is_real(_s[$ "cooldown"])) _bad += _t + ".cooldown(缺→按 0) ";
					if (!is_real(_s[$ "range"]))    _bad += _t + ".range(缺→按 0) ";
					if (!is_real(_s[$ "cycle"]))    _bad += _t + ".cycle(缺→按 0) ";
					if (!is_string(_s[$ "plant_type"]))   { _s[$ "plant_type"] = "normal";   _bad += _t + ".plant_type(补 normal) "; }
					if (!is_string(_s[$ "feature_type"])) { _s[$ "feature_type"] = "normal"; _bad += _t + ".feature_type(补 normal) "; }
					if (!is_string(_s[$ "target_card"]))  { _s[$ "target_card"] = "none";    _bad += _t + ".target_card(补 none) "; }
				}
			}
			// 卡必须有 skill 结构，否则 get_plant_data_with_skill 里 [1][等级] 会炸
			if (!variable_struct_exists(_json, "skill") || !is_struct(_json[$ "skill"])) {
				_json[$ "skill"] = { attr: "cycle", values: [60] };
				_bad += "skill(缺 → 补 cycle[60]) ";
			}
			break;
		}
		case "weapon": {
			if (!is_string(_json[$ "sprite"]) || _json[$ "sprite"] == "") { _json[$ "sprite"] = "spr_brazier"; _bad += "sprite(补 spr_brazier) "; }
			if (!is_string(_json[$ "icon"])   || _json[$ "icon"]   == "") { _json[$ "icon"] = _json[$ "sprite"]; _bad += "icon(补 sprite) "; }
			if (!is_real(_json[$ "atk"]))    _bad += "atk(缺→按 0) ";
			if (!is_real(_json[$ "cycle"]))  _bad += "cycle(缺→按 0) ";
			if (!is_string(_json[$ "slot"])) _bad += "slot(缺→用默认槽) ";
			break;
		}
		case "gem": {
			if (!is_string(_json[$ "icon"]) || _json[$ "icon"] == "") _bad += "icon(缺→无图标, 不崩) ";
			if (!is_string(_json[$ "slot"])) _bad += "slot(缺→用默认槽) ";
			break;
		}
		case "enemy": {
			if (!is_string(_json[$ "spr"]) || _json[$ "spr"] == "") { _json[$ "spr"] = "spr_brazier"; _bad += "spr(补 spr_brazier) "; }
			if (!is_real(_json[$ "hp"])) _bad += "hp(缺→按 0) ";
			break;
		}
	}

	if (_bad != "") {
		var _msg = "[mod 校验] " + _kind + " / " + string(_id) + ".json → " + _bad;
		shell_print(_msg);
		show_debug_message(_msg);
		mod_error_log(_msg);
	}
	return true;
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
	src_mod_check_json("card", _id, _c);
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

/// @function src_mod_card_vm_load(_buf, _dir, _id)
/// @desc 创建一张 mod 卡自己的虚拟机：函数表沿用全局 VM 注册表，
///       bin 的字符串池读入 vm，各块的字节码存进字典 vm.blocks（块名 → buffer）
/// @param _buf 该卡 bin 文件的 buffer，可为 undefined（只建空 VM）
/// @param _dir 卡所在目录（mod/cards/），执行 _VM_CONST_INIT/_OBJECT_CFG 前先写入
///             mod_dir，供块内加载贴图时作为候选路径
function src_mod_card_vm_load(_buf = undefined, _dir = "", _id = "") {
	var _vm = VM_Create();
	_vm.functions = global.__vm.functions;
	_vm.func_ret_types = global.__vm.func_ret_types;
	_vm[$ "blocks"] = ds_map_create();   // 二进制逻辑字典：块名 → 字节码 buffer
	_vm[$ "instances"] = ds_list_create();  // 该卡所有实例的容器：创建时加入、消耗时移除
	if (_dir != "") { _vm[$ "mod_dir"] = _dir; }
	// 侧挂行号表路径（编译器多写的 <同名>.lines）：报错定位用，平时不读
	if (_dir != "" && _id != "") { _vm[$ "src_lines"] = _dir + _id + ".lines"; }
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

	// 侧挂行号表 <同名>.lines（编译器多写的）：直接读进来，报错时把"字节偏移"翻成源码行号。
	// 表很小（一张卡几 KB）；没有这个文件就什么都不做，报错只显示"块名+位置"。
	// 格式：  B <块名字符串下标> <条数>   然后每条   <字节偏移> <源行号>
	if (variable_struct_exists(_vm, "src_lines") && file_exists(_vm[$ "src_lines"])) {
		_vm[$ "line_tab"] = ds_map_create();   // 块名 → ds_list（偶数位=字节偏移、奇数位=行号）
		var _lr = file_text_open_read(_vm[$ "src_lines"]);
		if (_lr != -1) {
			var _cur_name = "";
			var _cur_list = -1;
			while (!file_text_eof(_lr)) {
				var _ln = string_trim(file_text_read_string(_lr));
				file_text_readln(_lr);
				if (_ln == "") continue;
				if (string_char_at(_ln, 1) == "B") {
					if (_cur_list != -1) ds_map_set(_vm.line_tab, _cur_name, _cur_list);
					var _sp  = string_pos(" ", _ln);
					var _idx = real(string_copy(_ln, _sp + 1, string_pos(" ", string_delete(_ln, 1, _sp)) - 1));
					_cur_name = (_idx >= 0 && _idx < array_length(_vm.strings)) ? _vm.strings[_idx] : "";
					_cur_list = ds_list_create();
				} else if (_cur_list != -1) {
					var _sp2 = string_pos(" ", _ln);
					ds_list_add(_cur_list, real(string_copy(_ln, 1, _sp2 - 1)));
					ds_list_add(_cur_list, real(string_delete(_ln, 1, _sp2)));
				}
			}
			if (_cur_list != -1) ds_map_set(_vm.line_tab, _cur_name, _cur_list);
			file_text_close(_lr);
		}
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
		if (_block_name == "__LINES__") {
			// 行号表（编译器打包进来的伪块，二进制）：u32 段数；
			// 每段 u32 块名下标 + u32 条数；每条 u32 块内字节偏移 + u32 源行号。
			// 它不是可执行块，不进 vm.blocks，只解析成 line_tab（块名 → ds_list，偶数位=偏移、奇数位=行号）
			if (!variable_struct_exists(_vm, "line_tab")) _vm[$ "line_tab"] = ds_map_create();
			var _nsec = buffer_read(_bc_buf, buffer_u32);
			for (var _s = 0; _s < _nsec; _s++) {
				var _bidx = buffer_read(_bc_buf, buffer_u32);
				var _bcnt = buffer_read(_bc_buf, buffer_u32);
				var _bnam = (_bidx >= 0 && _bidx < array_length(_vm.strings)) ? _vm.strings[_bidx] : "";
				var _blst = ds_list_create();
				for (var _e = 0; _e < _bcnt; _e++) {
					ds_list_add(_blst, buffer_read(_bc_buf, buffer_u32));
					ds_list_add(_blst, buffer_read(_bc_buf, buffer_u32));
				}
				ds_map_set(_vm.line_tab, _bnam, _blst);
			}
			show_debug_message("[VM] 行号表载入: " + string(_nsec) + " 段 / " + string(_block_len) + " 字节");
			buffer_delete(_bc_buf);
		} else {
			_vm.blocks[? _block_name] = _bc_buf;
		}
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

	// ⚠️ 解码缓存必须跟着一起清掉，否则 reloadmod 等于没 reload：
	//    VM_Execute 把「块名 → VM_Decode 出来的值数组」缓存在 vm.codes（惰性解码、一块只解一次），
	//    而 fill 是**复用同一个 VM 结构体**换 bin 的，codes 里还是上一版 bin 解出来的指令数组，
	//    换完 blocks 后执行时缓存照命中 → 跑的还是旧代码；
	//    而且旧代码里的字符串池下标是按旧 strings 编的，不清就是新旧混用。
	//    （值数组本身是 GML 数组，map 清掉后没有引用，交给 GC 回收。）
	if (variable_struct_exists(_vm, "codes") && ds_exists(_vm[$ "codes"], ds_type_map)) {
		ds_map_clear(_vm[$ "codes"]);
	}

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
		if (_block_name == "__LINES__") {
			// 行号表（编译器打包进来的伪块，二进制）：u32 段数；
			// 每段 u32 块名下标 + u32 条数；每条 u32 块内字节偏移 + u32 源行号。
			// 它不是可执行块，不进 vm.blocks，只解析成 line_tab（块名 → ds_list，偶数位=偏移、奇数位=行号）
			if (!variable_struct_exists(_vm, "line_tab")) _vm[$ "line_tab"] = ds_map_create();
			var _nsec = buffer_read(_bc_buf, buffer_u32);
			for (var _s = 0; _s < _nsec; _s++) {
				var _bidx = buffer_read(_bc_buf, buffer_u32);
				var _bcnt = buffer_read(_bc_buf, buffer_u32);
				var _bnam = (_bidx >= 0 && _bidx < array_length(_vm.strings)) ? _vm.strings[_bidx] : "";
				var _blst = ds_list_create();
				for (var _e = 0; _e < _bcnt; _e++) {
					ds_list_add(_blst, buffer_read(_bc_buf, buffer_u32));
					ds_list_add(_blst, buffer_read(_bc_buf, buffer_u32));
				}
				ds_map_set(_vm.line_tab, _bnam, _blst);
			}
			show_debug_message("[VM] 行号表载入: " + string(_nsec) + " 段 / " + string(_block_len) + " 字节");
			buffer_delete(_bc_buf);
		} else {
			_vm.blocks[? _block_name] = _bc_buf;
		}
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
			global.mod_card_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
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
				_vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
				global.mod_card_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
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

/// @function src_mod_register_weapon(_id, _c)
/// @desc 用一份武器 JSON 注册一把武器（register_weapon），发射对象统一用 obj_weapon_mod；
///       副武器只有数值（盾牌逻辑在 obj_player_shield），obj 填 obj_player_shield。
///       可选 "bullet" 字段：无 bin 时 obj_weapon_mod 的默认攻击逻辑按此发射子弹
/// @param _id 武器 id
/// @param _c 武器配置结构体：
/// {
///   "name": "武器名", "description": "描述",
///   "sprite": "spr_xxx", "icon": "spr_xxx",     // 运行时按名加载
///   "slot": "main_weapon|secondary_weapon|super_weapon",
///   "atk": 10, "cycle": 78,
///   "atk_impact": [16档], "cycle_impact": [16档],
///   "hp_increase": 100,                          // 副武器
///   "shop": { "cost": "20000", "description": "商店描述" }  // 可选：注册商店（type=weapon）
/// }
/// @return true=注册成功
function src_mod_register_weapon(_id, _c) {
	if (!is_struct(_c)) return false;
	src_mod_check_json("weapon", _id, _c);
	var _spr = noone;
	var _spr_name = _c[$ "sprite"];
	if (is_string(_spr_name) && _spr_name != "") { _spr = get_load_sprite(_spr_name); }
	var _icon = noone;
	var _icon_name = _c[$ "icon"];
	if (is_string(_icon_name) && _icon_name != "") { _icon = get_load_sprite(_icon_name); }
	if (_icon == noone) { _icon = _spr; }

	var _data = {
		sprite: _spr,
		icon: _icon,
		obj: (_c[$ "slot"] == "secondary_weapon") ? obj_player_shield : obj_weapon_mod,
		slot: _c[$ "slot"] ?? "main_weapon",
		atk: _c[$ "atk"] ?? 0,
		cycle: _c[$ "cycle"] ?? 60,
		description: _c[$ "description"] ?? _id,
		name: _c[$ "name"] ?? _id,
	};
	if (variable_struct_exists(_c, "atk_impact"))   { _data[$ "atk_impact"]   = _c[$ "atk_impact"]; }
	if (variable_struct_exists(_c, "cycle_impact")) { _data[$ "cycle_impact"] = _c[$ "cycle_impact"]; }
	if (variable_struct_exists(_c, "hp_increase"))  { _data[$ "hp_increase"]  = _c[$ "hp_increase"]; }

	register_weapon(_id, _data);

	if (variable_struct_exists(_c, "shop")) {  // 商店（type=weapon，与原版武器一致）
		var _sp = _c[$ "shop"];
		register_goods(_id, {
			type: "weapon",
			cost: string(_sp[$ "cost"] ?? 0),
			unlock_item_id: _id,
			description: _sp[$ "description"] ?? "",
			display_name: _c[$ "name"] ?? _id,
		});
	}
	return true;
}

/// @function src_mod_weapons_init(_dir)
/// @desc 扫描 mod/weapons/ 目录下的所有 .json，每个文件注册一把武器，
///       文件名（不含 .json）即武器 id；同名 .bin 解析为该武器自己的虚拟机，
///       统一放进 global.mod_weapon_vms；注册成功的 id 记录进 global.mod_weapon_ids
/// @return 成功注册的武器数量；-1=目录不存在
function src_mod_weapons_init(_dir = "") {
	if (_dir == "") { _dir = working_directory + "mod/weapons/"; }
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_weapon_vms")) { global.mod_weapon_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 武器 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			global.mod_weapon_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
			global.mod_weapon_vms[? _id][$ "card_data"] = _json;
			global.mod_weapon_vms[? _id][$ "mod_dir"] = _dir;
			if (src_mod_register_weapon(_id, _json)) {
				show_debug_message("src_mod: 注册武器 " + _id);
				_count++;
			}
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_weapons_reload()
/// @desc 仅重载所有 mod 武器的 bin 代码（与 src_mod_reload 同套路），
///       不触碰 weapon_pool 注册表
/// @return 重载了 bin 的武器数量；-1=mod 目录不存在
function src_mod_weapons_reload() {
	var _dir = working_directory + "mod/weapons/";
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_weapon_vms")) { global.mod_weapon_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 武器 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			var _vm;
			if (ds_map_exists(global.mod_weapon_vms, _id)) {
				_vm = src_mod_card_vm_fill(global.mod_weapon_vms[? _id], _bin_buf, _dir);
			} else {
				_vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
				global.mod_weapon_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
			}
			_vm[$ "card_data"] = _json;
			_vm[$ "mod_dir"] = _dir;
			_count++;
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_register_gem(_id, _c)
/// @desc 用一份宝石 JSON 注册一颗宝石（register_gem），实体统一用 obj_gem_mod；
///       JSON 除 name/description/icon/slot/shop/passive 外其余字段原样透传进
///       宝石数据（max_level/first_cooldown/cooldown 数组等升级字段），效果逻辑在 .bin
/// @param _id 宝石 id
/// @param _c 宝石配置结构体：
/// {
///   "name": "宝石名", "description": "描述",
///   "icon": "spr_xxx",
///   "slot": "main_weapon|secondary_weapon|super_weapon",
///   "max_level": 10, "first_cooldown": 60, "cooldown": [11档],
///   "passive": false,     // true=纯被动，obj 填 noone 不创建实体
///   "shop": { "cost": "25000", "description": "商店描述" }  // 可选：注册商店（type=gem）
/// }
/// @return true=注册成功
function src_mod_register_gem(_id, _c) {
	if (!is_struct(_c)) return false;
	src_mod_check_json("gem", _id, _c);
	var _icon = noone;
	var _icon_name = _c[$ "icon"];
	if (is_string(_icon_name) && _icon_name != "") { _icon = get_load_sprite(_icon_name); }

	var _data = {
		name: _c[$ "name"] ?? _id,
		description: _c[$ "description"] ?? _id,
		icon: _icon,
		slot: _c[$ "slot"] ?? "main_weapon",
		obj: (_c[$ "passive"] == true) ? noone : obj_gem_mod,
	};
	// 其余字段（max_level/first_cooldown/cooldown 数组等）原样透传
	var _keys = variable_struct_get_names(_c);
	for (var _i = 0; _i < array_length(_keys); _i++) {
		var _k = _keys[_i];
		if (_k == "name" || _k == "description" || _k == "icon" || _k == "slot" || _k == "shop" || _k == "passive") continue;
		_data[$ _k] = _c[$ _k];
	}

	register_gem(_id, _data);

	if (variable_struct_exists(_c, "shop")) {  // 商店（type=gem，与原版宝石一致）
		var _sp = _c[$ "shop"];
		register_goods(_id, {
			type: "gem",
			cost: string(_sp[$ "cost"] ?? 0),
			unlock_item_id: _id,
			description: _sp[$ "description"] ?? "",
			display_name: _c[$ "name"] ?? _id,
		});
	}
	return true;
}

/// @function src_mod_gems_init(_dir)
/// @desc 扫描 mod/gems/ 目录下的所有 .json，每个文件注册一颗宝石，
///       文件名（不含 .json）即宝石 id；同名 .bin 解析为该宝石自己的虚拟机，
///       统一放进 global.mod_gem_vms
/// @return 成功注册的宝石数量；-1=目录不存在
function src_mod_gems_init(_dir = "") {
	if (_dir == "") { _dir = working_directory + "mod/gems/"; }
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_gem_vms")) { global.mod_gem_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 宝石 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			global.mod_gem_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
			global.mod_gem_vms[? _id][$ "card_data"] = _json;
			global.mod_gem_vms[? _id][$ "mod_dir"] = _dir;
			if (src_mod_register_gem(_id, _json)) {
				show_debug_message("src_mod: 注册宝石 " + _id);
				_count++;
			}
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_gems_reload()
/// @desc 仅重载所有 mod 宝石的 bin 代码（与 src_mod_reload 同套路），
///       不触碰 gems_pool 注册表
/// @return 重载了 bin 的宝石数量；-1=mod 目录不存在
function src_mod_gems_reload() {
	var _dir = working_directory + "mod/gems/";
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_gem_vms")) { global.mod_gem_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 宝石 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			var _vm;
			if (ds_map_exists(global.mod_gem_vms, _id)) {
				_vm = src_mod_card_vm_fill(global.mod_gem_vms[? _id], _bin_buf, _dir);
			} else {
				_vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
				global.mod_gem_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
			}
			_vm[$ "card_data"] = _json;
			_vm[$ "mod_dir"] = _dir;
			_count++;
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_register_attire(_id, _c)
/// @desc 用一份时装 JSON 注册一件时装（register_attire）。时装是纯外观：把 spr/icon
///       贴图名按运行时加载（get_load_sprite），装备逻辑（equip_attire/人物换皮）都是
///       原版流程，不用写任何 GML。可选 "shop" 注册商店：target_card=="player" 的
///       角色时装进 tab5（type=player_attire），卡片时装进 tab4（type=card_attire）
/// @param _id 时装 id
/// @param _c 时装配置结构体：
/// {
///   "name": "时装名",
///   "target_card": "player",              // 或卡片 id
///   "icon": "spr_xxx",                    // 时装图标（编辑菜单/商店用）
///   "spr": "spr_xxx" 或 ["spr_a","spr_b"],  // 单张贴图或动画帧数组
///   "card_slot_icon": ["spr_xxx",...],     // 可选：卡片卡槽图标帧数组
///   "shop": { "cost": "50000", "description": "商店描述" }  // 可选：注册商店
/// }
/// @return true=注册成功
function src_mod_register_attire(_id, _c) {
	if (!is_struct(_c)) return false;
	var _icon = noone;
	var _icon_name = _c[$ "icon"];
	if (is_string(_icon_name) && _icon_name != "") { _icon = get_load_sprite(_icon_name); }

	// spr：单张贴图名或帧名数组，逐张按名加载
	var _spr = noone;
	var _spr_val = _c[$ "spr"];
	if (is_string(_spr_val) && _spr_val != "") {
		_spr = get_load_sprite(_spr_val);
	} else if (is_array(_spr_val)) {
		_spr = [];
		for (var _i = 0; _i < array_length(_spr_val); _i++) {
			array_push(_spr, get_load_sprite(_spr_val[_i]));
		}
	}

	var _data = {
		target_card: _c[$ "target_card"] ?? "player",
		name: _c[$ "name"] ?? _id,
		icon: _icon,
		spr: _spr,
	};
	// card_slot_icon：卡槽图标帧数组（仅卡片时装用到，可选）
	if (is_array(_c[$ "card_slot_icon"])) {
		var _csi = [];
		var _csi_val = _c[$ "card_slot_icon"];
		for (var _i = 0; _i < array_length(_csi_val); _i++) {
			array_push(_csi, get_load_sprite(_csi_val[_i]));
		}
		_data[$ "card_slot_icon"] = _csi;
	}

	register_attire(_id, _data);

	if (variable_struct_exists(_c, "shop")) {  // 商店：角色时装 tab5，卡片时装 tab4
		var _sp = _c[$ "shop"];
		register_goods(_id, {
			type: (_data[$ "target_card"] == "player") ? "player_attire" : "card_attire",
			cost: string(_sp[$ "cost"] ?? 0),
			unlock_item_id: _id,
			description: _sp[$ "description"] ?? "",
			display_name: _data[$ "name"],
		});
	}
	return true;
}

/// @function src_mod_attires_init(_dir)
/// @desc 扫描 mod/attires/ 目录下的所有 .json，每个文件注册一件时装，
///       文件名（不含 .json）即时装 id。时装纯外观无逻辑，只有 json 没有 bin
/// @return 成功注册的时装数量；-1=目录不存在
function src_mod_attires_init(_dir = "") {
	if (_dir == "") { _dir = working_directory + "mod/attires/"; }
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 时装 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			if (src_mod_register_attire(_id, _json)) {
				show_debug_message("src_mod: 注册时装 " + _id);
				_count++;
			}
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_register_enemy(_id, _c)
/// @desc 用一份敌人 JSON 注册一个敌人（register_enemy），实体统一用 obj_enemy_mod
///       （继承 obj_enemy_parent，移动/攻击/死亡等基础行为全部复用父类，
///       特殊逻辑写在同名 .bin 里）。注册进 enemy_map 后，关卡 JSON 波次里直接
///       写敌人 id、插件里用 VM_SpawnEnemy/VM_SpawnBoss 都能生成它
/// @param _id 敌人 id
/// @param _c 敌人配置结构体：
/// {
///   "name": "敌人名", "description": "描述",
///   "spr": "spr_xxx",
///   "hp": 100, "shield": 0, "speed": 0.3,
///   "atk": 10, "cycle": 36, "range": 90,
///   "ash_proof": false, "feature": "land|water"
/// }
/// @return true=注册成功
function src_mod_register_enemy(_id, _c) {
	if (!is_struct(_c)) return false;
	src_mod_check_json("enemy", _id, _c);
	var _spr = noone;
	var _spr_name = _c[$ "spr"];
	if (is_string(_spr_name) && _spr_name != "") { _spr = get_load_sprite(_spr_name); }

	register_enemy(_id, {
		name: _c[$ "name"] ?? _id,
		_obj: obj_enemy_mod,
		hp: _c[$ "hp"] ?? 100,
		shield: _c[$ "shield"] ?? 0,
		description: _c[$ "description"] ?? "",
		speed: _c[$ "speed"] ?? 0.3,
		atk: _c[$ "atk"] ?? 10,
		cycle: _c[$ "cycle"] ?? 36,
		range: _c[$ "range"] ?? 90,
		ash_proof: _c[$ "ash_proof"] ?? false,
		spr: _spr,
		feature: _c[$ "feature"] ?? "land",
	});
	return true;
}

/// @function src_mod_enemies_init(_dir)
/// @desc 扫描 mod/enemies/ 目录下的所有 .json，每个文件注册一个敌人，
///       文件名（不含 .json）即敌人 id；同名 .bin 解析为该敌人自己的虚拟机，
///       统一放进 global.mod_enemy_vms
/// @return 成功注册的敌人数量；-1=目录不存在
function src_mod_enemies_init(_dir = "") {
	if (_dir == "") { _dir = working_directory + "mod/enemies/"; }
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_enemy_vms")) { global.mod_enemy_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 敌人 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);  // 去掉 .json
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			global.mod_enemy_vms[? _id] = src_mod_card_vm_load(_bin_buf, _dir, _id);
			global.mod_enemy_vms[? _id][$ "card_data"] = _json;
			global.mod_enemy_vms[? _id][$ "mod_dir"] = _dir;
			if (src_mod_register_enemy(_id, _json)) {
				show_debug_message("src_mod: 注册敌人 " + _id);
				_count++;
			}
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

/// @function src_mod_enemies_reload()
/// @desc 仅重载所有 mod 敌人的 bin 代码（与 src_mod_reload 同套路），
///       不触碰 enemy_map 注册表
/// @return 重载了 bin 的敌人数量；-1=mod 目录不存在
function src_mod_enemies_reload() {
	var _dir = working_directory + "mod/enemies/";
	if (!directory_exists(_dir)) {
		show_debug_message("src_mod: 目录不存在 " + _dir);
		return -1;
	}
	if (!variable_global_exists("mod_enemy_vms")) { global.mod_enemy_vms = ds_map_create(); }

	var _count = 0;
	var _file = file_find_first(_dir + "*.json", 0);
	while (_file != "") {
		var _path = _dir + _file;
		var _json = json_parse(src_mod_read_text(_path));
		if (!is_struct(_json)) {
			show_debug_message("src_mod: 敌人 JSON 解析失败 " + _path);
		} else {
			var _name = filename_name(_file);
			var _id = string_copy(_name, 1, string_length(_name) - 5);
			var _bin_path = string_replace(_path, ".json", ".bin");
			var _bin_buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
			var _vm;
			if (ds_map_exists(global.mod_enemy_vms, _id)) {
				_vm = src_mod_card_vm_fill(global.mod_enemy_vms[? _id], _bin_buf, _dir);
			} else {
				_vm = src_mod_card_vm_load(_bin_buf, _dir, _id);
				global.mod_enemy_vms[? _id] = _vm;   // 新建的 VM 必须写回，否则这次重载白干
			}
			_vm[$ "card_data"] = _json;
			_vm[$ "mod_dir"] = _dir;
			_count++;
		}
		_file = file_find_next();
	}
	file_find_close();
	return _count;
}

// ══════════════════════════════════════════════════════════════════════════
// mod 管理（shell 命令 listmod / reloadallmod / reloadmod <id|名称> 用）
// ══════════════════════════════════════════════════════════════════════════

/// @function src_mod_categories()
/// @desc mod 类别表（顺序即 listmod 的显示顺序）
///       label=显示名 / vmap=虚拟机全局 map / folder=目录名 / kind=注册分支
function src_mod_categories() {
	return [
		{ label: "卡",   vmap: "mod_card_vms",   folder: "cards",   kind: "card"   },
		{ label: "武器", vmap: "mod_weapon_vms", folder: "weapons", kind: "weapon" },
		{ label: "宝石", vmap: "mod_gem_vms",    folder: "gems",    kind: "gem"    },
		{ label: "敌人", vmap: "mod_enemy_vms",  folder: "enemies", kind: "enemy"  },
		{ label: "子弹", vmap: "mod_bullet_vms", folder: "bullets", kind: "bullet" },
		{ label: "特效", vmap: "mod_effect_vms", folder: "effects", kind: "effect" },
	];
}

/// @function src_mod_desc_of(_json, _id)
/// @desc 取简介：json 的 description，没有就退回 shop.description
function src_mod_desc_of(_json, _id) {
	if (!is_struct(_json)) return "";
	var _d = _json[$ "description"] ?? "";
	if (_d == "" && variable_struct_exists(_json, "shop")) {
		var _sp = _json[$ "shop"];
		if (is_struct(_sp)) _d = _sp[$ "description"] ?? "";
	}
	return string(_d);
}

/// @function src_mod_num(_kind, _id)
/// @desc 给一个 mod 分数字 id（= 第几个登记的 mod，1 起）。分过就返回原来的，
///       所以后加进来的 mod 不会把已有的编号挤走
function src_mod_num(_kind, _id) {
	if (!variable_global_exists("_mod_num_map")) {
		global._mod_num_map = ds_map_create();
		global._mod_num_list = [];
	}
	var _key = _kind + ":" + _id;
	if (ds_map_exists(global._mod_num_map, _key)) return global._mod_num_map[? _key];
	var _n = array_length(global._mod_num_list) + 1;
	global._mod_num_map[? _key] = _n;
	array_push(global._mod_num_list, { kind: _kind, id: _id });
	return _n;
}

/// @function src_mod_by_num(_n)
/// @desc 数字 id → { kind, id }；越界返回 undefined
function src_mod_by_num(_n) {
	if (!variable_global_exists("_mod_num_list")) return undefined;
	if (_n < 1 || _n > array_length(global._mod_num_list)) return undefined;
	return global._mod_num_list[_n - 1];
}

/// @function src_mod_all_ids()
/// @desc 所有已加载 mod 的 id（给 shell 补全用）
function src_mod_all_ids() {
	var _cats = src_mod_categories();
	var _ids = [];
	for (var _c = 0; _c < array_length(_cats); _c++) {
		if (!variable_global_exists(_cats[_c].vmap)) continue;
		var _m = variable_global_get(_cats[_c].vmap);
		var _keys = ds_map_keys_to_array(_m);
		for (var _i = 0; _i < array_length(_keys); _i++) array_push(_ids, _keys[_i]);
	}
	array_sort(_ids, true);
	return _ids;
}

/// @function src_mod_list()
/// @desc 列出当前已加载的 mod：每条 = [类别] id  名称  路径  简介
/// @return 多行字符串
function src_mod_list() {
	var _cats = src_mod_categories();
	var _lines = [];
	var _total = 0;
	for (var _c = 0; _c < array_length(_cats); _c++) {
		if (!variable_global_exists(_cats[_c].vmap)) continue;
		var _m = variable_global_get(_cats[_c].vmap);
		var _keys = ds_map_keys_to_array(_m);
		array_sort(_keys, true);   // ds_map 的 key 顺序不定，排一下
		for (var _i = 0; _i < array_length(_keys); _i++) {
			var _id = _keys[_i];
			var _vm = _m[? _id];
			var _jd = _vm[$ "card_data"];
			var _nm = is_struct(_jd) ? string(_jd[$ "name"] ?? _id) : _id;
			var _dir = string_replace(_vm[$ "mod_dir"] ?? "", working_directory, "");
			var _num = src_mod_num(_cats[_c].kind, _id);
			array_push(_lines, "[" + string(_num) + "] [" + _cats[_c].label + "] " + _id + "  " + _nm + "  " + _dir + "  " + src_mod_desc_of(_jd, _id));
			_total++;
		}
	}
	// 时装：只有 json + 贴图，没 bin/VM
	var _adir = working_directory + "mod/attires/";
	if (directory_exists(_adir)) {
		var _f = file_find_first(_adir + "*.json", 0);
		while (_f != "") {
			var _an = filename_name(_f);
			var _aid = string_copy(_an, 1, string_length(_an) - 5);
			if (variable_global_exists("attire_pool") && ds_map_exists(global.attire_pool, _aid)) {
				var _aj = json_parse(src_mod_read_text(_adir + _f));
				var _anm = is_struct(_aj) ? string(_aj[$ "name"] ?? _aid) : _aid;
				var _anum = src_mod_num("attire", _aid);
				array_push(_lines, "[" + string(_anum) + "] [时装] " + _aid + "  " + _anm + "  mod/attires/  " + src_mod_desc_of(_aj, _aid));
				_total++;
			}
			_f = file_find_next();
		}
		file_find_close();
	}
	if (_total == 0) return "[listmod] 没有已加载的 mod";
	var _out = "[listmod] 共 " + string(_total) + " 项（格式：[数字id] [类别] id  名称  路径  简介）\n";
	for (var _i = 0; _i < array_length(_lines); _i++) _out += _lines[_i] + "\n";
	return _out;
}

/// @function src_mod_register_new()
/// @desc 重扫所有 mod 目录，只注册**还没加载过**的（已在 VM map 里的跳过）
///       卡的卡池是单向注册（重复注册会在卡池里塞出两条），所以这里必须跳过已注册的
/// @return 汇总字符串
function src_mod_register_new() {
	var _cats = src_mod_categories();
	var _sum = "";
	var _total = 0;
	for (var _c = 0; _c < array_length(_cats); _c++) {
		var _cat = _cats[_c];
		var _dir = working_directory + "mod/" + _cat.folder + "/";
		if (!directory_exists(_dir) || !variable_global_exists(_cat.vmap)) {
			_sum += _cat.label + "0 ";
			continue;
		}
		var _m = variable_global_get(_cat.vmap);
		var _n = 0;
		var _f = file_find_first(_dir + "*.json", 0);
		while (_f != "") {
			var _fn = filename_name(_f);
			var _id = string_copy(_fn, 1, string_length(_fn) - 5);
			if (!ds_map_exists(_m, _id)) {           // 已加载过的跳过
				var _jd = json_parse(src_mod_read_text(_dir + _f));
				if (is_struct(_jd)) {
					var _bin_path = _dir + _id + ".bin";
					var _buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
					var _vm = src_mod_card_vm_load(_buf, _dir, _id);
					_vm[$ "card_data"] = _jd;
					_vm[$ "mod_dir"] = _dir;
					_m[? _id] = _vm;
					switch (_cat.kind) {
						case "card":   src_mod_register_card(_id, _jd);   break;
						case "weapon": src_mod_register_weapon(_id, _jd); break;
						case "gem":    src_mod_register_gem(_id, _jd);    break;
						case "enemy":  src_mod_register_enemy(_id, _jd);  break;
					}
					show_debug_message("src_mod: 新增注册 " + _cat.folder + "/" + _id);
					_n++;
				}
			}
			_f = file_find_next();
		}
		file_find_close();
		_total += _n;
		_sum += _cat.label + string(_n) + " ";
	}
	// 时装（无 bin）：没注册过的才注册
	var _an = 0;
	var _adir = working_directory + "mod/attires/";
	if (directory_exists(_adir)) {
		var _f2 = file_find_first(_adir + "*.json", 0);
		while (_f2 != "") {
			var _fn2 = filename_name(_f2);
			var _aid = string_copy(_fn2, 1, string_length(_fn2) - 5);
			var _has = variable_global_exists("attire_pool") && ds_map_exists(global.attire_pool, _aid);
			if (!_has) {
				var _aj = json_parse(src_mod_read_text(_adir + _f2));
				if (is_struct(_aj) && src_mod_register_attire(_aid, _aj)) _an++;
			}
			_f2 = file_find_next();
		}
		file_find_close();
	}
	_total += _an;
	_sum += "时装" + string(_an);
	return "[reloadallmod] 新增 " + string(_total) + " 项（卡/武器/宝石/敌人/子弹/特效/时装）：" + _sum;
}

/// @function src_mod_reload_id(_cat, _id)
/// @desc 重载单个 VM 类别的 mod：重读同名 .json/.bin 灌进原 VM（会重跑 _VM_CONST_INIT/_OBJECT_CFG）
/// @return true=成功
function src_mod_reload_id(_cat, _id) {
	var _m = variable_global_get(_cat.vmap);
	var _vm = _m[? _id];
	if (!is_struct(_vm)) return false;
	var _dir = _vm[$ "mod_dir"] ?? "";
	if (_dir == "") _dir = working_directory + "mod/" + _cat.folder + "/";
	var _json_path = _dir + _id + ".json";
	if (!file_exists(_json_path)) return false;
	var _jd = json_parse(src_mod_read_text(_json_path));
	var _bin_path = _dir + _id + ".bin";
	var _buf = file_exists(_bin_path) ? buffer_load(_bin_path) : undefined;
	_vm = src_mod_card_vm_fill(_vm, _buf, _dir);
	_m[? _id] = _vm;
	if (is_struct(_jd)) _vm[$ "card_data"] = _jd;
	// 数值重新注册（卡的卡池是单向注册，卡的数值改动要靠 reloadallmod/重开）
	switch (_cat.kind) {
		case "card":
			if (is_struct(_jd) && variable_global_exists("mod_cards")) global.mod_cards[? _id] = _jd;
			break;
		case "weapon": if (is_struct(_jd)) src_mod_register_weapon(_id, _jd); break;
		case "gem":    if (is_struct(_jd)) src_mod_register_gem(_id, _jd);    break;
		case "enemy":  if (is_struct(_jd)) src_mod_register_enemy(_id, _jd);  break;
	}
	return true;
}

/// @function src_mod_reload_attire(_key)
/// @desc 时装没有 bin，只能按 id 或名称重读 json 再注册一遍（贴图/数值改动生效）
/// @return true=成功
function src_mod_reload_attire(_key) {
	var _adir = working_directory + "mod/attires/";
	if (!directory_exists(_adir)) return false;
	var _by_id = _adir + _key + ".json";
	if (file_exists(_by_id)) {
		var _jd = json_parse(src_mod_read_text(_by_id));
		if (is_struct(_jd)) return src_mod_register_attire(_key, _jd);
		return false;
	}
	var _f = file_find_first(_adir + "*.json", 0);
	while (_f != "") {
		var _jd2 = json_parse(src_mod_read_text(_adir + _f));
		if (is_struct(_jd2) && string(_jd2[$ "name"] ?? "") == _key) {
			var _fn = filename_name(_f);
			var _id2 = string_copy(_fn, 1, string_length(_fn) - 5);
			file_find_close();
			return src_mod_register_attire(_id2, _jd2);
		}
		_f = file_find_next();
	}
	file_find_close();
	return false;
}

/// @function src_mod_reload_one(_key)
/// @desc 按 id 或名称重载单个 mod（卡/武器/宝石/敌人/子弹/特效/时装）
/// @return 结果字符串（给 shell 显示）
function src_mod_reload_one(_key) {
	if (_key == "") return "[reloadmod] 用法: reloadmod <数字id | mod id | 名称>";
	var _cats = src_mod_categories();
	// ① 数字 id（listmod 打印的那个）
	if (string_digits(_key) == _key) {
		var _hit = src_mod_by_num(real(_key));
		if (is_undefined(_hit)) return "[reloadmod] 没有这个编号: " + _key;
		if (_hit.kind == "attire") {
			if (src_mod_reload_attire(_hit.id)) return "[reloadmod] 已重注册时装 " + _hit.id + "（#" + _key + "）";
			return "[reloadmod] 重载失败: " + _hit.id;
		}
		for (var _cn = 0; _cn < array_length(_cats); _cn++) {
			if (_cats[_cn].kind == _hit.kind) {
				if (src_mod_reload_id(_cats[_cn], _hit.id)) {
					return "[reloadmod] 已重载 [" + _cats[_cn].label + "] " + _hit.id + "（#" + _key + "）";
				}
				return "[reloadmod] 重载失败: " + _hit.id;
			}
		}
		return "[reloadmod] 编号 " + _key + " 对应的类别未知";
	}
	// ② 按 id（文件名）
	for (var _c = 0; _c < array_length(_cats); _c++) {
		if (!variable_global_exists(_cats[_c].vmap)) continue;
		var _m = variable_global_get(_cats[_c].vmap);
		if (ds_map_exists(_m, _key)) {
			if (src_mod_reload_id(_cats[_c], _key)) return "[reloadmod] 已重载 [" + _cats[_c].label + "] " + _key;
			return "[reloadmod] 重载失败（同名 .json 找不到？）: " + _key;
		}
	}
	// ③ 按 json 里的 name
	for (var _c2 = 0; _c2 < array_length(_cats); _c2++) {
		if (!variable_global_exists(_cats[_c2].vmap)) continue;
		var _m2 = variable_global_get(_cats[_c2].vmap);
		var _keys = ds_map_keys_to_array(_m2);
		array_sort(_keys, true);
		for (var _i = 0; _i < array_length(_keys); _i++) {
			var _jd = _m2[? _keys[_i]][$ "card_data"];
			if (is_struct(_jd) && string(_jd[$ "name"] ?? "") == _key) {
				if (src_mod_reload_id(_cats[_c2], _keys[_i])) {
					return "[reloadmod] 已重载 [" + _cats[_c2].label + "] " + _keys[_i] + "（" + _key + "）";
				}
				return "[reloadmod] 重载失败: " + _keys[_i];
			}
		}
	}
	// ④ 时装
	if (src_mod_reload_attire(_key)) return "[reloadmod] 已重注册时装 " + _key;
	return "[reloadmod] 没找到 mod: " + _key;
}

