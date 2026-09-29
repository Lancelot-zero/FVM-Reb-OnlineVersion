function deck_slot_count() {
    deck_ensure_size();
    var n = 0;
    var _max = ds_list_size(global.selected_deck);
    for (var i = 0; i < _max; i++) {
        if (!deck_slot_is_empty(i)) n++;
    }
    return n;
}
