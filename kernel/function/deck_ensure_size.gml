function deck_ensure_size() {
    deck_empty_slot_ensure();
    var _max = deck_slot_max();
    while (ds_list_size(global.selected_deck) < _max) {
        ds_list_add(global.selected_deck, global.deck_empty_slot);
    }
}
