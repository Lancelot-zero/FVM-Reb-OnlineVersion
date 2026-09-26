if not is_placed{
	var logical_x = mouse_x;
	var logical_y = mouse_y;
	var platform_shift_x = 0;
	var platform_shift_y = 0;

	var plat = instance_position(mouse_x, mouse_y, obj_platform);
	if (plat != noone) {
		platform_shift_x = plat.visual_x_shift;
		platform_shift_y = plat.visual_y_shift;
		logical_x -= platform_shift_x;
		logical_y -= platform_shift_y;
	}

	var can_plant = (can_place_at_position(logical_x, logical_y, "normal","amphi","none"));
	if can_plant{
		is_placed = true
		global.is_paused = false
		var grid_pos = get_grid_position_from_world(logical_x, logical_y)
		x = grid_pos.x + platform_shift_x
		y = grid_pos.y+10 + platform_shift_y
		grid_row = grid_pos.row
		grid_col = grid_pos.col
		card_created(id,grid_col,grid_row);
		if(global.network.mode=="client"){
			var gem_index = 0
			if global.save_data.equipped_items.main_weapon.id != ""{
				var gem_list = global.save_data.equipped_items.main_weapon.gems
				for(var i = 0 ; i < array_length(gem_list);i++){
					var gem_id = gem_list[i]
					if array_get_index(global.banned_gems_online, gem_id) != -1 { continue; }
					var gem_info = get_gem_info(gem_id)
					if (is_struct(gem_info) && gem_info.obj != noone){
						// mod 宝石本地不建：和角色本体一样，这份本地预测不要，等服务器广播回来按角色重建
						if (gem_info.obj == obj_gem_mod) continue;
						global._mod_pending_gem_id = gem_id;
						var _gi = instance_create_depth(390,213+gem_index*80,-500,gem_info.obj);
						// 给 mod 宝石挂"发射方"的信息：插件宝石脚本靠这几个变量定位（别用 parent_player，会和原版语义打架）
						_gi.mod_point_parent_player = id;
						_gi.mod_point_grid_row      = grid_row;
						_gi.mod_point_grid_col      = grid_col;
						_gi.mod_point_gem_level     = get_gem_level(gem_id);
						gem_index++
					}
				}
			}
			instance_destroy(id);
			exit;
		}
		audio_play_sound(snd_place1,0,0)
		instance_create_depth(x,y,-2,obj_place_effect)
		var plany_list = ds_grid_get(global.grid_plants,grid_col,grid_row)
		if global.grid_terrains[grid_row][grid_col].type == "water"{
			var card = instance_create_depth(x,y-10,depth+1,obj_wooden_plate)
			card_created(card,grid_col,grid_row)
			
		}
		var gem_index = 0
		if global.save_data.equipped_items.main_weapon.id != ""&&global._VM_ban_weapon==false{
			var main_info = get_weapon_info(global.save_data.equipped_items.main_weapon.id)
			global._mod_pending_weapon_id = global.save_data.equipped_items.main_weapon.id
			var main_weapon_inst = instance_create_depth(x-10,y-100,depth-1,main_info.obj)
			global._mod_pending_weapon_id = ""
			main_weapon_inst.parent_player = id
			if (variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, global.save_data.equipped_items.main_weapon.id)) {
				main_weapon_inst.weapon_id = global.save_data.equipped_items.main_weapon.id
				with (main_weapon_inst) event_perform(ev_step, ev_step_normal)
			}
			main_weapon_inst.grid_row = grid_row
			main_weapon_inst.grid_col = grid_col
			cycle = main_info.cycle
			var gem_list = global.save_data.equipped_items.main_weapon.gems
			for(var i = 0 ; i < array_length(gem_list);i++){
				var gem_id = gem_list[i]
				if array_get_index(global.banned_gems_online, gem_id) != -1 { continue; }
				var gem_info = get_gem_info(gem_id)
				if (is_struct(gem_info) && gem_info.obj != noone){
					global._mod_pending_gem_id = gem_id;
					var _gem_inst = instance_create_depth(390,213+gem_index*80,-500,gem_info.obj);
					// 发射方信息（插件宝石靠它定位；不用 parent_player，免得和原版语义打架）
					_gem_inst.mod_point_parent_player = id;
					_gem_inst.mod_point_grid_row      = grid_row;
					_gem_inst.mod_point_grid_col      = grid_col;
					_gem_inst.mod_point_gem_level     = get_gem_level(gem_id);
					// mod 宝石：归属写完了再调一次它的步（初始化在它的 Step 顶部）
					if (variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
						with (_gem_inst) event_perform(ev_step, ev_step_normal);
					}
					gem_index++
				}
			}
		}
		if global.save_data.equipped_items.secondary_weapon.id != ""&&global._VM_ban_shield==false{
			var s_inst = instance_create_depth(x,y,depth,obj_player_shield)
			s_inst.parent_player = id
			s_inst.grid_row = grid_row
			s_inst.grid_col = grid_col
			var main_info = get_weapon_info(global.save_data.equipped_items.secondary_weapon.id)
			// hp_increase 是条件字段（JSON 里不写就没有），不能无条件读
			if (variable_struct_exists(main_info, "hp_increase")) {
				hp += main_info.hp_increase
				max_hp += main_info.hp_increase
			}
			if get_gem_index("health_gem") != -1{
				hp += get_gem_info("health_gem").hp_increase * (get_gem_level("health_gem")+1)
				max_hp += get_gem_info("health_gem").hp_increase * (get_gem_level("health_gem")+1)
			}
			// 副武器再挂一个 obj_weapon_mod：
			//   obj_player_shield 照旧负责加血 + 原版盾宝石（产火苗/减速/流血/护盾/强化）
			//   这个实例负责跑 mod 盾 .bin 的三个块，并把武器贴图显示出来
			// 只有真的加载了 mod 盾 .bin 才挂，免得到内置盾（cookie/oreo/cut_cake）头上画个图标
			var _sec_id = global.save_data.equipped_items.secondary_weapon.id
			if variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, _sec_id){
				global._mod_pending_weapon_id = _sec_id		
				var _mod_shield = instance_create_depth(x-10,y-100,depth-1,obj_weapon_mod)
				global._mod_pending_weapon_id = ""
				_mod_shield.parent_player = id
				_mod_shield.weapon_id = _sec_id
				with (_mod_shield) event_perform(ev_step, ev_step_normal)
				_mod_shield.grid_row = grid_row
				_mod_shield.grid_col = grid_col
			}
			// 副武器自己的宝石也要建对象（原来只建了盾牌本体，宝石一个都不出现）
			var sec_gem_list = global.save_data.equipped_items.secondary_weapon.gems
			for(var i = 0 ; i < array_length(sec_gem_list);i++){
				var gem_id = sec_gem_list[i]
				if array_get_index(global.banned_gems_online, gem_id) != -1 { continue; }
				var gem_info = get_gem_info(gem_id)
				if (is_struct(gem_info) && gem_info.obj != noone){
					global._mod_pending_gem_id = gem_id;
					var _gi = instance_create_depth(390,213+gem_index*80,-500,gem_info.obj);
					// 发射方信息（插件宝石靠它定位；不用 parent_player，免得和原版语义打架）
					_gi.mod_point_parent_player = id;
					_gi.mod_point_grid_row      = grid_row;
					_gi.mod_point_grid_col      = grid_col;
					_gi.mod_point_gem_level     = get_gem_level(gem_id);
					// mod 宝石：归属写完了再调一次它的步（初始化在它的 Step 顶部）
					if (variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
						with (_gi) event_perform(ev_step, ev_step_normal);
		}
					gem_index++
				}
			}
		}
		if global.save_data.equipped_items.super_weapon.id != ""&&global._VM_ban_super_weapon==false{
			var main_info = get_weapon_info(global.save_data.equipped_items.super_weapon.id)
			global._mod_pending_weapon_id = global.save_data.equipped_items.super_weapon.id
			var main_weapon_inst = instance_create_depth(x-10,y-100,depth-1,main_info.obj)
			global._mod_pending_weapon_id = ""
			main_weapon_inst.parent_player = id
			if (variable_global_exists("mod_weapon_vms") && ds_map_exists(global.mod_weapon_vms, global.save_data.equipped_items.super_weapon.id)) {
				main_weapon_inst.weapon_id = global.save_data.equipped_items.super_weapon.id
				with (main_weapon_inst) event_perform(ev_step, ev_step_normal)
			}
			main_weapon_inst.grid_row = grid_row
			main_weapon_inst.grid_col = grid_col
			// 超级武器的宝石同理
			var sup_gem_list = global.save_data.equipped_items.super_weapon.gems
			for(var i = 0 ; i < array_length(sup_gem_list);i++){
				var gem_id = sup_gem_list[i]
				if array_get_index(global.banned_gems_online, gem_id) != -1 { continue; }
				var gem_info = get_gem_info(gem_id)
				if (is_struct(gem_info) && gem_info.obj != noone){
					global._mod_pending_gem_id = gem_id;
					var _gi = instance_create_depth(390,213+gem_index*80,-500,gem_info.obj);
					// 发射方信息（插件宝石靠它定位；不用 parent_player，免得和原版语义打架）
					_gi.mod_point_parent_player = id;
					_gi.mod_point_grid_row      = grid_row;
					_gi.mod_point_grid_col      = grid_col;
					_gi.mod_point_gem_level     = get_gem_level(gem_id);
					// mod 宝石：归属写完了再调一次它的步（初始化在它的 Step 顶部）
					if (variable_global_exists("mod_gem_vms") && ds_map_exists(global.mod_gem_vms, gem_id)) {
						with (_gi) event_perform(ev_step, ev_step_normal);
		}
					gem_index++
				}
			}
		}
	}
	
}