if (ds_exists(chomp_sound_list, ds_type_list)) {
	ds_list_destroy(chomp_sound_list);
}
// 离开战斗房：卸载 VM_LoadSound 载入的本地音效（VM_InitRoomEntry 只管进房那侧）
vm_audio_unload_all();