function get_cookbook_data(cookbook_id){
	return ds_map_find_value(global.cookbook_pool,cookbook_id)
}
