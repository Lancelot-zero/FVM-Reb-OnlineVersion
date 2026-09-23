// 机枪小笼包：包子走弹幕管理器（原来自己生成 obj_xiaolongbao_bullet 实例）
// 出生点本行 row 顶 +10；皮肤 gatling_popcorn 有 _1 / _2 两档，命中特效也跟着换
_net_un = (variable_instance_get(id, "_net_card_equipped_attire_id") == noone)
var _skin = (_net_un && card_equipped_attire_id(plant_id) == "gatling_popcorn")
         || variable_instance_get(id, "_net_card_equipped_attire_id") == "gatling_popcorn";

var _spr = "spr_xiaolongbao_bullet";
var _esp = "";                                   // 非皮肤：用 obj_xiaolongbao_bullet_effect 自带贴图
if _skin {
	_spr = "spr_gatling_popcorn_bullet";
	_esp = "spr_gatling_popcorn_bullet_effect";
	if shape == 1 {
		_spr = "spr_gatling_popcorn_bullet_1";
		_esp = "spr_gatling_popcorn_bullet_effect_1";
	}
	if shape == 2 {
		_spr = "spr_gatling_popcorn_bullet_2";
		_esp = "spr_gatling_popcorn_bullet_effect_2";
	}
}
var _fr = sprite_get_number(get_load_sprite(_spr));
var _by = global.grid_offset_y + global.grid_cell_size_y * grid_row + 10;

bullet_screen_add_Ex(_spr, _fr, 0, 1.8, 1, x + 40, _by, 8, 0, atk, 1, -1,
                     "normal", "obj_xiaolongbao_bullet_effect", "", "normal", 5, 0, -1, 0.15, _esp);
