function add_to_deck(card_id, shape, slot_index = -1) {
    var card_data = deck_get_card_data(card_id, shape);
    if (card_data == noone) return false;

    // 确保卡组槽位已初始化（长度 = max_slot，空槽用 [deck_empty_slot] 占位）
    deck_ensure_size();

    var deck_entry = ds_map_create();
    deck_entry[? "card_id"] = card_id;
    deck_entry[? "shape"] = shape;
    deck_entry[? "data"] = card_data;

    // 槽位选择：精确填入指定槽（读档还原用）或最靠前空槽
    var idx = -1;
    if (slot_index >= 0 && slot_index < ds_list_size(global.selected_deck)) {
        idx = slot_index;
    }
    else {
        idx = deck_slot_first_empty();
    }

    if (idx == -1) { // 卡组已满
        ds_map_destroy(deck_entry);
        return false;
    }

    // 覆盖旧槽位前清理旧数据
    var old = global.selected_deck[| idx];
    if (old != global.deck_empty_slot && ds_exists(old, ds_type_map)) {
        ds_map_destroy(old);
    }
    global.selected_deck[| idx] = deck_entry;
    return true;
}
