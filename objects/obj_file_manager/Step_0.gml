if global.lose_focus_pause{
	if !window_has_focus(){
		audio_pause_all()
		audio_pause = true
	}
	else{
		if audio_pause{
			audio_resume_all()
			audio_pause = false
		}
	}
}

// 输入法屏蔽心跳（obj_file_manager 是持久对象，跨房间存活）
// v5 的心跳挂在 obj_game_init 上，而它只在 room_init 存在且非持久 → 进游戏后循环就死了
if (!variable_instance_exists(id, "ime_tick")) {
    ime_tick = 0;
}
ime_tick++;
if (ime_tick >= 60) {
    ime_tick = 0;
    if (global.ime_block && native_disable_ime != undefined) {
        native_disable_ime(window_handle());
    }
}
