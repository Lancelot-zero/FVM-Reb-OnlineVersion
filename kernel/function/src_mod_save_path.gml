function src_mod_save_path(_rel) {
	if (global.mod_enabled) {
		directory_create("mod/saves");
		return "mod/saves/" + _rel;
	}
	return "saves/" + _rel;
}
