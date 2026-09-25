image_xscale = 1.8
image_yscale = 1.8
instance_create_depth(x+380,y+403,depth-1,obj_closepackage_btn)
package_cols = 9 //背包格子行列数
/// @function package_tab_rows(_tab)
/// @desc 本页需要的行数，按内容量自动算（下限 16 行），武器/宝石/材料页同样自动扩行
///       1=卡片（卡池） 2=武器+宝石 3=材料
function package_tab_rows(_tab) {
	var _rows = 16;
	if (_tab == 1) {
		var _n = 0;
		if (variable_global_exists("player_deck") && ds_exists(global.player_deck, ds_type_list)) {
			_n = ds_list_size(global.player_deck) div 2;   // 卡池：一对两条
		}
		_rows = max(_rows, (_n + package_cols - 1) div package_cols);
	}
	else if (_tab == 2) {
		// 武器从第 0 行排，宝石接在武器之后（原布局：无武器时宝石从第 1 行起）
		var _w = array_length(global.save_data.unlocked_weapons);
		var _g = array_length(global.save_data.unlocked_gems);
		var _w_rows = (_w + package_cols - 1) div package_cols;
		var _gem_top = (_w > 0) ? _w_rows : 1;
		var _g_rows = (_g > 0) ? ((_g + package_cols - 1) div package_cols) : 0;
		_rows = max(_rows, _gem_top + _g_rows);
	}
	else {
		// 材料页按各材料登记的 pos_y 排布
		var _keys = ds_map_keys_to_array(global.material_pool);
		var _max_row = 0;
		for (var _i = 0; _i < array_length(_keys); _i++) {
			var _md = get_material_info(_keys[_i]);
			if (!is_undefined(_md)) _max_row = max(_max_row, _md.pos_y + 1);
		}
		_rows = max(_rows, _max_row);
	}
	return _rows;
}
package_rows = package_tab_rows(1)
info_button_select = 1
package_button_select = 1
is_submenu_opened = false
gem_start_line = 0
y_offset = 0
package_surface = -1
//创建背包栏位选择按钮
var btn1 = instance_create_depth(x-300,y-455,depth-1,obj_packageselect_btn)
btn1.type = "Package"
btn1.button_index = 1
btn1.sprite_index = spr_packageselect_btn_1
var btn2 = instance_create_depth(x-70,y-455,depth-1,obj_packageselect_btn)
btn2.type = "Package"
btn2.button_index = 2
btn2.sprite_index = spr_packageselect_btn_2
var btn5 = instance_create_depth(x+160,y-455,depth-1,obj_packageselect_btn)
btn5.type = "Package"
btn5.button_index = 3
btn5.sprite_index = spr_packageselect_btn_5
var btn3 = instance_create_depth(x-1170,y-454,depth-1,obj_packageselect_btn)
btn3.type = "Player Info"
btn3.button_index = 1
btn3.sprite_index = spr_packageselect_btn_3
var btn4 = instance_create_depth(x-1020,y-454,depth-1,obj_packageselect_btn)
btn4.type = "Player Info"
btn4.button_index = 2
btn4.sprite_index = spr_packageselect_btn_4
var btn6 = instance_create_depth(x-870,y-454,depth-1,obj_packageselect_btn)
btn6.type = "Player Info"
btn6.button_index = 3
btn6.sprite_index = spr_packageselect_btn_6

hover_card_index = -1; // 当前悬停的卡片索引
hover_weapon_index = -1
hover_gem_index = -1
hover_material_index = -1
view_max_shapes = 0

global.room_battle_front = true


