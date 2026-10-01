

	if other.hp > 0 and can_hit(target_type,other.target_type){
		with(other){
			if other.burnt == 1{
				global.audio.play(snd_fire_hit,0,0)
			}
			else{
				global.audio.play(hit_sound,0,0)
			}
			damage_amount = other.damage
			damage_type = other.damage_type
			event_user(0)
	
		}
		if burnt == 0{
			instance_create_depth(x,y,depth,obj_xiaolongbao_bullet_effect)
		}
		else{
			var inst = instance_create_depth(x+25,y,depth,obj_fire_bullet_effect)
			inst.sprite_index = spr_fire_bullet_effect
		}
		instance_destroy()
	}
