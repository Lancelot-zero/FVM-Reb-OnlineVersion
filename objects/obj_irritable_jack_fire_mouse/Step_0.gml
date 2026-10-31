// Inherit the parent event
if global.is_paused{
	exit
}

if hp <=0 && state != ENEMY_STATE.DEAD{
	state = ENEMY_STATE.DEAD
	timer = 0
}

if state == ENEMY_STATE.DEAD{
	image_alpha -= 0.1
	if image_alpha <= 0{
		instance_destroy()
	}
}

if state == ENEMY_STATE.ATTACK && hp > 0{
	timer = 0
	state = ENEMY_STATE.ACTING
}

event_inherited();

if state == ENEMY_STATE.ACTING && hp > 0 && state != ENEMY_STATE.DEAD{
	if instance_exists(target_plant){
		with target_plant{
			if plant_id != "player"{
				if !invincible && plant_id != "cotton_candy"{
					hp -= 2000
					array_push(last_attack_ids, other.id)
					array_push(last_attack_classes, object_get_name(other.object_index))
					array_push(last_attack_types, "enemy")
					array_push(last_attack_damages, 2000)
					event_user(2)
				}
			}
			else{
				var _dmg = max(hp - 10, 0)
				hp = 10
				array_push(last_attack_ids, other.id)
				array_push(last_attack_classes, object_get_name(other.object_index))
				array_push(last_attack_types, "enemy")
				array_push(last_attack_damages, _dmg)
				event_user(2)
			}
			var effect = instance_create_depth(x-15,y+15,-800,obj_wine_bottle_bomb_explode)
			effect.interval = 0
			effect.sprite_index = spr_julie_incendiary_bomb_effect
			effect.max_time = 45
			effect.is_parent = true
			effect.row = grid_row
			effect.col = grid_col
		}
	}
	instance_destroy()
}

