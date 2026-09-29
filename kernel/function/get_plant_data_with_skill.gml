function get_plant_data_with_skill(plant_id, shape, upgrade_level, skill_level) {
    // 获取基础升级数据
    var upgrade_data = get_plant_upgrade(plant_id, shape, upgrade_level);
    if (upgrade_data == undefined) {
        return undefined;
    }
    var info_id = global.skill_registry[? plant_id][0]
	upgrade_data[? info_id] = global.skill_registry[? plant_id][1][skill_level]
	return(upgrade_data)
    //// 复制升级数据，避免修改原始数据
    //var result_data = ds_map_create()
	//ds_map_copy(result_data,upgrade_data);
    
    //// 获取技能数据
    //var skill_data = get_card_skill_data(plant_id, shape, skill_level);
    //if (skill_data != undefined) {
    //    // 应用技能效果（覆盖特定属性）
    //    var keys = ds_map_keys(skill_data);
    //    for (var i = 0; i < ds_map_size(skill_data); i++) {
    //        var key = keys[i];
    //        // 跳过非数值属性（如名称、描述等）
    //        if (key != "name" && key != "description" && key != "shape" && key != "level") {
    //            result_data[? key] = skill_data[? key];
    //        }
    //    }
    //}
}
