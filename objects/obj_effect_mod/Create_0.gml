// 通用 mod 特效对象
// 由插件 .bin 通过 VM_CreateInstance("obj_effect_mod", x, y) 创建，
// 然后用 VM_SetProp(inst, "mod_type", "特效类型名") 指定逻辑。
// destroy_timer: -1=不自动销毁, 0=立即销毁, >0=每帧减1，到0销毁
mod_type = variable_global_exists("_mod_pending_effect_id") ? global._mod_pending_effect_id : "";
destroy_timer = -1;
_mod_initialized = false;
_mod_vm = undefined;
image_speed = 0;

// 如果创建前已经通过 global._mod_pending_effect_id 指定类型，则立即初始化
if (mod_type != "" && variable_global_exists("mod_effect_vms") && ds_map_exists(global.mod_effect_vms, mod_type)) {
  _mod_vm = global.mod_effect_vms[? mod_type];
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