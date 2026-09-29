function mod_inst_count(_vm) {
	if (is_undefined(_vm) || !variable_struct_exists(_vm, "inst_count")) return 0;
	return _vm.inst_count;
}
