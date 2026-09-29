function show_notice(notice_text, life) {
    // 创建通知数据结构
    var notice = {
        text: notice_text,
        life: life,
        total_life: life,
        frames: 0,
        alpha: 0,
        scale: 1.2,
        pos_x: room_width / 2,
        pos_y: room_height / 3,
        target_y: camera_get_view_y(view_camera[0]) + camera_get_view_height(view_camera[0]) / 3
    };
    
    // 如果没有全局通知列表，则创建一个
    if (!variable_global_exists("notice_list")) {
        global.notice_list = [];
    }
    
    // 将新通知添加到列表
    array_push(global.notice_list, notice);
}
