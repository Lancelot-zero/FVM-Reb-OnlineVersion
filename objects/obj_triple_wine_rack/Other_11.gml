// 三线酒架：三发子弹走弹幕管理器（原来自己生成 obj_triplewinerack_bullet 实例）
// 出生点统一在本行（row 顶 +10，和爱神/战神一个口径），上下两发靠 ty 拐到目标行
// 越界（最上行往上 / 最下行往下）→ 打本行、x 收 20、不渐变
_net_un = (variable_instance_get(id, "_net_card_equipped_attire_id") == noone)
var _skin = (_net_un && card_equipped_attire_id(plant_id) == "wine_rack_sagittarius")
         || variable_instance_get(id, "_net_card_equipped_attire_id") == "wine_rack_sagittarius";

var _spr = "spr_triplewinerack_bullet";
var _esp = "spr_triplewinerack_bullet_effect";   // 原版是 obj_coffeecup_bullet_effect 换这张贴图
if _skin {
	_spr = "spr_wine_rack_sagittarius_bullet";
	_esp = "spr_wine_rack_sagittarius_bullet_effect";
	if shape >= 2 {
		_spr = "spr_wine_rack_sagittarius_bullet_1";
		_esp = "spr_wine_rack_sagittarius_bullet_effect_1";
	}
}
var _fr  = sprite_get_number(get_load_sprite(_spr));
var _goy = global.grid_offset_y;
var _gch = global.grid_cell_size_y;
var _by  = _goy + _gch * grid_row + 10;          // 本行出生 y
var _eob = "obj_coffeecup_bullet_effect";

// 本行那一发（不渐变）
bullet_screen_add_Ex(_spr, _fr, 0, 1.8, 0.333, x + 40, _by, 8, 0, atk, 1, -1,
                     "normal", _eob, "", "normal", 5, 0, -1, 0.15, _esp);

// 上一行
var _ux  = x + 40;
var _tyu = _goy + _gch * (grid_row - 1) + 10;
if grid_row - 1 < 0 { _ux = x + 20; _tyu = -1; }
bullet_screen_add_Exs(_spr, _fr, 0, 1.8, 0.333, _ux, _by, 8, 0, atk, 1, -1,
                      "normal", _eob, "", "normal", 5, 0, _tyu, 0.15, _esp);

// 下一行
var _dx  = x + 40;
var _tyd = _goy + _gch * (grid_row + 1) + 10;
if grid_row + 1 >= global.grid_rows { _dx = x + 20; _tyd = -1; }
bullet_screen_add_Exs(_spr, _fr, 0, 1.8, 0.333, _dx, _by, 8, 0, atk, 1, -1,
                      "normal", _eob, "", "normal", 5, 0, _tyd, 0.15, _esp);

// shape 1：中间再加一发
if shape == 1 {
	bullet_screen_add_Ex(_spr, _fr, 0, 1.8, 0.333, x + 80, _by, 8, 0, atk, 1, -1,
	                     "normal", _eob, "", "normal", 5, 0, -1, 0.15, _esp);
}

audio_play_sound(snd_shot, 0, 0);
