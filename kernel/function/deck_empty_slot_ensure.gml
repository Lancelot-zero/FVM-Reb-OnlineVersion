function deck_empty_slot_ensure() {
    if (!variable_global_exists("deck_empty_slot") || is_undefined(global.deck_empty_slot) || global.deck_empty_slot == -1) {
        global.deck_empty_slot = ds_map_create();
        global.deck_empty_slot[? "card_id"] = "";
        global.deck_empty_slot[? "shape"] = -1;
        global.deck_empty_slot[? "data"] = noone;
    }
    return global.deck_empty_slot;
}
