// music_enabled=false（VM_PauseBGM）时整段跳过：内置 BGM 的续播/暂停管理全停，
// 否则 VM_PauseBGM 按下后下一帧就被这里 resume/重播，一次调用压不住
if music_enabled{
	if audio_is_paused(battle_music) and not global.is_paused{
		audio_resume_sound(battle_music)
	}
	if not audio_is_playing(battle_music) and not global.is_paused{
		audio_play_sound(battle_music,0,0)
	}
	if audio_is_playing(battle_music) and global.is_paused{
		audio_pause_sound(battle_music)
	}

	if global.laboretory_room{
		var current_music_volum = audio_group_get_gain(music)
		audio_sound_gain(battle_music,current_music_volum)
		audio_sound_gain(new_battle_music,current_music_volum)
	}
}

// VM 音乐轨（VM_PlayMusic 的句柄）：实际增益 = 本轨音量 × 设置背景音乐音量，每帧同步；
// 游戏暂停跟着挂起/恢复；播完自动清出注册表。
// 放 music_enabled 门外面：VM 音乐轨由 VM_StopMusic 管，不受 VM_PauseBGM 的原版 BGM 让路影响
if variable_global_exists("_VM_music_instances"){
	var _mvol = global.music_volume
	var _i = 0
	while _i < ds_list_size(global._VM_music_instances){
		var _inst = global._VM_music_instances[| _i]
		if audio_is_playing(_inst) or audio_is_paused(_inst){
			if global.is_paused{
				if audio_is_playing(_inst){ audio_pause_sound(_inst) }
			}
			else{
				if audio_is_paused(_inst){ audio_resume_sound(_inst) }
				var _g = ds_map_exists(global._VM_music_gain, _inst) ? global._VM_music_gain[? _inst] : 1
				audio_sound_gain(_inst, _g * _mvol)
			}
			_i++
		}
		else{
			ds_list_delete(global._VM_music_instances, _i)
			if ds_map_exists(global._VM_music_gain, _inst){ ds_map_delete(global._VM_music_gain, _inst) }
			if variable_global_exists("_VM_sound_len") && ds_map_exists(global._VM_sound_len, _inst){ ds_map_delete(global._VM_sound_len, _inst) }
		}
	}
}