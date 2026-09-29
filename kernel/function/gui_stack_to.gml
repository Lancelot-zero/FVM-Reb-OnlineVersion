function gui_stack_to(_stack, _room) {
    if (!room_exists(_room)) return;
    for (var i = 0; i < array_length(_stack._rooms); i++) {
        if (_stack._rooms[i] == _room) {
            gui_stack_pop_until(_stack, _room);
            return;
        }
    }
    array_push(_stack._rooms, _room);
    gui_stack_sync(_stack);
}
