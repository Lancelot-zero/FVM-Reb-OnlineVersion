// 逐条画子弹
// 角度用子弹自己的 angle（默认 0；卡片效果把方向翻转时会 +180，看着才是"弹回去了"）
var _i = count - 1;
while (_i >= 0) {
    var _b = list[_i];
    if (_b.spr != -1) {
        // frame 是小数累加器（anim_speed 可能是小数），取整后当子图索引
        draw_sprite_ext(_b.spr, floor(_b.frame), _b.x, _b.y, _b.scale, _b.scale, _b.angle, c_white, 1);
    }
    _i -= 1;
}
