// Inherit the parent event
if global.is_paused{
	exit
}

if hp <=0 && state != ENEMY_STATE.DEAD{
	state = ENEMY_STATE.DEAD
	timer = 0
}

if state == ENEMY_STATE.ATTACK{
	timer = 0
	state = ENEMY_STATE.ACTING
}

if state == ENEMY_STATE.ACTING{
	if instance_exists(target_plant){
		with target_plant{
			if plant_id != "player"{
				if !invincible{
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
		}
	}
	image_index = floor(timer/5) mod 5 + 10
	if timer == 25-1{
		hp -= maxhp
	}
}

event_inherited();

