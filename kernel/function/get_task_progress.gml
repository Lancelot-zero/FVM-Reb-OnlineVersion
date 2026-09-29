function get_task_progress(task_id,progress_index){
	if is_task_unlocked(task_id){
		var task_index = get_task_index(task_id)
		return global.save_data.tasks[task_index].progress[progress_index]
	}
}
