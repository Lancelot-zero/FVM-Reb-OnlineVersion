
    if (not audio_is_playing(readyroom_music)) {
        // 停止可能存在的暂停实例
        audio_stop_sound(readyroom_music);
        // 从头开始播放新实例
        global.audio.play(readyroom_music, 0, 0);
    }
if keyboard_check_pressed(vk_escape) || mouse_check_button_pressed(mb_right){
	if instance_exists(obj_quit_confirm){
		instance_destroy(obj_quit_confirm)
	}
	else{
		if !is_submenu_open{
			instance_create_depth(room_width / 2,room_height / 2,-100,obj_quit_confirm)
		}
	}
}

// 飞入动画推进（线性：每帧固定位移，到点后由槽位绘制接手）
if fly_active{
	fly_t++
	if fly_t >= fly_dur fly_active = false
}

if instance_exists(obj_quit_confirm) || instance_exists(obj_level_preview){
	is_submenu_open = true
}
else{
	is_submenu_open = false
}

// 选卡区快捷键：光标悬停在卡池卡片上时按「卡槽 N」，直接把这张卡放进第 N 槽
// （槽号与右侧卡槽显示一致；只认已解锁槽位，目标槽在另一页时先翻页，否则飞行终点落在看不见的格子）
if (hover_card_index != -1 && !is_submenu_open && !instance_exists(obj_package_bg)) {
	var _hot_card = global.player_deck[| hover_card_index * 2]
	for (var _k = 1; _k <= deck_slot_max(); _k++) {
		var _hot_key = global.keybind_map[? "卡槽" + string(_k)]
		if (_hot_key == undefined) continue
		if (!keyboard_check_pressed(_hot_key)) continue

		var _hot_slot = _k - 1
		var _hot_page = (_hot_slot >= 11) ? 10 : 0
		if (deck_first_slot_index != _hot_page) deck_first_slot_index = _hot_page

		global.audio.play(snd_button, 0, 0)
		if (add_to_deck(_hot_card, get_card_info_simple(_hot_card).shape, _hot_slot)) {
			// 飞入卡槽：起点 = 绘制时记下的悬停卡片屏幕位置
			fly_add = true
			fly_spr = hover_card_spr
			fly_sx = hover_card_x
			fly_sy = hover_card_y
			fly_tx = x + 805 + (_hot_slot - deck_first_slot_index) * 86
			fly_ty = y + 132
			fly_slot = _hot_slot
			fly_dur = 18
			fly_t = 0
			fly_active = true
		}
		break
	}
}

// 清空时批量飞回卡池：倒着删避免下标错位
for (var _bi = array_length(fly_batch) - 1; _bi >= 0; _bi--){
	fly_batch[_bi].t++
	if fly_batch[_bi].t >= fly_batch[_bi].dur array_delete(fly_batch, _bi, 1)
}
