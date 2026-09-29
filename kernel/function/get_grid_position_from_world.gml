// get_grid_position_from_world(wx, wy, raw) —— 世界坐标 → 格子（含回绕到扩展区）
//   已按 scripts/get_grid_position_from_world/get_grid_position_from_world.gml 逐行核对：算式一致
//   （rel = wx - global.grid_offset_x；col = floor(rel / cell_size)；!raw 时按 cols/rows + 64 回绕；
//     再算格心 offset + col*cell + cell/2），返回 { col, row, x, y }。
//   等价实现另见：DLL_VM/outer/game_a.cpp 的 grid_world() / grid_wrap_col() / grid_wrap_row()、
//   transformGML2txt/vm_builtin.cpp 的 get_grid_position_from_world()。
//   raw = false 是字面量默认值（翻译器支持），保留。
//   拿不准的地方：无。
function get_grid_position_from_world(wx, wy, raw = false) {
    // 转换为相对于网格的坐标
    var rel_x = wx - global.grid_offset_x;
    var rel_y = wy - global.grid_offset_y;
    
    // 计算网格索引
    var col = floor(rel_x / global.grid_cell_size_x);
    var row = floor(rel_y / global.grid_cell_size_y);
    if (!raw) {
        if (col < 0) {
            col = col + global.grid_cols + 64
        }
        if (col >= global.grid_cols + 64) {
            col = col - global.grid_cols - 64
        }
        if (row < 0) {
            row = row + global.grid_rows + 64
        }
        if (row >= global.grid_rows + 64) {
            row = row - global.grid_rows - 64
        }
    }
    
    // 计算网格中心位置
    var center_x = global.grid_offset_x + col * global.grid_cell_size_x + global.grid_cell_size_x / 2;
    var center_y = global.grid_offset_y + row * global.grid_cell_size_y + global.grid_cell_size_y / 2;
    
    return {
        col: col,
        row: row,
        x: center_x,
        y: center_y
    };
}
