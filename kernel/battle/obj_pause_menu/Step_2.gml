// ESC键关闭菜单
    if (keyboard_check_pressed(vk_escape)) {
		if instance_exists(obj_config_menu){
			instance_destroy(obj_config_menu)
		}
        instance_destroy();
        global.is_paused = false;
        global.show_menu = false;
		if (global.network.mode == "server") {
			var _cl = global.network.connected_clients;
			for (var i = 0; i < array_length(_cl); i++) {
				send_message(_cl[i], MSG_SERVER_ACTION, 3);
			}
		}
    }