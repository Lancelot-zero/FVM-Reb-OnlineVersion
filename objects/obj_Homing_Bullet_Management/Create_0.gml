// obj_Homing_Bullet_Management —— 追踪弹管理器（专管会拐弯、会追目标的子弹）
//
// 和 obj_Bullet_Screen_Management 同一套思路：一个实例管一堆子弹，子弹是结构体不是实例，
// 省掉每颗子弹自己的 Step / Draw 开销。区别在于每颗子弹**每帧都改追"全场最左的可命中敌人"**
// （同原版糖葫芦：出现更靠左的就换），再用 point_direction + lengthdir 重写 vx/vy。
//
// 绘制：**所有子弹都自转**（角度 = -spin，每帧 +spin_speed 度，默认 6，
//       和原版糖葫芦 / 章鱼烧的 `image_angle = -timer * 6` 一致）。朝向只影响飞行。
//
// 命中：照抄敌人的受击事件 event_user(0) —— 播敌人自己的 hit_sound、闪白、
//       按 damage_type 走护盾结算（normal 只打盾 / pierce 盾血一起 / 其它无视护盾）。
//       判定方式：**只管自己锁定的那个目标**（子弹身上存着 target id），
//       两者的像素距离 <= 90x85 即命中，不扫网格、不穿透、打一下就消失。
//
// list 元素字段（在屏幕管理器那套之上多出来的）：
//   spd         速度大小（追踪必须存标量：vx/vy 是每帧算出来的）
//   angle       当前朝向（度，**只用于飞行**，不用于绘制）
//   spin        自转累加器：每帧 + spin_speed，绘制时取 -spin
//   spin_speed  自转速度（度/帧）
//   target      目标实例 id（每帧从索敌快照重挑）
//   turn        每帧最大转向角（度）；<=0 = 直接指向目标，不留惯性
//   damage_type 伤害类型（normal / pierce / 其它），决定护盾怎么吃伤害
//   death_spr   销毁对象的贴图覆盖（可空）
// 固定值：hits = 1（打一下就消失）、life = -1（不自动到期）、anim_speed = 1。
// 其余字段（spr / frames / scale / frame / x / y / vx / vy / dmg /
// target_type / death_obj / death_mod）含义与 obj_Bullet_Screen_Management 完全一致。
//
// 索敌快照存在这个实例自己身上（见下面的 scan_* / frame_id），详见 Step_0.gml。
//
// 懒创建：homing_bullet_add 发现场上没有就自己造一个；obj_battle 开局也会建一个。

list = [];

depth = -700;   // 和屏幕弹幕管理器同层：在敌人（0 / -200）前面，仍在 UI（-2900 以下）后面

// ── 数量上限 ──
// 子弹只在出界/命中时才删，攻击频率一高就会无限堆积（卡住的更是一直不消失），
// 所以设硬上限：homing_bullet_add 发现 list 已经到 max_bullets 时**不再生成子弹**，
// 改为直接对当帧锁定的目标结算一次伤害（伤害不丢，个数卡死在上限）。
max_bullets = 400;

// ── 索敌快照（每帧只扫一遍全场，同帧所有子弹共享）──
// 按【敌人类型】分 6 个固定桶：normal / air / dance / obstacle / diver / underground，
// 每桶存"最左、同 x 时血最高"的那个敌人。原版糖葫芦 / 章鱼烧是每帧扫一遍缓存在 global，
// 这里放在管理器实例自己身上（全场就一个实例），省掉 global 读写、也不用每帧新建 ds_map。
frame_id   = 0;
scan_id    = array_create(6, noone);
scan_x     = array_create(6, 0);
scan_hp    = array_create(6, -1);
scan_types = ["normal", "air", "dance", "obstacle", "diver", "underground"];
