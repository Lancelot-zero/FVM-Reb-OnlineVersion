function is_task_unlocked(task_id){
	for(var i = 0 ; i < array_length(global.save_data.tasks);i++){
		if global.save_data.tasks[i].id == task_id{
			return true
		}
	}
	return false
}
