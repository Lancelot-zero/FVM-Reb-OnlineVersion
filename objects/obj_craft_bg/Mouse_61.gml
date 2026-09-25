// 两页行高不同（卡片 96 / 宝石 88），滚动上限按当前页内容自动扩
var _rows = (button_select == 0) ? craft_tab_rows(0) : craft_tab_rows(1)
var _step = (button_select == 0) ? 96 : 88
var _max = max(0, _step * _rows - 815)
if y_offset <= _max - 40{
	y_offset += 40
}
else{
	y_offset = _max
}