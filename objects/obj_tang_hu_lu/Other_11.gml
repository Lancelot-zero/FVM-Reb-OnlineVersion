// 糖葫芦开火：改成往【追踪弹管理器 obj_Homing_Bullet_Management】里塞一颗结构体子弹，
// 不再创建 obj_tanghulu_bullet 实例（那个对象从此不用了，留着备查）。
//
// 口径和原版 obj_tanghulu_bullet 逐条对齐：
//   · 索敌   = 每帧改追「全场最左的可命中敌人」（管理器每帧只扫一遍，同帧所有子弹共享）
//   · 类型   = "air_only"（和卡片 Create 里的 target_type 一致，只打空中）
//   · 速度   = 10，出生点 (x, y-95)，缩放 1.8
//   · 自转   = 6 度/帧（原版 image_angle = -timer*6）
//   · 命中   = 即消失，并在命中处生成 obj_coke_bomb_explode 后盖上 spr_tanghulu_bullet_effect
//   · 音效/闪白/护盾 = 管理器里照抄了敌人的 event_user(0)（播 hit_sound、flash_value、
//                     按 damage_type 结算护盾），所以和原版一致
//
// 目标传 noone：管理器下一帧自己挑最左的可命中敌人，所以卡片这边不用再算目标，
// 上面 Create 里那个 find_priority_enemy() 也就不再被调用了。

var _spr = spr_tanghulu_bullet;
if shape == 1 { _spr = spr_tanghulu_bullet_1 }
if shape == 2 { _spr = spr_tanghulu_bullet_2 }

homing_bullet_add(_spr, 1.8, x, y - 95, 10, atk, "air_only", "obj_coke_bomb_explode", "", "spr_tanghulu_bullet_effect", 0);
