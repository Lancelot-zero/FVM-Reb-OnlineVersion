function unlock_gem(gem_id){
	if not is_gem_unlocked(gem_id){
		var index = array_length(global.save_data.unlocked_gems)
		global.save_data.unlocked_gems[index] = {"id":gem_id,"level":0,"max_level":0}
		save_file(global.save_slot)
	}
}
