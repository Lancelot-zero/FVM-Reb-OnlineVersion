function disconnect() {
    if (global.network.mode != "client") {
        return "[网络] 当前不是客户端模式";
    }
    file_cache_clear();
    if (variable_global_exists("room_members")) {
        ds_map_clear(global.room_members);
    }
    global.room_name = "";
    show_notice("你已经退出房间", 120);
    if (global.network.server_socket != -1) {
        network_destroy(global.network.server_socket);
    }
    global.network.mode = "offline";
    global.network.server_socket = -1;
    global.network.target_ip = "";
    global.network.target_port = 27085;
    global.network.is_connected = false;
    global.mod_net_player_id = 0;   // 断开连接 → 清掉"我在房间里的 id"
    global.net_player_number = 1;   // 同时清掉房间里的人数
	
	if (gui_stack_get_top(global.gui_stack) ==  room_battle)
		array_pop(global.gui_stack);   // 原来写的是 global.gui_stack.pop()：方法调用语法翻译器不吃，换成等价的 array_pop
    return "[网络] 已断开连接";
	
}
