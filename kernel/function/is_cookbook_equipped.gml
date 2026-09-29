function is_cookbook_equipped(cookbook_id){
	var cd = get_cookbook_data(cookbook_id)
	var cl = global.save_data.equipped_cookbook[cd.rank]
	if array_get_index(cl,cookbook_id) != -1{
		return true
	}
	return false
}
