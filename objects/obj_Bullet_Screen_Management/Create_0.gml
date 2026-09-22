// obj_Bullet_Screen_Management —— 屏幕子弹管理器（弹幕管理器）
// 由 obj_battle 的 Create 开局创建一个（全场就这一个）。
// 所有子弹都塞进 list，由它统一迭代、统一绘制。
// 目的是把「每颗子弹一个实例」换成「一个实例管一堆子弹」，省掉实例自身的 Step / Draw 开销。
//
// list 每个元素是一个结构体，字段：
//   spr          贴图（sprite 资源索引；字符串形式在 bullet_screen_add 里已查缓存转成索引）
//   frames       总帧数（这段动画一共多少帧）
//   cell_range   可打击范围：方格子半径，以子弹所在格为中心上下左右各扩 N 格；0 = 只算本格
//   scale        缩放（横竖同值）
//   angle        绘制角度（度）：默认 0，只有卡片效果翻转方向时才 +180（否则看着像没弹回去）
//   anim_speed   动画速度：每帧 frame 自增多少，可以是小数；1 = 每帧走一帧
//   frame        帧数计数（每帧 += anim_speed，到 frames 回绕，循环播放；小数累加器）
//   x, y         坐标
//   vx, vy       每帧速度
//   dmg          伤害（每帧结算一次的伤害值）
//   hits         伤害计数：正数 = 最多命中多少个，每命中一个减 1，减到 0 移除
//                （所以 1 = 打一个就消失，3 = 同一帧内最多穿三个；同一个敌人每帧最多挨一次，
//                  按格子里敌人数组的顺序结算，不按距离排序）；
//                -1 = 不限次数，范围内**所有**可命中敌人都结算；0 = 立即移除
//   life         存活帧数：倒计时，每帧 -1，减到 0 移除；初始 <= 0（如 -1）表示不自动移除
//   target_type  子弹类型：决定能命中哪些敌人（can_hit 的 card_target_type 那一套：
//                all / normal / air / air_only / pierce / track / throw / rotate / d_fruit）
//   damage_type  伤害类型（bullet_screen_add 里固定 "normal"）：命中走 damage_enemy，
//                也就是敌人的受击事件（闪白/音效/护盾 + 各敌人自己重写的 Other_10）；
//                normal = 有盾只打盾 / pierce = 盾血一起掉 / 其它 = 无视护盾
//   death_obj    销毁对象：非空时，在这颗子弹销毁的位置创建这个对象（字符串资产名；
//                写 "xxx" 或 "obj_xxx" 都行，找不到会补一次 "obj_" 前缀再找）
//                注意：出界销毁**不**生成销毁对象
//   death_mod    销毁对象是 mod 对象时，填它的 mod 名字（创建后写进 mod_type）；
//                原生对象留空
//
//   —— 卡片效果（只有 bullet_screen_add_Ex / VM_BulletScreenAdd_Ex 加的子弹才参与）——
//   flag         标志数值：这颗子弹**还能接受哪些类别**的卡片效果（位掩码）
//                bit1 = 过火类 / bit2 = 解冻类 / 4、8、16… = 自定义类
//                子弹进到格子中心带时与本格卡片的 bullet_flag 取且运算，>0 就应用并消位，
//                所以同一类卡片对同一颗子弹只生效一次；解冻位生效后额外「或上 1」= 变成能被点燃
//   freeze       累计冰冻帧数：命中时写给敌人的 ice_timer
//   卡片侧字段（实例变量，mod 卡用 VM_SetProp 写）：
//                bullet_flag（去重位）、bullet_mul_dmg（乘伤害，默认1）、bullet_add_dmg（加伤害，默认0）、
//                bullet_flip_x / bullet_flip_y（反向，0/1）、bullet_angle_add（画面旋转角度，默认0）、
//                bullet_freeze_mul（乘冰冻帧，默认1）、bullet_freeze_add（加冰冻帧，默认0）
//
// 消失条件有三条：出界（x<0 / x>2200 / y<0 / y>1200，静默删）、伤害次数用完、存活帧数走完。

list = [];

// 画在敌人（0 / -200）前面，但仍在 UI（-2900 以下）后面
depth = -700;
