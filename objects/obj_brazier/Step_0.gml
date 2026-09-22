if global.is_paused{
	exit
}
event_inherited(); 
// 弹幕卡片效果：攻击力会随星级/形态变，每帧刷新给弹幕用的值
bullet_mul_dmg = atk
var current_flash_speed = flash_speed
if is_slowdown{
	current_flash_speed *= 2
}


