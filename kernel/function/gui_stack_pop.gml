function gui_stack_pop(_stack) {
    if (array_length(_stack._rooms) == 0) return;
    array_pop(_stack._rooms);
    if (array_length(_stack._rooms) == 0) {
        array_push(_stack._rooms, room_menu);
    }
    gui_stack_sync(_stack);
}
