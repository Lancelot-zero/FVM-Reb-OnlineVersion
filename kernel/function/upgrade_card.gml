function upgrade_card(card_id, levels) {
    for (var i = 0; i < array_length(global.save_data.unlocked_cards); i++) {
        if (global.save_data.unlocked_cards[i].id == card_id) {
            // 找到卡片，升级等级
            global.save_data.unlocked_cards[i].max_level = levels;
			global.save_data.unlocked_cards[i].level = levels;
            save_file(global.save_slot); // 立即保存
            return true;
        }
    }
    
    // 未找到卡片
    return false;
}
