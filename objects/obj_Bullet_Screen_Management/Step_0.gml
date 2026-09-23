// 每帧：移动 -> 出界静默删除 -> 动画回绕 -> 打范围内敌人 -> 存活倒计时 -> 到期移除
if (global.is_paused) exit;

var _len = count;
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

// 格子加成快照：每格把卡的 bullet_flag 或起来（每帧一次，9x7 格可以忽略）
// 子弹只用一次位测试就能排除掉绝大多数"这一格根本没卡能吃我"的情况，
// 命中掩码才走下面那套逐卡循环（吃位、换贴图、加伤）
var _cols = 0;
var _snap_ok = false;
if (_has_grid && variable_global_exists("grid_plants")) {
    _cols = global.grid_cols;
    var _ncells = _cols * _rows;
    // 尺寸变了（换图）就重建；下面两个循环会把每一格都写一遍，不用先清零
    if (array_length(cell_flag) != _ncells) cell_flag = array_create(_ncells, 0);
    for (var _sr = 0; _sr < _rows; _sr++) {
        var _srow = _sr * _cols;
        for (var _sc = 0; _sc < _cols; _sc++) {
            var _cardlist = ds_grid_get(global.grid_plants, _sc, _sr);
            var _cnum = ds_list_size(_cardlist);
            var _mask = 0;
            for (var _sk = 0; _sk < _cnum; _sk++) {
                var _card = ds_list_find_value(_cardlist, _sk);
                if (!instance_exists(_card)) continue;
                if (!variable_instance_exists(_card, "bullet_flag")) continue;
                _mask = _mask | _card.bullet_flag;
            }
            cell_flag[_srow + _sc] = _mask;
        }
    }
    _snap_ok = true;
}


if(obj_battle.battle_time%20==0){
	show_notice("弹幕数量"+string(_len),20)
}
var _i = _len - 1;
while (_i >= 0) {

    var _b = list[_i];

    // 1. 坐标推进
    _b.x += _b.vx;
    _b.y += _b.vy;

    // 1.5 行渐变（可选）：ty >= 0 时朝目标行的世界 y 靠拢（原版水管弹那个 y 向 lerp）
    //     够近就吸附并把 ty 清成 -1 —— 之后这颗弹就是纯直线，不再每帧花这笔计算
    //     ⚠️ 碰撞行是按 y 每帧反算的，所以渐变过程中会依次命中经过的那几行，和原版一致
    if (_b.ty >= 0) {
        if (abs(_b.ty - _b.y) <= 8) {
            _b.y  = _b.ty;
            _b.ty = -1;
        } else {
            _b.y += (_b.ty - _b.y) * _b.lk;
        }
    }
	
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


        // 3.5 卡片效果：子弹进到「格子中心带」时，看那一格卡片有没有能吃它的标志数值
        //     规则：子弹 flag 与卡片 bullet_flag 做【与运算】，> 0 就应用效果，然后消位
        //           → 同一类卡片的加成对同一颗子弹只生效一次
        //     bit1 = 过火类：换点燃贴图 spr_fire_bullet + 放大 1.8 + 播 snd_bullet_burnt
        //     bit2 = 解冻类：换包子贴图 spr_xiaolongbao_bullet + 播 snd_bullet_burnt；
        //                    不启用增益；子弹自己的冰冻帧数清零；之后「或上 1」= 变成可以被点燃
        //     4/8/16…= 自定义类：没有内置表现，效果全看卡片字段
        //     卡片字段（实例变量，mod 卡用 VM_SetProp 写，原版卡在 Create 里写）：
        //       bullet_flag       去重位（本卡属于哪些类）
        //       bullet_mul_dmg    伤害倍率（默认 1）     bullet_add_dmg    伤害加值（默认 0）
        //       bullet_flip_x     反向 x（0/1）           bullet_flip_y     反向 y（0/1）
        //       bullet_freeze_mul 冰冻帧数倍率（默认 1）   bullet_freeze_add 冰冻帧数加值（默认 0）
        if (_snap_ok && _b.flag > 0) {
            var _fx = (_b.x - _gox) / _gcsx;
            var _pc = floor(_fx);
            var _pr = floor((_b.y - _goy) / _gcsz);
            // 中心带：和 obj_battle 收录 bullet_array_special 同一口径（格子中间 50%）
            if (_pc >= 0 && _pc < _cols && _pr >= 0 && _pr < _rows
                && (_fx - _pc) >= 0.15 && (_fx - _pc) <= 0.85
                && (_b.flag & cell_flag[_pr * _cols + _pc]) != 0) {
                var _cards = ds_grid_get(global.grid_plants, _pc, _pr);
                var _ncard = ds_list_size(_cards);
                for (var _ci = 0; _ci < _ncard; _ci++) {
                    var _car = ds_list_find_value(_cards, _ci);
                    if (!instance_exists(_car)) continue;
                    if (!variable_instance_exists(_car, "bullet_flag")) continue;
                    var _cf = _car.bullet_flag;
                    if (_cf <= 0) continue;
                    if ((_b.flag & _cf) == 0) continue;

                    var _thaw = ((_cf & 2) != 0);
                    // ① 内置表现
                    if ((_cf & 1) != 0) {                                  // 过火类
                        _b.spr    = spr_fire_bullet;
                        _b.frames = sprite_get_number(_b.spr);
                        _b.scale  = 1.8;                                       // 同原版 image_xscale/yscale = 1.8（直接设，不是乘）
                        audio_play_sound(snd_bullet_burnt, 0, 0);
                    }
                    if (_thaw) {                                           // 解冻类
                        _b.spr    = spr_xiaolongbao_bullet;
                        _b.frames = sprite_get_number(_b.spr);
                        audio_play_sound(snd_bullet_burnt, 0, 0);
                    }
                    // ② 增益（解冻位不启用；解冻另外把子弹自己的冰冻帧数清零）
                    if (_thaw) {
                        _b.freeze = 0;
                    } else {
                        var _mul = variable_instance_exists(_car, "bullet_mul_dmg")    ? _car.bullet_mul_dmg    : 1;
                        var _add = variable_instance_exists(_car, "bullet_add_dmg")    ? _car.bullet_add_dmg    : 0;
                        var _fm  = variable_instance_exists(_car, "bullet_freeze_mul") ? _car.bullet_freeze_mul : 1;
                        var _fa  = variable_instance_exists(_car, "bullet_freeze_add") ? _car.bullet_freeze_add : 0;
                        _b.dmg    = _b.dmg * _mul + _add;
                        _b.freeze = _b.freeze * _fm + _fa;
                        if (variable_instance_exists(_car, "bullet_flip_x") && _car.bullet_flip_x) _b.vx = -_b.vx;
                        if (variable_instance_exists(_car, "bullet_flip_y") && _car.bullet_flip_y) _b.vy = -_b.vy;
                        // 旋转量也由卡片给（原版布丁是 +180）；不填就是 0，只反向不转
                        if (variable_instance_exists(_car, "bullet_angle_add")) _b.angle += _car.bullet_angle_add;
                    }
                    // ③ 消位（解冻后额外获得"能被点燃"的位）
                    _b.flag = _b.flag & (~_cf);
                    if (_thaw) _b.flag = _b.flag | 1;
                    if (_b.flag <= 0) break;      // 没位可吃了，剩下的卡不用再看
                }
            }
        }
		
		//if(obj_battle.battle_time%20==0){
        // 4. 命中：打击范围内 可命中敌人，按格子里敌人数组的顺序依次结算
        //    打击范围是「方格子」：以子弹所在格为中心，上下左右各扩 cell_range 格（0=只算本格）
        //    伤害计数 -1（不限次数）→ 范围内敌人**全部**结算，永不减
        //    伤害计数 >0（有限次数）→ 这一帧最多打这么多个，打一个减一个，减到 0 就停手
        //                              （所以计数=1 就是打一个就消失，计数=3 就是同帧内穿三个）
        //    同一个敌人每帧最多挨一次（一个格子只遍历一遍）
        if (_has_grid && _b.hits != 0) {
            var _col = floor((_b.x - _gox) / _gcsx);
            var _row = floor((_b.y - _goy) / _gcsz);
            if (_row >= 0 && _row < _rows) {
                var _all  = (_b.hits < 0);
                var _left = _b.hits;
                var _cr = _b.cell_range;
                if (_cr < 0) _cr = 0;

                // ── 快路：cell_range = 0（绝大多数子弹）只扫自己那一格，两道循环全免 ──
                if (_cr == 0 && _col < _stride && (_col >= 0 || variable_global_exists("enemy_array_left"))) {
                    var _cell = (_col < 0) ? global.enemy_array_left[_row] : global.enemy_array[_row * _stride + _col];
                    var _cn   = array_length(_cell);
                    for (var _k = 0; _k < _cn; _k++) {
                        var _e = _cell[_k];
                        if (!instance_exists(_e)) continue;
                        if (_e.hp <= 0) continue;
                        if ((_b.ttype & _e.tbit) == 0) continue;
                        damage_enemy(_e, _b.dmg, _b.damage_type);
                        if (_b.freeze > 0 && _e.ice_ok) {
                            if (_e.ice_timer < _b.freeze) _e.ice_timer = _b.freeze;
                        }
                        if (!_all) {
                            _left -= 1;
                            if (_left <= 0) break;
                        }
                    }
                } else {
                // ── 慢路：cell_range > 0，扫 (2r+1) 行的格子（逻辑和上面快路一样，只是格子多）──
                var _r1 = _row - _cr;  if (_r1 < 0)         _r1 = 0;
                var _r2 = _row + _cr;  if (_r2 >= _rows)    _r2 = _rows - 1;

                if (_col < 0) {
                    // ── 负列（场外左侧，格 -1 / -2 …）：这些敌人不在 global.enemy_array 里 ──
                    //    obj_battle 把它们按行收在 global.enemy_array_left[row]（那一行所有负列敌人）
                    //    所以这里扫那几行的 left 列表 —— 不补这一段的话，-1/-2 列的老鼠永远打不到
                    if (variable_global_exists("enemy_array_left")) {
                        for (var _r = _r1; _r <= _r2; _r++) {
                            var _cell = global.enemy_array_left[_r];
                            var _cn   = array_length(_cell);
                            for (var _k = 0; _k < _cn; _k++) {
                                var _e = _cell[_k];
                                if (!instance_exists(_e)) continue;
                                if (_e.hp <= 0) continue;
                                if ((_b.ttype & _e.tbit) == 0) continue;
                                damage_enemy(_e, _b.dmg, _b.damage_type);
                                if (_b.freeze > 0 && _e.ice_ok) {
                                    if (_e.ice_timer < _b.freeze) _e.ice_timer = _b.freeze;
                                }
                                if (!_all) {
                                    _left -= 1;
                                    if (_left <= 0) break;
                                }
                            }
                            if (!_all && _left <= 0) break;
                        }
                    }
                } else if (_col < _stride) {
                    // ── 场内：按格子扫（左右各扩 cell_range 格，列不会退到 0 以下）──
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
                                if ((_b.ttype & _e.tbit) == 0) continue;
                                // 走敌人自己的受击事件（闪白 + 音效 + 护盾，含各敌人自己重写的 Other_10），
                                // 和原版子弹命中一样 —— 不再直接 _e.hp -= dmg 手抄父对象那 16 行。
                                // 伤害类型固定在 bullet_screen_add 里（normal），和追踪弹管理器同一口径。
                                damage_enemy(_e, _b.dmg, _b.damage_type);
                                // 冰冻：子弹累计的冰冻帧数写给敌人（只加不减，同原版 ice_timer）
                                if (_b.freeze > 0 && _e.ice_ok) {
                                    if (_e.ice_timer < _b.freeze) _e.ice_timer = _b.freeze;
                                }
                                if (!_all) {
                                    _left -= 1;
                                    if (_left <= 0) break;
                                }
                            }
                            if (!_all && _left <= 0) break;
                        }
                        if (!_all && _left <= 0) break;
                    }
                }
                }
                if (!_all) _b.hits = _left;
            }
        }
		//}	

        // 5. 存活倒计时
        if (_b.life > 0) _b.life -= 1;

        // 6. 到期（伤害次数用完 或 存活帧数走完）→ 生成销毁对象
        if (_b.hits == 0 || _b.life == 0) {
            // 被卡片点燃过的弹（spr 已被换成火焰弹）走原版的火焰命中特效
            if (_b.death_obj != "" && _b.spr == spr_fire_bullet) {
                var _fo = asset_get_index("obj_fire_bullet_effect");
                if (_fo != -1) {
                    var _fd = instance_create_depth(_b.x, _b.y, depth - 1, _fo);
                    _fd.sprite_index = spr_fire_bullet_effect;
                }
            } else if (_b.death_obj != "") {
                var _obj = asset_get_index(_b.death_obj);
                if (_obj == -1) _obj = asset_get_index("obj_" + _b.death_obj);
                if (_obj != -1) {
                    var _d = instance_create_depth(_b.x, _b.y, depth - 1, _obj);
                    // mod 对象（obj_bullet_mod / obj_effect_mod / obj_enemy_mod 等）还要指定 mod 名字
                    if (_b.death_mod != "") _d.mod_type = _b.death_mod;
                    // 皮肤特效：同一个销毁对象换贴图（原版三线酒架/机枪小笼包就是这么做的）
                    if (_b.death_spr != "") _d.sprite_index = get_load_sprite(_b.death_spr);
                }
            }
            _gone = true;
        }
    }

    // 7. 删除 = 和末尾那颗**交换**，然后 count 减一（数组长度不变、不 array_delete）
    //    交换而不是只覆盖，是为了让"每槽一个结构体"的对应关系保持住：
    //    空闲槽永远指向空闲结构体，下次新增原地填字段才不会改到活子弹
    if (_gone) {
        count -= 1;
        var _t = list[_i];
        list[_i] = list[count];
        list[count] = _t;
    }

    _i -= 1;
}

// 8. 合并索引重建（只在合并打开时做；关闭时这一步完全跳过，零开销）
//    主循环里的交换删除会改下标，所以必须等循环跑完、下标稳定了再写一遍
//    每格 16 槽，只登记"这一帧还活着且还能命中"的弹（hits == 0 / life == 0 的马上要消失）
//    ⚠️ 上面两条 exit（暂停 / 没子弹）会跳过这里，此时表是上一帧的：检索侧靠 idx < count 判定，安全
if (_snap_ok && merge_enable) {
    var _cells = _cols * _rows;
    if (array_length(cell_n) != _cells) {
        cell_n      = array_create(_cells, 0);
        bullet_grid = array_create(_cells * 16, -1);
    } else {
        for (var _ci = 0; _ci < _cells; _ci++) cell_n[_ci] = 0;
    }
    for (var _gi = 0; _gi < count; _gi++) {
        var _gb = list[_gi];
        if (_gb.hits == 0 || _gb.life == 0) continue;
        var _gc = floor((_gb.x - _gox) / _gcsx);
        if (_gc < 0 || _gc >= _cols) continue;
        var _gr = floor((_gb.y - _goy) / _gcsz);
        if (_gr < 0 || _gr >= _rows) continue;
        var _gk = _gr * _cols + _gc;
        var _gn = cell_n[_gk];
        if (_gn < 16) {
            bullet_grid[_gk * 16 + _gn] = _gi;
            cell_n[_gk] = _gn + 1;
        }
    }
}

// 9. 合并统计（临时调试：每秒一行；debug_merge = false 就只计数不打印）
if (count > count_max) count_max = count;
_stat_frame += 1;
if (debug_merge && _stat_frame >= 60) {
    show_debug_message("[弹幕] 合并=" + string(merge_n - _stat_merge)
                     + " (累计 严" + string(merge1_n) + "/宽" + string(merge2_n) + ")"
                     + " 新建=" + string(add_n - _stat_add)
                     + " 丢弃=" + string(drop_n - _stat_drop)
                     + " 当前=" + string(count) + " 峰值=" + string(count_max));
    _stat_merge = merge_n;
    _stat_add   = add_n;
    _stat_drop  = drop_n;
    _stat_frame = 0;
}

