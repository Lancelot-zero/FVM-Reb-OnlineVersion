function get_load_sprite(_name) {
	var _native_spr = asset_get_index(_name);
    if (_native_spr != -1 && sprite_exists(_native_spr)) {
        ds_map_add(global._pid_reverse, _native_spr, _name);
        return _native_spr;
    }
	// 先查 VM 临时贴图缓存（bin 专属贴图，文件传输/本地加载的都在这里）
	if (ds_map_exists(global._VM_sprite_temp_cache, _name)) {
		return global._VM_sprite_temp_cache[? _name];
	}
	// 再查 VM 永久贴图缓存（VM_LoadSpritePerm_Ex，bin 重载不清）
	if (ds_map_exists(global._VM_sprite_cache, _name)) {
		return global._VM_sprite_cache[? _name];
	}
	if (ds_map_exists(global._sprite_cache,_name)){
		return global._sprite_cache[? _name];
	}
	var _tmp = working_directory + "_ph_empty.png";
    var _placeholder = sprite_add(_tmp, 1, false, false, 0, 0);
	
	ds_map_add(global._sprite_cache, _name , _placeholder);
    ds_map_add(global._pid_reverse, _placeholder, _name);
	ds_map_add(global._sprite_state, _name , empty_load);
    return _placeholder;
}
