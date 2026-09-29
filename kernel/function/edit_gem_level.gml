function edit_gem_level(gem_id,new_level){
	for(var i = 0;i < array_length(global.save_data.unlocked_gems);i++){
		if global.save_data.unlocked_gems[i].id == gem_id{
			global.save_data.unlocked_gems[i].level = new_level
		}
	}
}
