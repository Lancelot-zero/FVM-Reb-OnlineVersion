function get_card_skill_data(card_id, shape, skill_level) {
    if (!ds_map_exists(global.skill_registry, card_id)) {
        return undefined;
    }
    
    var card_skills = global.skill_registry[? card_id];
    var shape_str = string(shape);
    
    if (!ds_map_exists(card_skills, shape_str)) {
        return undefined;
    }
    
    var skill_list = card_skills[? shape_str];
    
    if (skill_level < 0 || skill_level >= ds_list_size(skill_list)) {
        return undefined;
    }
    
    return skill_list[| skill_level];
}
