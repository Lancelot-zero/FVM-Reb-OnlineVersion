function is_weapon_unlocked(weapon_id) {
    for (var i = 0; i < array_length(global.save_data.unlocked_weapons); i++) {
        if (global.save_data.unlocked_weapons[i].id == weapon_id) {
            return true;
        }
    }
    return false;
}
