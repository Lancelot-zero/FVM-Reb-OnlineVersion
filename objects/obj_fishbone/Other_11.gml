with obj_enemy_parent{
		// 友化敌人不算敌人：跳过，不结算伤害
		if (variable_instance_exists(id, "is_friend") && is_friend) continue
		if place_meeting(x,y,other) && hp > 0  && grid_row == other.grid_row && can_hit(other.target_type,target_type){
			hp -= other.atk
			event_user(0)
			global.audio.play(snd_hit1,0,0)
		}
	}