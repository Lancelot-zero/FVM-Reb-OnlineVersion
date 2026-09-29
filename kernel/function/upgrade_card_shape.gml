function upgrade_card_shape(card_id, shapes) {
    for (var i = 0; i < array_length(global.save_data.unlocked_cards); i++) {
        if (global.save_data.unlocked_cards[i].id == card_id) {
            // 找到卡片，升级等级
            global.save_data.unlocked_cards[i].max_shape = shapes;
			global.save_data.unlocked_cards[i].shape = shapes;
            save_file(global.save_slot); // 立即保存
            return true;
        }
    }
    
    // 未找到卡片
    return false;
}
