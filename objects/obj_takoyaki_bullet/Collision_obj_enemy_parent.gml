// 友化敌人不算敌人：跳过，不结算碰撞
if (variable_instance_exists(other, "is_friend") && other.is_friend) exit



	if other.hp > 0 and target_enemy == other.id  and can_hit(target_type,other.target_type){
		with(other){
			
			global.audio.play(hit_sound,0,0)
			
			damage_amount = other.damage
			damage_type = other.damage_type
			event_user(0)
	
		}

		instance_create_depth(x,y,depth,obj_takoyaki_bullet_effect)
		
		global._takoyaki_buttet_number-=1
		instance_destroy()
		
	}
