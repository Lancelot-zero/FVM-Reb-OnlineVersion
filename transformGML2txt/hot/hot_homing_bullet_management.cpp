// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// ============================================================
// obj_Homing_Bullet_Management —— 追踪弹管理器（3 个块，12.9 KB）
//   一个实例管一堆子弹，子弹是**结构体**（不是实例）。
//   每帧先是"全场索敌快照"（按敌人类型分 6 个桶），再逐颗重挑目标 / 转向 / 推进 / 命中 / 到期。
//
//   ── 存储：原生 C++ 数组，不是 VM 容器 ──
//   原版 list 是"动态数组 + 每颗子弹一个 ds_map"，字段全用字符串键查表；这里换成定长
//   POD 数组 g_homing[]，字段就是成员变量。为什么能换：这个结构的读写两端都在 C++
//   ——读是这个文件的 Step/Draw，写是原版 scr_command_VM.gml 的 homing_bullet_add
//   （scripts/ 不参与转换，那是被 C++ VM 取代的那一层）。361 个 bin 全扫过：
//   scan_id / scan_x / scan_hp / scan_bit / max_bullets / spin_speed / anchor 全 0 命中，
//   GML 侧没人扒这个结构。
//
//   容量固定 HB_MAX = 1000（原版是动态增长、上限 max_bullets = 400）。
//   长度就是 g_homing_n（等价 GML 的 array_length(list)）：
//     加弹 → 原地填 g_homing[g_homing_n++]；删除 → 末尾那条挪到 i，n 减一。
//   ⚠ 单例：全场只有 obj_Homing_Bullet_Management 那一个实例（homing_bullet_add 找不到
//     会自己建一个）；再建第二个就会共用这张表（原版每实例一份）。
// ============================================================

static const int HB_MAX = 1000;          // = Create 写进实例变量的 max_bullets

struct HomingBullet {
    Value  spr;            // 贴图（sprite 索引）
    Value  damage_type;    // 伤害类型（damage_enemy 的护盾口径）
    Value  death_obj;      // 销毁对象（资产名），空 = 不生成
    Value  death_mod;      // 销毁对象是 mod 对象时填 mod 名
    Value  death_spr;      // 销毁对象的贴图覆盖
    double x, y, vx, vy;
    double spd, dmg, hits, life;
    double scale, angle, anim_speed, frame, frames;
    double spin, spin_speed;
    int    target;         // 锁定目标（实例 id；-4 = noone，每帧按 mode 重挑）
    int    anchor;         // 模式 1 的"发射卡"实例 id（-4 = 没有）
    int    mode;           // 索敌模式：0 = 全场最左；1 = 先打发射卡前方 150px
    int    ttype;          // 命中位掩码
};
static HomingBullet g_homing[HB_MAX];
static int g_homing_n = 0;               // 活动子弹数（= GML 的 array_length(list)）

// 索敌快照：6 个桶 = normal / air / dance / obstacle / diver / underground
//   （顺序必须和 bullet_type_mask 的位定义一致；原版还有个 scan_types 存这 6 个名字，
//     但只写不读，删了 —— 顺序就写在这行注释里）
static int    g_scan_id[6];              // 每桶"最左、同 x 时血最高"的敌人 id（-4 = noone）
static double g_scan_x[6];
static double g_scan_hp[6];
static const int g_scan_bit[6] = { 1, 2, 4, 8, 16, 32 };

// Create_0.gml（52 行）
bool hot_homing_create(Vm& vm) {
    int s = vm.cur_instance;
    Instance* in = inst_get(s);
    if (!in) return true;

    in->depth = mk_num(-700);
    in->vars["max_bullets"] = mk_num(HB_MAX);     // ⚠ 要跟 g_homing 的大小一致

    // 原版这里 list = []、scan_id/x/hp = array_create(6, ...)、scan_types/scan_bit = [...]：
    // 全是本对象自己的内部量，换成文件级原生数组后 Create 只要把状态清干净。
    g_homing_n = 0;
    for (int t = 0; t < 6; t++) { g_scan_id[t] = -4; g_scan_x[t] = 0; g_scan_hp[t] = -1; }
    return true;
}


// Draw_0.gml（12 行）—— 逐条画，所有追踪弹都自转（角度 = -spin）
bool hot_homing_draw(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    for (int i = g_homing_n - 1; i >= 0; i--) {
        HomingBullet& b = g_homing[i];
        if (num_of(&b.spr) == -1) continue;
        Value d[9];
        d[0] = b.spr;
        d[1] = mk_num(std::floor(b.frame));
        d[2] = mk_num(b.x);
        d[3] = mk_num(b.y);
        d[4] = mk_num(b.scale);
        d[5] = mk_num(b.scale);
        d[6] = mk_num(-b.spin);
        d[7] = mk_num(16777215);                                  // c_white
        d[8] = mk_num(1);
        call_fn_v(vm, "draw_sprite_ext", d, 9, -1);
    }
    return true;
}


// Step_0.gml（184 行）—— 索敌快照 → 重挑目标 → 转向 → 推进 → 命中 → 到期
bool hot_homing_step(Vm& vm) {
    int s = vm.cur_instance;
    if (!inst_get(s)) return true;
    if (gtrue(vm, "is_paused")) return true;

    int len = g_homing_n;
    if (len <= 0) return true;

    // ── 索敌快照：按敌人类型分 6 桶，每桶存"最左、同 x 时血最高" ──
    //    ⚠ with (obj_enemy_parent) 在 GML 里**包含子对象**（obj_enemy_mod），inst_is 管这个
    for (int t = 0; t < 6; t++) {
        g_scan_id[t] = -4;                    // noone
        g_scan_x[t]  = 0;
        g_scan_hp[t] = -1;
    }
    for (size_t k = 1; k < g_instances.size(); k++) {
        if (!g_instances[k].alive) continue;
        if (!inst_is((int)k, "obj_enemy_parent")) continue;
        int e = (int)k;
        if (!(get_num(e, "hp") > 0 && get_num(e, "y") > 0)) continue;
        const Value* tt = inst_var(e, "target_type");
        int slot = -1;
        if      (str_is(vm, tt, "normal"))      slot = 0;
        else if (str_is(vm, tt, "air"))         slot = 1;
        else if (str_is(vm, tt, "dance"))       slot = 2;
        else if (str_is(vm, tt, "obstacle"))    slot = 3;
        else if (str_is(vm, tt, "diver"))       slot = 4;
        else if (str_is(vm, tt, "underground")) slot = 5;
        if (slot < 0) continue;
        int    sid = g_scan_id[slot];
        double sx  = g_scan_x[slot], shp = g_scan_hp[slot];
        double ex = get_num(e, "x"), ehp = get_num(e, "hp");
        if (sid == -4 || ex < sx || (ex == sx && ehp > shp)) {
            g_scan_id[slot] = e;
            g_scan_x[slot]  = ex;
            g_scan_hp[slot] = ehp;
        }
    }

    // 网格参数只读一次（只有模式 1 的"本行正前方"索敌要用）
    bool has_grid = global_exists("enemy_array") && global_exists("grid_cols") && global_exists("grid_rows");
    double stride = has_grid ? gnumv(vm, "grid_cols") + 2 : 0;
    double rows   = has_grid ? gnumv(vm, "grid_rows") : 0;
    Value enemy_array; enemy_array.t = MEM_UNDEFINED; enemy_array.v = 0;
    if (has_grid) enemy_array = gv(vm, "enemy_array");

    for (int i = len - 1; i >= 0; i--) {
        HomingBullet& b = g_homing[i];
        bool gone = false;
        b.spin += b.spin_speed;                                                     // 自转

        // ── 0. 追踪：重挑目标 + 重算 vx/vy（必须在推进之前）──
        if (b.spd != 0) {
            int best = -4;
            int anchor = b.anchor;
            if (b.mode == 1 && has_grid && inst_get(anchor)) {
                double arow = get_num(anchor, "grid_row"), acol = get_num(anchor, "grid_col");
                if (arow >= 0 && arow < rows && acol >= 0) {
                    double ax = get_num(anchor, "x");
                    double rowb = arow * stride;
                    double cend = std::fmin(acol + 1, stride - 1);
                    double bhp = -1;
                    for (double fc = acol; fc <= cend; fc++) {
                        Value farr = array_v(vm, enemy_array, (int)(rowb + fc));
                        if (farr.t != MEM_ARRAY) continue;
                        int fn = container_size(farr);
                        for (int fk = 0; fk < fn; fk++) {
                            Value fe = array_v(vm, farr, fk);
                            int fid = (int)num_of(&fe);
                            if (!inst_get(fid)) continue;
                            if (get_num(fid, "hp") <= 0 || get_num(fid, "y") <= 0) continue;
                            if ((b.ttype & (int)get_num(fid, "tbit")) == 0) continue;
                            double fx = get_num(fid, "x");
                            if (fx < ax || fx > ax + 150) continue;
                            if (get_num(fid, "hp") > bhp) { bhp = get_num(fid, "hp"); best = fid; }
                        }
                    }
                }
            }
            if (best == -4) {                                  // 兜底：快照里最左的可命中敌人
                double best_x = gnumv(vm, "room_width"), best_hp = -1;
                for (int qt = 0; qt < 6; qt++) {
                    int qid = g_scan_id[qt];
                    if (qid == -4) continue;
                    if ((b.ttype & g_scan_bit[qt]) == 0) continue;
                    double qx = g_scan_x[qt], qhp = g_scan_hp[qt];
                    if (qx < best_x || (qx == best_x && qhp > best_hp)) { best_x = qx; best_hp = qhp; best = qid; }
                }
            }
            int tg = b.target;
            if (best != -4) b.target = best;
            else if (!inst_get(tg) || get_num(tg, "hp") <= 0) b.target = -4;

            tg = b.target;
            if (inst_get(tg)) {
                // 不留惯性：每帧直接把朝向设成"正对目标"（瞄 y-75）
                Value pa[4];
                pa[0] = mk_num(b.x); pa[1] = mk_num(b.y);
                pa[2] = mk_num(get_num(tg, "x"));  pa[3] = mk_num(get_num(tg, "y") - 75);
                double ang = num_of(&call_ret(vm, "point_direction", pa, 4));
                b.angle = ang;
                Value la[2]; la[0] = mk_num(b.spd); la[1] = mk_num(ang);
                b.vx = num_of(&call_ret(vm, "lengthdir_x", la, 2));
                b.vy = num_of(&call_ret(vm, "lengthdir_y", la, 2));
            }
        }

        // ── 1. 推进 ──
        double bx = b.x + b.vx;
        double by = b.y + b.vy;
        b.x = bx;
        b.y = by;

        if (bx < 0 || bx > 2200 || by < 0 || by > 1200) {
            gone = true;                                       // 出界：静默删除
        } else {
            // ── 3. 动画帧 ──
            if (b.frames > 1) {
                double fr = b.frame + b.anim_speed;
                if (fr >= b.frames || fr < 0) {
                    fr = std::fmod(fr, b.frames);
                    if (fr < 0) fr += b.frames;
                }
                b.frame = fr;
            }
            // ── 4. 命中：只认锁定的那个目标（像素距离口径，和原版一致）──
            int tg = b.target;
            if (b.hits != 0 && inst_get(tg) && get_num(tg, "hp") > 0) {
                if ((b.ttype & (int)get_num(tg, "tbit")) != 0
                        && std::fabs(get_num(tg, "x") - bx) <= 90
                        && std::fabs(get_num(tg, "y") - by) <= 85) {
                    // 走敌人自己的受击事件（闪白 + 音效 + 护盾），不在这儿手抄父对象那 16 行
                    Value da[3];
                    da[0] = mk_num(tg);
                    da[1] = mk_num(b.dmg);
                    da[2] = b.damage_type;
                    call_fn_v(vm, "damage_enemy", da, 3, -1);
                    b.hits = 0;
                }
            }
            // ── 5. 存活倒计时 ──
            if (b.life > 0) b.life -= 1;
            // ── 6. 到期 → 生成销毁对象 ──
            if (b.hits == 0 || b.life == 0) {
                // GML: asset_get_index(death_obj)，没有就再试 "obj_" + death_obj。
                // 我们这边"对象"就是名字（没有数字 asset 索引），所以这里换成"名字认不认得"：
                //   obj_known 查的就是 VM 块表 + 硬编码表，跟 asset_get_index != -1 一个意思
                std::string nm = pool_text(vm.strings, &b.death_obj);
                if (!nm.empty()) {
                    std::string full = nm;
                    if (!obj_known(vm, full)) full = "obj_" + nm;
                    if (obj_known(vm, full)) {
                        Value ca[4];
                        ca[0] = mk_num(bx); ca[1] = mk_num(by);
                        ca[2] = mk_num(get_num(s, "depth") - 1);
                        ca[3] = mk_str(vm, full.c_str());
                        Value d = call_ret(vm, "instance_create_depth", ca, 4);
                        int did = d.t == MEM_INT ? d.v : -1;
                        if (inst_get(did)) {
                            if (!pool_text(vm.strings, &b.death_mod).empty())
                                inst_var_set(did, "mod_type", b.death_mod);
                            if (!pool_text(vm.strings, &b.death_spr).empty()) {
                                Value ga[1]; ga[0] = b.death_spr;
                                inst_var_set(did, "sprite_index", call_ret(vm, "get_load_sprite", ga, 1));
                            }
                        }
                    }
                }
                gone = true;
            }
        }

        // ── 7. 交换删除：末尾那条挪到 i，长度减一（原版是 list[i] = list[last] + array_delete）
        if (gone) {
            g_homing_n--;
            if (i != g_homing_n) g_homing[i] = g_homing[g_homing_n];
        }
    }
    return true;
}
