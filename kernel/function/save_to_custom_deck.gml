function save_to_custom_deck(deck_index,deck_name){
	var _max = deck_slot_max()
	var card_arr = array_create(_max, "")
	for(var i = 0; i < _max;i++){
		if (!deck_slot_is_empty(i)){
			card_arr[i] = global.selected_deck[| i][? "card_id"]
		}
	}
	global.save_data.saved_decks[deck_index].card_id = card_arr
	save_file(global.save_slot)
}
