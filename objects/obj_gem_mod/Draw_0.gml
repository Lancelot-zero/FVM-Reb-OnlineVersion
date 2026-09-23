// 该宝石虚拟机里的绘制块；没有则默认画 sprite_index
var _drawn = false;
if (gem_id != "" && variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
	var _vm = global.mod_gem_vms[? gem_id];
	if (ds_map_exists(_vm.blocks, "_OBJECT_DRAW")) {
		var _bak_cur = global._VM_cur_card;
		global._VM_cur_card = id;
		var _bak_vm = global.__vm;
		global.__vm = _vm;
		VM_Execute(_vm, _vm.blocks[? "_OBJECT_DRAW"], "_OBJECT_DRAW");
		global.__vm = _bak_vm;
		global._VM_cur_card = _bak_cur;
		_drawn = true;
	}
}
if (!_drawn) draw_self();

// 缩放后的半径（悬停判定和冷却遮罩都用）
var _hw = sprite_width * image_xscale / 2;
var _hh = sprite_height * image_yscale / 2;

// 预留属性 cooldown_timer：冷却遮罩 + 剩余秒数（照原版宝石）
if (cooldown_timer > 0) {
	draw_set_valign(fa_middle);
	draw_set_halign(fa_center);
	draw_set_font(font_hei);
	draw_set_alpha(0.5);
	draw_set_colour(c_black);
	draw_roundrect(x - _hw, y - _hh, x + _hw - 3, y + _hh - 3, false);
	draw_set_alpha(1);
	draw_set_colour(c_white);
	draw_text(x, y, string(floor(cooldown_timer / 60)));
}

// 说明框：鼠标悬停，或插件把 on_click 置 true
if (is_struct(gem_info) && (on_click || point_in_rectangle(mouse_x, mouse_y, x - _hw, y - _hh, x + _hw, y + _hh))) {
	var _desc = string(gem_info.description);
	if (cooldown_timer > 0) _desc += "\n正在冷却中";
	if (tooltip_text != "") _desc += "\n" + tooltip_text;
	draw_set_font(font_pixel);
	draw_set_halign(fa_left);
	draw_set_valign(fa_top);
	var _tw = string_width_ext(_desc, 0.65, 180);
	var _th = string_height_ext(_desc, 0.65, 180);
	var _bx = x + 40;
	var _by = y - (_th + 27) / 2;
	draw_set_color(#323250);
	draw_rectangle(_bx, _by, _bx + _tw + 20, _by + _th + 27, true);
	draw_set_color(merge_color(c_yellow, c_white, 0.3));
	draw_rectangle(_bx, _by, _bx + _tw + 20, _by + _th + 27, false);
	draw_set_color(c_black);
	draw_text(_bx + 10, _by + 5, _desc);
	draw_set_halign(fa_left);
	draw_set_valign(fa_top);
	draw_set_alpha(1);
}

// 预留属性 gem_level：等级星星
if (gem_level > 0) draw_sprite_ext(spr_star_slot, gem_level - 1, x - 23, y - 23, 0.75, 0.75, 0, c_white, 1);
