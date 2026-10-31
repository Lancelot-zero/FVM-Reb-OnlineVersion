// Inherit the parent event
if skipped or hp <= 0{
	sprite_index = spr_machine_skateboard_mouse
	move_anim = 8
	attack_anim = 6
	death_anim = 15
	move_speed = 0.3
}

// 修改后的僵尸Step事件
if global.is_paused{
	exit
}
// 友化相关字段兜底（照父类）
if (!variable_instance_exists(id, "is_friend")) is_friend = 0;
if (!variable_instance_exists(id, "target_enemy")) target_enemy = noone;
if ice_timer > 0{
	ice_timer--
	is_slowdown = true
}
else{
	is_slowdown = false
}
if frozen_timer > 0{
	current_frozen = true
	frozen_timer--
	is_frozen = true
}
else{
	is_frozen = false
}
if scare_timer > 0{
	scare_timer--
	is_scare = true
}
else{
	is_scare = false
}
if flash_value > 0 {
	flash_value -= 10
}
if hp <= 0{
	frozen_timer = 0
	scare_timer = 0
	left_move_flashs = 0
	stun_timer = 0
}
var current_atk_cycle = 0
var current_move_speed = 0
if is_slowdown{
	flash_speed = 12
	current_move_speed = move_speed / 2
	current_atk_cycle = atk_cycle*2
}
else{
	flash_speed = 6
	current_move_speed = move_speed
	current_atk_cycle = atk_cycle
}
if left_move_flashs > 0{
	y += y_move
	left_move_flashs--
}
if is_frozen || is_scare{
	exit
}

var zombie_grid = get_grid_position_from_world(x, y);

timer++;

// 状态处理前，先检查目标植物是否存在
if (instance_exists(target_plant) && target_plant.hp <= 0) {
    target_plant = noone;  // 目标已被消灭
}
if (instance_exists(target_enemy) && target_enemy.hp <= 0) {
    target_enemy = noone;  // 目标敌人已被消灭
}

// 状态机
switch(state) {
    case ENEMY_STATE.IDLE: {
        // 空闲状态不执行操作
        break;
    }
    
    case ENEMY_STATE.NORMAL: {
        // 移动和动画逻辑
        // 友化倒戈往右走
        if (is_friend) {
            x += current_move_speed;
        } else {
            x -= current_move_speed;
        }
        
		if helmet_hp > 0 && hp > maxhp - helmet_hp{
			if ((hp + helmet_hp - maxhp)/maxhp > hurt_rate) {
	            image_index = floor(timer / flash_speed) mod move_anim;
	        } else {
	            image_index = (floor(timer / flash_speed) mod move_anim) + move_anim;
	        }
		}
		else{
	        if (hp/(maxhp - helmet_hp) > hurt_rate) {
	            image_index = floor(timer / flash_speed) mod move_anim;
	        } else {
	            image_index = (floor(timer / flash_speed) mod move_anim) + move_anim;
	        }
		}
        
        // ── 友化互攻索敌：友化找正常敌人（前方=右），正常找面前的友化敌人 ──
        var enemy_in_range = noone;
        if (variable_global_exists("enemy_array") && variable_global_exists("enemy_array_friend")
            && zombie_grid.row >= 0 && zombie_grid.row < global.grid_rows) {
            var _stride = global.grid_cols + 2;
            var _start_c = max(zombie_grid.col - 2, 0);
            var _end_c = min(zombie_grid.col + 2, _stride - 1);
            var _elist = (is_friend) ? global.enemy_array : global.enemy_array_friend;
            for(var _c = _start_c; _c <= _end_c; _c++)
            {
                var _el = _elist[zombie_grid.row * _stride + _c];
                for(var _ei = 0; _ei < array_length(_el); _ei++)
                {
                    var _e = _el[_ei];
                    if (!instance_exists(_e)) continue;
                    var _et = (variable_instance_exists(_e, "target_type")) ? _e.target_type : "";
                    if (_et != target_type) continue;
                    var _edx = _e.x - x;
                    var _efront = false;
                    if (attack_range > 0) {
                        _efront = (is_friend) ? (_edx > 0 && _edx < attack_range)
                                              : (_edx < 0 && _edx > -attack_range);
                    } else {
                        _efront = (is_friend) ? (_edx < 0 && _edx > attack_range)
                                              : (_edx > 0 && _edx < -attack_range);
                    }
                    if (_efront) {
                        enemy_in_range = _e;
                        break;
                    }
                }
                if (enemy_in_range != noone) break;
            }
        }
        if (enemy_in_range != noone) {
            state = ENEMY_STATE.ATTACK;
            target_enemy = enemy_in_range;
            target_plant = noone;
            attack_timer = 0;  // 重置攻击计时器
            timer = 0;         // 重置动画计时器
            break;
        }
        if (is_friend) break;   // 友化不啃植物，没敌人可打就继续往右走

        // 检测前方植物
        var plant_in_range = noone;
		
		var plant_order_list = global._eat_scratch;
		plant_order_list[0] = noone; plant_order_list[1] = noone; plant_order_list[2] = noone; plant_order_list[3] = noone;

		if (variable_global_exists("grid_plants") && ds_exists(global.grid_plants, ds_type_grid)) {
			var start_c = max(zombie_grid.col - 2, -1);
			var end_c = min(zombie_grid.col + 2, global.grid_cols - 1);

			for(var _c = start_c; _c <= end_c; _c++)
			{
				if(zombie_grid.row<0||zombie_grid.row>=global.grid_rows)continue;
				var _list = ds_grid_get(global.grid_plants, (_c+global.grid_cols+64)%(global.grid_cols+64), zombie_grid.row);
				for(var _item = 0; _item < ds_list_size(_list); _item++)
				{
					var plant_inst = ds_list_find_value(_list, _item);
					if (!instance_exists(plant_inst)) continue;

					var dx = plant_inst.x - x;
					var dy = plant_inst.y - y;
					var is_in_front = false;

					if (attack_range > 0) {
						is_in_front = (dx < 0 && dx > -attack_range);
					} else {
						is_in_front = (dx > 0 && dx < -attack_range);
					}

					if (is_in_front && plant_inst.grid_row == zombie_grid.row && (plant_inst.feature_type != "dwarf" || (plant_inst.feature_type == "dwarf" && giant_type)))
					{
						for (var i = 0; i < ds_list_size(global.eat_order); i++)
						{
							var tar_type = ds_list_find_value(global.eat_order, i);
							if (plant_inst.plant_type == tar_type)
							{
								plant_order_list[i] = plant_inst;
								break;
							}
						}
					}
				}
			}
		}
		
		for(var i = 0 ; i < 4 ; i++){
			if plant_order_list[i] != noone{
				plant_in_range = plant_order_list[i]
				break
			}
		}
        
        // 如果找到目标植物，进入攻击状态
        if (plant_in_range != noone) {
            state = ENEMY_STATE.ATTACK;
            target_plant = plant_in_range;
            target_enemy = noone;
            attack_timer = 0;  // 重置攻击计时器
            timer = 0;         // 重置动画计时器
        }
        break;
    }
    
	case ENEMY_STATE.ACTING:{
		break;
	}
	
    case ENEMY_STATE.ATTACK: {
        // 攻击动画
        if (hp/maxhp > hurt_rate) {
            image_index = (floor(timer / flash_speed) mod attack_anim + move_anim * 2);
        } else {
            image_index = (floor(timer / flash_speed) mod attack_anim + move_anim * 2 + attack_anim);
        }
        if not skipped{
			if instance_exists(target_plant){
				if array_get_index(block_list,target_plant.plant_id) == -1{
			
					move_speed = 1.80
				}
				else{
					move_speed = 0
				}
			}
			else{
				move_speed = 1.8
			}
			if  ((hp/maxhp > hurt_rate) && image_index == move_anim * 2 + attack_anim-1) or ((hp/maxhp <= hurt_rate) && image_index == move_anim * 2 + attack_anim*2-1){
				skipped = true
			}
			if (is_friend) {
				x += current_move_speed;
			} else {
				x -= current_move_speed;
			}
		}
		
		else{
	        // ── 友化互攻索敌（同 NORMAL 那套）──
	        var enemy_in_range = noone;
	        if (variable_global_exists("enemy_array") && variable_global_exists("enemy_array_friend")
	            && zombie_grid.row >= 0 && zombie_grid.row < global.grid_rows) {
	            var _stride = global.grid_cols + 2;
	            var _start_c = max(zombie_grid.col - 2, 0);
	            var _end_c = min(zombie_grid.col + 2, _stride - 1);
	            var _elist = (is_friend) ? global.enemy_array : global.enemy_array_friend;
	            for(var _c = _start_c; _c <= _end_c; _c++)
	            {
	                var _el = _elist[zombie_grid.row * _stride + _c];
	                for(var _ei = 0; _ei < array_length(_el); _ei++)
	                {
	                    var _e = _el[_ei];
	                    if (!instance_exists(_e)) continue;
	                    var _et = (variable_instance_exists(_e, "target_type")) ? _e.target_type : "";
	                    if (_et != target_type) continue;
	                    var _edx = _e.x - x;
	                    var _efront = false;
	                    if (attack_range > 0) {
	                        _efront = (is_friend) ? (_edx > 0 && _edx < attack_range)
	                                              : (_edx < 0 && _edx > -attack_range);
	                    } else {
	                        _efront = (is_friend) ? (_edx < 0 && _edx > attack_range)
	                                              : (_edx > 0 && _edx < -attack_range);
	                    }
	                    if (_efront) {
	                        enemy_in_range = _e;
	                        break;
	                    }
	                }
	                if (enemy_in_range != noone) break;
	            }
	        }
	        if (enemy_in_range != noone) {
	            target_enemy = enemy_in_range;
	            target_plant = noone;
	        }
	        else if (is_friend) {
	            // 友化互攻目标没了 → 回走路状态继续往右索敌
	            state = ENEMY_STATE.NORMAL;
	            target_enemy = noone;
	            attack_timer = 0;  // 重置攻击计时器
	            timer = 0;         // 重置动画计时器
	        }
	        else{
	        // 检测前方植物
	        var plant_in_range = noone;
        
		var plant_order_list = global._eat_scratch;
		plant_order_list[0] = noone; plant_order_list[1] = noone; plant_order_list[2] = noone; plant_order_list[3] = noone;

		if (variable_global_exists("grid_plants") && ds_exists(global.grid_plants, ds_type_grid)) {
			var start_c = max(zombie_grid.col - 2, -1);
			var end_c = min(zombie_grid.col + 2, global.grid_cols - 1);

			for(var _c = start_c; _c <= end_c; _c++)
			{
				if(zombie_grid.row<0||zombie_grid.row>=global.grid_rows)continue;
				var _list = ds_grid_get(global.grid_plants, (_c+global.grid_cols+64)%(global.grid_cols+64), zombie_grid.row);
				for(var _item = 0; _item < ds_list_size(_list); _item++)
				{
					var plant_inst = ds_list_find_value(_list, _item);
					if (!instance_exists(plant_inst)) continue;

					var dx = plant_inst.x - x;
					var dy = plant_inst.y - y;
					var is_in_front = false;

					if (attack_range > 0) {
						is_in_front = (dx < 0 && dx > -attack_range);
					} else {
						is_in_front = (dx > 0 && dx < -attack_range);
					}

					if (is_in_front && plant_inst.grid_row == zombie_grid.row && (plant_inst.feature_type != "dwarf" || (plant_inst.feature_type == "dwarf" && giant_type)))
					{
						for (var i = 0; i < ds_list_size(global.eat_order); i++)
						{
							var tar_type = ds_list_find_value(global.eat_order, i);
							if (plant_inst.plant_type == tar_type)
							{
								plant_order_list[i] = plant_inst;
								break;
							}
						}
					}
				}
			}
		}
			for(var i = 0 ; i < 4 ; i++){
				if plant_order_list[i] != noone{
					plant_in_range = plant_order_list[i]
					break
				}
			}
			if (plant_in_range != noone) {
	            target_plant = plant_in_range;
	            target_enemy = noone;
	        }
			else{
				state = ENEMY_STATE.NORMAL;
	            target_plant = plant_in_range;
	            target_enemy = noone;
	            attack_timer = 0;  // 重置攻击计时器
	            timer = 0;         // 重置动画计时器
			}
        }
        
	        // 攻击处理
	        attack_timer++;
	        if (attack_timer >= current_atk_cycle) {
	            if (target_enemy != noone && instance_exists(target_enemy)) {
	                // 友化 ↔ 正常互攻：走敌人受击流程
	                with (target_enemy) {
	                    damage_amount = other.atk;
	                    damage_type = "normal";
	                    event_user(0);
	                }
	                //播放音效
	                var a = irandom_range(0,2)
	                global.audio.play(ds_list_find_value(obj_battle.chomp_sound_list,a),0,0)
	            }
	            else if (target_plant != noone && instance_exists(target_plant)) {
	            // 对目标植物造成伤害
	            with (target_plant) {
					if !invincible{
						hp -= other.atk;
					}
	                array_push(last_attack_ids, other.id)
	                array_push(last_attack_classes, object_get_name(other.object_index))
	                array_push(last_attack_types, "enemy")
	                array_push(last_attack_damages, other.atk)
	                event_user(2)
	                // 播放受击效果
	                if (instance_exists(other)) {
	                    //instance_create_depth(x, y, depth - 1, obj_plant_hit);
	                }
	            }
			
	            //播放音效
				var a = irandom_range(0,2)
				global.audio.play(ds_list_find_value(obj_battle.chomp_sound_list,a),0,0)
	            }
	            else {
	                // 两个目标都没了：清掉，回走路状态
	                target_enemy = noone;
	                target_plant = noone;
	                state = ENEMY_STATE.NORMAL;
	                timer = 0;
	            }
	            // 重置攻击计时器
	            attack_timer = 0;
	        }
		}
        break;
    }
    
    case ENEMY_STATE.DEAD: {
		ice_timer = 0
		frozen_timer = 0
        // 死亡动画
        if image_index >= death_anim + move_anim * 2 + attack_anim * 2 - 1 {
            image_alpha -= 0.08;
        } else {
            image_index = (floor(timer / flash_speed) mod death_anim) + move_anim * 2 + attack_anim * 2;
        }
        break;
    }
}

// 死亡处理
if (hp <= 0 && state != ENEMY_STATE.DEAD) {
    timer = 0;
    state = ENEMY_STATE.DEAD;
    target_plant = noone;  // 清除攻击目标
}

// 透明度处理
if (image_alpha <= 0 && state == ENEMY_STATE.DEAD) {
    instance_destroy();
}




// 更新僵尸的网格位置和深度

var base_depth = -10 - (zombie_grid.row * 45) - (zombie_grid.col * 5);
depth = base_depth - 4.5; // 僵尸比植物稍微靠后一点（在护罩外侧和咖啡豆之间）

// 保持网格位置更新
grid_col = zombie_grid.col;
grid_row = zombie_grid.row;

if x < global.grid_offset_x-150 && hp > 0 && not place_meeting(x,y,obj_cat) && is_friend == 0{
	if (global.network.mode == "server") {
		var _clients = global.network.connected_clients;
		for (var i = 0; i < array_length(_clients); i++) {
			send_message(_clients[i], MSG_GAME_OVER, 0);
		}
	}
	if (global.network.mode != "client") {
		global.is_paused = true
		global.game_over = true
		instance_create_depth(room_width/2,room_height/2,-3001,obj_game_over)
		global.audio.play(snd_lose,0,0)
	}
}

// 友化敌人完全走出战场两端 → 销毁（不算敌人，不能一直留在场上）
if (is_friend && (x < 0 || x > 2300)) {
	instance_destroy();
}



