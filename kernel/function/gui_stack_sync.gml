function gui_stack_sync(_stack) {
    var _n = array_length(_stack._rooms);
    if (_n == 0) return;
    var _top = _stack._rooms[_n - 1];
    if (_top == room) return;
    if (!room_exists(_top)) return;
    room_goto(_top);
}
