 // Inherit the parent event
event_inherited();
hp = 800
maxhp = 800
move_anim = 8
attack_anim = 4
death_anim = 12
move_speed = 0
mouse_id = "bat_mouse"
attack_range = 90

target_type = "air"

state = ENEMY_STATE.APPEAR
sprite_index = spr_bat_mouse
special_ash = true
anim_timer = 0

target_col = -1
target_row = -1
banding_target_inst = noone
carry_switch = false      // 携带开关：false=原行为；true=不偷植物，目标指向跟随自身
carry_target = noone      // 携带目标指向（开关开启时其 x/y 跟随自身）