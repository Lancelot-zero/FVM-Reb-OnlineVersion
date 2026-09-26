// 延迟初始化：允许创建后再用 VM_SetProp 设置 mod_type
if (!_mod_initialized && mod_type != "" && variable_global_exists("mod_effect_vms") && ds_map_exists(global.mod_effect_vms, mod_type)) {
  _mod_vm = global.mod_effect_vms[? mod_type];
  _mod_initialized = true;
  mod_inst_add(_mod_vm, id);

  if (ds_map_exists(_mod_vm.blocks, "_OBJECT_CREATE")) {
    var _bak_last = global._VM_last_created_card;
    global._VM_last_created_card = id;
    var _bak_cur = global._VM_cur_card;
    global._VM_cur_card = id;
    var _bak_vm = global.__vm;
    global.__vm = _mod_vm;
    VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
    global.__vm = _bak_vm;
    global._VM_cur_card = _bak_cur;
    global._VM_last_created_card = _bak_last;
  }
}

// 执行特效自己的 _OBJECT_DRAW，否则画本体精灵
if (_mod_initialized && ds_map_exists(_mod_vm.blocks, "_OBJECT_DRAW")) {
  var _bak_cur = global._VM_cur_card;
  global._VM_cur_card = id;
  var _bak_vm = global.__vm;
  global.__vm = _mod_vm;
  VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_DRAW"], "_OBJECT_DRAW");
  global.__vm = _bak_vm;
  global._VM_cur_card = _bak_cur;
  exit;
}
draw_self();