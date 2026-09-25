// 每个实例绘制时执行该卡虚拟机里的绘制块（单点调用）
event_inherited();
if (plant_id != "" && variable_global_exists("mod_card_vms") && ds_map_exists(global.mod_card_vms, plant_id)) {
	var _vm = global.mod_card_vms[? plant_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_DRAW")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;      // 块内 API 取当前实例
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_DRAW"], "_OBJECT_DRAW");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
	}
}

// ══════════════════════════════════════════════════════════════════════
// 鼠标移入时的白色半透明遮罩（mod_hover_mask = 1 才开）
//   做法：用项目的 hit_effect_2 着色器把同一张贴图再画一遍 ——
//   该着色器是 `gl_FragColor.rgb = v_vColour.rgb`（RGB 换成顶点色、alpha 保留贴图），
//   所以 c_white + 指定 alpha 得到的就是"贴图形状的纯白半透明剪影"。
//   ⚠️ 不能用 draw_sprite_ext(..., c_white, a)：c_white 是不上色，等于把贴图再画一遍（只是变半透明）
//   画在本体之后，所以在最上层
// ══════════════════════════════════════════════════════════════════════
if (mod_hover_mask == 1) {
	// position_meeting 靠碰撞遮罩；贴图没有遮罩（运行期加载的图等）时用 bbox 兜底
	var _hovered = position_meeting(mouse_x, mouse_y, id);
	if (!_hovered) _hovered = point_in_rectangle(mouse_x, mouse_y, bbox_left, bbox_top, bbox_right, bbox_bottom);
	if (_hovered) {
		shader_set(hit_effect_2);
		draw_sprite_ext(sprite_index, image_index, x, y, image_xscale, image_yscale, image_angle, c_white, mod_hover_alpha);
		shader_reset();
	}
}
