function draw_notices() {
    if (!variable_global_exists("notice_list")) return;
    
    for (var i = 0; i < array_length(global.notice_list); i++) {
        var notice = global.notice_list[i];
        
        // 设置字体和颜色
        var text = notice.text;
        var pos_x = notice.pos_x;
        var pos_y = notice.pos_y;
        var alpha = notice.alpha;
        var scale = notice.scale;
		if variable_global_exists("_VM_notice_scale")&&global._VM_notice_scale!=-1{
			scale = global._VM_notice_scale;
			global._VM_notice_scale=-1
		}
        
		draw_set_font(font_yuan);
		
        // 计算文本尺寸
        var text_width = string_width(text) * scale;
        var text_height = string_height(text) * scale;
        
        // 绘制半透明黑色背景框
        var padding = 5 * scale;
        draw_set_alpha(0.7 * alpha);
        draw_set_color(c_black);
        draw_roundrect(
            pos_x - text_width/2 - padding,
            pos_y - text_height*scale + padding,
            pos_x + text_width/2 + padding,
            pos_y - text_height + padding*2,
            false // 不绘制轮廓
        );
        
        // 绘制白色描边
        draw_set_alpha(alpha);
        draw_set_color(c_white);
        //draw_set_line_width(2 * scale);
        draw_roundrect(
            pos_x - text_width/2 - padding,
            pos_y - text_height*scale + padding,
            pos_x + text_width/2 + padding,
            pos_y - text_height + padding*2,
            true // 绘制轮廓
        );
        
        // 绘制绿色文本
        draw_set_color(make_color_rgb(0, 255, 0)); // 绿色
		if variable_global_exists("_VM_notice_color_r")&&global._VM_notice_color_r!=-1{
			var n_color_r = min(max(0,global._VM_notice_color_r),255)
			var n_color_g = min(max(0,global._VM_notice_color_g),255)
			var n_color_b = min(max(0,global._VM_notice_color_b),255)
			draw_set_color(make_color_rgb(n_color_r, n_color_g, n_color_b));
			global._VM_notice_color_r=-1
		}
        draw_set_font(font_yuan);
        draw_set_halign(fa_center);
        draw_set_valign(fa_middle);
        draw_set_alpha(alpha);
        draw_text_ext_transformed(
            pos_x, pos_y-3, text,90,1920,scale,scale,0
        );
        
        // 重置绘制设置
        draw_set_alpha(1);
        draw_set_color(c_white);
        //draw_set_line_width(1);
    }
}
