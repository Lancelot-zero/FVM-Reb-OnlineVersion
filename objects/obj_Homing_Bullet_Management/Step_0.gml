// 每帧：索敌快照（全场只扫一遍）-> 重挑目标 -> 重算朝向/速度 -> 推进 -> 出界静默删除
//       -> 动画回绕 -> 撞到目标就结算伤害 -> 存活倒计时 -> 到期移除（生成销毁对象）
if (global.is_paused) exit;

var _len = array_length(list);
if (_len <= 0) exit;

frame_id += 1;

// ── 索敌快照：每帧只扫一遍全场，按【敌人类型】分 6 个固定桶存"最左、同 x 血最高"的敌人 ──
//    同帧所有子弹共享，和原版糖葫芦/章鱼烧的 global._tanghulu_scan_frame 一个套路，
//    但那份是按"子弹类型"缓存一份，这里是按敌人类型分桶，不同类型子弹也能共用。
//    ⚠️ with 里面 self 是敌人，取管理器的变量必须写 other.（裸写 scan_id 会被当成敌人身上的，
//       报 "Variable obj_xxx.scan_id not set"）。
{
    for (var _t = 0; _t < 6; _t++) {
        scan_id[_t] = noone;
        scan_x[_t]  = 0;
        scan_hp[_t] = -1;
    }
    with (obj_enemy_parent) {
        if (hp > 0 && y > 0) {
            var _slot = -1;
            switch (target_type) {
                case "normal":      _slot = 0; break;
                case "air":         _slot = 1; break;
                case "dance":       _slot = 2; break;
                case "obstacle":    _slot = 3; break;
                case "diver":       _slot = 4; break;
                case "underground": _slot = 5; break;
            }
            if (_slot >= 0) {
                if (other.scan_id[_slot] == noone || x < other.scan_x[_slot] || (x == other.scan_x[_slot] && hp > other.scan_hp[_slot])) {
                    other.scan_id[_slot] = id;
                    other.scan_x[_slot]  = x;
                    other.scan_hp[_slot] = hp;
                }
            }
        }
    }
}

// 网格参数只读一次，循环里不再碰 global（只有模式 1 的"本行正前方"索敌要用）
var _has_grid = variable_global_exists("enemy_array") && variable_global_exists("grid_cols") && variable_global_exists("grid_rows");
var _stride = 0;
var _rows   = 0;
if (_has_grid) {
    _stride = global.grid_cols + 2;
    _rows   = global.grid_rows;
}

var _i = _len - 1;
while (_i >= 0) {

    var _b = list[_i];
    var _gone = false;

    // 自转：所有追踪弹都转，速度由 spin_speed 决定（默认 6，同原版 image_angle = -timer*6）
    _b.spin += _b.spin_speed;

    // 0. 追踪：每帧重挑目标 + 重算 vx/vy（必须在坐标推进之前）
    //    模式 0：只咬"全场最左的可命中敌人"（糖葫芦 / 大力神的口径）。快照里挑，全场每帧只扫一遍。
    //    模式 1：**先打发射卡前方** —— 锚点（发射卡）本行、正前方 150px 内血最高的那个；
    //            找不到才退回模式 0 的"全场最左"（章鱼烧 / 月神的口径）。
    if (_b.spd != 0) {
        var _best = noone;

        if (_b.mode == 1 && _has_grid && instance_exists(_b.anchor)) {
            // ── 前方优先：查锚点本列 + 右一列的网格数组（和原版章鱼烧 _find_right_cell 同范围）──
            var _arow = _b.anchor.grid_row;
            var _acol = _b.anchor.grid_col;
            if (_arow >= 0 && _arow < _rows && _acol >= 0) {
                var _ax   = _b.anchor.x;
                var _rowb = _arow * _stride;
                var _cend = min(_acol + 1, _stride - 1);
                var _bhp  = -1;
                for (var _fc = _acol; _fc <= _cend; _fc++) {
                    var _farr = global.enemy_array[_rowb + _fc];
                    var _fn   = array_length(_farr);
                    for (var _fk = 0; _fk < _fn; _fk++) {
                        var _fe = _farr[_fk];
                        if (!instance_exists(_fe)) continue;
                        if (_fe.hp <= 0 || _fe.y <= 0) continue;
                        if ((_b.ttype & _fe.tbit) == 0) continue;
                        if (_fe.x < _ax || _fe.x > _ax + 150) continue;
                        if (_fe.hp > _bhp) { _bhp = _fe.hp; _best = _fe; }
                    }
                }
            }
        }

        if (_best == noone) {
            // ── 兜底（也是模式 0 的全部）：本帧快照里挑最左的可命中敌人 ──
            var _best_x = room_width;
            var _best_hp = -1;
            for (var _qt = 0; _qt < 6; _qt++) {
                if (scan_id[_qt] == noone) continue;
                if ((_b.ttype & scan_bit[_qt]) == 0) continue;
                if (scan_x[_qt] < _best_x || (scan_x[_qt] == _best_x && scan_hp[_qt] > _best_hp)) {
                    _best_x = scan_x[_qt];
                    _best_hp = scan_hp[_qt];
                    _best = scan_id[_qt];
                }
            }
        }

        if (_best != noone) _b.target = _best;
        else if (!instance_exists(_b.target) || _b.target.hp <= 0) _b.target = noone;

        if (instance_exists(_b.target)) {
            // 不留惯性：每帧直接把朝向设成"正对目标"（同原版 point_direction 每帧重算）
            _b.angle = point_direction(_b.x, _b.y, _b.target.x, _b.target.y - 75);
            _b.vx = lengthdir_x(_b.spd, _b.angle);
            _b.vy = lengthdir_y(_b.spd, _b.angle);
        }
    }

    // 1. 坐标推进
    _b.x += _b.vx;
    _b.y += _b.vy;

    // 2. 出界：静默删除，不生成销毁对象（边界和 obj_bullet_mod 一致）
    if (_b.x < 0 || _b.x > 2200 || _b.y < 0 || _b.y > 1200) {
        _gone = true;
    } else {

        // 3. 动画帧计数（循环，速度由 anim_speed 决定，可以是小数）
        if (_b.frames > 1) {
            _b.frame += _b.anim_speed;
            if (_b.frame >= _b.frames || _b.frame < 0) {
                _b.frame = _b.frame mod _b.frames;
                if (_b.frame < 0) _b.frame += _b.frames;
            }
        }

        // 4. 命中：只判定**锁定的那个目标** —— 子弹身上本来就存着目标 id（第 0 步刚更新过），
        //    靠近了就直接结算，不用再扫网格找敌人（原版糖葫芦/章鱼烧的碰撞事件也是同一口径：
        //    `if other.hp > 0 and target_enemy == other.id and can_hit(...)`）。
        //    判定用像素距离（贴图重叠口径）：糖葫芦弹 103x82（半宽 51 / 半高 41），
        //    一般敌人半宽 30~60、半高 40，所以重叠大约在 |dx| <= 90 / |dy| <= 85。
        //    ⚠️ 命中类型必须再查一次（现在是一次 tbit 位与）：敌人的 target_type 运行时会变（蝙蝠鼠 normal<->air、
        //       潜水鼠 normal<->diver、幽灵鼠变暗 -> invisible、铁人鼠 normal<->air），
        //       锁定之后再改类型就不再是"可打"的了 —— 原版这里同样会重新判一次。
        //    ⚠️ 代价：子弹穿过"非目标"的敌人时不会掉血（原版也只认 target_enemy）；
        //       追踪弹本来就直奔目标，正常情况看不出来。
        if (_b.hits != 0 && instance_exists(_b.target) && _b.target.hp > 0) {
            if ((_b.ttype & _b.target.tbit) != 0
             && abs(_b.target.x - _b.x) <= 90 && abs(_b.target.y - _b.y) <= 85) {
                // 走敌人自己的受击事件（闪白 + 音效 + 护盾，含各敌人自己重写的 Other_10），
                // 和原版子弹命中一样 —— 不再在这儿手抄父对象那 16 行。
                damage_enemy(_b.target, _b.dmg, _b.damage_type);
                _b.hits = 0;
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
                    if (_b.death_mod != "") _d.mod_type = _b.death_mod;
                    // 有些子弹用的销毁对象是通用的、要靠换贴图区分（例如糖葫芦借 obj_coke_bomb_explode
                    // 再把自己的 spr_tanghulu_bullet_effect 盖上去）
                    if (_b.death_spr != "") _d.sprite_index = get_load_sprite(_b.death_spr);
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
