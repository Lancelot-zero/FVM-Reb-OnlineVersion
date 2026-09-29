function edit_attire_state(attire_id,new_state){
	//未解锁则立即退出
	if (!is_attire_unlocked(attire_id)){
		return
	}
	//在时装数组中寻找对应时装并修改状态
	var attire_list = global.save_data.attires
	for(var i = 0 ; i < array_length(attire_list) ; i++){
		if attire_list[i].attire_id == attire_id{
			global.save_data.attires[i].state = new_state
		}
	}
}
