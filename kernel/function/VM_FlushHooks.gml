function VM_FlushHooks() {
    while (array_length(global._VM_hook_queue) > 0) {
        var _q = global._VM_hook_queue;
        global._VM_hook_queue = [];
        for (var _i = 0; _i < array_length(_q); _i++) {
            var _e = _q[_i];
            if (_e.key == "card")        global._VM_last_created_card   = _e.id;
            if (_e.key == "card_del")    global._VM_last_destroyed_card = _e.id;
            if (_e.key == "enemy")       global._VM_last_created_enemy  = _e.id;
            if (_e.key == "enemy_kill")  global._VM_last_killed_enemy   = _e.id;
            if (_e.key == "platform_idle") global._VM_last_idle_platform = _e.id;
            var _hook_name = "";
            switch (_e.key) {
                case "card":            _hook_name = "_VM_CARD_CREATED";      break;
                case "card_del":        _hook_name = "_VM_CARD_DESTROYED";    break;
                case "enemy":           _hook_name = "_VM_ENEMY_SPAWNED";     break;
                case "enemy_kill":      _hook_name = "_VM_ENEMY_KILLED";      break;
                case "platform_idle":   _hook_name = "_VM_PLATFORM_IDLE_END"; break;
            }
            global._VM_cur_dead_snap = _e.snap;
            // 全局（地图脚本）没定义这个块时 _e.buf 是 undefined —— 只有 mod 单位挂了这个事件，
            // 那就跳过地图脚本的执行，直接交给 vm_hook_run 跑各 mod 的块
            vm_hook_run(_hook_name);   // mod 侧：同一时机，各自查块（读上面刚设好的 _VM_last_* 全局）
            global._VM_cur_dead_snap = undefined;
        }
    }
    ds_map_clear(global._VM_dead_snaps);   // 销毁逻辑处理完，释放快照
}
