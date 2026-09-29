function deselect_shovel() {
    obj_shovel_slot.is_selected = false;
    global.selected_slot = noone;
    
    // 恢复默认鼠标光标
    //cursor_sprite = spr_default_cursor;
}
