// 通用 mod 子弹对象
// 由插件 .bin 通过 VM_CreateInstance("obj_bullet_mod", x, y) 创建，
// 然后用 VM_SetProp(inst, "mod_type", "子弹类型名") 指定逻辑。
// destroy_timer: -1=不自动销毁, 0=立即销毁, >0=每帧减1，到0销毁
mod_type = variable_global_exists("_mod_pending_bullet_id") ? global._mod_pending_bullet_id : "";
destroy_timer = -1;
_mod_initialized = false;
_mod_vm = undefined;
image_speed = 0;

// 当前帧/上一帧格子坐标（用于子弹路径碰撞检测）
grid_col = -1;
grid_row = -1;
prev_grid_col = -1;
prev_grid_row = -1;
var _gp = get_grid_position_from_world(x, y, true);
grid_col = _gp.col;
grid_row = _gp.row;
prev_grid_col = grid_col;
prev_grid_row = grid_row;

// 如果创建前已经通过 global._mod_pending_bullet_id 指定类型，则立即初始化
if (mod_type != "" && variable_global_exists("mod_bullet_vms") && ds_map_exists(global.mod_bullet_vms, mod_type)) {
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