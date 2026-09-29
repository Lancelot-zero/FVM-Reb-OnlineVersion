function have_material(material_id){
	var length = array_length(global.save_data.inventory)
	for(var i = 0 ; i < length ; i++){
		if global.save_data.inventory[i].id == material_id{
			return true
		}
	}
	return false
}
