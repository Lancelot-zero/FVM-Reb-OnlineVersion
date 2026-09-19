var _cur = global.map_sprite_current;
var _tgt = global.map_sprite_target;

if (_cur == _tgt) {
    draw_sprite_ext(_cur, map_spr_index, 0, 0, 1, 1, 0, c_white, 1);
    global.map_fade_alpha = 0;
} else {
    var _a = global.map_fade_alpha;
    draw_sprite_ext(_cur, map_spr_index, 0, 0, 1, 1, 0, c_white, 1 - _a);
    draw_sprite_ext(_tgt, map_spr_index, 0, 0, 1, 1, 0, c_white, _a);
    _a += global.map_fade_step;
    if (_a >= 1) {
        global.map_sprite_current = _tgt;
        _a = 0;
    }
    global.map_fade_alpha = _a;
}


// 固定64个贴图槽
for (var _i = 0; _i < 64; _i++) {
    var _slot = global.map_draw_slots[_i];
    if (_slot.sprite == noone || _slot.sprite == "" || _slot.sprite == -1) continue;
    draw_sprite_ext(_slot.sprite, 0, _slot.x, _slot.y, _slot.image_xscale, _slot.image_yscale, _slot.image_angle, c_white, _slot.alpha);
}



draw_set_valign(fa_top)
draw_set_halign(fa_left)
draw_set_color(c_white)
draw_set_font(font_yuan)
draw_text(0,0,"FPS:"+string(fps))
draw_text(0,25,"加速:"+(speed_up ? "开" : "关") + "（shift）")
draw_text(0,50,"暂停（空格）\n菜单（ESC）")

