// calculate_plant_depth(col, row, plant_type) —— 植物深度值
//   照抄自 scripts/card_depth/card_depth.gml 的 calculate_plant_depth（第 6-31 行）：
//     base_depth = -10 - row*45 - col*5，再按层级名扣 0..4，最后 clamp 到 [-330, -10]。
//   等价实现另见：DLL_VM/outer/game_a.cpp 的 plant_depth()（第 181-190 行）、
//   transformGML2txt/vm_builtin.cpp 的 calculate_plant_depth()（第 594-613 行）——三处算式一致。
//   差异（照抄时只改了写法，没改语义）：参考版用 array_index_of(layer_order, plant_type)，
//   而 array_index_of 不是 native/game API 里的名字（它是 card_depth.gml 里自己定义的辅助函数，
//   一个文件只能装一个函数，没法搬过来），所以这里把"查表返回下标"展开成等价的 if 链。
//   照原文：找不到层级名时 layer_offset 保持 0（不扣）。
function calculate_plant_depth(col, row, plant_type) {
    // 基础深度值：下一行深度减少45，同一行从左到右深度递减5
    var base_depth = -10 - (row * 45) - (col * 5);

    // 根据植物类型设置层级偏移；层级顺序（从大到小，即从前面到后面）：
    // lilypad(0) / shield_inner(1) / normal(2) / shield_outer(3) / coffee(4)，每层扣 1 点
    var layer_offset = 0;
    if (plant_type == "lilypad") {
        layer_offset = 0;
    } else if (plant_type == "shield_inner") {
        layer_offset = -1;
    } else if (plant_type == "normal") {
        layer_offset = -2;
    } else if (plant_type == "shield_outer") {
        layer_offset = -3;
    } else if (plant_type == "coffee") {
        layer_offset = -4;
    }

    // 最终深度值
    var depth_value = base_depth + layer_offset;

    // 确保深度在-10至-330范围内
    depth_value = clamp(depth_value, -330, -10);

    return depth_value;
}
