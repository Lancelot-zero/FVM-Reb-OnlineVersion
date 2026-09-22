// 每帧：移动 -> 出界静默删除 -> 动画回绕 -> 打范围内敌人 -> 存活倒计时 -> 到期移除
if (global.is_paused) exit;

var _len = array_length(list);
if (_len <= 0) exit;

// 网格参数只读一次，循环里不再碰 global
// 注意：这里等价于 get_grid_position_from_world(x, y, true) 的裸算法（不回绕），
//       但不建那个 {col,row,x,y} 结构体——每颗子弹每帧省一次分配
var _has_grid = variable_global_exists("enemy_array") && variable_global_exists("grid_cols") && variable_global_exists("grid_rows")
             && variable_global_exists("grid_offset_x") && variable_global_exists("grid_offset_y")
             && variable_global_exists("grid_cell_size_x") && variable_global_exists("grid_cell_size_y");
var _stride = 0;
var _rows   = 0;
var _gox    = 0;
var _goy    = 0;
var _gcsx   = 1;
var _gcsz   = 1;
if (_has_grid) {
    _stride = global.grid_cols + 2;
    _rows   = global.grid_rows;
    _gox    = global.grid_offset_x;
    _goy    = global.grid_offset_y;
    _gcsx   = global.grid_cell_size_x;
    _gcsz   = global.grid_cell_size_y;
    if (_gcsx <= 0 || _gcsz <= 0) _has_grid = false;
}

var _i = _len - 1;
while (_i >= 0) {

    var _b = list[_i];

    // 1. 坐标推进
    _b.x += _b.vx;
    _b.y += _b.vy;

    var _gone = false;

    // 2. 出界：静默删除，不生成销毁对象（边界跟 obj_bullet_mod 一致）
    if (_b.x < 0 || _b.x > 2200 || _b.y < 0 || _b.y > 1200) {
        _gone = true;
    } else {

        // 3. 动画帧计数（循环，速度由 anim_speed 决定，可以是小数）
        if (_b.frames > 1) {
            _b.frame += _b.anim_speed;
            // 用 mod 一步回绕，不要用 while 减——anim_speed 给大了会卡住
            if (_b.frame >= _b.frames || _b.frame < 0) {
                _b.frame = _b.frame mod _b.frames;
                if (_b.frame < 0) _b.frame += _b.frames;
            }
        }

        // 4. 命中：打击范围内 可命中敌人，按格子里敌人数组的顺序依次结算
        //    打击范围是「方格子」：以子弹所在格为中心，上下左右各扩 cell_range 格（0=只算本格）
        //    伤害计数 -1（不限次数）→ 范围内敌人**全部**结算，永不减
        //    伤害计数 >0（有限次数）→ 这一帧最多打这么多个，打一个减一个，减到 0 就停手
        //                              （所以计数=1 就是打一个就消失，计数=3 就是同帧内穿三个）
        //    同一个敌人每帧最多挨一次（一个格子只遍历一遍）
        if (_has_grid && _b.hits != 0) {
            var _col = floor((_b.x - _gox) / _gcsx);
            var _row = floor((_b.y - _goy) / _gcsz);
            if (_row >= 0 && _row < _rows && _col >= 0 && _col < _stride) {
                var _all  = (_b.hits < 0);
                var _left = _b.hits;
                var _cr = _b.cell_range;
                if (_cr < 0) _cr = 0;
                var _r1 = _row - _cr;  if (_r1 < 0)         _r1 = 0;
                var _r2 = _row + _cr;  if (_r2 >= _rows)    _r2 = _rows - 1;
                var _c1 = _col - _cr;  if (_c1 < 0)         _c1 = 0;
                var _c2 = _col + _cr;  if (_c2 >= _stride)  _c2 = _stride - 1;
                for (var _r = _r1; _r <= _r2; _r++) {
                    var _base = _r * _stride;
                    for (var _c = _c1; _c <= _c2; _c++) {
                        var _cell = global.enemy_array[_base + _c];
                        var _cn   = array_length(_cell);
                        for (var _k = 0; _k < _cn; _k++) {
                            var _e = _cell[_k];
                            if (!instance_exists(_e)) continue;
                            if (_e.hp <= 0) continue;
                            if (!can_hit(_b.target_type, _e.target_type)) continue;
                            _e.hp -= _b.dmg;
                            if (!_all) {
                                _left -= 1;
                                if (_left <= 0) break;
                            }
                        }
                        if (!_all && _left <= 0) break;
                    }
                    if (!_all && _left <= 0) break;
                }
                if (!_all) _b.hits = _left;
            }
        }

        // 5. 存活倒计时
        if (_b.life > 0) _b.life -= 1;

        // 6. 到期（伤害次数用完 或 存活帧数走完）→ 生成销毁对象
        if (_b.hits == 0 || _b.life == 0) {
            if (_b.death_obj != "") {
                var _obj = asset_get_index(_b.death_obj);
                if (_obj == -1) _obj = asset_get_index("obj_" + _b.death_obj);
                if (_obj != -1) {
                    var _d = instance_create_depth(_b.x, _b.y, depth - 1, _obj);
                    // mod 对象（obj_bullet_mod / obj_effect_mod / obj_enemy_mod 等）还要指定 mod 名字
                    if (_b.death_mod != "") _d.mod_type = _b.death_mod;
                }
            }
            _gone = true;
        }
    }

    // 7. 交换删除：把末尾那条挪过来，再砍掉末尾
    if (_gone) {
        var _last = array_length(list) - 1;
        list[_i] = list[_last];
        array_delete(list, _last, 1);
    }

    _i -= 1;
}
