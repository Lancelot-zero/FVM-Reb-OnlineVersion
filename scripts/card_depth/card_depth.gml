/// @func calculate_plant_depth(col, row, plant_type)
/// @desc 根据网格坐标和植物类型计算深度值
/// @param {int} col 网格列
/// @param {int} row 网格行
/// @param {string} plant_type 植物类型
function calculate_plant_depth(col, row, plant_type) {
    // 基础深度值：下一行深度减少45，同一行从左到右深度递减5
    var base_depth = -10 - (row * 45) - (col * 5);
    
    // 根据植物类型设置层级偏移
    var layer_offset = 0;
    
    // 定义层级顺序（从大到小，即从前面到后面）；static避免每次调用重新分配数组（内容只读）
    static layer_order = ["lilypad", "shield_inner", "normal", "shield_outer", "coffee"];
    
    // 查找植物类型在层级顺序中的位置
    var layer_index = array_index_of(layer_order, plant_type);
    
    if (layer_index != -1) {
        // 每个层级占用1点深度值
        layer_offset = -layer_index;
    }
    
    // 最终深度值
    var depth_value = base_depth + layer_offset;
    
    // 确保深度在-10至-330范围内
    depth_value = clamp(depth_value, -330, -10);
    
    return depth_value;
}

/// @func array_index_of(array, value)
/// @desc 在数组中查找值的索引
/// @param {array} array 要搜索的数组
/// @param {mixed} value 要查找的值
function array_index_of(array, value) {
    for (var i = 0; i < array_length(array); i++) {
        if (array[i] == value) {
            return i;
        }
    }
    return -1;
}

/// @func sort_plants_in_grid(col, row)
/// @desc 对同一网格内的植物进行深度排序
/// @param {int} col 网格列
/// @param {int} row 网格行
function sort_plants_in_grid(col, row) {
    var plant_list = ds_grid_get(global.grid_plants, col, row);
    
    // 获取有效的植物实例
    var active_plants = ds_list_create();
    for (var i = 0; i < ds_list_size(plant_list); i++) {
        var plant_id = ds_list_find_value(plant_list, i);
        if (instance_exists(plant_id)) {
            ds_list_add(active_plants, plant_id);
        }
    }
    
    // 清空原始列表并重新添加有效植物
    ds_list_clear(plant_list);
    for (var i = 0; i < ds_list_size(active_plants); i++) {
        var plant_id = ds_list_find_value(active_plants, i);
        ds_list_add(plant_list, plant_id);
    }
    
    // 释放临时列表
    ds_list_destroy(active_plants);
    
    // 按深度值排序（从大到小，即从前面到后面）
    // 按 depth_value 降序（原来用 ds_list_sort + 匿名函数比较器）
    // 翻译器不支持匿名函数 → 改成"数组 + 手写插入排序"，只用已支持的原生函数
    var _n = ds_list_size(plant_list);
    var _arr = [];
    for (var i = 0; i < _n; i++) array_push(_arr, ds_list_find_value(plant_list, i));
    for (var i = 1; i < array_length(_arr); i++) {
        var _cur = _arr[i];
        var _j = i - 1;
        while (_j >= 0 && _arr[_j].depth_value < _cur.depth_value) {
            array_set(_arr, _j + 1, _arr[_j]);
            _j -= 1;
        }
        array_set(_arr, _j + 1, _cur);
    }
    ds_list_clear(plant_list);
    for (var i = 0; i < array_length(_arr); i++) ds_list_add(plant_list, _arr[i]);
    
    // 更新实例深度（确保绘制顺序正确）
    for (var i = 0; i < ds_list_size(plant_list); i++) {
        var plant = ds_list_find_value(plant_list, i);
        plant.depth = plant.depth_value + (i * 0.001); // 微小偏移确保正确绘制顺序
    }
}