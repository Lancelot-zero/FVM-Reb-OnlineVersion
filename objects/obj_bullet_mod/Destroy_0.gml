// 执行子弹自己的 _OBJECT_DESTROY，并移出 VM 实例列表
if (_mod_initialized) {
  if (ds_map_exists(_mod_vm.blocks, "_OBJECT_DESTROY")) {
    var _bak_last = global._VM_last_destroyed_card;
    global._VM_last_destroyed_card = id;
    var _bak_cur = global._VM_cur_card;
    global._VM_cur_card = id;
    var _bak_vm = global.__vm;
    global.__vm = _mod_vm;
    VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_DESTROY"], "_OBJECT_DESTROY");
    global.__vm = _bak_vm;
    global._VM_cur_card = _bak_cur;
    global._VM_last_destroyed_card = _bak_last;
  }

  mod_inst_del(_mod_vm, id, false);   // 子弹只减计数，不进 instances 列表
}