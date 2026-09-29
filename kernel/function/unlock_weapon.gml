function unlock_weapon(weapon_id){
	if not is_weapon_unlocked(weapon_id){
		var index = array_length(global.save_data.unlocked_weapons)
		global.save_data.unlocked_weapons[index] = {"id":weapon_id}
		save_file(global.save_slot)
	}
}
