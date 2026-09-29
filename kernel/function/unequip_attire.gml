function unequip_attire(attire_id){
	//如果未解锁则退出
	if (!is_attire_unlocked(attire_id)){
		return
	}
	//将自身设置为未装备
	edit_attire_state(attire_id,"unequipped")
}
