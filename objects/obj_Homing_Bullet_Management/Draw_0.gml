// 逐条画子弹 —— 所有子弹都**自转**（角度 = -spin，每帧 +6 度），
// 和原版糖葫芦 / 章鱼烧的 `image_angle = -timer * 6` 一致。
// 朝向（_b.angle）只用于飞行，不用于绘制。
var _i = array_length(list) - 1;
while (_i >= 0) {
    var _b = list[_i];
    if (_b.spr != -1) {
        // frame 是小数累加器（anim_speed 可能是小数），取整后当子图索引
        draw_sprite_ext(_b.spr, floor(_b.frame), _b.x, _b.y, _b.scale, _b.scale, -_b.spin, c_white, 1);
    }
    _i -= 1;
}
