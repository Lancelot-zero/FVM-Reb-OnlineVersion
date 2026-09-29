function clear_deck() {
    var _max = deck_slot_max();
    ds_list_clear(global.selected_deck);
    deck_empty_slot_ensure();
    for (var i = 0; i < _max; i++) {
        ds_list_add(global.selected_deck, global.deck_empty_slot);
    }
}
