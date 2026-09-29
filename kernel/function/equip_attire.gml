function equip_attire(attire_id){
	//如果未解锁则退出
	if (!is_attire_unlocked(attire_id)){
		return
	}
	//获取卡片对应的全部时装
	var attire_data = get_attire_info(attire_id)
	var attire_id_list = get_card_attire_list(attire_data.target_card)
	//将其他时装全部设置为未装备
	for(var i = 0 ; i < array_length(attire_id_list) ; i++){
		edit_attire_state(attire_id_list[i],"unequipped")
	}
	//将自身设置为已装备
	edit_attire_state(attire_id,"equipped")
}
