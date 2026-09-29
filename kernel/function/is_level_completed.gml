function is_level_completed(level_id) {
    for (var i = 0; i < array_length(global.save_data.completed_levels); i++) {
        if (global.save_data.completed_levels[i] == level_id) {
            return true;
        }
    }
    return false;
}
