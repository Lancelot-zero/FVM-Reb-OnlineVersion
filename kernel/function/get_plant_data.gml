function get_plant_data(plant_id) {
    if (ds_map_exists(global.plant_registry, plant_id)) {
        return global.plant_registry[? plant_id];
    }
    return undefined;
}
