function shell_print(msg) {
	shell_log_write(msg);   // sudo 认证后同时落盘到 mod/shell.log
	inst_id = inst_224771E1; 
    // 检查实例是否存在
    if (!instance_exists(inst_id)) {
        show_debug_message("[shell_push_output] 实例 " + string(inst_id) + " 不存在");
        return;
    }
    
    // 通过 with 操作该实例
    with (inst_id) {
        // 检查 output 数组是否存在
        if (!variable_struct_exists(self, "output")) {
            show_debug_message("[shell_push_output] 实例 " + string(inst_id) + " 没有 output 数组");
            return;
        }
        // 压入消息
        array_push(output, msg);
    }
}
