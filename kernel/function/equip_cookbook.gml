function equip_cookbook(cookbook_id){
	if !is_cookbook_equipped(cookbook_id){
		var cd = get_cookbook_data(cookbook_id)
		array_push(global.save_data.equipped_cookbook[cd.rank],cookbook_id)
	}
}
