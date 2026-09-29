function get_card_attire_list(card_id){
	var attire_id_list = []
	//遍历时装数组寻找目标卡片为card_id的时装
	var attire_list = global.save_data.attires
	for(var i = 0 ; i < array_length(attire_list) ; i++){
		var attire_data = get_attire_info(attire_list[i].attire_id)
		if !is_struct(attire_data) continue      // 同上：没注册的时装直接跳过
		if attire_data.target_card == card_id{
			//如果找到，加入到时装id数组中
			array_push(attire_id_list,attire_list[i].attire_id)
		}
	}
	return attire_id_list
}
