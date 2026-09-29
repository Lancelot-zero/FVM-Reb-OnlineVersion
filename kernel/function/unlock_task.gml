function unlock_task(task_id){
	if !is_task_unlocked(task_id){
		var task_data = get_task_data(task_id)
		var length = array_length(task_data.requirements)
		var task_struct = {
			"id":task_id,
			"state":"new",
			"progress":[]
		}
		for(var i = 0; i < length;i++){
			array_push(task_struct.progress,0)
		}
		array_push(global.save_data.tasks,task_struct)
	}
}
