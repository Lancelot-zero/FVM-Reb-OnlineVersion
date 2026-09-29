function mod_card_vm_deck_check(_vm) {
	if (is_undefined(_vm) || !is_struct(_vm)) return;
	if (!variable_struct_exists(_vm, "mod_id")) return;   // 只有卡片 mod 的 VM 有 mod_id
	var _in = false;
	if (variable_global_exists("selected_deck") && ds_exists(global.selected_deck, ds_type_list)) {
		var _n = ds_list_size(global.selected_deck);
		for (var _i = 0; _i < _n; _i++) {
			var _e = global.selected_deck[| _i];
			if (_e == undefined || _e == noone) continue;
			if (!ds_map_exists(_e, "card_id")) continue;
			if (_e[? "card_id"] == _vm.mod_id) { _in = true; break; }
		}
	}
	if (_in) {
		_vm[$ "deck_active"] = true;
	} else if (variable_struct_exists(_vm, "deck_active")) {
		variable_struct_remove(_vm, "deck_active");
	}
}
