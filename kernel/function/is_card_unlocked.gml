function is_card_unlocked(card_id) {
    for (var i = 0; i < array_length(global.save_data.unlocked_cards); i++) {
        if (global.save_data.unlocked_cards[i].id == card_id) {
            return true;
        }
    }
    return false;
}
