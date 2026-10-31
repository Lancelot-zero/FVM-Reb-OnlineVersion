
// 前层贴图槽（VM_SetDrawSlot_front），绘制在火焰UI前面
for (var _i = 0; _i < 64; _i++) {
    var _slot = global.map_draw_slots_front[_i];
    if (_slot.sprite == noone || _slot.sprite == "" || _slot.sprite == -1) continue;
    draw_sprite_ext(_slot.sprite, 0, _slot.x, _slot.y, _slot.image_xscale, _slot.image_yscale, _slot.image_angle, c_white, _slot.alpha);
}

if(global._VM_conveyor_belt){
	
	draw_sprite_ext(spr_flame_indicator, 0, 350, 0, 0.9, 0.9, 0, c_white, 1)
	draw_sprite_ext(spr_slot_top,0,350+83*1.8,0,65+45*(15-1),1.8,0,c_white,1)
	if not instance_exists(obj_shovel_slot) battle_slots_place_shovel()
	
}
else{
	draw_sprite_ext(spr_flame_indicator, 0, 350, 0, 0.9, 0.9, 0, c_white, 1)

	// 卡槽数量：只由 create_battle_slots / battle_slots_relayout 更新（插槽、删槽后刷新）
	//   这里只读，不自己算 —— 否则插槽/删槽后长条和铲子不会跟着变
	var slot_length = variable_global_exists("battle_slot_count") ? real(global.battle_slot_count) : deck_slot_count()
	if slot_length < 0 slot_length = 0
	//show_debug_message("slot_length:"+string(slot_length))
	if slot_length <= 0{
		// 0 槽：没有卡槽框，只剩铲子（它会贴在火焰区右边，见 battle_slots_place_shovel）
	}
	else if slot_length <= 14{
		draw_sprite_ext(spr_slot_top,0,350+83*1.8,0,40+45*(slot_length-1),1.8,0,c_white,1)
	}
	else{
		draw_sprite_ext(spr_slot_top,0,350+83*1.8,0,40+45*(15-1),1.8,0,c_white,1)
		draw_sprite_ext(spr_slot_corner, 0, 350+83*1.8+80+90*(15-1), 0, 0.9, 0.9, 0, c_white, 1)
		draw_sprite_ext(spr_slot_right,0,350+83*1.8+80+90*(15-1)-1,18*1.8,0.9,59*(slot_length-14),0,c_white,1)
	}
	if not instance_exists(obj_shovel_slot) battle_slots_place_shovel()
	draw_set_halign(fa_center)
	draw_set_valign(fa_middle)
	draw_set_font(font_hei)
	draw_set_color(c_black)
	draw_text(420,143,string(global.flame))
	draw_set_halign(fa_left)
	draw_set_valign(fa_top)
}


