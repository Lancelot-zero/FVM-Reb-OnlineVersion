// ==========================================
// 网络同步配置 — 白名单 & 规则
// ==========================================
global.network_game_version = "2.3.2.0908";

// ---- 普通属性白名单 ----
// 帧末通过 MSG_EVENT_ACTIONS / modify 自动采集同步
global._sync_keys = [
	// 网格位置
	"target_col",
	"target_row",
	"grid_col",
	"grid_row",
	"col",
	"row",
	// 视觉
	"sprite_index",
	"image_index",
	"image_alpha",
	"image_speed",
	"image_xscale",
	"image_angle",
	// 移动平台
	"start_col",
	"start_row",
	"width",
	"length",
	"move_distance",
	"move_direction",
	"move_axis",
	"boundary_idle_duration",
	"move_speed",
	"initial_offset",
	"initial_idle_duration",
	// 运动 / 特效
	"cvspeed",
	"chspeed",
	"cgravity",
	"x_speed",
	"y_speed",
	"ground_y",
	"water",
	"center_x",
	"center_y",
	"is_hole",
	"is_reversed",
	// 状态机
	"hp_modified",
	"state",
	"skill_timer",
	"jump_times",
	"skill_choose",
	"skill_change_style",
	"move_time",
	"max_time",
	"interval",
	// 杂项
	"dir",
	"type",
	"is_parent",
	"map_spr",
	"map_spr_index",
	"damage",
	"bomb_col",
];

// ---- 实例引用属性白名单 ----
// 这些字段存的是实例 ID，发送时转 net_id，接收时还原
global._sync_ref_keys = [
	"train_head",
];

// ---- BOSS 产物白名单 ----
// 列表内的对象创建时触发"先 spawn 再 modify"两步同步
global.boss_spawn_sync_list = [
	// angelababy
	obj_angelababy_summon,
	obj_angelababy_star,
	obj_angelababy_target,
	obj_angelababy_diamond,
	// paul
	obj_paul_bullet,
	obj_paul_bullet_effect,
	// blonde_mary
	obj_blonde_mary_bullet,
	// mario / pink_paul
	obj_mario_pipeline,
	obj_pink_paul_tentacle_drop,
	// pete
	obj_pete_spike,
	obj_pete_claw,
	obj_pete_claw_effect,
	obj_pete_missile,
	// pharaoh
	obj_pharaoh_bandage,
	obj_pharaoh_coffin,
	obj_pharaoh_hole,
	// hells_messenger
	obj_messenger_ignis_fatuus,
	obj_messenger_poop,
	obj_messenger_mace,
	// fog
	obj_fog,
	// julie
	obj_julie_missile,
	// mouse_train
	obj_mouse_train_1_bullet,
	obj_mouse_train_1_body,
	obj_mouse_train_2_body,
	obj_mouse_train_3_body,
	obj_mouse_train_3_explode,
	obj_mouse_train_3_butter,
	// iron_man
	obj_iron_man,
	// machine
	obj_machine_iron_pan_mouse,
	// xiaoming
	obj_xiaoming_text,
	// vajra
	obj_vajra_lava_effect,
	obj_vajra_lava,
	obj_vajra_spike,
	obj_vajra_lightning,
	// buzz
	obj_buzz_wind,
	// paratrooper
	obj_paratrooper_mouse,
	obj_paratrooper_mouse_shield,
	// irritable_jack
	obj_irritable_jack_fire_mouse,
	obj_irritable_jack_fire,
	obj_irritable_jack_rock_skill_3,
	obj_irritable_jack_rock_skill_4,
	// baron
	obj_baron_needle,
	obj_baron_bats,
	obj_baron_blade,
	// rumble
	obj_rumble_missile,
	obj_rumble_laser,
	// ice_residue
	obj_ice_residue_ball,
	obj_ice_residue_bullet,
	// arno
	obj_arno_bullet,
	obj_arno_bullet_effect,
	// engineer
	obj_engineer_bullet_effect,
	// 卡牌特效
	obj_card_inhale_effect,
	obj_card_heal_effect,
	// 场景物件
	obj_huge_wave_text,
	obj_barrier,
	obj_obstacle,
	obj_lava_burn_effect,
	obj_ladder,
	obj_lava,
	obj_cloud,
	obj_ghost_mouse,
	obj_in_water_effect,
	obj_mummy_mouse,
	obj_apple_football_fan_mouse,
	obj_wind_tunnel,
	// 小怪灰烬
	obj_mouse_ash_death,
	// panda
	obj_little_panda_mouse,
	obj_little_undersea_panda_mouse,
	// 敌人产物补全（航母/蜂巢/潜艇/蝙蝠/蜗牛/士兵）
	obj_machine_bomb_mouse,
	obj_machine_bee,
	obj_undersea_panda_mouse,
	obj_windmill_fish_mouse,
	obj_bat_mouse,
	obj_bat_mouse_target,
	obj_snail_mouse,
	obj_soldier_mouse,
	// 敌人子弹补全（工程车/企鹅/铁人/机械鲨鱼/炸弹鼠）
	obj_engineer_bullet,
	obj_penguin_bullet,
	obj_iron_man_bullet,
	obj_shark_1_bullet,
	obj_machine_bomb_mouse_bullet,
	// 护甲飞片
	obj_enemy_armor,
	// 战斗进度条
	obj_level_progress_bar,
	// 遗迹boss产物
	obj_war_god_duck,
	obj_war_god_gear,
	obj_war_god_soldier,
	obj_war_god_summon,
	obj_war_god_wood,
	obj_hercules_laser,
	obj_thor_body,
];
