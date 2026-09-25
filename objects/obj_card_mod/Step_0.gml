if global.is_paused{
	exit
}

// 每个实例每帧执行该卡虚拟机里的步进块（单点调用）
event_inherited();

// 父对象 Step 里 hp<=0 会销毁本实例。实例一旦没了，
// 块内 VM_GetProp(self, ...) 会返回 undefined，接下去做算术就是 DoAdd 报错。
if (!instance_exists(id)) exit;
if (hp <= 0) exit;

// 冻住：计时不动，也不进 VM（原来这是每张卡自己在 STEP 第一句写的）
if (is_frozen) exit;

// ══════════════════════════════════════════════════════════════════════
// 1) 一轮计时：每帧 +1，到一轮长度归零
//    一轮长度 = mod_tick_max；不设就用卡的 cycle（技能改 cycle 会自动跟上）
// ══════════════════════════════════════════════════════════════════════
var _tmax = mod_tick_max;
if (_tmax <= 0) _tmax = cycle;
if (_tmax <= 0) _tmax = 1;

mod_tick_cycle += 1;
if (mod_tick_cycle >= _tmax) {
	mod_tick_cycle = 0;
}

// ══════════════════════════════════════════════════════════════════════
// 2) 索敌（按需）：条件 "norm_attack" 或卡写了 mod_enemy_check = 1 才算，别的卡一次不算
//    区域 mod_step_enemy_area（每 4 个值一组 [上,下,左,右]，可写多组，任一组命中即可）
//    类型 mod_enemy_types（空 = 任意）
//    实现：global.has_enemy_* 每行前缀和（obj_battle 每帧重建）
// ══════════════════════════════════════════════════════════════════════
if (mod_enemy_check == 1 || mod_step_enter_condition == "norm_attack") {
	// 索敌区域：每 4 个值一组 [上, 下, 左, 右]，可以写多组
	//   4 个值 = 1 个区域、8 个值 = 2 个区域、12 个 = 3 个……任一组命中就算"有敌人"
	//   凑不满一组（<4 个）→ 用默认区域 [1,1,0,99]（本行 ±1、自己那格往右）
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
		// 一个类型都没认出来（写错了）→ 按"任意类型"兜底，别把卡永远卡死
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

	// 没敌人 → 顺手把攻击动画收回待机（只在自己索敌的卡上做，且只在 IDLE/ATTACK 之间切，
	// 不碰卡片自定义状态如睡眠），这样卡不用为了收招专门进一次 VM
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
//    ""            每帧进（默认；没设 / 写错都按这个兜底）
//    "mod"         battle_time % mod_step_enter_var == 0（全场共享，同 var 的卡同帧进）
//    "norm"        mod_tick_cycle 命中窗口表（不看敌人）
//    "norm_attack" 有敌人 且 命中窗口表；窗口表空 = 有敌人就每帧进（类型看 mod_enemy_types）
//    "wait"        mod_step_enter_var 每帧自减 1，减到 0 进；值非法 → 进一次 + 清空条件
//    "hp_change"   血量相对上一帧变了才进（掉血、回血都算；出生时基线 = 当前血量）
//    "hp_change_mod" 同 "hp_change"，但触发后进 mod_step_enter_var 帧冷却，冷却内再掉血也不进
//    窗口表 mod_step_enter_arr：负数 = 从一轮末尾倒数（-35 → 一轮长度-35）
//    命中窗口 → mod_step_enter_index = 窗口下标（0 起），否则 -1
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
		_enter = false;                       // 没敌人：不进（obj 自己知道，不用问卡）
	} else if (is_array(mod_step_enter_arr) && array_length(mod_step_enter_arr) > 0) {
		_enter = false;
		for (var _i = 0; _i < array_length(mod_step_enter_arr); _i++) {
			var _v = mod_step_enter_arr[_i];
			if (is_string(_v)) {              // 表里写了字符串（写错）→ 兜底每帧进，别把卡永远卡死
				_enter = true;
				break;
			}
			if (_v < 0) _v = _tmax + _v;      // 负数 = 从一轮末尾倒数
			if (mod_tick_cycle == _v) {
				_enter = true;
				mod_step_enter_index = _i;    // 告诉卡：这是第几个窗口
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

} else if (_cond == "hp_change") {
	// 血量相对上一帧变了才进（掉血、回血都算，只要 != 就触发）
	// 基线每帧无条件跟上：见过一次之后要立刻归位，不然下一帧还是"变了"，就退化成每帧进
	_enter = (hp != mod_hp_last);
	mod_hp_last = hp;

} else if (_cond == "hp_change_mod") {
	// 同 "hp_change"，但触发一次后进冷却：mod_step_enter_var 帧内再掉血也不再进
	var _cdv = mod_step_enter_var;
	if (!is_real(_cdv) || _cdv < 0) _cdv = 0;   // 没写 / 写错 → 无冷却，等同 "hp_change"

	if (mod_tick_cool > 0) {
		mod_tick_cool -= 1;
		_enter = false;
	} else if (hp != mod_hp_last) {
		_enter = true;
		mod_tick_cool = _cdv;                   // 触发即上冷却
	} else {
		_enter = false;
	}
	// 基线照样每帧跟上：冷却期内的伤害算吞掉，出冷却后不会被旧变化再骗进一次
	mod_hp_last = hp;
}

// 命中开火窗口 + 确实有敌人 → 进攻击动画
// （卡只管发弹，不用自己写 state；没敌人时上面索敌那段已经收回待机）
if (mod_step_enter_index >= 0 && mod_has_enemy &&
    (state == CARD_STATE.IDLE || state == CARD_STATE.ATTACK)) {
	state = CARD_STATE.ATTACK;
}

// ══════════════════════════════════════════════════════════════════════
// 4) 跑卡的 _OBJECT_STEP
// ══════════════════════════════════════════════════════════════════════
if (_enter && plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_STEP")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;      // 块内 API 取当前实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_STEP"], "_OBJECT_STEP");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}

// ══════════════════════════════════════════════════════════════════════
// 批量改属性：两套，行为一样 —— 开关开着就每帧遍历 id，把第 i 个 id 的
//   属性[i] 设成 值[i]（三个数组按最短长度配对）
//   放在跑 VM 之后，卡这一帧里写好的数组/开关当帧生效
// ══════════════════════════════════════════════════════════════════════
if (mod_set_on == 1) {
	var _n = array_length(mod_set_id);
	var _np = array_length(mod_set_prop);
	var _nv = array_length(mod_set_val);
	if (_np < _n) _n = _np;
	if (_nv < _n) _n = _nv;
	for (var _i = 0; _i < _n; _i++) {
		var _tgt = mod_set_id[_i];
		if (is_real(_tgt) && _tgt < 0) {           // VM 的包装 id → 真实实例 id
			var _real = ds_map_find_value(global._VM_id_to_real, -_tgt);
			if (!is_undefined(_real)) _tgt = _real;
		}
		if (instance_exists(_tgt)) {
			variable_instance_set(_tgt, mod_set_prop[_i], mod_set_val[_i]);
		}
	}
}

if (mod_set2_on == 1) {
	var _n = array_length(mod_set2_id);
	var _np = array_length(mod_set2_prop);
	var _nv = array_length(mod_set2_val);
	if (_np < _n) _n = _np;
	if (_nv < _n) _n = _nv;
	for (var _i = 0; _i < _n; _i++) {
		var _tgt = mod_set2_id[_i];
		if (is_real(_tgt) && _tgt < 0) {
			var _real = ds_map_find_value(global._VM_id_to_real, -_tgt);
			if (!is_undefined(_real)) _tgt = _real;
		}
		if (instance_exists(_tgt)) {
			variable_instance_set(_tgt, mod_set2_prop[_i], mod_set2_val[_i]);
		}
	}
}
