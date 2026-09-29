function remove_task(task_id,is_complete){
	if is_task_unlocked(task_id){
		if is_complete{
			array_push(global.save_data.completed_tasks,task_id)
		}
		var task_index = get_task_index(task_id)
		array_delete(global.save_data.tasks,task_index,1)
	}
}
