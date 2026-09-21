if global.is_paused {
  exit
}

// 记录当前帧与上一帧格子坐标（用于路径碰撞检测）
prev_grid_col = grid_col;
prev_grid_row = grid_row;
var _gp = get_grid_position_from_world(x, y, true);
grid_col = _gp.col;
grid_row = _gp.row;

// 延迟初始化：允许创建后再用 VM_SetProp 设置 mod_type
if (!_mod_initialized && mod_type != "" && variable_global_exists("mod_bullet_vms") && ds_map_exists(global.mod_bullet_vms, mod_type)) {
  _mod_vm = global.mod_bullet_vms[? mod_type];
  _mod_initialized = true;
  ds_list_add(_mod_vm.instances, id);

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

// 执行子弹自己的 _OBJECT_STEP
if (_mod_initialized && ds_map_exists(_mod_vm.blocks, "_OBJECT_STEP")) {
  var _bak_cur = global._VM_cur_card;
  global._VM_cur_card = id;
  var _bak_vm = global.__vm;
  global.__vm = _mod_vm;
  VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
  global.__vm = _bak_vm;
  global._VM_cur_card = _bak_cur;
}

if (!instance_exists(id)) exit;

// 自动销毁
if (destroy_timer == 0) {
  instance_destroy();
  exit;
}
if (destroy_timer > 0) {
  destroy_timer -= 1;
  if (destroy_timer <= 0) instance_destroy();
}