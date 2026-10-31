image_xscale = 1.8
image_yscale = 1.8
image_speed = 0
atk = 10 //每次攻击伤害
atk_cycle = 0.6 * 60 //攻击间隔
move_speed = 0.32 //移动速度（像素/秒）
move_speed_modify = 1
hp = 100
maxhp = 100
hurt_rate = 0.5 //切换至受伤状态所需生命值比例
state = ENEMY_STATE.NORMAL //状态
flash_speed = 6
attack_anim = 4 //攻击动画帧数
move_anim = 8
death_anim = 13
flash_value = 0 //受伤闪光计时器
timer = 0
attack_timer = 0
target_plant = noone
target_enemy = noone   // 友化互攻目标：友化敌人打正常敌人，正常敌人也打友化敌人
is_friend = 0          // 友化：1 = 已友化（帮玩家打敌人、不啃植物、倒戈往右走），0 = 正常敌人
attack_range = 90
immune_to_ash = false
ice_timer = 0
is_slowdown = false
frozen_timer = 0
is_frozen = false
ice_sprite = spr_mouse_frozen
helmet_hp = 0
helmet_max_hp = 0
shield_hp = 0
shield_max_hp = 0
special_ash = false
current_frozen = false
is_stun = false
stun_timer = 0
stun_sprite = spr_mouse_stun
mouse_id = ""
giant_type = false
target_type = "normal"
is_boss = false
can_dropped = false

// =======猫驱赶相关=======
is_scare = false
scare_timer = 0
y_move = 0
left_move_flashs = 0

// 加载Shader
shader_hit = hit_effect_2
u_progress = shader_get_uniform(shader_hit, "u_progress");
u_flashColor = shader_get_uniform(shader_hit, "u_flashColor");

flash_color = #FFFFFF

// 友化着色：创建时属性，友化敌人用专属颜色+透明度画纯色剪影（见 Draw_0），颜色为 RGB 三参数
friend_color_r = 128
friend_color_g = 0
friend_color_b = 128
friend_alpha = 0.5

grid_col = 99
grid_row = 99

damage_amount = 0
damage_type = ""

hit_sound = snd_hit1

water_effect_timer = 0
hp_modified = false
frame_count = 0

block_mouse_id_list = ["mouse_train_1"]

random_count = 0
_state_prev = state


random_seed = irandom_range(0,4096)
pre_state = -1 
pre_hp = -1

// 命中位掩码 + 吃不吃冰冻：由敌人自己持有（Step_2 每帧刷新），不再依赖 obj_battle 的每帧遍历。
// 这里按父对象的默认 target_type 先算一次，子对象改了 target_type 后下一帧就正了 —— 关键是不会"未定义"。
tbit   = enemy_tbit_of(target_type);
ice_ok = 1 ;


// ===== 上梯越过植物（梯子功能）=====
can_use_ladder=true
climb_stage = 0        // 0=未越障 1=沿本行水平越障中
climb_end_x = 0        // 落点 x（植物左侧相邻格右边界）
climb_base_y = 0       // 越障起始 y（本行基准，弧线结束时回落到这里）
climb_total_dist = 1   // 起点到落点的水平总距离（算弧线进度用，防除零）
climb_arc_h = 40       // 弧线最高上抬像素（远小于一格 116，不会跨行）
climb_speed = 2        // 每帧移动像素（越障速度，越小越慢）
climb_y = 0
