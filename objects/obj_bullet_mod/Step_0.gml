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

// ══════════════════════════════════════════════════════════════════════
// 进 VM 的入口闸门 —— 和 obj_card_mod 同款，但去掉子弹用不上的（没有 hp / 没有索敌区域）
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 | "cell" 跨格 | "cell_row" 跨行 | "attack" 有敌人
// ══════════════════════════════════════════════════════════════════════
var _enter = true;
var _cond  = mod_step_enter_condition;

if (_cond == "mod") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0 && instance_exists(obj_battle)) {
		_enter = ((obj_battle.battle_time mod mod_step_enter_var) == 0);
	}

} else if (_cond == "wait") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0) {
		mod_step_enter_var -= 1;
		_enter = (mod_step_enter_var <= 0);
	} else {
		mod_step_enter_condition = "";        // 不合法：进一次 + 清空
	}

} else if (_cond == "cell") {
	// 跨格那一帧才进。上面 6~10 行已经把 prev_* 写成上一帧、grid_* 写成当前帧
	_enter = (grid_col != prev_grid_col || grid_row != prev_grid_row);

} else if (_cond == "cell_row") {
	_enter = (grid_row != prev_grid_row);

} else if (_cond == "attack") {
	// 只看"场上还有没有敌人"：子弹多，不做卡那套区域前缀和
	mod_has_enemy = (instance_number(obj_enemy_parent) > 0);
	_enter = mod_has_enemy;
}

// 执行子弹自己的 _OBJECT_STEP
if (_enter && _mod_initialized && ds_map_exists(_mod_vm.blocks, "_OBJECT_STEP")) {
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

x += vx
y += vy
if x > 2200 or y > 1200 or x < 0 or y < 0 {
    instance_destroy();
}