// 平台计数减1，归零时恢复原始地形
var _is_x = (variable_instance_exists(id, "move_axis") && move_axis == "x");
var _c_off = _is_x ? current_offset : 0;
var _r_off = (!_is_x) ? current_offset : 0;
var _cs = start_col + _c_off;
var _rs = start_row + _r_off;
for (var _c = _cs; _c < _cs + width; _c++) {
    for (var _r = _rs; _r < _rs + length; _r++) {
        if (_r >= 0 && _r < global.grid_rows && _c >= 0 && _c < global.grid_cols) {
            global.grid_platform_count[_r][_c]--;
            if (global.grid_platform_count[_r][_c] <= 0) {
                global.grid_platform_count[_r][_c] = 0;
                global.grid_terrains[_r][_c].type = global.grid_terrains_original[_r][_c];
            }
        }
    }
}