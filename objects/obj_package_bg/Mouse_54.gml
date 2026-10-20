if package_button_select == 1{
	if not is_submenu_opened{
		if hover_card_index != -1{
			global.audio.play(snd_button,0,0)
			var _hover = hover_card_index
			var _depth = depth
			instance_destroy()   // 先销毁背包：其 Destroy 会把 room_battle_front 置回 false
			// 再创建情报岛：其 Create 会重新置 true，保证开着岛时背后选卡界面不可点
			var inst = instance_create_depth(1380,room_height/2,_depth-5,obj_info_island_bg)
			inst.select_card_index = _hover
		}
	}
}
if package_button_select == 2{
	if not is_submenu_opened{
		if hover_gem_index != -1{
			global.audio.play(snd_button,0,0)
			var inst = instance_create_depth(room_width/2,room_height/2,depth-5,obj_gem_edit_menu)
			inst.target_card_index = hover_gem_index
			is_submenu_opened = true
		}
	}
}