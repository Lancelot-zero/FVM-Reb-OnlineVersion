function remove_from_deck(i) {
    deck_ensure_size();
    if (i < 0 || i >= ds_list_size(global.selected_deck)) return;
    if (deck_slot_is_empty(i)) return;
    var old = global.selected_deck[| i];
    if (old != global.deck_empty_slot && ds_exists(old, ds_type_map)) {
        ds_map_destroy(old);
    }
    global.selected_deck[| i] = global.deck_empty_slot;
}
