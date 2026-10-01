if obj_readyroom_manager.selected_custom_deck != deck_index && !obj_readyroom_manager.is_submenu_open{
	obj_readyroom_manager.selected_custom_deck = deck_index
	global.audio.play(snd_button,0,0)
	load_custom_deck(deck_index-1)
}