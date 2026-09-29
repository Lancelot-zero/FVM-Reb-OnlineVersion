function network_broadcast_shovel_remove(col, row, net_id, flame_amount) {
    if (global.network.mode != "server") return;

    var _list = global.network.connected_clients;
    var _size = array_length(_list);
    for (var _i = 0; _i < _size; _i++) {
        var _socket = _list[_i];
        send_message(_socket, MSG_REMOVE_UNIT, net_id, col, row, flame_amount);
    }
}
