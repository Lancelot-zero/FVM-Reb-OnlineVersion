function get_task_data(task_id){
	return ds_map_find_value(global.task_pool,task_id)
}
