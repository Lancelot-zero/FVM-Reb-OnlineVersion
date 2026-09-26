// 通用 mod 特效对象
// 由插件 .bin 通过 VM_CreateInstance("obj_effect_mod", x, y) 创建，
// 然后用 VM_SetProp(inst, "mod_type", "特效类型名") 指定逻辑。
// destroy_timer: -1=不自动销毁, 0=立即销毁, >0=每帧减1，到0销毁
mod_type = variable_global_exists("_mod_pending_effect_id") ? global._mod_pending_effect_id : "";
destroy_timer = -1;
_mod_initialized = false;
_mod_vm = undefined;
image_speed = 0;

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（可选；不写 = 每帧进）
//   mod_step_enter_condition
//     "" / 认不出    每帧进（默认）
//     "mod"          按全场帧号对齐：battle_time % mod_step_enter_var == 0
//     "wait"         倒数：mod_step_enter_var 每帧 -1，减到 <= 0 进
//                    ⚠️ 进完之后要把 var 设回正数，否则此后每帧都进
//   mod_step_enter_var   "mod" = 间隔帧数；"wait" = 还要等几帧
// ⚠️ 特效没有 hp、也不跟网格走（没有 grid_col/grid_row），
//    所以 hp_change、cell、索敌那几类条件这里都没有
// ══════════════════════════════════════════════════════════════════════════
mod_step_enter_condition = ""   // "" / "mod" / "wait"
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧

// 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
mod_countdown = 0    // 倒计时（帧）：> 0 时每帧 -1
mod_alpha_add = 0    // 每帧加到 image_alpha 上的量（负数 = 渐隐）；只在倒计时 > 0 时加

// 偏移：每帧 Step 结束时把这两个量加到 x/y 上；0 = 不动
// mod 特效脚本用 VM_SetProp(self, "mod_prestep_dx", 值) 改
// ⚠️ 和 obj_weapon_mod 不同：武器每帧会按 parent_player 重置 x/y，所以那边是「固定偏移」；
//    特效的 x/y 没人重置，每帧加一次 = 匀速移动（dx/dy 就是每帧位移量）
//    —— 但设了 mod_follow_instance 之后 x/y 每帧会被跟随重置，这两个量又变回「固定偏移」
mod_prestep_dx = 0
mod_prestep_dy = 0

// 跟随目标：设成一个实例（VM 给的实例 id 也能直接用），本特效的 x/y 每帧贴到它身上
// undefined / 目标已销毁 = 不跟随（保持自己原来的 x/y）
// mod 特效脚本用 VM_SetProp(self, "mod_follow_instance", 目标) 改
mod_follow_instance = undefined

// 如果创建前已经通过 global._mod_pending_effect_id 指定类型，则立即初始化
if (mod_type != "" && variable_global_exists("mod_effect_vms") && ds_map_exists(global.mod_effect_vms, mod_type)) {
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