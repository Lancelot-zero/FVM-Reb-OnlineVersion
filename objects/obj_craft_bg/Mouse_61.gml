var _rows = (button_select == 0) ? craft_tab_rows(0) : craft_tab_rows(1)
var _step = (button_select == 0) ? 96 : 88
var _max = max(0, _step * _rows - 815)
if y_offset_target <= _max - 40{
	y_offset_target += 40
}
else{
	y_offset_target = _max
}
