function get_plant_shape_data(plant_id, shape) {
    var plant_data = get_plant_data(plant_id);
    if (plant_data == undefined) return undefined;
    
    var shapes_map = plant_data[? "shapes"];
    if (ds_map_exists(shapes_map, string(shape))) {
        return shapes_map[? string(shape)];
    }
    
    return undefined;
}
