save_file(global.save_slot)

global.laboratory_manager.dispose()

// 退出前卸载鼠标限频钩子。
// WH_MOUSE_LL 要由持有它的线程退出才会从系统钩子链上摘掉，故须在进程结束前停掉。
if (native_stop_mouse_limit != undefined) {
    native_stop_mouse_limit();
}
