// 通用 mod 子弹对象
// 由插件 .bin 通过 VM_CreateInstance("obj_bullet_mod", x, y) 创建，
// 然后用 VM_SetProp(inst, "mod_type", "子弹类型名") 指定逻辑。
// destroy_timer: -1=不自动销毁, 0=立即销毁, >0=每帧减1，到0销毁
mod_type = variable_global_exists("_mod_pending_bullet_id") ? global._mod_pending_bullet_id : "";
destroy_timer = -1;
_mod_initialized = false;
_mod_vm = undefined;
image_speed = 0;
vector_x = 0
vx = 0
vy = 0

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（卡在创建后用 VM_SetProp 写；不写 = 每帧进）
//   mod_step_enter_condition
//     "" / 认不出     每帧进（默认）
//     "mod"           按全场帧号对齐：battle_time % mod_step_enter_var == 0
//     "wait"          倒数：mod_step_enter_var 每帧 -1，减到 <= 0 进
//                      ⚠️ 进完之后要卡自己把 var 设回正数，否则此后每帧都进
//     "cell"          跨格才进：grid_col 或 grid_row 相对上一帧变了
//                     （两个值 obj 每帧已经算好，零成本；子弹最常用这个）
//                     ⚠️ 时机：格子坐标是在 x += vx 之前算的，所以触发的是
//                        "进格后的第一帧"，比实际越界晚一帧；一帧跨多格也只触发一次
//     "cell_row"      只跨行才进（grid_row 变了）；直线弹用不上，抛物弹有用
//     "attack"        场上还有敌人才进（全局判定，不看具体位置）
//   mod_step_enter_var   "mod" = 间隔帧数；"wait" = 还要等几帧
// obj 每帧回写（卡只读）：
//   mod_has_enemy    "attack" 模式下：1 = 场上还有敌人（其他模式恒为 0）
//   grid_col / grid_row / prev_grid_col / prev_grid_row  当前帧 / 上一帧格子坐标
// ══════════════════════════════════════════════════════════════════════════
mod_step_enter_condition = ""   // "" / "mod" / "wait" / "cell" / "cell_row" / "attack"
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧
mod_has_enemy            = 0    // 【只读】"attack" 模式下 1 = 场上还有敌人


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
  //ds_list_add(_mod_vm.instances, id);

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