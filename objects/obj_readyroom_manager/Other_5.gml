if (audio_is_playing(readyroom_music) or audio_is_paused(readyroom_music)) {
        audio_stop_sound(readyroom_music);
}

// ── 插件音频生命周期：回菜单/选关 = 离开插件，卸载 VM 音效与音乐轨（别漏音到菜单）──
//   开战不卸：下一屏是 room_battle，插件继续用同一份音频（战斗结束 obj_battle CleanUp 会卸）
if (variable_global_exists("gui_stack") && gui_stack_get_top(global.gui_stack) != room_battle) {
    vm_audio_unload_all();
}
