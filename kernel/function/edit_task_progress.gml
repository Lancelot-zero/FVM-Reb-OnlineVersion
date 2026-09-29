function edit_task_progress(task_id,progress_index,new_progress){
	if is_task_unlocked(task_id){
		var task_index = get_task_index(task_id)
		global.save_data.tasks[task_index].progress[progress_index] = new_progress
	}
}
