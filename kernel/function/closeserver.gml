function closeserver() {
    if (global.network.mode != "server") {
        return "[网络] 当前不是服务器模式";
    }

    // 断开所有客户端连接
    var _clients = global.network.connected_clients;
    for (var i = 0; i < array_length(_clients); i++) {
        network_destroy(_clients[i]);  // 使用 network_destroy
    }

    // 关闭服务器套接字
    if (global.network.server_socket != -1) {
        network_destroy(global.network.server_socket); 
    }

    global.network.mode = "offline";
    global.network.server_socket = -1;
    global.network.connected_clients = [];
    global.network.server_ip = "";
    global.mod_net_player_id = 0;   // 关闭服务器 → 清掉"我在房间里的 id"
    global.net_player_number = 1;   // 同时清掉房间里的人数

    return "[网络] 服务器已关闭";
}
