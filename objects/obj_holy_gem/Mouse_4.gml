if cooldown_timer <= 0{
	if (global.network.mode == "client") {
		send_message(global.network.server_socket, MSG_ACTIVE_SKILL_REQUEST, gem_id, obj_player_character.grid_col, obj_player_character.grid_row, gem_level)
		global.audio.play(snd_button,0,0)
		cooldown_timer = cooldown
		return;
	}
	global.audio.play(snd_button,0,0)

	// 特效与提示统一走 network_active_skill，保证服务端 / 单机 / 客机表现一致
	network_active_skill(gem_id, obj_player_character.grid_col, obj_player_character.grid_row, gem_level)
	if (global.network.mode == "server") {
		network_broadcast_active_skill(gem_id, obj_player_character.grid_col, obj_player_character.grid_row, gem_level)
	}

	cooldown_timer = cooldown
}
