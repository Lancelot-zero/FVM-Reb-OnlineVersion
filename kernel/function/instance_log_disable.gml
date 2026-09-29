function instance_log_disable(){
	global._evt_log_enabled = false;
	global._evt_created   = [];
	global._evt_destroyed = [];
}
