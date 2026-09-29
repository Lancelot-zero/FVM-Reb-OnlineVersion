function mod_sync_props_tick() {
	if (global.network.mode != "server") return;
	var _vm_maps = [];
	if (variable_global_exists("mod_card_vms"))   array_push(_vm_maps, global.mod_card_vms);
	if (variable_global_exists("mod_weapon_vms")) array_push(_vm_maps, global.mod_weapon_vms);
	if (variable_global_exists("mod_gem_vms"))    array_push(_vm_maps, global.mod_gem_vms);
	if (variable_global_exists("mod_enemy_vms"))  array_push(_vm_maps, global.mod_enemy_vms);

	var _clients = global.network.connected_clients;
	if (array_length(_clients) == 0) return;   // 没有客户端，不用打包

	for (var _m = 0; _m < array_length(_vm_maps); _m++) {
		var _vmap = _vm_maps[_m];
		if (!ds_exists(_vmap, ds_type_map)) continue;
		var _ids = ds_map_keys_to_array(_vmap);
		for (var _mi = 0; _mi < array_length(_ids); _mi++) {
			var _vm = _vmap[? _ids[_mi]];
			if (!is_struct(_vm)) continue;
			if (!variable_struct_exists(_vm, "arrays") || !ds_exists(_vm.arrays, ds_type_map)) continue;
			if (!ds_map_exists(_vm.arrays, "mod_sync")) continue;          // 没声明就跳过
			var _fields = _vm.arrays[? "mod_sync"];
			if (!is_array(_fields) || array_length(_fields) == 0) continue;
			if (!variable_struct_exists(_vm, "instances")) continue;

			var _insts = _vm.instances;
			var _n_inst = ds_list_size(_insts);
			for (var _k = 0; _k < _n_inst; _k++) {
				var _inst = _insts[| _k];
				if (!instance_exists(_inst)) continue;
				// 没有 net_id 的（子弹 / 特效 / 本地单位）本来就不跨端
				if (!ds_map_exists(global.network.map_instance_id_net_id, _inst)) continue;

				if (!variable_instance_exists(_inst, "_mod_sync_last")) _inst._mod_sync_last = {};
				var _cache = _inst._mod_sync_last;
				if (!is_struct(_cache)) { _cache = {}; _inst._mod_sync_last = _cache; }

				var _send = {};
				var _changed = 0;
				for (var _f = 0; _f < array_length(_fields); _f++) {
					var _nm = _fields[_f];
					if (!is_string(_nm) || _nm == "") continue;
					if (!variable_instance_exists(_inst, _nm)) continue;
					var _v  = variable_instance_get(_inst, _nm);
					var _vs = json_stringify(_v);                                  // 数组/结构也能比
					var _old = variable_struct_exists(_cache, _nm) ? _cache[$ _nm] : undefined;
					if (_old != _vs) { _send[$ _nm] = _v; _changed++; }
					_cache[$ _nm] = _vs;
				}
				if (_changed == 0) continue;

				var _nid  = global.network.map_instance_id_net_id[? _inst];
				var _json = json_stringify(_send);
				for (var _c = 0; _c < array_length(_clients); _c++) {
					send_message(_clients[_c], MSG_MODIFY_PROP, _nid, _json);
				}
			}
		}
	}
}
