function get_plant_upgrade(plant_id, shape, level) {
    var shape_data = get_plant_shape_data(plant_id, shape);
    if (shape_data == undefined) return undefined;
    
    var upgrades = shape_data[? "upgrades"];
    if (ds_map_exists(upgrades, string(level))) {
        return upgrades[? string(level)];
    }
    
    return undefined;
}
