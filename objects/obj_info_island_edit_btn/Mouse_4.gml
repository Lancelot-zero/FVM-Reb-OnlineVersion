if obj_info_island_bg.info_button_select == 1 and obj_info_island_bg.select_card_index!= -1{
global.audio.play(snd_button,0,0)
if not obj_info_island_bg.is_submenu_opened{
		
			global.audio.play(snd_button,0,0)
			var inst = instance_create_depth(room_width/2,room_height/2,depth-5,obj_info_island_edit_menu)
			obj_info_island_bg.is_submenu_opened = true
		
	}
}