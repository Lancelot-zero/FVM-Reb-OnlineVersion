function file_cache_clear() {
    // 清理 sprite manager 注册：遍历 file_cache.sprites，按 name 删除对应条目
    var _keys = ds_map_keys_to_array(global.file_cache.sprites);
    for (var _i = 0; _i < array_length(_keys); _i++) {
        var _name = _keys[_i];
        var _spr = global.file_cache.sprites[? _name];
        // 从 _sprite_cache 移除（name → sprite）
        if (ds_map_exists(global._sprite_cache, _name)) {
            ds_map_delete(global._sprite_cache, _name);
        }
        // 从 _sprite_state 移除
        if (ds_map_exists(global._sprite_state, _name)) {
            ds_map_delete(global._sprite_state, _name);
        }
        // 从 _pid_reverse 移除（sprite → name），占位和真精灵都清
        if (ds_map_exists(global._pid_reverse, _spr)) {
            ds_map_delete(global._pid_reverse, _spr);
        }
    }

    ds_map_destroy(global.file_cache.sprites);
    global.file_cache.sprites = ds_map_create();
    ds_map_destroy(global.file_cache.audio);
    global.file_cache.audio = ds_map_create();
    ds_map_destroy(global.file_cache.fingerprints);
    global.file_cache.fingerprints = ds_map_create();

    global.file_cache.expected_sprite = "";
    global.file_cache.expected_audio = {};

    show_debug_message("[客户端] 文件缓存已清空");
}
