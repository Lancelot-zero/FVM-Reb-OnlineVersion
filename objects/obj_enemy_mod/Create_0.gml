// 通用 mod 敌人对象：src_mod 注册的所有敌人共用此对象（继承 obj_enemy_parent，
// 移动/攻击/死亡等基础行为全部复用父类），特殊逻辑在 .bin 里
// 敌人 id 由生成逻辑在创建前写入 global._mod_pending_enemy_id
event_inherited();
enemy_id = variable_global_exists("_mod_pending_enemy_id") ? global._mod_pending_enemy_id : ""
mod_enemy_inited_id = ""   // 已按哪个 id 初始化过（联机时 id 晚到，Step 里补跑）

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（可选；不写 = 每帧进）
//   mod_step_enter_condition
//     "" / 认不出      每帧进（默认）
//     "mod"            按全场帧号对齐：battle_time % mod_step_enter_var == 0
//     "wait"           倒数：mod_step_enter_var 每帧 -1，减到 <= 0 进
//                      ⚠️ 进完之后要把 var 设回正数，否则此后每帧都进
//     "cell"           跨格才进（grid_col / grid_row 相对上一帧变了）
//     "hp_change"      血量变了才进（掉血、回血都算；读父类的 pre_hp）
//     "hp_change_mod"  同 "hp_change"，但触发一次后进 mod_step_enter_var 帧冷却
//     "card"           父类索敌当前有目标才进（target_plant 有效）
//     "card_mod"       同 "card"，但触发一次后进 mod_step_enter_var 帧冷却
//   mod_step_enter_var   "mod" = 间隔帧数；"wait" = 还要等几帧；带 _mod 后缀 = 冷却帧数
// obj 每帧回写（卡只读）：
//   prev_grid_col / prev_grid_row  上一帧格子坐标（"cell" 用）
//   mod_has_card                   1 = 父类索敌当前有目标（仅 card / card_mod 模式更新）
// ⚠️ pre_hp 能直接用：父类把 pre_hp = hp 写在 End Step（Step_2），
//    本对象的 Step 先跑，读到的还是上一帧末的血量
// ══════════════════════════════════════════════════════════════════════════
mod_step_enter_condition = ""   // "" / "mod" / "wait" / "cell" / "hp_change" / "hp_change_mod" / "card" / "card_mod"
// 盯梢（实现见 Step）：插件用命名数组 mod_watch 声明"要盯的属性名"，任一属性值变了，这一帧也进 _OBJECT_STEP
mod_changed_prop = ""    // 【只读】这一帧第一个发生变化的盯梢属性名（没变化 = ""）
mod_watch_last  = []     // 【obj 内部】上一帧 mod_watch 各属性的值（和数组一一对应）
mod_watch_changed = []  // 【obj 内部】这一帧发生变化的盯梢属性名（每个各进一次 _OBJECT_STEP）
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧；*_mod = 冷却帧数
mod_tick_cool            = 0    // 【obj 内部】*_mod 模式的冷却剩余帧数
mod_has_card             = 0    // 【只读】card / card_mod 模式下 1 = 索敌有目标

// 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
mod_countdown = 0    // 倒计时（帧）：> 0 时每帧 -1
mod_alpha_add = 0    // 每帧加到 image_alpha 上的量（负数 = 渐隐）；只在倒计时 > 0 时加

// 鼠标移入时的白色半透明遮罩（在 Draw 里画，用的还是自家贴图）
mod_hover_mask  = 0      // 1 = 鼠标压在贴图上时显示白色半透明遮罩
mod_hover_alpha = 0.35   // 遮罩的透明度（0~1，越大越白）

// 上一帧格子坐标：按当前位置直接算，和第一帧 Step 里算出来的值一致，不会误触发一次 "cell"
prev_grid_col = floor((x - global.grid_offset_x) / global.grid_cell_size_x)
prev_grid_row = floor((y - global.grid_offset_y) / global.grid_cell_size_y)

/// @function mod_enemy_init()
/// @desc 套用注册表数值 + 执行插件的 _OBJECT_CREATE。
///       联机时 enemy_id 是敌人建好之后才由 MSG_MODIFY_PROP 补过来的，
///       所以独立成函数：Create 时 id 已就位就直接跑，晚到的由 Step 补跑一次。
function mod_enemy_init() {
	if (enemy_id == "" || mod_enemy_inited_id == enemy_id) return;

	// 非法 mod 敌人（本机没注册这个 enemy_id）→ 换成一个普通小老鼠（铁锅鼠）：
	//   继承同一个 net_id，后续 hp / 位置同步仍然作用在这只替身上；本实例随后销毁（Step 里会 exit）
	var _known = (variable_global_exists("enemy_map") && ds_map_exists(global.enemy_map, enemy_id))
	          || (variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id));
	if (!_known) {
		show_debug_message("[mod 敌人] 未注册的 enemy_id，换成普通老鼠: " + string(enemy_id));
		var _nid = ds_map_exists(global.network.map_instance_id_net_id, id) ? global.network.map_instance_id_net_id[? id] : -1;
		var _rep = instance_create_depth(x, y, depth, obj_iron_pan_mouse);
		if (_nid != -1) set_net_id(_rep.id, _nid);
		_rep.grid_row = grid_row;
		_rep.grid_col = grid_col;
		instance_destroy(id);
		return;
	}

	mod_enemy_inited_id = enemy_id;

	// 原版敌人是每个对象在 Create 里写死自己的数值，mod 敌人统一从注册表套用
	if (enemy_id != "" && variable_global_exists("enemy_map")) {
		var _info = global.enemy_map[? enemy_id];
		if (is_struct(_info)) {
			if (!is_undefined(_info.spr) && _info.spr != noone) { sprite_index = _info.spr; }
			hp = _info.hp ?? hp;
			maxhp = hp;
			shield_hp = _info.shield ?? 0;
			shield_max_hp = shield_hp;
			move_speed = _info.speed ?? move_speed;
			atk = _info.atk ?? atk;
			atk_cycle = _info.cycle ?? atk_cycle;
			attack_range = _info.range ?? attack_range;
			immune_to_ash = _info.ash_proof ?? false;
			feature = _info.feature ?? "land";
		}
	}

	// 创建时加入该敌人的实例容器，并执行该敌人虚拟机里的创建块
	if (enemy_id != "" && variable_global_exists("mod_enemy_vms") && ds_map_exists(global.mod_enemy_vms, enemy_id)) {
		var _vm = global.mod_enemy_vms[? enemy_id];
		mod_inst_add(_vm, id);
		if (ds_map_exists(_vm.blocks, "_OBJECT_CREATE")) {
			var _bak_last = global._VM_last_created_card;
			global._VM_last_created_card = id;      // 块内 VM_GetLastCreatedCard() 取到本实例
			var _bak_cur = global._VM_cur_card;
			global._VM_cur_card = id;               // 块内 VM_GetCurCard() 取到本实例
			var _bak_vm = global.__vm;
			global.__vm = _vm;                      // API 函数按全局 VM 读参，执行期间切到该敌人的 VM
			VM_Execute(_vm, _vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
			global.__vm = _bak_vm;
			global._VM_cur_card = _bak_cur;
			global._VM_last_created_card = _bak_last;
		}
	}
}

if (enemy_id != "") mod_enemy_init();
