function update_notices() {
    if (!variable_global_exists("notice_list")) return;
    
    var camera_x = camera_get_view_x(view_camera[0]);
    var camera_y = camera_get_view_y(view_camera[0]);
    var camera_w = camera_get_view_width(view_camera[0]);
    var camera_h = camera_get_view_height(view_camera[0]);
    
    for (var i = array_length(global.notice_list) - 1; i >= 0; i--) {
        var notice = global.notice_list[i];
        notice.frames++;
        
        // 更新位置以确保通知保持在屏幕中心偏上
        notice.pos_x = room_width / 2;
        //notice.target_y = camera_y + camera_h / 3;
        //notice.pos_y = notice.target_y;
        
        // 前20帧：淡入和缩放效果
        if (notice.frames <= 10) {
            notice.alpha = notice.frames / 10;
            notice.scale = 2.2 - (0.2 * (notice.frames / 10));
        }
        // 生命周期结束前的阶段
        else if (notice.frames > notice.life) {
            // 向上移动并淡出
            notice.pos_y -= 2;
            notice.alpha = 1 - ((notice.frames - notice.life) / 20);
            
            // 当完全透明时移除通知
            if (notice.alpha <= 0) {
                array_delete(global.notice_list, i, 1);
                continue;
            }
        }
        
        global.notice_list[i] = notice;
    }
}
