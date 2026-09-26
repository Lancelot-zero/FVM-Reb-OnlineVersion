if global.is_paused {
  exit
}

// ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用，实现在 src_mod.gml）──
mod_base_tick(id);

// ══════════════════════════════════════════════════════════════════════
// bullet_collide = 0：把碰撞掩码换成一个"空掩码"精灵 → 引擎**完全跳过**它的碰撞检测
//   （比在碰撞事件里提前退出快：那样每帧还是要做重叠检测）
//   空掩码精灵是运行时造的，缓存到 global._mod_nomask_spr，全场共用一张
//   恢复：bullet_collide 改回 1 时把 mask_index 复位（-1 = 用 sprite_index）
// ══════════════════════════════════════════════════════════════════════
if (bullet_collide == 0) {
  if (!variable_global_exists("_mod_nomask_spr") || global._mod_nomask_spr == -1 || !sprite_exists(global._mod_nomask_spr)) {
    try {
      var _nm = sprite_add(working_directory + "_ph_empty.png", 1, false, false, 0, 0);
      // 签名：sprite_collision_mask(精灵, sepmasks, bboxmode, bbleft, bbtop, bbright, bbbottom, kind, tolerance)
      //   sepmasks = false（所有子图同一遮罩）
      //   bboxmode = 2（自定义）；四个 bbox 全 0 = **零面积** → 和任何东西都不重叠
      //   kind = 0（矩形 bboxkind_rectangular）；tolerance = 0
      sprite_collision_mask(_nm, false, 2, 0, 0, 0, 0, 0, 0);
      global._mod_nomask_spr = _nm;
    } catch (_e) {
      global._mod_nomask_spr = -1;   // 这个版本没有 sprite_collision_mask：退回"事件里提前退出"（见碰撞事件）
    }
  }
  if (global._mod_nomask_spr != -1 && mask_index != global._mod_nomask_spr) mask_index = global._mod_nomask_spr;
} else if (variable_global_exists("_mod_nomask_spr") && global._mod_nomask_spr != -1 && mask_index == global._mod_nomask_spr) {
  mask_index = -1;   // 关掉开关就恢复成"用自家贴图的掩码"
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
  // 子弹只计数、不进 instances 列表（见 Draw_0.gml）

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
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 | "cell" 跨格 | "cell_row" 跨行
//   | "cell_target_row" 在目标行里 | "cell_target_col" 在目标列里
//   | "attack" 场上还有敌人 | "attack_cell" 当前这一格有敌人 | "attack_collision" 撞到敌人
// ══════════════════════════════════════════════════════════════════════
var _enter = true;
var _cond  = mod_step_enter_condition;
// ── 盯梢 mod_watch：声明的属性里哪个值变了，就记进 mod_watch_changed ──
//    下面每有一个变化就各进一次 _OBJECT_STEP（mod_changed_prop = 那个属性名），条件符合再进一次
//    名字先当**本实例属性**读；实例上没有、但有同名**全局变量**时，就当全局读
mod_changed_prop = "";
array_resize(mod_watch_changed, 0);
if (_mod_initialized && variable_struct_exists(_mod_vm, "arrays")) {
	var _wvm = _mod_vm;
	if (ds_map_exists(_wvm.arrays, "mod_watch")) {
		var _wlist = _wvm.arrays[? "mod_watch"];
		if (is_array(_wlist)) {
			var _wn = array_length(_wlist);
			if (array_length(mod_watch_last) != _wn) {
				// 列表长度变了（刚声明 / 改了）：这一帧只对齐快照，不报变化
				array_resize(mod_watch_last, _wn);
				for (var _wi = 0; _wi < _wn; _wi++) {
					var _wv0 = variable_instance_get(id, _wlist[_wi]);
					if (is_undefined(_wv0) && variable_global_exists(_wlist[_wi])) _wv0 = variable_global_get(_wlist[_wi]);   // 名字不在实例上 → 当全局读
					mod_watch_last[_wi] = _wv0;
				}
			} else {
				for (var _wi = 0; _wi < _wn; _wi++) {
					var _wv = variable_instance_get(id, _wlist[_wi]);
					if (is_undefined(_wv) && variable_global_exists(_wlist[_wi])) _wv = variable_global_get(_wlist[_wi]);   // 名字不在实例上 → 当全局读
					if (_wv != mod_watch_last[_wi]) {
						array_push(mod_watch_changed, _wlist[_wi]);   // 每个变化的属性各进一次 Step
						mod_watch_last[_wi] = _wv;
					}
				}
			}
		}
	}
}

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

} else if (_cond == "cell_target_row" || _cond == "cell_target_col") {
	// **在**目标所在行 / 列里就进（不是"跨过去那一帧"，是在着就一直进）
	//   目标：优先 bullet_target 实例（用它的 grid_row/grid_col），
	//         否则用 bullet_target_x / bullet_target_y 换算格子；
	//         两个都没设（实例 -1 且坐标都是 0）→ 永不进
	var _has_t = false;
	var _trow = 0;
	var _tcol = 0;
	if (bullet_target != -1 && instance_exists(bullet_target)) {
		_trow = bullet_target.grid_row;
		_tcol = bullet_target.grid_col;
		_has_t = true;
	} else if (bullet_target_x != 0 || bullet_target_y != 0) {
		var _tgp = get_grid_position_from_world(bullet_target_x, bullet_target_y, true);
		_trow = _tgp.row;
		_tcol = _tgp.col;
		_has_t = true;
	}
	if (_has_t) {
		if (_cond == "cell_target_row") _enter = (grid_row == _trow);
		else                           _enter = (grid_col == _tcol);
	} else {
		_enter = false;
	}

} else if (_cond == "attack") {
	// 只看"场上还有没有敌人"：子弹多，不做卡那套区域前缀和
	mod_has_enemy = (instance_number(obj_enemy_parent) > 0);
	_enter = mod_has_enemy;

} else if (_cond == "attack_collision") {
	// 撞到敌人了才进（碰撞事件把撞到的敌人攒进 mod_collision_enemies）
	//   一帧撞几个都在这一个数组里 → 只进一次 VM，脚本自己遍历
	//   1 次碰撞和 N 次碰撞都只进一次；进完 VM 后列表自动清空
	_enter = (array_length(mod_collision_enemies) > 0);

} else if (_cond == "attack_cell") {
	// 只看"子弹当前这一格有没有敌人"（用 obj_battle 每帧重建的敌人表 global.enemy_array）
	//   比 "attack" 精确：只有飞到有敌人的格子才进 VM —— 路径碰撞 / 命中特效最省
	//   出界到左边（grid_col < 0）时查 enemy_array_left[row]
	mod_has_enemy = 0;
	if (variable_global_exists("enemy_array") && variable_global_exists("grid_cols")
		&& grid_row >= 0 && grid_row < global.grid_rows) {
		var _stride = global.grid_cols + 2;
		if (grid_col >= 0 && grid_col < _stride) {
			var _idx = grid_row * _stride + grid_col;
			if (_idx >= 0 && _idx < array_length(global.enemy_array)) {
				var _cell = global.enemy_array[_idx];
				if (is_array(_cell)) mod_has_enemy = (array_length(_cell) > 0);
			}
		} else if (grid_col < 0 && variable_global_exists("enemy_array_left")) {
			if (grid_row < array_length(global.enemy_array_left)) {
				var _left_cell = global.enemy_array_left[grid_row];
				if (is_array(_left_cell)) mod_has_enemy = (array_length(_left_cell) > 0);
			}
		}
	}
	_enter = (mod_has_enemy == 1);
}

// 执行子弹自己的 _OBJECT_STEP
// 这一帧进几次：watch 每个变化的属性各 1 次，条件符合再 1 次
var _run_total = array_length(mod_watch_changed) + (_enter ? 1 : 0);
for (var _run_k = 0; _run_k < _run_total; _run_k++) {
	if (!instance_exists(id)) break;
	mod_changed_prop = (_run_k < array_length(mod_watch_changed)) ? mod_watch_changed[_run_k] : "";
	if (_mod_initialized && ds_map_exists(_mod_vm.blocks, "_OBJECT_STEP")) {
  var _bak_cur = global._VM_cur_card;
  global._VM_cur_card = id;
  var _bak_vm = global.__vm;
  global.__vm = _mod_vm;
  VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
  global.__vm = _bak_vm;
  global._VM_cur_card = _bak_cur;
}
}

if (!instance_exists(id)) exit;

// 碰撞列表用完就清：这一帧的 VM 已经有机会读过它了（不管用的是哪种闸门）
// ⚠️ 无条件清：只清 "attack_collision" 的话，用 attack_cell / attack / "" 的子弹
//    撞一次就往里堆一次、谁也不读，白白涨内存
array_resize(mod_collision_enemies, 0);
mod_collision_hit = 0;

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

// ══════════════════════════════════════════════════════════════════════
// 抛物线（bullet_parabola = 1）：覆盖上面的直线位移，按弧线飞向目标
//   起点/目标只在**起飞那一帧**取一次；飞完停在目标点（vx/vy 清零）
// ══════════════════════════════════════════════════════════════════════
if (bullet_parabola == 1) {
  if (bullet_para_t < 0) {
    bullet_para_x0 = x;
    bullet_para_y0 = y;
    if (bullet_target != -1 && instance_exists(bullet_target)) {
      bullet_target_x = bullet_target.x;
      bullet_target_y = bullet_target.y;
    }
    bullet_para_t    = 0;
    bullet_para_dist = point_distance(bullet_para_x0, bullet_para_y0, bullet_target_x, bullet_target_y);
    if (bullet_parabola_h < 0) bullet_parabola_h = bullet_para_dist * 0.25;   // 没写弧高就按距离自动
  }

  bullet_para_t += 1;
  var _pt = max(1, bullet_parabola_time);
  var _pf = min(1, bullet_para_t / _pt);
  x = lerp(bullet_para_x0, bullet_target_x, _pf);
  y = lerp(bullet_para_y0, bullet_target_y, _pf) - bullet_parabola_h * sin(pi * _pf);

  if (bullet_para_t >= _pt) {
    // 落地：停在目标点，交给碰撞结算 / destroy_timer / 脚本处理
    x = bullet_target_x;
    y = bullet_target_y;
    vx = 0;
    vy = 0;
    bullet_parabola = 0;
    bullet_para_t   = -1;
  }
}

// ══════════════════════════════════════════════════════════════════════
// 路径数组：插件用 VM 命名数组 dx_arr / dy_arr 描述"每帧位移"（见 Create_0.gml）
//   x/y 各加上第 (bullet_path_i mod 长度) 项，然后计数器 +1；两串长度不一致时**按短的算**。
//   放在 vx/vy 和抛物线**之后** → 是叠加，不会覆盖别的移动（不用路径就留空数组，行为不变）。
// ══════════════════════════════════════════════════════════════════════
if (_mod_initialized && variable_struct_exists(_mod_vm, "arrays")) {
  var _dxa = ds_map_exists(_mod_vm.arrays, "dx_arr") ? _mod_vm.arrays[? "dx_arr"] : undefined;
  var _dya = ds_map_exists(_mod_vm.arrays, "dy_arr") ? _mod_vm.arrays[? "dy_arr"] : undefined;
  if (is_array(_dxa) && is_array(_dya)) {
    var _plen = min(array_length(_dxa), array_length(_dya));
    if (_plen > 0) {
      var _pk = bullet_path_i mod _plen;
      x += _dxa[_pk];
      y += _dya[_pk];
      bullet_path_i += 1;
    }
  }
}

if x > 2200 or y > 1200 or x < 0 or y < 0 {
    instance_destroy();
}