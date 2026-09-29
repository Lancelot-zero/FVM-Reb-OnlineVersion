function get_task_state(task_id){
	if is_task_unlocked(task_id){
		var task_index = get_task_index(task_id)
		return global.save_data.tasks[task_index].state
	}
}
