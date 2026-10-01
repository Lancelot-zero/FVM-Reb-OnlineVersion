if on_click{
	instance_destroy(obj_config_menu)
	global.audio.play(snd_button,0,0)
	if global.menu_screen{
		obj_player_info_ui.menu_type = 0
	}
}