function deck_slot_max() {
    if (variable_global_exists("save_data") && !is_undefined(global.save_data)
        && variable_struct_exists(global.save_data, "unlocked_items")) {
        return global.save_data.unlocked_items.max_slot;
    }
    return 21;
}
