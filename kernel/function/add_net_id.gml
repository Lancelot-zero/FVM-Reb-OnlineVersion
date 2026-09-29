function add_net_id(ins_id){
	ds_map_add(global.network.map_net_id_instance_id,global.network.net_instance_count,ins_id);
	ds_map_add(global.network.map_instance_id_net_id,ins_id,global.network.net_instance_count);
	global.network.net_instance_count++;
}
