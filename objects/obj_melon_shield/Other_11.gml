var _x = x;
var _y = y;
var _range = 250
with (obj_enemy_parent) {
	// 友化敌人不算敌人：跳过，不结算伤害
	if (variable_instance_exists(id, "is_friend") && is_friend) continue

	if (hp > 0 && point_distance(x, y, _x, _y) < _range && grid_row >= other.grid_row-1&&grid_row <= other.grid_row+1) {
		hp -= other.bleed_damage
		event_user(0)
	}
}