function mod_card_vm_deck_refresh() {
	if (!variable_global_exists("mod_card_vms") || !ds_exists(global.mod_card_vms, ds_type_map)) return;
	var _ids = ds_map_keys_to_array(global.mod_card_vms);
	for (var _i = 0; _i < array_length(_ids); _i++) {
		mod_card_vm_deck_check(global.mod_card_vms[? _ids[_i]]);
	}
}
