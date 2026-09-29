function edit_task_state(task_id,new_state){
	if is_task_unlocked(task_id){
		var task_index = get_task_index(task_id)
		global.save_data.tasks[task_index].state = new_state
	}
}
