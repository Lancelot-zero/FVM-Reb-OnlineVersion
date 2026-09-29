function complete_level(level_id) {
    // 检查关卡是否已完成
    for (var i = 0; i < array_length(global.save_data.completed_levels); i++) {
        if (global.save_data.completed_levels[i] == level_id) {
            // 关卡已完成，无需重复添加
            return true;
        }
    }
    
    // 添加新完成的关卡
    array_push(global.save_data.completed_levels, level_id);
    save_file(global.save_slot); // 立即保存
    return true;
}
