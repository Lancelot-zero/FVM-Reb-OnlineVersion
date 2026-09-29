function unlock_card(card_id, level, shape, skill) {
    // 检查卡片是否已解锁
    for (var i = 0; i < array_length(global.save_data.unlocked_cards); i++) {
        if (global.save_data.unlocked_cards[i].id == card_id) {
            // 卡片已存在，更新等级和转职
            global.save_data.unlocked_cards[i].level = level;
            global.save_data.unlocked_cards[i].shape = shape;
			global.save_data.unlocked_cards[i].skill = skill;
			global.save_data.unlocked_cards[i].max_shape = shape;
			global.save_data.unlocked_cards[i].max_level = level;
            save_file(global.save_slot); // 立即保存
            return true;
        }
    }
    
    // 卡片未解锁，添加新卡片
    var new_card = {
        id: card_id,
        level: level,
        shape: shape,
		skill: skill,
		max_level: level,
		max_shape: shape
    };
    
    array_push(global.save_data.unlocked_cards, new_card);
    save_file(global.save_slot); // 立即保存
    return true;
}
