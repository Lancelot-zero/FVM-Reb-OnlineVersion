if global.is_paused {
  exit
}

// ── 基础属性：倒计时 + 每帧透明度变化（所有 mod 对象通用，实现在 src_mod.gml）──
mod_base_tick(id);

// ══════════════════════════════════════════════════════════════════════
// 跟随目标：mod_follow_instance 有设且实例还在，就把 x/y 贴到它身上
// 写在最前面，让后面的 _OBJECT_CREATE / _OBJECT_STEP 也读到跟随后的位置
// ══════════════════════════════════════════════════════════════════════
if (!is_undefined(mod_follow_instance) && instance_exists(mod_follow_instance)) {
  x = mod_follow_instance.x;
  y = mod_follow_instance.y;
}

// 延迟初始化：允许创建后再用 VM_SetProp 设置 mod_type
if (!_mod_initialized && mod_type != "" && variable_global_exists("mod_effect_vms") && ds_map_exists(global.mod_effect_vms, mod_type)) {
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

// ══════════════════════════════════════════════════════════════════════
// 进 VM 的入口闸门
//   "" 每帧 | "mod" 每 N 帧 | "wait" 减到 0
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
}

// 执行特效自己的 _OBJECT_STEP
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

// 自动销毁
if (destroy_timer == 0) {
  instance_destroy();
  exit;
}
if (destroy_timer > 0) {
  destroy_timer -= 1;
  if (destroy_timer <= 0) instance_destroy();
}

// ══════════════════════════════════════════════════════════════════════
// 偏移：每帧结束时把这两个量加到 x/y 上（初始都是 0 = 不动）
// mod 特效脚本用 VM_SetProp(self, "mod_prestep_dx", 值) / ("mod_prestep_dy", 值) 改
// ⚠️ 特效的 x/y 没有谁会每帧重置，所以这里是【每帧位移】而不是固定偏移 ——
//    写 3 就是每帧往右 3 像素（匀速移动）。要"固定在某个偏移位置"得另说
// ══════════════════════════════════════════════════════════════════════
x += mod_prestep_dx;
y += mod_prestep_dy;