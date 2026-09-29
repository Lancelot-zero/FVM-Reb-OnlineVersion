function unlock_attire(attire_id){
	//在时装数组中添加对应时装
	if (!is_attire_unlocked(attire_id)){
		array_push(global.save_data.attires,{"attire_id":attire_id,"state":"unequipped"})
	}
}
