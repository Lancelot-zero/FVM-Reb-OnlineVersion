// Inherit the parent event
if global.is_paused{
	exit
}

var dis_list = []
with obj_oil_lamp{
	var dis = abs(grid_row - other.grid_row) + abs(grid_col - other.grid_col)
	if shape >= 1{
		array_push(dis_list,0)
	}
	else{
		array_push(dis_list,dis)
	}
}
// 过火判定：不找 obj_brazier 实例了，改看格子快照里的过火标志位
//   global.cell_flag[格] = 这一格所有卡片 bullet_flag 的按位或（obj_battle 每帧重建）
//   bit1（& 1）= 过火类，火盆现在写的就是这个位；mod 卡片用 VM_SetProp 设 bullet_flag=1 也一样算
//   只看自己和上下左右四格（更远的距离原来也不参与判定），取值和原来一致：
//   同一格 → 1（会点亮）、相邻 1 格 → 4（不点亮）
if (variable_global_exists("cell_flag")) {
	var _cols = global.grid_cols;
	var _rows = global.grid_rows;
	if (array_length(global.cell_flag) == _cols * _rows) {
		for (var _dr = -1; _dr <= 1; _dr++) {
			var _r = grid_row + _dr;
			if (_r < 0 || _r >= _rows) continue;
			for (var _dc = -1; _dc <= 1; _dc++) {
				var _c = grid_col + _dc;
				if (_c < 0 || _c >= _cols) continue;
				if ((global.cell_flag[_r * _cols + _c] & 1) == 0) continue;
				var _dis = abs(_dr) + abs(_dc);
				if (_dis == 0)      array_push(dis_list, 1);
				else if (_dis == 1) array_push(dis_list, 4);
			}
		}
	}
}


var min_dis = 10
for(var i = 0 ; i < array_length(dis_list) ; i++){
	if dis_list[i] < min_dis{
		min_dis = dis_list[i]
	}
}
if min_dis <= 2{
	light_level = 2
	target_type = "normal"
}
else if min_dis == 2{
	light_level = 1
	target_type = "normal"
}
else{
	light_level = 0
	target_type = "invisible"
}


if light_level > 0 && image_alpha < 1{
	image_alpha += 0.05
}

if light_level == 0 && image_alpha > 0.5{
	image_alpha -= 0.05
}

event_inherited();

if is_frozen || is_stun{
	exit
}

if state == ENEMY_STATE.ACTING{
	timer++

	var current_move_speed = move_speed
	if is_slowdown{
		current_move_speed = move_speed / 2
	}
	if hp > maxhp*hurt_rate{
		image_index = floor(timer/flash_speed) mod move_anim
	}
	else{
		image_index = floor(timer/flash_speed) mod move_anim + move_anim
	}
	x -= current_move_speed
}


