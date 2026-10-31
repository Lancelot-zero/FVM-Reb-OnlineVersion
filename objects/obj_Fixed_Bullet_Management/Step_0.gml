// 每帧：动画推进 -> 播完最后一帧结算伤害（指定敌人只打那一个 / 范围全打）-> 交换删除
if (global.is_paused) exit;

var _len = count;
if (_len <= 0) exit;

// 网格参数只读一次，循环里不再碰 global（范围攻击要按格子扫半径内的敌人）
var _has_grid = variable_global_exists("enemy_array") && variable_global_exists("grid_cols") && variable_global_exists("grid_rows")
             && variable_global_exists("grid_offset_x") && variable_global_exists("grid_offset_y")
             && variable_global_exists("grid_cell_size_x") && variable_global_exists("grid_cell_size_y");
var _stride = 0;
var _rows   = 0;
var _gox    = 0;
var _goy    = 0;
var _gcsx   = 1;
var _gcsz   = 1;
var _has_left = false;
if (_has_grid) {
    _stride = global.grid_cols + 2;
    _rows   = global.grid_rows;
    _gox    = global.grid_offset_x;
    _goy    = global.grid_offset_y;
    _gcsx   = global.grid_cell_size_x;
    _gcsz   = global.grid_cell_size_y;
    if (_gcsx <= 0 || _gcsz <= 0) _has_grid = false;
    _has_left = variable_global_exists("enemy_array_left");
}

// 范围结算的公共段：闭包只建一次（每帧一颗，不在子弹循环里重建）。
// ⚠️ 子弹 _b 必须当参数传进来，不能靠闭包捕获——GML 闭包捕获 var 局部变量不可靠
//    （error_log 里报过 "Variable _b not set before reading it"，就是这个原因崩的）。
var _enemy_in_range = function(_b, _e) {
    if (!instance_exists(_e)) return;
    if (_e.hp <= 0) return;
    if ((_b.ttype & _e.tbit) == 0) return;
    if (point_distance(_e.x, _e.y, _b.x, _b.y) > _b.radius) return;
    damage_enemy(_e, _b.dmg, _b.damage_type);
    if (_b.freeze > 0 && _e.ice_ok && _e.ice_timer < _b.freeze) _e.ice_timer = _b.freeze;
    if (_b.frozen > 0 && _e.ice_ok && _e.frozen_timer < _b.frozen) _e.frozen_timer = _b.frozen;
};

var _i = _len - 1;
while (_i >= 0) {

    var _b = list[_i];

    // 1. 动画推进
    _b.frame += _b.anim_speed;

    // 2. 贴图漂移：从出生点按 spd 飞向伤害定点 (x, y)，到点就停（纯视觉，不影响结算点）
    if (_b.spd > 0) {
        var _dx = _b.x - _b.draw_x;
        var _dy = _b.y - _b.draw_y;
        var _dist = point_distance(0, 0, _dx, _dy);
        if (_dist <= _b.spd) {
            _b.draw_x = _b.x;
            _b.draw_y = _b.y;
        } else {
            _b.draw_x += _dx * _b.spd / _dist;
            _b.draw_y += _dy * _b.spd / _dist;
        }
    }

    // 3. 播完最后一帧：结算伤害
    if (_b.frame >= _b.frames) {
        if (_b.dmg > 0) {
            // ── 独立攻击：指定了敌人 id → 只打这一个（先判断存活且 hp>0），不看半径/命中掩码 ──
            //    目标死了/没了就**直接跳过**，绝不能掉进下面的范围分支：
            //    范围扫描是为"没指定目标"的子弹准备的（会扫到别的敌人，还触发过闭包报错崩溃）
            if (_b.target_id != noone) {
                if (instance_exists(_b.target_id) && _b.target_id.hp > 0) {
                    var _t = _b.target_id;
                    damage_enemy(_t, _b.dmg, _b.damage_type);
                    if (_b.freeze > 0 && _t.ice_ok && _t.ice_timer < _b.freeze) _t.ice_timer = _b.freeze;
                    if (_b.frozen > 0 && _t.ice_ok && _t.frozen_timer < _b.frozen) _t.frozen_timer = _b.frozen;
                }
            }
            // ── 范围攻击：没指定敌人 → 扫半径覆盖的格子，所有可命中敌人各打一下 ──
            else if (_has_grid) {
                // 半径覆盖的格子范围（半径按格子换算 + 1 格余量）
                var _cc  = floor((_b.x - _gox) / _gcsx);
                var _crr = floor((_b.y - _goy) / _gcsz);
                var _gcr = ceil(_b.radius / _gcsx) + 1;
                if (_gcr < 1) _gcr = 1;
                var _c1 = _cc - _gcr;
                var _c2 = _cc + _gcr;
                var _r1 = _crr - _gcr;  if (_r1 < 0)        _r1 = 0;
                var _r2 = _crr + _gcr;  if (_r2 >= _rows)   _r2 = _rows - 1;

                for (var _r = _r1; _r <= _r2; _r++) {
                    // 负列（场外左侧）：不在 enemy_array 里，按行收在 enemy_array_left[row]
                    if (_c1 < 0 && _has_left) {
                        var _celll = global.enemy_array_left[_r];
                        var _cnl   = array_length(_celll);
                        for (var _k = 0; _k < _cnl; _k++) _enemy_in_range(_b, _celll[_k]);
                    }
                    var _base = _r * _stride;
                    var _cs = _c1;  if (_cs < 0) _cs = 0;
                    var _ce = _c2;  if (_ce >= _stride) _ce = _stride - 1;
                    for (var _c = _cs; _c <= _ce; _c++) {
                        var _cell = global.enemy_array[_base + _c];
                        var _cn   = array_length(_cell);
                        for (var _k = 0; _k < _cn; _k++) _enemy_in_range(_b, _cell[_k]);
                    }
                }
            }
        }

        // 4. 销毁效果：非空时在伤害定点创建一个实例（通用对象 + 换贴图那套，同追踪弹）
        if (_b.death_obj != "") {
            var _obj = asset_get_index(_b.death_obj);
            if (_obj == -1) _obj = asset_get_index("obj_" + _b.death_obj);
            if (_obj != -1) {
                var _d = instance_create_depth(_b.x, _b.y, depth - 1, _obj);
                if (_b.death_mod != "") _d.mod_type = _b.death_mod;
                if (_b.death_spr != "") _d.sprite_index = get_load_sprite(_b.death_spr);
            }
        }

        // 5. 移除 = 和末尾那颗**交换**，然后 count 减一（数组长度不变、不 array_delete）
        count -= 1;
        var _t = list[_i];
        list[_i] = list[count];
        list[count] = _t;
    }

    _i -= 1;
}
