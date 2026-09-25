if global.is_paused {
	exit
}

// ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用，实现在 src_mod.gml）──
mod_base_tick(id);

// 跟随放置它的玩家
if (instance_exists(parent_player)) {
	depth = parent_player.depth - 1
	x = parent_player.x - 10
	y = parent_player.y - 100
	grid_row = parent_player.grid_row
	grid_col = parent_player.grid_col
}

// 动画自动播放：与 obj_card_parent 同款，配置属性（flash_speed/idle_anim/attack_anim/state）
// 由 .bin 在 _OBJECT_CREATE/_OBJECT_STEP 里通过 VM_SetProp 设置
if (timer < flash_speed - 1) {
	timer++;
} else {
	switch (state) {
		case CARD_STATE.IDLE:
			if (image_index < idle_anim) {
				image_index++
			} else {
				image_index = 0;
			}
			break;
		case CARD_STATE.ATTACK:
			if (image_index >= (idle_anim + 1) && image_index <= (idle_anim + attack_anim)) image_index++;
			else image_index = (idle_anim + 1);
			break;
	}
	timer = 0;
}

// ══════════════════════════════════════════════════════════════════════
// 1) 一轮计时：每帧 +1，到一轮长度归零
//    一轮长度 = mod_tick_max；不设就用这把武器的 cycle
// ══════════════════════════════════════════════════════════════════════
var _tmax = mod_tick_max;
if (_tmax <= 0) _tmax = cycle;
if (_tmax <= 0) _tmax = 1;

mod_tick_cycle += 1;
if (mod_tick_cycle >= _tmax) {
	mod_tick_cycle = 0;
}

// ══════════════════════════════════════════════════════════════════════
// 2) 索敌（按需）：条件 "norm_attack" 或武器写了 mod_enemy_check = 1 才算
//    （和 obj_card_mod 同一套区域前缀和，口径完全一致）
// ══════════════════════════════════════════════════════════════════════
if (mod_enemy_check == 1 || mod_step_enter_condition == "norm_attack") {
	var _area   = mod_step_enemy_area;
	var _groups = 0;
	if (is_array(_area)) _groups = floor(array_length(_area) / 4);
	if (_groups < 1) {
		_area   = [1, 1, 0, 99];
		_groups = 1;
	}

	// 要看的类型（空 = 任意）
	var _ty  = mod_enemy_types;
	var _any = (!is_array(_ty) || array_length(_ty) == 0);
	var _w_n = _any; var _w_o = _any; var _w_d = _any;
	var _w_a = _any; var _w_c = _any; var _w_u = _any;
	if (!_any) {
		for (var _k = 0; _k < array_length(_ty); _k++) {
			var _t = _ty[_k];
			if (_t == "" || _t == "all") { _w_n = true; _w_o = true; _w_d = true; _w_a = true; _w_c = true; _w_u = true; }
			else if (_t == "normal")      _w_n = true;
			else if (_t == "obstacle")    _w_o = true;
			else if (_t == "diver")       _w_d = true;
			else if (_t == "air")         _w_a = true;
			else if (_t == "dance")       _w_c = true;
			else if (_t == "underground") _w_u = true;
		}
		// 一个类型都没认出来（写错了）→ 按"任意类型"兜底，别把武器永远卡死
		if (_w_n == false && _w_o == false && _w_d == false && _w_a == false && _w_c == false && _w_u == false) {
			_w_n = true; _w_o = true; _w_d = true; _w_a = true; _w_c = true; _w_u = true;
		}
	}

	mod_has_enemy = 0;
	if (variable_global_exists("has_enemy_normal") && grid_row >= 0 && grid_col >= 0) {
		var _stride = global.grid_cols + 2;
		for (var _g = 0; _g < _groups; _g++) {
			var _gi = _g * 4;
			var _up    = _area[_gi];
			var _down  = _area[_gi + 1];
			var _left  = _area[_gi + 2];
			var _right = _area[_gi + 3];

			var _r1 = grid_row - _up;    if (_r1 < 0) _r1 = 0;
			var _r2 = grid_row + _down;  if (_r2 > global.grid_rows - 1) _r2 = global.grid_rows - 1;
			var _c1raw = grid_col - _left;                       // 可能是负数（场外左列 -1 / -2 …）
			var _c1 = _c1raw;            if (_c1 < 0) _c1 = 0;
			var _c2 = grid_col + _right; if (_c2 > global.grid_cols + 1) _c2 = global.grid_cols + 1;
			var _before = _c1 - 1;   // -1 = 左边界外，前缀和按 0 算
			if (array_length(global.has_enemy_normal) > _r2 * _stride + _c2) {
				for (var _r = _r1; _r <= _r2; _r++) {
					var _b = _r * _stride;
					var _n = global.has_enemy_normal[_b + _c2]      - ((_before >= 0) ? global.has_enemy_normal[_b + _before]      : 0);
					var _o = global.has_enemy_obstacle[_b + _c2]    - ((_before >= 0) ? global.has_enemy_obstacle[_b + _before]    : 0);
					var _d = global.has_enemy_diver[_b + _c2]       - ((_before >= 0) ? global.has_enemy_diver[_b + _before]       : 0);
					var _a = global.has_enemy_air[_b + _c2]         - ((_before >= 0) ? global.has_enemy_air[_b + _before]         : 0);
					var _c = global.has_enemy_dance[_b + _c2]       - ((_before >= 0) ? global.has_enemy_dance[_b + _before]       : 0);
					var _u = global.has_enemy_underground[_b + _c2] - ((_before >= 0) ? global.has_enemy_underground[_b + _before] : 0);
					if ((_w_n && _n > 0) || (_w_o && _o > 0) || (_w_d && _d > 0) ||
					    (_w_a && _a > 0) || (_w_c && _c > 0) || (_w_u && _u > 0)) {
						mod_has_enemy = 1;
						break;
					}
				}
			}
			// 场外左列（-1 / -2 …）：has_enemy_* 只统计 grid_col >= 0，负列敌人被 obj_battle
			// 按行收在 enemy_array_left[row] 里，所以窗口越过左边界时要另查一遍
			if (mod_has_enemy == 0 && _c1raw < 0 && variable_global_exists("enemy_array_left")) {
				var _wmask = 0;
				if (_w_n) _wmask = _wmask | HIT_NORMAL;
				if (_w_o) _wmask = _wmask | HIT_OBSTACLE;
				if (_w_d) _wmask = _wmask | HIT_DIVER;
				if (_w_a) _wmask = _wmask | HIT_AIR;
				if (_w_c) _wmask = _wmask | HIT_DANCE;
				if (_w_u) _wmask = _wmask | HIT_UNDERGROUND;
				for (var _lr = _r1; _lr <= _r2; _lr++) {
					if (_lr >= array_length(global.enemy_array_left)) break;
					var _llst = global.enemy_array_left[_lr];
					var _ln   = array_length(_llst);
					for (var _lk = 0; _lk < _ln; _lk++) {
						var _le = _llst[_lk];
						if (!instance_exists(_le) || _le.hp <= 0) continue;
						if (_le.grid_col < _c1raw) continue;      // 比窗口左界还远的不算
						if ((_le.tbit & _wmask) == 0) continue;   // 类型不在窗口里
						mod_has_enemy = 1;
						break;
					}
					if (mod_has_enemy == 1) break;
				}
			}
			if (mod_has_enemy == 1) break;
		}
	}

	// 没敌人 → 顺手把攻击动画收回待机（只在自己索敌的武器上做，且只在 IDLE/ATTACK 之间切）
	if (mod_has_enemy == 0 && (state == CARD_STATE.IDLE || state == CARD_STATE.ATTACK)) {
		state = CARD_STATE.IDLE;
	}
}

// ── 敌人计时器：有敌人才走，没敌人清 0（节奏重数）──
if (mod_has_enemy) {
	mod_tick_enemy += 1;
	if (mod_tick_enemy >= _tmax) {
		mod_tick_enemy = 0;
	}
} else {
	mod_tick_enemy = 0;
}

// ══════════════════════════════════════════════════════════════════════
// 3) 这一帧要不要进 VM —— mod_step_enter_condition
// ══════════════════════════════════════════════════════════════════════
var _cond  = mod_step_enter_condition;
var _enter = true;
mod_step_enter_index = -1;

if (_cond == "mod") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0 && instance_exists(obj_battle)) {
		_enter = ((obj_battle.battle_time mod mod_step_enter_var) == 0);
	}

} else if (_cond == "norm" || _cond == "norm_attack") {
	if (_cond == "norm_attack" && !mod_has_enemy) {
		_enter = false;                       // 没敌人：不进
	} else if (is_array(mod_step_enter_arr) && array_length(mod_step_enter_arr) > 0) {
		_enter = false;
		for (var _i = 0; _i < array_length(mod_step_enter_arr); _i++) {
			var _v = mod_step_enter_arr[_i];
			if (is_string(_v)) {              // 表里写了字符串（写错）→ 兜底每帧进
				_enter = true;
				break;
			}
			if (_v < 0) _v = _tmax + _v;      // 负数 = 从一轮末尾倒数
			if (mod_tick_cycle == _v) {
				_enter = true;
				mod_step_enter_index = _i;    // 告诉武器：这是第几个窗口
				break;
			}
		}
	}

} else if (_cond == "wait") {
	if (is_real(mod_step_enter_var) && mod_step_enter_var > 0) {
		mod_step_enter_var -= 1;
		_enter = (mod_step_enter_var <= 0);
	} else {
		mod_step_enter_condition = "";        // 不合法：进一次 + 清空
	}
}

// 命中开火窗口 + 确实有敌人 → 进攻击动画
if (mod_step_enter_index >= 0 && mod_has_enemy &&
    (state == CARD_STATE.IDLE || state == CARD_STATE.ATTACK)) {
	state = CARD_STATE.ATTACK;
}

// 每个实例每帧执行该武器虚拟机里的步进块（单点调用），逻辑全部在 .bin 里
if (_enter && weapon_id != "" && variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, weapon_id)) {
	var _vm = global.mod_weapon_vms[? weapon_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_STEP")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}

// ══════════════════════════════════════════════════════════════════════
// 偏移：每帧结束时把这两个量加到 x/y 上（初始都是 0 = 不偏移）
// mod 武器脚本用 VM_SetProp(self, "mod_prestep_dx", 值) / ("mod_prestep_dy", 值) 改
// 上面「跟随 parent_player」那段每帧会把 x/y 重置成 player.x-10 / player.y-100，
// 所以偏移必须加在它后面，否则会被覆盖掉
// ⚠️ 加在 VM 之后 = 只影响绘制和别的对象读到的位置；
//    _OBJECT_STEP 块里读到的 x/y 还是没加偏移的基准值
// ══════════════════════════════════════════════════════════════════════
x += mod_prestep_dx;
y += mod_prestep_dy;
