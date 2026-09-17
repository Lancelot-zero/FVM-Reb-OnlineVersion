// VM hook: 敌人销毁时触发（入队延迟到帧末执行，销毁瞬间快照属性供 VM_GetKilledProp 读取）
global._VM_last_killed_enemy = id;
if (buffer_exists(global._VM_ENEMY_KILLED)) VM_QueueHook(global._VM_ENEMY_KILLED, "enemy_kill", id);

if !global.laboretory_room{
	var is_drop = random_range(0,100)
	if is_drop < 10{
		instance_create_depth(x,y-50,depth-200,obj_coin)
	}
}