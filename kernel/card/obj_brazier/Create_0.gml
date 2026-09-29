// obj_small_furnace 的 Create 事件
//plant_id = "small_fire";  // 唯一标识符
event_inherited();  // 继承父对象属性
plant_id = "brazier"; 
// 设置对象类型和精灵

event_user(0)
sprite_index = spr_brazier;
if shape == 1{
	sprite_index = spr_brazier_1
}
else if shape == 2{
	sprite_index = spr_brazier_2
}
if(variable_instance_get(id, "_net_card_equipped_attire_id")==noone){
	if card_equipped_attire_id(plant_id) != -1{
		var spr_list = get_attire_info(card_equipped_attire_id(plant_id)).spr
		sprite_index = spr_list[shape]
	}
}



// ========== 特定属性默认值 ==========
if shape >= 1{
	hp *= 5
	max_hp *= 5
}
attack_anim = 26;
idle_anim = 11
flash_speed = 5
plant_type = "normal"
is_slowdown = false

// ── 弹幕卡片效果：管理器里的子弹进到本格中心带时生效 ──
// bullet_flag 的 bit1 = 过火类（换点燃贴图 + 放大 1.8 + 点燃音效）
// bullet_mul_dmg = 点燃时伤害 × 本卡攻击力
bullet_flag    = 1
bullet_mul_dmg = atk

