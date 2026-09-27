// ============================================================
// hot_bullet_screen_management.cpp —— obj_Bullet_Screen_Management（屏幕弹幕管理器）
//
//   全场只有一个实例（obj_battle 开局建），子弹**不是实例**，是数组里的一颗结构体。
//
//   ── 存储：原生 C++ 数组，不是 VM 容器 ──
//   原版是 array_create(800) + 每槽一个 ds_map，字段全用字符串键查表：每帧每颗子弹
//   二十来次哈希 + 字符串比较，800 颗就是一万多次。这里换成定长 POD 数组 g_bullets[]：
//   字段就是成员变量，Step/Draw 直接读写（b.x += b.vx 这种）。
//
//   为什么能换：这个结构的**读写两端都在 C++**——
//     · 读：这个文件的 Step / Draw；
//     · 写：原版 scr_command_VM.gml 里的 bullet_screen_add / _Ex / _Exs（scripts/ 不参与
//       转换，那是被 C++ VM 取代的那一层，所以它天生就是 C++ 侧）。
//   而且翻译出来的 361 个 bin 里**一个字段名都没有**（扫过：bullet_grid / cell_n /
//   bullet_max / death_obj / ttype / anim_speed 全 0 命中），说明 GML 侧没人扒这个结构。
//
//   数组仍然是"固定容量 + count 活动长度"：加弹原地填 [count] 再 count+1，
//   删除把末尾那颗挪过来再 count-1（不做压缩、不挪整段）。
//
//   消失条件三条：出界（静默删）/ 伤害次数用完 / 存活帧数走完。
//   闸门、以及"卡片的 flag 效果"（bullet_flag 位运算 + 消位）都在 Step 里。
//
//   ⚠ "加子弹"那一侧（bullet_screen_add_Ex / _Exs 这两个原生）**还没接**，
//     所以现在数组是空的；它还缺 get_load_sprite / sprite_exists / sprite_get_number
//     三个贴图原生（现在都是空壳）。逻辑照抄齐，这几个接上就活。
// ============================================================

#include "hot_common.h"
#include "hot_objects.h"

// 弹丸本体（定长 800）。字段对 GML 是纯内部的（见文件头），所以是文件级原生数组，
// 不进 VM 容器、也不进头文件：碰它的就这个文件的 Step/Draw 和以后的"加弹"那一侧。
//   ⚠ 单例：全场只有 obj_battle 建的那一个管理器（原版每实例一份，这里按"只有一个"做）。
static const int BS_MAX = 800;           // = Create 写进实例变量的 bullet_max
struct Bullet {
    Value  spr;            // 贴图：sprite 索引；卡片效果会换成贴图**名字符串**（"spr_fire_bullet"）
    Value  damage_type;    // 伤害类型（走 damage_enemy 的护盾口径）
    Value  death_obj;      // 销毁对象（资产名），空 = 不生成
    Value  death_mod;      // 销毁对象是 mod 对象时填 mod 名
    Value  death_spr;      // 销毁对象的贴图覆盖
    double x, y, vx, vy;   // 位置 / 每帧位移
    double dmg, hits, life, freeze;
    double scale, angle, anim_speed, frame, frames, cell_range;
    double ty, lk;         // 行渐变：目标世界 y / 每帧靠拢比例
    int    ttype;          // 命中位掩码
    int    flag;           // 还能吃哪些类别的卡片效果（位掩码）
};
static Bullet g_bullets[BS_MAX];
static std::vector<int> g_bullet_grid;   // cells*16：每格最多 16 颗的子弹下标（-1 = 空）
static std::vector<int> g_cell_n;        // 每格现有的子弹数

// Create_0.gml（119 行）
bool hot_bulletscreen_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    in->depth = mk_num(-700);                      // 内置：UI(-2900 以下) 之后、敌人(0/-200) 之前
    in->vars["bullet_max"]   = mk_num(BS_MAX);     // ⚠ 要跟 g_bullets 的大小一致
    in->vars["count"]        = mk_num(0);          // 活动子弹数（唯一的活动长度，Step/Draw/加弹都读它）

    // 子弹合并（默认关闭 = 原逻辑：加不进去就丢）
    in->vars["merge_enable"] = mk_num(0);
    in->vars["merge_lv1"]    = mk_num(500);
    in->vars["merge_lv2"]    = mk_num(800);
    in->vars["debug_merge"]  = mk_num(0);
    {
        static const char* stats[] = { "merge_n", "merge1_n", "merge2_n", "add_n", "drop_n",
                                       "count_max", "_stat_frame", "_stat_merge", "_stat_add", "_stat_drop" };
        for (size_t i = 0; i < sizeof(stats) / sizeof(stats[0]); i++)
            in->vars[stats[i]] = mk_num(0);
    }
    // 原版在这里 array_create(bullet_max) 再填 800 个空 ds_map 当模板 —— 原生数组不需要
    // 预填（每颗子弹在加的时候每个字段都会被整份写一遍，没有"继承上一颗"的字段），
    // 所以那张 24 个默认值的表直接删掉了。这里只要把活动长度和各种索引清空。
    g_cell_n.clear();
    g_bullet_grid.clear();
    return true;
}

// Draw_0.gml（11 行）：逐条画 —— 角度用子弹自己的 angle（卡片把方向翻转时会 +180）
bool hot_bulletscreen_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    for (int i = (int)get_num(s, "count") - 1; i >= 0; i--) {
        Bullet& b = g_bullets[i];
        if (b.spr.t == MEM_INT && b.spr.v == -1) continue;       // spr == -1：没贴图
        Value d[9];
        d[0] = b.spr;
        d[1] = mk_num(std::floor(b.frame));                      // frame 是小数累加器，取整当子图
        d[2] = mk_num(b.x);
        d[3] = mk_num(b.y);
        d[4] = mk_num(b.scale);
        d[5] = mk_num(b.scale);
        d[6] = mk_num(b.angle);
        d[7] = mk_num(16777215);                                 // c_white
        d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    return true;
}

// Destroy_0.gml（4 行）：原版显式 array_resize(list, 0) 断开对子弹数组的引用（不等 GC）
bool hot_bulletscreen_destroy(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    set_num(s, "count", 0);
    g_cell_n.clear();
    g_bullet_grid.clear();
    return true;
}

// ── Step 里"打一个敌人"和"扫一格"两段（快路/慢路共用）──
//   伤害走敌人自己的受击事件（damage_enemy）；冰冻是"只加不减"地写 ice_timer
static void bs_hit_enemy(Vm& vm, Bullet& b, int e) {
    Value da[3];
    da[0] = mk_num(e);
    da[1] = mk_num(b.dmg);
    da[2] = b.damage_type;
    call_fn_v(vm, "damage_enemy", da, 3, -1);
    if (b.freeze > 0 && truthy(inst_var(e, "ice_ok"))) {
        if (get_num(e, "ice_timer") < b.freeze) set_num(e, "ice_timer", b.freeze);
    }
}

// 扫一个格子的敌人列表；返回 true = 次数用完，该停了（_all 时永不 true）
static bool bs_scan_cell(Vm& vm, Bullet& b, const Value& cell, bool all, double* left) {
    int cn = container_size(cell);
    for (int k = 0; k < cn; k++) {
        int e = (int)num_of(&container_at(cell, k));
        if (!inst_get(e)) continue;
        if (get_num(e, "hp") <= 0) continue;
        if ((b.ttype & (int)get_num(e, "tbit")) == 0) continue;
        bs_hit_enemy(vm, b, e);
        if (!all) {
            *left -= 1;
            if (*left <= 0) break;
        }
    }
    return (!all && *left <= 0);
}

// Step_0.gml（394 行）
//   一帧顺序：推进 → 出界 → 动画 → 卡片效果 → 障碍 → 命中 → 存活倒计时 → 到期 → 交换删除
bool hot_bulletscreen_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue(vm, "is_paused")) return true;

    // ── 网格参数只读一次（等价 get_grid_position_from_world(x,y,true) 的裸算法，不建结构体）──
    bool has_grid = global_exists("enemy_array") && global_exists("grid_cols") && global_exists("grid_rows")
                 && global_exists("grid_offset_x") && global_exists("grid_offset_y")
                 && global_exists("grid_cell_size_x") && global_exists("grid_cell_size_y");
    double stride = 0, rows = 0, gox = 0, goy = 0, gcsx = 1, gcsz = 1;
    if (has_grid) {
        stride = gnumv(vm, "grid_cols") + 2;
        rows   = gnumv(vm, "grid_rows");
        gox    = gnumv(vm, "grid_offset_x");
        goy    = gnumv(vm, "grid_offset_y");
        gcsx   = gnumv(vm, "grid_cell_size_x");
        gcsz   = gnumv(vm, "grid_cell_size_y");
        if (gcsx <= 0 || gcsz <= 0) has_grid = false;
    }
    double cols = has_grid ? gnumv(vm, "grid_cols") : 0;

    // 快照（cell_flag / cell_terrain_flag 由 obj_battle 每帧重建）：长度对得上才认为本帧有效
    Value cell_flag = gv(vm, "cell_flag");
    Value terrain   = gv(vm, "cell_terrain_flag");
    bool snap_ok = has_grid && cols > 0 && rows > 0 && global_exists("cell_flag")&& (double)container_size(cell_flag) == cols * rows;
    bool snap_ok_ob = has_grid && cols > 0 && rows > 0 && global_exists("cell_terrain_flag")&& (double)container_size(terrain) == cols * rows;

    int len = (int)get_num(s, "count");
    if (len <= 0) return true;

    Value enemy_array = gv(vm, "enemy_array");
    Value enemy_left  = gv(vm, "enemy_array_left");
    Value grid_plants = gv(vm, "grid_plants");

    for (int i = len - 1; i >= 0; i--) {
        Bullet& b = g_bullets[i];                    // 直接改数组里的那一颗（原版是改句柄指着的 ds_map）

        bool gone = false;
        bool kill_all = false;          // 卡片要求立即销毁（bullet_destroy 为负）
        double bx = b.x + b.vx;
        double by = b.y + b.vy;
        b.x = bx;
        b.y = by;

        // 1.5 行渐变（可选）：ty >= 0 时朝目标行的世界 y 靠拢，够近就吸附并把 ty 清 -1
        if (b.ty >= 0) {
            if (std::fabs(b.ty - by) <= 8) {
                b.y = b.ty;
                by = b.ty;
                b.ty = -1;
            } else {
                by += (b.ty - by) * b.lk;
                b.y = by;
            }
        }

        if (bx < 0 || bx > 2200 || by < 0 || by > 1200) {
            gone = true;                                   // 2. 出界：静默删
        } else {
            // 3. 动画帧计数（循环，anim_speed 可以是小数；用 mod 一步回绕）
            if (b.frames > 1) {
                double fr = b.frame + b.anim_speed;
                if (fr >= b.frames || fr < 0) {
                    fr = std::fmod(fr, b.frames);
                    if (fr < 0) fr += b.frames;
                }
                b.frame = fr;
            }

            // 3.5 卡片效果：子弹进到"格子中心带"时，flag 与本格卡片的 bullet_flag 做与运算
            if (snap_ok && b.flag > 0) {
                double fx = (bx - gox) / gcsx;
                double pc = std::floor(fx);
                double pr = std::floor((by - goy) / gcsz);
                if (pc >= 0 && pc < cols && pr >= 0 && pr < rows
                        && (fx - pc) >= 0.15 && (fx - pc) <= 0.85
                        && ((b.flag & (int)array_n(vm, cell_flag, (int)(pr * cols + pc))) != 0)) {
                    // 这一格的卡片列表
                    Value ga[3]; ga[0] = grid_plants; ga[1] = mk_num(pc); ga[2] = mk_num(pr);
                    Value cards = call_ret(vm, "ds_grid_get", ga, 3);
                    Value la[1]; la[0] = cards;
                    int ncard = (int)num_of(&call_ret(vm, "ds_list_size", la, 1));
                    for (int ci = 0; ci < ncard; ci++) {
                        Value fa[2]; fa[0] = cards; fa[1] = mk_num(ci);
                        int car = (int)num_of(&call_ret(vm, "ds_list_find_value", fa, 2));
                        if (!inst_get(car)) continue;
                        Value* cfp = inst_var(car, "bullet_flag");
                        if (!cfp) continue;                       // variable_instance_exists
                        int cf = (int)num_of(cfp);
                        if (cf <= 0) continue;
                        if ((b.flag & cf) == 0) continue;

                        bool thaw = ((cf & 2) != 0);
                        // ① 内置表现
                        if ((cf & 1) != 0) {                      // 过火类
                            b.spr = mk_str(vm, "spr_fire_bullet");
                            Value sa[1]; sa[0] = mk_str(vm, "spr_fire_bullet");
                            b.frames = num_of(&call_ret(vm, "sprite_get_number", sa, 1));
                            b.scale = 1.8;
                            Value au[3]; au[0] = mk_str(vm, "snd_bullet_burnt"); au[1] = mk_num(0); au[2] = mk_num(0);
                            call_fn_v(vm, "audio_play_sound", au, 3, -1);
                        }
                        if (thaw) {                               // 解冻类
                            b.spr = mk_str(vm, "spr_xiaolongbao_bullet");
                            Value sa[1]; sa[0] = mk_str(vm, "spr_xiaolongbao_bullet");
                            b.frames = num_of(&call_ret(vm, "sprite_get_number", sa, 1));
                            Value au[3]; au[0] = mk_str(vm, "snd_bullet_burnt"); au[1] = mk_num(0); au[2] = mk_num(0);
                            call_fn_v(vm, "audio_play_sound", au, 3, -1);
                        }
                        // ② 增益（解冻位不启用；解冻另外把子弹自己的冰冻帧数清零）
                        Value* p = NULL;
                        if (thaw) {
                            b.freeze = 0;
                        } else {
                            double mul = (p = inst_var(car, "bullet_mul_dmg"))    ? num_of(p) : 1;
                            double add = (p = inst_var(car, "bullet_add_dmg"))    ? num_of(p) : 0;
                            double fm  = (p = inst_var(car, "bullet_freeze_mul")) ? num_of(p) : 1;
                            double fa2 = (p = inst_var(car, "bullet_freeze_add")) ? num_of(p) : 0;
                            b.dmg    = b.dmg * mul + add;
                            b.freeze = b.freeze * fm + fa2;
                            if ((p = inst_var(car, "bullet_flip_x")) && num_of(p) != 0)
                                b.vx = -b.vx;
                            if ((p = inst_var(car, "bullet_flip_y")) && num_of(p) != 0)
                                b.vy = -b.vy;
                            if ((p = inst_var(car, "bullet_angle_add")))
                                b.angle += num_of(p);
                            if ((p = inst_var(car, "bullet_scale"))) {
                                double sc = num_of(p);
                                if (is_numv(p) && sc > 0) b.scale *= sc;
                            }
                            if ((p = inst_var(car, "bullet_hits_add"))) {
                                double ha = num_of(p);
                                if (is_numv(p) && ha > 0 && b.hits >= 0)
                                    b.hits += ha;
                            }
                        }
                        // 销毁指令：0 = 不启用；负数 = 立即销毁；正数 = 只减穿透次数
                        if ((p = inst_var(car, "bullet_destroy"))) {
                            double bd = num_of(p);
                            if (is_numv(p) && bd != 0) {
                                if (bd < 0) kill_all = true;
                                else if (b.hits >= 0) {
                                    double h = b.hits - bd;
                                    b.hits = h < 0 ? 0 : h;
                                }
                            }
                        }
                        // ③ 消位（解冻后额外获得"能被点燃"的位）
                        b.flag &= ~cf;
                        if (thaw) b.flag |= 1;

                        // ④ 每弹每卡只做一次：穿过计数 + 可选的充能
                        add_num(car, "bullet_pass_count", 1);
                        if ((p = inst_var(car, "bullet_charge_self")) && num_of(p) != 0)
                            add_num(car, "bullet_charge_dmg", b.dmg);

                        if (kill_all) break;
                        if (b.flag <= 0) break;                   // 没位可吃了
                    }
                }
            }
            if (kill_all) b.hits = 0;                             // 立即销毁 → 交给第 6 步

            // 3.6 障碍物：非穿透子弹（hits >= 0）飞进有障碍的格子 → 直接消耗掉
            if (snap_ok_ob && b.hits >= 0) {
                double obc = std::floor((bx - gox) / gcsx);
                double obr = std::floor((by - goy) / gcsz);
                if (obc >= 0 && obc < cols && obr >= 0 && obr < rows && ((int)array_n(vm, terrain, (int)(obr * cols + obc)) & 1) != 0)   // bit0 = 障碍
                    b.hits = 0;
            }

            // 4. 命中：打击范围内可命中敌人（cell_range 格方形范围），按格子里敌人数组顺序结算
            if (has_grid && b.hits != 0) {
                double col = std::floor((bx - gox) / gcsx);
                double row = std::floor((by - goy) / gcsz);
                if (row >= 0 && row < rows) {
                    bool all = (b.hits < 0);
                    double left = b.hits;
                    double cr = b.cell_range;
                    if (cr < 0) cr = 0;

                    if (cr == 0 && col < stride && (col >= 0 || global_exists("enemy_array_left"))) {
                        // 快路：只扫自己那一格（绝大多数子弹）
                        Value cell = (col < 0) ? container_at(enemy_left, (int)row): container_at(enemy_array, (int)(row * stride + col));
                        bs_scan_cell(vm, b, cell, all, &left);
                    } else {
                        // 慢路：扫 (2r+1) 行
                        double r1 = row - cr; if (r1 < 0) r1 = 0;
                        double r2 = row + cr; if (r2 >= rows) r2 = rows - 1;
                        if (col < 0) {
                            // 负列（场外左侧）：敌人在 global.enemy_array_left[row] 里
                            if (global_exists("enemy_array_left")) {
                                for (double r = r1; r <= r2; r++) {
                                    Value cell = container_at(enemy_left, (int)r);
                                    if (bs_scan_cell(vm, b, cell, all, &left)) break;
                                }
                            }
                        } else if (col < stride) {
                            double c1 = col - cr; if (c1 < 0) c1 = 0;
                            double c2 = col + cr; if (c2 >= stride) c2 = stride - 1;
                            for (double r = r1; r <= r2; r++) {
                                double base = r * stride;
                                for (double c = c1; c <= c2; c++) {
                                    Value cell = container_at(enemy_array, (int)(base + c));
                                    if (bs_scan_cell(vm, b, cell, all, &left)) break;
                                }
                                if (!all && left <= 0) break;
                            }
                        }
                    }
                    if (!all) b.hits = left;
                }
            }

            // 5. 存活倒计时
            if (b.life > 0) b.life -= 1;

            // 6. 到期 → 生成销毁对象
            if (b.hits == 0 || b.life == 0) {
                std::string dname = pool_text(vm.strings, &b.death_obj);
                if (!dname.empty() && str_is(vm, &b.spr, "spr_fire_bullet")) {
                    // 被卡片点燃过的弹（spr 已换成火焰弹）走原版的火焰命中特效
                    if (obj_known(vm, "obj_fire_bullet_effect")) {
                        Value fa[4];
                        fa[0] = mk_num(bx); fa[1] = mk_num(by);
                        fa[2] = mk_num(get_num(s, "depth") - 1);
                        fa[3] = mk_str(vm, "obj_fire_bullet_effect");
                        Value fd = call_ret(vm, "instance_create_depth", fa, 4);
                        int fid = fd.t == MEM_INT ? fd.v : -1;
                        if (inst_get(fid)) inst_var_set(fid, "sprite_index", mk_str(vm, "spr_fire_bullet_effect"));
                    }
                } else if (!dname.empty()) {
                    // GML: asset_get_index(death_obj)，找不到再补一次 "obj_" 前缀
                    std::string full = dname;
                    if (!obj_known(vm, full)) full = "obj_" + dname;
                    if (obj_known(vm, full)) {
                        Value da[4];
                        da[0] = mk_num(bx); da[1] = mk_num(by);
                        da[2] = mk_num(get_num(s, "depth") - 1);
                        da[3] = mk_str(vm, full.c_str());
                        Value dv = call_ret(vm, "instance_create_depth", da, 4);
                        int did = dv.t == MEM_INT ? dv.v : -1;
                        if (inst_get(did)) {
                            if (b.death_mod.t == MEM_STRING) inst_var_set(did, "mod_type", b.death_mod);
                            std::string sname = pool_text(vm.strings, &b.death_spr);
                            if (!sname.empty()) {
                                Value ga[1]; ga[0] = b.death_spr;
                                inst_var_set(did, "sprite_index", call_ret(vm, "get_load_sprite", ga, 1));
                            }
                        }
                    }
                }
                gone = true;
            }
        }

        // 7. 删除 = 把末尾那颗挪到 i，count 减一
        //    原版还要把被删的那颗写回末尾（array_set(list, c, b)），那是为了把预填的
        //    "空子弹模板"留在数组末尾；原生数组没有模板，槽 c 已经越界，下一颗加进来时
        //    每个字段都会被整份覆盖，所以这一句可以省。
        if (gone) {
            int c = (int)get_num(s, "count") - 1;
            set_num(s, "count", c);
            if (c != i) g_bullets[i] = g_bullets[c];
        }
    }

    // 8. 合并索引重建（只在合并打开时做；关闭时整段跳过）
    if (snap_ok && truthy(inst_var(s, "merge_enable"))) {
        int cells = (int)(cols * rows);
        g_cell_n.assign(cells, 0);
        if ((int)g_bullet_grid.size() != cells * 16) g_bullet_grid.assign(cells * 16, -1);
        int cnt = (int)get_num(s, "count");
        for (int gi = 0; gi < cnt; gi++) {
            Bullet& gb = g_bullets[gi];
            if (gb.hits == 0 || gb.life == 0) continue;
            double gc = std::floor((gb.x - gox) / gcsx);
            if (gc < 0 || gc >= cols) continue;
            double gr = std::floor((gb.y - goy) / gcsz);
            if (gr < 0 || gr >= rows) continue;
            int gk = (int)(gr * cols + gc);
            int gn = g_cell_n[gk];
            if (gn < 16) {
                g_bullet_grid[gk * 16 + gn] = gi;
                g_cell_n[gk] = gn + 1;
            }
        }
    }

    // 9. 合并统计（debug_merge = false 就只计数不打印）
    if (get_num(s, "count") > get_num(s, "count_max")) set_num(s, "count_max", get_num(s, "count")); add_num(s, "_stat_frame", 1);
    if (truthy(inst_var(s, "debug_merge")) && get_num(s, "_stat_frame") >= 60) {
        // 字符串拼接照 GML 的格式来（宿主调 show_debug_message 看输出）
        char buf[256];
        double mn = get_num(s, "merge_n") - get_num(s, "_stat_merge");
        double an = get_num(s, "add_n")   - get_num(s, "_stat_add");
        double dn = get_num(s, "drop_n")  - get_num(s, "_stat_drop");
        snprintf(buf, sizeof(buf), "[弹幕] 合并=%d (累计 严%d/宽%d) 新建=%d 丢弃=%d 当前=%d 峰值=%d",
                 (int)mn, (int)get_num(s, "merge1_n"), (int)get_num(s, "merge2_n"),
                 (int)an, (int)dn, (int)get_num(s, "count"), (int)get_num(s, "count_max"));
        Value m[1]; m[0] = mk_str(vm, buf);
        call_fn_v(vm, "show_debug_message", m, 1, -1);
        set_num(s, "_stat_merge", get_num(s, "merge_n"));
        set_num(s, "_stat_add",   get_num(s, "add_n"));
        set_num(s, "_stat_drop",  get_num(s, "drop_n"));
        set_num(s, "_stat_frame", 0);
    }
    return true;
}
