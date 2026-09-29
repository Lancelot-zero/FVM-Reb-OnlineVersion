function network_apply_plant_level(_plant,use=false) {
	if(use==false)exit;
	if(_plant.object_index == obj_player_character){exit;}
    // 1. 清理本地存档创建的星标
    if (variable_instance_exists(_plant, "banding_star_obj")) {
        if (instance_exists(_plant.banding_star_obj)) {
            instance_destroy(_plant.banding_star_obj);
            _plant.banding_star_obj = noone;
        }
    }

    // 2. 设置网络值
    var level = _plant.current_level;
    var skill = _plant.skill ;
    var shape = _plant.shape ;

    // 3. 重新计算属性
    var upgrade_data = get_plant_data_with_skill(_plant.plant_id, shape, level, skill);
    if (upgrade_data != undefined) {
        //_plant.hp = upgrade_data[? "hp"];
        _plant.max_hp = _plant.hp;
        _plant.atk = upgrade_data[? "atk"];
        _plant.range = upgrade_data[? "range"];
        _plant.cooldown = upgrade_data[? "cooldown"];
        _plant.cycle = upgrade_data[? "cycle"];
        if (ds_map_exists(upgrade_data, "flame_produce")) {
            _plant.flame_produce = upgrade_data[? "flame_produce"];
        }
    }

    // 4. 创建正确星级的星标
    if (level >= 4) {
        var star_spr = -1;
        switch (level) {
            case 4: star_spr = spr_star_4; break;
            case 5: star_spr = spr_star_5; break;
            case 6: star_spr = spr_star_6; break;
            case 7: star_spr = spr_star_7; break;
            case 8: star_spr = spr_star_8; break;
            case 9: star_spr = spr_star_9; break;
            case 10: star_spr = spr_star_10; break;
            case 11: star_spr = spr_star_11; break;
            case 12: star_spr = spr_star_12; break;
            case 13: star_spr = spr_star_13; break;
            case 14: star_spr = spr_star_14; break;
            case 15: star_spr = spr_star_15; break;
            case 16: star_spr = spr_star_16; break;
        }
        var inst = instance_create_depth(_plant.x, _plant.y - 5, _plant.depth - 1, obj_stars);
        inst.sprite_index = star_spr;
        inst.parent_card = _plant.id;
        _plant.banding_star_obj = inst.id;
    }

}
