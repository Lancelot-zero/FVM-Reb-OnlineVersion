if global.is_paused{
	exit
}

// ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用，实现在 src_mod.gml）──
mod_base_tick(id);

// 记录上一帧格子坐标：下面 event_inherited() 里父类 Step 会重算 grid_col/grid_row
prev_grid_col = grid_col;
prev_grid_row = grid_row;

// enemy_id 晚到（联机：服务器建完后补发 MSG_MODIFY_PROP）：补跑一次初始化
if (enemy_id != "" && mod_enemy_inited_id != enemy_id) {
	mod_enemy_init();
	// 非法 mod 敌人已被换成普通老鼠（本实例已销毁）→ 本帧 Step 到此为止，别再跑父类
	if (!instance_exists(id)) exit;
}

// 父类处理移动/攻击/死亡等基础行为（同时刷新 target_plant / grid_col / grid_row）
event_inherited();

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口闸门
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0 | "cell" 跨格 | "hp_change" 血量变
//   | "hp_change_mod" 血量变+冷却 | "card" 有索敌目标 | "card_mod" 有目标+冷却
// ══════════════════════════════════════════════════════════════════════════
var _enter = true;
var _cond  = mod_step_enter_condition;
// ── 盯梢 mod_watch：声明的属性里哪个值变了，就记进 mod_watch_changed ──
//    下面每有一个变化就各进一次 _OBJECT_STEP（mod_changed_prop = 那个属性名），条件符合再进一次
//    名字先当**本实例属性**读；实例上没有、但有同名**全局变量**时，就当全局读
mod_changed_prop = "";
array_resize(mod_watch_changed, 0);
if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _wvm = global.mod_enemy_vms[? enemy_id];
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
	_enter = (grid_col != prev_grid_col || grid_row != prev_grid_row);

} else if (_cond == "hp_change") {
	// 父类的 pre_hp 在 End Step 才更新，这里读到的是上一帧末的血量
	_enter = (pre_hp != hp);

} else if (_cond == "hp_change_mod") {
	_cdv = mod_step_enter_var;
	if (!is_real(_cdv) || _cdv < 0) _cdv = 0;      // 没写 / 写错 → 无冷却
	if (mod_tick_cool > 0) {
		mod_tick_cool -= 1;
		_enter = false;
	} else if (pre_hp != hp) {
		_enter = true;
		mod_tick_cool = _cdv;
	} else {
		_enter = false;
	}

} else if (_cond == "card" || _cond == "card_mod") {
	mod_has_card = instance_exists(target_plant);   // 父类索敌结果
	if (_cond == "card") {
		_enter = mod_has_card;
	} else {
		_cdv = mod_step_enter_var;
		if (!is_real(_cdv) || _cdv < 0) _cdv = 0;
		if (mod_tick_cool > 0) {
			mod_tick_cool -= 1;
			_enter = false;
		} else if (mod_has_card) {
			_enter = true;
			mod_tick_cool = _cdv;
		} else {
			_enter = false;
		}
	}
}

// 每个实例每帧执行该敌人虚拟机里的步进块（单点调用），特殊逻辑全部在 .bin 里
// 这一帧进几次：watch 每个变化的属性各 1 次，条件符合再 1 次
var _run_total = array_length(mod_watch_changed) + (_enter ? 1 : 0);
for (var _run_k = 0; _run_k < _run_total; _run_k++) {
	if (!instance_exists(id)) break;
	mod_changed_prop = (_run_k < array_length(mod_watch_changed)) ? mod_watch_changed[_run_k] : "";
	if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
	var _vm = global.mod_enemy_vms[? enemy_id];
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
}
