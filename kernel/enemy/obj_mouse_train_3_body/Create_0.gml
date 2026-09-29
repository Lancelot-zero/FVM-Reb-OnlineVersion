sprite_index = get_load_sprite("spr_mouse_train_2_body_idle1");  //转化额外添加保证触发
sprite_index = get_load_sprite("spr_mouse_train_2_body_idle1");  //转化额外添加保证触发
// Inherit the parent event
event_inherited();
step_ready = false

mouse_id = "mouse_train_3"
jump_times = 0
skill_timer = 0
state = BOSS_STATE.APPEAR
hp = 200000
maxhp = 200000
immune_to_ash = true
wait_time = 0
move_time = 250
cave = noone
sprite_index = get_load_sprite("spr_mouse_train_2_body_appear1")
is_boss = true
train_dir = 0
skill_3_style = false

skill_choose = 0
skill_count = 0
image_alpha = 1
appear = false
avaliable_pos = ds_list_create()
missle_coord = []

skill_change_style = 0

x_move_speed = 0
y_move_speed = 0

train_head = id
is_reversed = false

frame_count = 0