// 通用 mod 宝石对象：src_mod 注册的所有宝石共用此对象
// 宝石 id 由装备放置逻辑在创建前写入 global._mod_pending_gem_id
// 只负责初始化和分发 VM 块，效果逻辑全部在 .bin 里
gem_id = variable_global_exists("_mod_pending_gem_id") ? global._mod_pending_gem_id : ""
gem_info = get_gem_info(gem_id)
cooldown = 60
parent_player = noone
// 发射方信息（给插件宝石脚本定位用；创建宝石的地方会覆盖这几个值）
// ⚠️ parent_player 保持 noone 时，obj_battle/Step_2 那边已经有 instance_exists 守卫，不会再读 noone.object_index
mod_point_parent_player = noone
mod_point_grid_row      = 0
mod_point_grid_col      = 0
mod_point_gem_level     = 0
// ---- 预留显示属性：插件在 .bin 里用 VM_SetProp(self,"名",值) 改，核心只负责按值渲染 ----
on_click       = false   // 鼠标进来/离开时由 Mouse_10/Mouse_11 置位（插件也可以自己置）
clickable      = false   // 主动宝石：插件在 _OBJECT_CREATE 里置 1；置 1 后点击才有效，点一下自动重新走冷却
clicked        = false   // 左键点了一下且当时不在冷却（Mouse_4 置 true），插件处理完自己设回 0
cooldown_timer = 0       // 剩余冷却帧：>0 画冷却遮罩+秒数、说明里加「正在冷却中」，每帧自减（点击时 Mouse_4 设回 cooldown）
gem_level      = 0       // >0 画等级星星，默认取存档等级
tooltip_text   = ""      // 追加在宝石说明后面的自定义文本
if (gem_id != "") {
	var _gl = get_gem_level(gem_id)
	if (!is_undefined(_gl)) gem_level = _gl
}
image_speed = 0
// 动画配置（与 obj_card_parent 同款）：.bin 可在 _OBJECT_CREATE 用 VM_SetProp 覆盖
timer = 0
flash_speed = 5
idle_anim = 0
attack_anim = 0
state = CARD_STATE.IDLE;
if (is_struct(gem_info)) {
	if (!is_undefined(gem_info.icon) && gem_info.icon != noone) {
		sprite_index = gem_info.icon
	}
	// 冷却取自宝石数据（原版宝石都是 cooldown 数组，按等级取）
	if (is_array(gem_info.cooldown)) {
		var _lv = get_gem_level(gem_id)
		cooldown = gem_info.cooldown[min(_lv, array_length(gem_info.cooldown) - 1)]
	} else if (!is_undefined(gem_info.first_cooldown)) {
		cooldown = gem_info.first_cooldown
	}
}

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（可选；不写 = 每帧进）
//   mod_step_enter_condition
//     "" / 认不出    每帧进（默认）
//     "mod"          按全场帧号对齐：battle_time % mod_step_enter_var == 0
//     "wait"         倒数：mod_step_enter_var 每帧 -1，减到 <= 0 进
//                    ⚠️ 进完之后要把 var 设回正数，否则此后每帧都进
//     "cool"         冷却结束才进（cooldown_timer <= 0）；主动宝石用这个
//   mod_step_enter_var   "mod" = 间隔帧数；"wait" = 还要等几帧
// ⚠️ 宝石没有 hp、不移动（图标钉在 390, 213 + gem_index*80），
//    所以卡片/敌人那套 hp_change、cell、索敌条件这里都没有
// ══════════════════════════════════════════════════════════════════════════
mod_step_enter_condition = ""   // "" / "mod" / "wait" / "cool"
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧

// 创建时加入该宝石的实例容器，并执行该宝石虚拟机里的创建块
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
	ds_list_add(_vm.instances, id);
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
