// ══════════════════════════════════════════════════════════════════════
// 和敌人碰撞（obj_enemy_parent）：记录 + **按字段自动结算**
//   字段在 _OBJECT_CREATE 里用 VM_SetProp 设（见 obj_bullet_mod/Create_0.gml 的说明）：
//     bullet_damage / bullet_damage_type      伤害
//     bullet_slow / bullet_slow_frame         减速（写敌人 ice_timer）
//     bullet_freeze_chance / bullet_freeze_frame   概率完全冻结（写 frozen_timer）
//     bullet_stun_chance / bullet_stun_frame       概率眩晕（写 stun_timer）
//     bullet_hits                            撞几次后消失；**-1 = 不消耗（默认）**
//                                            用完不是当场销毁，而是 destroy_timer = 0，
//                                            让卡片下一次 Step 先收到这次碰撞（见文件末尾）
//   同时写 mod_collision_enemy / mod_collision_hit，供 "attack_collision" 闸门进 VM
//
//   ⚠️ 触发条件是"两个实例的碰撞掩码相交"，所以子弹必须有有效贴图（sprite_index）。
//   ⚠️ 贴着敌人不动时每帧都会触发一次（伤害/状态也每帧结算一次）。
//   ⚠️ 概率用 random(1)，联机下各客户端可能掷出不同结果。
// ══════════════════════════════════════════════════════════════════════

// 开关：bullet_collide = 0 时整颗子弹**不参与碰撞**（不结算、不记录、attack_collision 也不进）
//   ⚠️ 只是"不理这次碰撞"：重叠检测本身还是会跑（引擎不提供单实例关碰撞掩码的开关）
if (bullet_collide == 0) exit;

// 已经打死的敌人（hp <= 0，正在播死亡动画）不再重复结算一次 —— 和引擎自己的弹幕
// 一个口径（obj_Bullet_Screen_Management 里就是 `if (_e.hp <= 0) continue;`）。
// 不跳的话：贴着一只正在消失的敌人会每帧再扣一遍血、日志也会一直刷。
if (other.hp <= 0) exit;

mod_collision_enemy = other.id;
mod_collision_hit   = 1;
array_push(mod_collision_enemies, other.id);   // 一帧撞到几个就攒几个（进 VM 后自动清空）

// 先把要用的值取出来（后面伤害结算可能把敌人打死）
var _dmg   = bullet_damage;
var _dtype = bullet_damage_type;
var _slow  = (bullet_slow == 1) ? bullet_slow_frame : 0;
var _frz   = (bullet_freeze_chance > 0 && random(1) < bullet_freeze_chance) ? bullet_freeze_frame : 0;
var _stun  = (bullet_stun_chance   > 0 && random(1) < bullet_stun_chance)   ? bullet_stun_frame   : 0;

// ① 伤害：走敌人自己的受击事件（闪白 / 音效 / 护盾判定都对）
if (_dmg > 0) damage_enemy(other.id, _dmg, _dtype);

// ② 状态：取"更长的那一个"，不会把敌人身上已有的更长的状态缩短
if (instance_exists(other) && (_slow > 0 || _frz > 0 || _stun > 0)) {
	with (other) {
		if (_slow > 0 && ice_timer    < _slow) ice_timer    = _slow;
		if (_frz  > 0 && frozen_timer < _frz)  frozen_timer = _frz;
		if (_stun > 0 && stun_timer   < _stun) stun_timer   = _stun;
	}
}

// ③ 消耗次数：第一次撞到才按 bullet_hits 初始化；-1 = 穿透，永不消耗
if (bullet_hits_init == 0) {
	bullet_hits_left = bullet_hits;
	bullet_hits_init = 1;
}
if (bullet_hits != -1) {
	bullet_hits_left -= 1;
	if (bullet_hits_left <= 0) {
		// ⚠️ 不在这里 instance_destroy()：碰撞事件跑在 Step 之后，当场销毁的话
		//    子弹下一次 Step 已经不存在了 —— 卡片的 "attack_collision" / "attack_cell"
		//    闸门永远看不到这次碰撞（伤害照算，但脚本收不到通知）。
		//    destroy_timer = 0 = 交给它自己下一次 Step：先跑 VM（卡片读碰撞列表、打印、放特效），
		//    然后才发现 destroy_timer == 0 → 销毁。
		destroy_timer = 0;
	}
}
