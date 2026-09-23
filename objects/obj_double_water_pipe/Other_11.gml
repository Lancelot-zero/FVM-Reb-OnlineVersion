// 双向水管：前后两发（shape 1 起前向多发）走弹幕管理器
// 原来自己生成 obj_waterpipe_bullet 实例；出生点用本行 row 顶 +10
b_count++

_net_un = (variable_instance_get(id, "_net_card_equipped_attire_id") == noone)
var _skin = (_net_un && card_equipped_attire_id(plant_id) == "water_pipe_libra")
         || variable_instance_get(id, "_net_card_equipped_attire_id") == "water_pipe_libra";

var _spr = "spr_waterpipe_bullet";
if _skin { _spr = "spr_water_pipe_libra_bullet"; }
var _fr = sprite_get_number(get_load_sprite(_spr));
var _by = global.grid_offset_y + global.grid_cell_size_y * grid_row + 10;
var _eob = "obj_waterpipe_bullet_effect";

if b_count == 1 || (b_count >= 2 && shape >= 1){
	bullet_screen_add_Ex(_spr, _fr, 0, 1.6, 1, x + 40, _by, 8, 0, atk, 1, -1,
	                     "normal", _eob, "", "normal", 5, 0);
}

// 向后那一发：vx = -8、画面转 180
bullet_screen_add_Ex(_spr, _fr, 0, 1.6, 1, x - 40, _by, -8, 0, atk, 1, -1,
                     "normal", _eob, "", "normal", 5, 180);

audio_play_sound(snd_shot, 0, 0);
