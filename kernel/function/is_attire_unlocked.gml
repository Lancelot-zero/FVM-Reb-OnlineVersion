function is_attire_unlocked(attire_id){
	var attire_list = global.save_data.attires
	for(var i = 0 ; i < array_length(attire_list) ; i++){
		if attire_list[i].attire_id == attire_id{
			return true
		}
	}
	return false
}
