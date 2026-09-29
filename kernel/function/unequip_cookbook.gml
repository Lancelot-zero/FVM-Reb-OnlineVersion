function unequip_cookbook(cookbook_id){
	if is_cookbook_equipped(cookbook_id){
		var cd = get_cookbook_data(cookbook_id)
		array_delete(global.save_data.equipped_cookbook[cd.rank],array_get_index(global.save_data.equipped_cookbook[cd.rank],cookbook_id),1)
	}
}
