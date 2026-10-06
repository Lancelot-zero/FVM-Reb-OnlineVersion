if instance_exists(obj_craft_bg){
	if obj_craft_bg.button_select != button_index{
		obj_craft_bg.button_select = button_index
		obj_craft_bg.current_uprade_target_id = ""
		obj_craft_bg.y_offset = 0
		global.audio.play(snd_button,0,0)
	}
}