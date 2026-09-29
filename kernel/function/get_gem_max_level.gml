function get_gem_max_level(gem_id){
	var gem_level = 0
	for(var i = 0;i < array_length(global.save_data.unlocked_gems);i++){
		if global.save_data.unlocked_gems[i].id == gem_id{
			gem_level = global.save_data.unlocked_gems[i].max_level
		}
	}
	return gem_level
}
