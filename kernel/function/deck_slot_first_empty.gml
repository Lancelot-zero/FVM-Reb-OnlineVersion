function deck_slot_first_empty() {
    deck_ensure_size();
    var _max = ds_list_size(global.selected_deck);
    for (var i = 0; i < _max; i++) {
        if (deck_slot_is_empty(i)) return i;
    }
    return -1;
}
