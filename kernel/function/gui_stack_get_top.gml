function gui_stack_get_top(_stack) {
    var _n = array_length(_stack._rooms);
    if (_n == 0) return undefined;
    return _stack._rooms[_n - 1];
}
