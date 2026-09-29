// obj_small_furnace 的 Create 事件
//plant_id = "small_fire";  // 唯一标识符
event_inherited();  // 继承父对象属性
plant_id = "cherry_pudding"; 
// 设置对象类型和精灵

event_user(0)
sprite_index = spr_cherry_pudding;
if shape == 1{
	sprite_index = spr_cherry_pudding_1
}
else if shape == 2{
	sprite_index = spr_cherry_pudding_2
}
// ========== 特定属性默认值 ==========
attack_anim = 26;
idle_anim = 9
flash_speed = 6
plant_type = "normal"
is_slowdown = false

heal_wait = 0
current_hp = 0
bleed_damage = 0

_net_cookbook_equipped = ""

if(is_cookbook_equipped("pineapple_pudding")){
	_net_cookbook_equipped= "pineapple_pudding"
}

// ── 弹幕卡片效果：管理器里的子弹进到本格中心带时生效 ──
// bullet_flag 的 bit4 = 自定义类（反弹）：伤害 + 本卡攻击力、飞行方向掉头、画面转 180°
// 两个轴都反：横向弹 vy≈0、竖向弹 vx≈0，各自只有主导轴起作用；斜弹整体掉头
bullet_flag      = 4
bullet_add_dmg   = atk
bullet_flip_x    = 1
bullet_flip_y    = 1
bullet_angle_add = 180