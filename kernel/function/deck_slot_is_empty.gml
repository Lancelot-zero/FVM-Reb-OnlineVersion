function deck_slot_is_empty(i) {
    if (i < 0 || i >= ds_list_size(global.selected_deck)) return true;
    var e = global.selected_deck[| i];
    if (e == global.deck_empty_slot) return true;
    if (e == undefined || e == noone) return true;
    return (e[? "card_id"] == "");
}
