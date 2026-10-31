// obj_Fixed_Bullet_Management —— 定点子弹管理器（位置固定的效果弹）
// 和 obj_Bullet_Screen_Management 同一套思路：一个实例管一堆子弹，子弹是结构体不是实例。
// 区别：伤害**定点**结算——播完最后一帧在伤害点结算并销毁。
//       贴图本身可选动：设出身位置 + 移动速度后，动画会从出生点飞向伤害点（纯视觉，不影响结算点）。
// 用途：爆炸 / 地面效果 / 召唤物这类"落点播动画 → 最后一下出伤害"的效果。
// 懒创建：bullet_fixed_add 发现场上没有就自己造一个（和 homing_bullet_add 一样）。
//
// list 元素字段：
//   spr          贴图（sprite 资源索引；字符串形式在 bullet_fixed_add 里已查缓存转成索引）
//   frames       总帧数（动画一共多少帧；<=0 或超过贴图实际帧数时取贴图自身帧数）
//   frame        帧数计数（每帧 += anim_speed，到 frames 结算并移除；不循环）
//   anim_speed   动画速度：每帧 frame 自增多少，可以是小数；1 = 每帧走一帧
//   scale        缩放（横竖同值）
//   angle        绘制角度（度）：定点弹不移动，角度就是贴图朝向，默认 0
//   x, y         伤害定点（像素）：最后一帧在这结算伤害/生成销毁对象；贴图动画不一定在这画
//   dmg          伤害（最后一帧结算；<=0 = 纯表现不结算伤害）
//   radius       结算范围半径（像素，圆形距离）：只在范围攻击（没指定 target_id）时用
//   target_id    独立攻击：指定敌人实例 id（noone = 不指定）。指定后**只**对这个敌人结算伤害
//                （先判断存活且 hp>0，不看半径、不看命中掩码）；不指定才按 radius 扫范围打全体
//   target_type  子弹类型：决定能命中哪些敌人（can_hit 那一套：all / normal / air / ...）
//   ttype        命中掩码（bullet_type_mask 算好写进来的），判定时和敌人的 tbit 位与
//   damage_type  伤害类型：normal = 有盾只打盾 / pierce = 盾血一起掉 / 其它（explosion 等）= 无视护盾
//   freeze       冷冻减速帧数：>0 时写给命中敌人的 ice_timer（只加不减；受 ice_ok 限制）
//   frozen       冷冻冻住帧数：>0 时写给命中敌人的 frozen_timer（只加不减；受 ice_ok 限制）
//                减速（is_slowdown）和冻住（is_frozen）是两套独立计时器，分开传
//   death_obj    销毁对象：非空时在这颗子弹销毁的位置创建该对象（字符串资产名；
//                mod 对象配合 death_mod 填 mod 名，原生对象留空）
//   death_spr    销毁对象的贴图覆盖：非空时把创建出来的销毁对象贴图换成它（皮肤特效用）
//   spawn_x/y    出身位置：贴图动画的绘制起点（像素）；动画从这里出发按 spd 飞向伤害点 (x, y)
//   draw_x/y     当前绘制位置（每帧按 spd 向伤害点推进；spd=0 时 = 出生点不动）
//   spd          移动速度：贴图每帧向伤害点移动的像素；0 = 原地播（经典定点弹）
//
// 消失条件只有一条：动画播完（frame >= frames），结算完就移除。

// 子弹表：**固定大小的数组**，每个槽位预先放好一颗"空子弹"结构体 + count 表示前几个是有效的
//   [0, count) 是有效子弹，[count, bullet_max) 是回收池
//   新增：拿 list[count] 这个现成结构体原地填字段，然后 count += 1（不新建结构体）
//   删除：list[i] = list[count-1]（末尾那颗挪过来顶替），再把死掉的那颗放回末尾当回收池
//   全程不 array_push / array_delete（数组长度永远不变）
bullet_max = 200;                        // 容量：改这个数就行
list       = array_create(bullet_max);
for (var _i = 0; _i < bullet_max; _i++) {
    list[_i] = {
        spr:         -1,
        frames:      1,
        frame:       0,
        anim_speed:  1,
        scale:       1,
        angle:       0,
        x:           0,
        y:           0,
        dmg:         0,
        radius:      0,
        target_id:   noone,
        target_type: "all",
        ttype:       127,  // 命中掩码（bullet_type_mask 算好写进来的），判定时和敌人的 tbit 位与
        damage_type: "normal",
        freeze:      0,
        frozen:      0,
        death_obj:   "",
        death_mod:   "",
        death_spr:   "",
        spawn_x:     0,
        spawn_y:     0,
        draw_x:      0,
        draw_y:      0,
        spd:         0
    };
}
count = 0;

depth = -700;   // 和另外两个子弹管理器同层：在敌人（0 / -200）前面，仍在 UI（-2900 以下）后面
