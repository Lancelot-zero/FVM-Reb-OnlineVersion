// 逐条画定点弹：画在 draw_x/draw_y（出身位置出发按 spd 漂移中的当前位置），
// frame 取整当子图索引，不循环（播完 Step 里就删了）；伤害定点 x/y 不在这画
var _i = count - 1;
while (_i >= 0) {
    var _b = list[_i];
    if (_b.spr != -1) {
        draw_sprite_ext(_b.spr, floor(_b.frame), _b.draw_x, _b.draw_y, _b.scale, _b.scale, _b.angle, c_white, 1);
    }
    _i -= 1;
}
