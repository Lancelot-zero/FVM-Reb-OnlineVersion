/// @description 切换音乐
if (is_string(battle_music)) battle_music = get_load_audio(battle_music);
if (is_string(new_battle_music)) new_battle_music = get_load_audio(new_battle_music);
if battle_music != new_battle_music{
if (audio_exists(battle_music)) audio_stop_sound(battle_music)
battle_music = new_battle_music
}