if global.is_paused {
	exit
}

// 登记实例 + 跑该宝石 .bin 的 _OBJECT_CREATE（原来在 Create 里，挪到 Step 顶部）
//   原因：Create 是在 instance_create_depth 里跑的，那时放置逻辑还没写
//         mod_point_parent_player / grid_row / grid_col / gem_level（建完实例紧接着才写）；
//         现在建宝石的地方改成「建实例 → 写 mod_point_* → 调一次步」，
//         所以这一步要在 Step 里做 —— 插件 _OBJECT_CREATE 里才读得到那四个归属字段。
//   没被手动调用步的（插件自己建的宝石）第一次自然 Step 也会在这里初始化。
if (!_mod_initialized && gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
	_mod_initialized = true;
	mod_inst_add(_vm, id);
	if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
		var _bak_last = global._VM_last_created_card;
		global._VM_last_created_card = id;
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		global._VM_last_created_card = _bak_last;
	}
}

// ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用，实现在 src_mod.gml）──
mod_base_tick(id);

// 预留属性 cooldown_timer：每帧自减（点击时由 Mouse_4 设回 cooldown；插件触发时也可以自己重写）
if (cooldown_timer > 0) cooldown_timer--;

// 动画自动播放：与 obj_card_parent 同款，配置属性由 .bin 通过 VM_SetProp 设置
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
// 进 VM 的入口闸门
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 | "cool" 冷却结束（cooldown_timer <= 0）
//   | "enemy_area" 区域内有敌人 | "enemy_area_mod" 同上 + 冷却
//   | "card_area" 区域内有卡片  | "card_area_mod" 同上 + 冷却
// ══════════════════════════════════════════════════════════════════════
var _enter = true;
var _cond  = mod_step_enter_condition;
// ── 盯梢 mod_watch：声明的属性里哪个值变了，就记进 mod_watch_changed ──
//    下面每有一个变化就各进一次 _OBJECT_STEP（mod_changed_prop = 那个属性名），条件符合再进一次
//    名字先当**本实例属性**读；实例上没有、但有同名**全局变量**时，就当全局读
mod_changed_prop = "";
array_resize(mod_watch_changed, 0);
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _wvm = global.mod_gem_vms[? gem_id];
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
var _cdv   = 0;

// ── 区域检测（只有 "enemy_area*" / "card_area*" 会做，其它条件零开销）──
//   锚点格子：优先跟着"放置它的角色"走（mod_point_parent_player），
//             角色不在了就用创建时记下的 mod_point_grid_row / col
if (_cond == "enemy_area" || _cond == "enemy_area_mod"
 || _cond == "card_area"  || _cond == "card_area_mod") {

	var _arow = mod_point_grid_row;
	var _acol = mod_point_grid_col;
	if (mod_point_parent_player != noone && instance_exists(mod_point_parent_player)) {
		_arow = mod_point_parent_player.grid_row;
		_acol = mod_point_parent_player.grid_col;
	}

	// ── 区域内有没有敌人（和卡片那套同一口径：前缀和 + 类型筛选）──
	if (_cond == "enemy_area" || _cond == "enemy_area_mod") {
		mod_has_enemy = 0;
		if (variable_global_exists("has_enemy_normal") && _arow >= 0 && _acol >= 0) {
			var _area = mod_step_enemy_area;
			var _groups = (is_array(_area)) ? floor(array_length(_area) / 4) : 0;
			if (_groups < 1) { _area = [1, 1, 0, 99]; _groups = 1; }

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
				if (_w_n == false && _w_o == false && _w_d == false && _w_a == false && _w_c == false && _w_u == false) {
					_w_n = true; _w_o = true; _w_d = true; _w_a = true; _w_c = true; _w_u = true;
				}
			}

			var _stride = global.grid_cols + 2;
			for (var _g = 0; _g < _groups; _g++) {
				var _gi = _g * 4;
				var _r1 = _arow - _area[_gi];       if (_r1 < 0) _r1 = 0;
				var _r2 = _arow + _area[_gi + 1];   if (_r2 > global.grid_rows - 1) _r2 = global.grid_rows - 1;
				var _c1raw = _acol - _area[_gi + 2];
				var _c1 = _c1raw;                   if (_c1 < 0) _c1 = 0;
				var _c2 = _acol + _area[_gi + 3];   if (_c2 > global.grid_cols + 1) _c2 = global.grid_cols + 1;
				var _before = _c1 - 1;
				if (array_length(global.has_enemy_normal) > _r2 * _stride + _c2) {
					for (var _r = _r1; _r <= _r2; _r++) {
						var _b = _r * _stride;
						var _n = global.has_enemy_normal[_b + _c2]      - ((_before >= 0) ? global.has_enemy_normal[_b + _before]      : 0);
						var _o = global.has_enemy_obstacle[_b + _c2]    - ((_before >= 0) ? global.has_enemy_obstacle[_b + _before]    : 0);
						var _d = global.has_enemy_diver[_b + _c2]       - ((_before >= 0) ? global.has_enemy_diver[_b + _before]       : 0);
						var _a = global.has_enemy_air[_b + _c2]         - ((_before >= 0) ? global.has_enemy_air[_b + _before]         : 0);
						var _cc = global.has_enemy_dance[_b + _c2]      - ((_before >= 0) ? global.has_enemy_dance[_b + _before]      : 0);
						var _u = global.has_enemy_underground[_b + _c2] - ((_before >= 0) ? global.has_enemy_underground[_b + _before] : 0);
						if ((_w_n && _n > 0) || (_w_o && _o > 0) || (_w_d && _d > 0) ||
						    (_w_a && _a > 0) || (_w_c && _cc > 0) || (_w_u && _u > 0)) {
							mod_has_enemy = 1;
							break;
						}
					}
				}
				// 场外左列（-1 / -2 …）的敌人单独收在 enemy_array_left[row] 里
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
							if (_le.grid_col < _c1raw) continue;
							if ((_le.tbit & _wmask) == 0) continue;
							mod_has_enemy = 1;
							break;
						}
						if (mod_has_enemy == 1) break;
					}
				}
				if (mod_has_enemy == 1) break;
			}
		}
	}

	// ── 区域内有没有卡片（玩家底座 / 空位不算）──
	if (_cond == "card_area" || _cond == "card_area_mod") {
		mod_has_card = 0;
		if (variable_global_exists("grid_plants")) {
			var _carea = mod_step_card_area;
			var _cgroups = (is_array(_carea)) ? floor(array_length(_carea) / 4) : 0;
			if (_cgroups < 1) { _carea = [1, 1, 0, 99]; _cgroups = 1; }
			for (var _cg = 0; _cg < _cgroups && mod_has_card == 0; _cg++) {
				var _ci = _cg * 4;
				var _cr1 = _arow - _carea[_ci];      if (_cr1 < 0) _cr1 = 0;
				var _cr2 = _arow + _carea[_ci + 1];  if (_cr2 > global.grid_rows - 1) _cr2 = global.grid_rows - 1;
				var _cc1 = _acol - _carea[_ci + 2];  if (_cc1 < 0) _cc1 = 0;
				var _cc2 = _acol + _carea[_ci + 3];  if (_cc2 > global.grid_cols - 1) _cc2 = global.grid_cols - 1;
				for (var _cr = _cr1; _cr <= _cr2 && mod_has_card == 0; _cr++) {
					for (var _ccx = _cc1; _ccx <= _cc2; _ccx++) {
						var _lst = ds_grid_get(global.grid_plants, _ccx, _cr);
						if (!is_real(_lst) || !ds_exists(_lst, ds_type_list)) continue;
						var _lnum = ds_list_size(_lst);
						for (var _ck = 0; _ck < _lnum; _ck++) {
							var _cd = ds_list_find_value(_lst, _ck);
							if (!instance_exists(_cd)) continue;
							if (!variable_instance_exists(_cd, "plant_id")) continue;
							if (_cd.plant_id == "" || _cd.plant_id == "player") continue;
							mod_has_card = 1;
							break;
						}
						if (mod_has_card == 1) break;
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

} else if (_cond == "cool") {
	_enter = (cooldown_timer <= 0);

} else if (_cond == "enemy_area" || _cond == "enemy_area_mod") {
	// 区域内有敌人就进；"_mod" 版进一次后要等 mod_step_enter_var 帧才能再进
	if (_cond == "enemy_area") {
		_enter = (mod_has_enemy == 1);
	} else {
		_cdv = mod_step_enter_var;
		if (!is_real(_cdv) || _cdv < 0) _cdv = 0;
		if (mod_tick_cool > 0) {
			mod_tick_cool -= 1;
			_enter = false;
		} else if (mod_has_enemy == 1) {
			_enter = true;
			mod_tick_cool = _cdv;
		} else {
			_enter = false;
		}
	}

} else if (_cond == "card_area" || _cond == "card_area_mod") {
	// 区域内有卡片就进；"_mod" 版同上带冷却
	if (_cond == "card_area") {
		_enter = (mod_has_card == 1);
	} else {
		_cdv = mod_step_enter_var;
		if (!is_real(_cdv) || _cdv < 0) _cdv = 0;
		if (mod_tick_cool > 0) {
			mod_tick_cool -= 1;
			_enter = false;
		} else if (mod_has_card == 1) {
			_enter = true;
			mod_tick_cool = _cdv;
		} else {
			_enter = false;
		}
	}
}

// 每个实例每帧执行该宝石虚拟机里的步进块（单点调用），逻辑全部在 .bin 里
// 这一帧进几次：watch 每个变化的属性各 1 次，条件符合再 1 次
var _run_total = array_length(mod_watch_changed) + (_enter ? 1 : 0);
for (var _run_k = 0; _run_k < _run_total; _run_k++) {
	if (!instance_exists(id)) break;
	mod_changed_prop = (_run_k < array_length(mod_watch_changed)) ? mod_watch_changed[_run_k] : "";
	if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
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
}
