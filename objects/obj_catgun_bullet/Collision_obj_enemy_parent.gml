// 友化敌人不算敌人：跳过，不结算碰撞
if (variable_instance_exists(other, "is_friend") && other.is_friend) exit

var _damage = damage 
if other.hp > 0 and row == other.grid_row  and can_hit(target_type,other.target_type){
	with(other){
		scare_timer = 30
		left_move_flashs = 30
		if grid_row == 0 {
			if global.row_feature[1] == global.row_feature[0]{
				y_move = global.grid_cell_size_y/left_move_flashs
				grid_row += 1
			}
		}
		else if grid_row == global.grid_rows - 1{
			if global.row_feature[global.grid_rows - 2] == global.row_feature[global.grid_rows - 1]{
				y_move = -global.grid_cell_size_y/left_move_flashs
				grid_row -= 1
			}
		}
		else{
			var im = irandom_range(1,100)
			var up_water = global.row_feature[grid_row - 1] != global.row_feature[grid_row]
			var down_water = global.row_feature[grid_row + 1] != global.row_feature[grid_row]
			if down_water {im = 25}
			if up_water {im = 75}
			if not (up_water && down_water){
				if im > 50{
					y_move = global.grid_cell_size_y/left_move_flashs
					grid_row += 1
				}
				else{
					y_move = -global.grid_cell_size_y/left_move_flashs
					grid_row -= 1
				}
			}
		}
	}
	instance_destroy()
}