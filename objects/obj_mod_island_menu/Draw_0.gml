// 和原版世界地图一样：半透明黑遮罩
if global.menu_screen {
    draw_set_alpha(0.5);
    draw_rectangle_color(0, 0, room_width, room_height, c_black, c_black, c_black, c_black, false);
    draw_set_alpha(1);
}

// 默认 mod 大地图背景，按原始比例居中，不铺满全屏
var _bg_spr = src_mod_default_map_bg();
if (!sprite_exists(_bg_spr)) _bg_spr = spr_world_map_menu;

var _bg_scale = 0.9;
var _bg_w = sprite_get_width(_bg_spr) * _bg_scale;
var _bg_h = sprite_get_height(_bg_spr) * _bg_scale;
draw_sprite_stretched(
    _bg_spr,
    0,
    (room_width - _bg_w) / 2,
    (room_height - _bg_h) / 2,
    _bg_w,
    _bg_h
);

draw_set_halign(fa_center);
draw_set_valign(fa_middle);

for (var _i = 0; _i < array_length(state.positions); _i++) {
    var _p = state.positions[_i];
    draw_text(_p.x, _p.y + 90, _p.name);
}

draw_set_halign(fa_left);
draw_set_valign(fa_top);