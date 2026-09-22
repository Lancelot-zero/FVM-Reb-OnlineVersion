/// @function load_file(file_slot)
/// @desc 加载存档文件到全局变量global.save_data中
/// @param {real} file_slot 存档槽位
function load_file(file_slot) {
	var file_path = src_mod_save_path("save" + string(file_slot) + ".json")
    // 检查存档文件是否存在
    if (!file_exists(file_path)) {
        // 如果存档不存在，创建初始存档数据
        reset_file(file_slot)
        return true;
    }
    
    // 打开存档文件
    var file = file_text_open_read(file_path);
    if (file == -1) {
        show_debug_message("无法打开存档文件!");
        return false;
    }
    
    // 读取文件内容
    var json_string = "";
    while (!file_text_eof(file)) {
        json_string += file_text_read_string(file);
        file_text_readln(file);
    }
    file_text_close(file);
    
    // 解析JSON字符串
    try {
        global.save_data = json_parse(json_string);
        show_debug_message("存档加载成功!");
		if global.save_data.version == 1.0 || global.save_data.version == "1.1"|| global.save_data.version == "1.2"|| global.save_data.version == "1.3"{
			reset_file(file_slot)
		}
		else{
			if global.save_data.version == "1.4"{
				if array_get_index(global.save_data.completed_levels,"cocoa_island_night") != -1{
					unlock_weapon("double_water_gun")
				}
				if array_get_index(global.save_data.completed_levels,"abyss") != -1{
					unlock_card("chocolate_pult",0,0,5)
				}
				global.save_data.version = 1.5
			}
			if global.save_data.version == 1.5{
				global.save_data.attires = []
				global.save_data.version = 1.6
			}
			if global.save_data.version == 1.6{
				global.save_data.player.crown_version = "0.0.0"
				global.save_data.version = 1.7
			}
			if global.save_data.version == 1.7{
				global.save_data.equipped_cookbook = [[],[],[]]
				global.save_data.version = 1.8
			}
		}
		// 卡池里已不存在的卡（mod 卡被删掉或改名后留下的孤儿记录）不进卡表：
		// 它们会在强化实验室/包裹等界面画成空框幽灵——点得中、能强化，但没有图没花费。
		// 只放进内存里的 global.save_orphan_cards，**不动存档结构**；
		// save_file 写盘时会把它们并回 unlocked_cards，所以存档内容和以前一样。
		global.save_orphan_cards = [];
		if (is_array(global.save_data.unlocked_cards)) {
			var _uc   = global.save_data.unlocked_cards;
			var _keep = [];
			for (var _i = 0; _i < array_length(_uc); _i++) {
				if (deck_get_card_data(_uc[_i].id, _uc[_i].shape) != noone) array_push(_keep, _uc[_i]);
				else array_push(global.save_orphan_cards, _uc[_i]);
			}
			if (array_length(global.save_orphan_cards) > 0) {
				show_debug_message("[save] 有 " + string(array_length(global.save_orphan_cards))
				                 + " 张卡未注册，本次会话内不显示（存档不动，写档时原样写回）");
			}
			global.save_data.unlocked_cards = _keep;
		}
        return true;
    } catch(e) {
        show_debug_message("存档解析错误: " + string(e));
        return false;
    }
}

function reset_file(file_slot){
	//重置到初始存档
	global.save_orphan_cards = [];   // 新档不带上一个档摘出来的未注册卡
	global.save_data = {
            "version": 1.8,
            "player": {
                "gold": 0,
                "level": 1,
                "experience": 0,
				"name":"Player",
				"total_time":0,
				"crown_version":"0.0.0"
            },
            "unlocked_cards": [
                {"id": "small_fire", "level": 0, "shape": 0,"skill":0,"max_level":0,"max_shape":0},
				{"id": "toast_bread", "level": 0, "shape": 0,"skill":0,"max_level":0,"max_shape":0},
				{"id": "xiao_long_bao", "level": 0, "shape": 0,"skill":0,"max_level":0,"max_shape":0},
				{"id": "flour_sack", "level": 0, "shape": 0,"skill":0,"max_level":0,"max_shape":0}
            ],
            "completed_levels": [],
            "inventory": [],
            "unlocked_items": {
                "max_card_level": 0,
                "max_skill_level": 0,
                "max_gem_level": 0,
				"max_slot":5,
				"max_shape":[],
				"shovel":"normal",
				"elite_unlocked":false,
				"mario_mouse_killed":false,
				"arno_killed":false
            },
            "unlocked_weapons": [
                {"id": "long_bao_gun"}
            ],
			"unlocked_gems":[],
			"equipped_items":{
				"main_weapon":{
					"id":"long_bao_gun",
					"gems":[]
				},
				"secondary_weapon":{
					"id":"",
					"gems":[]
				},
				"super_weapon":{
					"id":"",
					"gems":[]
				}
			},
			"saved_decks":[
				{"name":"卡组1","card_id":[]},
				{"name":"卡组2","card_id":[]},
				{"name":"卡组3","card_id":[]},
				{"name":"卡组4","card_id":[]},
				{"name":"卡组5","card_id":[]},
				{"name":"卡组6","card_id":[]} 
			],
			"tasks":[
				{
					"id":"main_level_0",
					"progress":[0],
					"state":"new"
				},
				{
					"id":"card_upgrade_1",
					"progress":[0],
					"state":"new"
				},
				{
					"id":"flame_save_1",
					"progress":[0,0],
					"state":"new"
				},
				{
					"id":"perfect_challenge_1",
					"progress":[0,0,0],
					"state":"new"
				},
				{
					"id":"hardcore_challenge_1",
					"progress":[0,0,0],
					"state":"new"
				}
			],
			"completed_tasks":[],
			"attires":[],
			"equipped_cookbook":[[],[],[]]
        };
	save_file(file_slot)
}