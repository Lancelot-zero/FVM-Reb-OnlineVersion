function is_gem_unlocked(gem_id) {
    for (var i = 0; i < array_length(global.save_data.unlocked_gems); i++) {
        if (global.save_data.unlocked_gems[i].id == gem_id) {
            return true;
        }
    }
    return false;
}
