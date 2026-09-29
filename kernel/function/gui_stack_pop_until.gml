function gui_stack_pop_until(_stack, _room) {
    while (array_length(_stack._rooms) > 0 && _stack._rooms[array_length(_stack._rooms) - 1] != _room) {
        array_pop(_stack._rooms);
    }
    if (array_length(_stack._rooms) == 0) {
        if (!room_exists(_room)) return;
        array_push(_stack._rooms, _room);
    }
    gui_stack_sync(_stack);
}
