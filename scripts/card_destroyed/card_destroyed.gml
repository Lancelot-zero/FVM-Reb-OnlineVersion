/// @function plant_destroyed(plant_inst)
/// @description 当植物销毁时调用，更新网格数据
/// @param {instance} plant_inst 植物实例
function card_destroyed(plant_inst) {
    global._VM_last_destroyed_card = plant_inst.id;
    // 帧内去重：显式调用 + Destroy 事件会各入队一次，避免 hook 一帧触发两次
    // 只对「玩家带进战斗的卡」触发（和 _VM_CARD_CREATED 同一判据，见 vm_card_hook_allowed）
    if (!ds_map_exists(global._VM_dead_snaps, plant_inst.id) && vm_card_hook_allowed(plant_inst)) VM_QueueHook(global._VM_CARD_DESTROYED, "card_del", plant_inst.id);

    var col = plant_inst.grid_col;
    var row = plant_inst.grid_row;
    
    // 检查位置是否有效
    if (col < 0 || col >= global.grid_cols || row < 0 || row >= global.grid_rows) {
        return;
    }
    
    // 获取该网格的植物列表
    var plant_list = ds_grid_get(global.grid_plants, col, row);
    
    // 从列表中移除植物
    var index = ds_list_find_index(plant_list, plant_inst);
    if (index != -1) {
        ds_list_delete(plant_list, index);
    }
    
    // 更新剩余植物的深度偏移
    //update_card_depths(col, row);
}