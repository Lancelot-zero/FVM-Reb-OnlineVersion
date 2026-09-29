function vm_hook_run(_name) {
    if (!variable_global_exists("_VM_hooks")) return;
    if (!ds_map_exists(global._VM_hooks, _name)) return;
    var _list = global._VM_hooks[? _name];
    if (array_length(_list) == 0) return;

    var _bak = global.__vm;
    for (var _i = 0; _i < array_length(_list); _i++) {
        var _vm = _list[_i];
        if (is_undefined(_vm)) continue;
        if (!variable_struct_exists(_vm, "blocks")) continue;
        if (!ds_map_exists(_vm.blocks, _name)) continue;   // 这个 VM 没有这个块 → 跳过
        // mod 单位 VM：条件符合就叫醒 —— 场上有它的实例 / 带 __hookall__ /
        // 卡片 mod 专属 deck_active（这张卡在出战卡组里）
        if (variable_struct_exists(_vm, "is_mod_vm") && !variable_struct_exists(_vm, "hook_all")
            && !variable_struct_exists(_vm, "deck_active")) {
            if (mod_inst_count(_vm) <= 0) continue;
        }
        global.__vm = _vm;
    }
    global.__vm = _bak;
}
