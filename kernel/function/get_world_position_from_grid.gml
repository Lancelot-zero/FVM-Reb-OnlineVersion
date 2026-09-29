// get_world_position_from_grid(col, row) —— 格子 → 世界坐标（get_grid_position_from_world 的逆运算）
//   照抄自 scripts/get_grid_position_from_world/get_grid_position_from_world.gml 里的同名函数
//   （那个 .gml 里第二个 function，第 41-54 行）。
//   等价实现另见：transformGML2txt/vm_builtin.cpp 的 get_world_position_from_grid、
//   DLL_VM/outer/game_a.cpp 的 grid_world()、vm_builtin.cpp 的 put_cell_struct()。
//   返回 { col, row, x, y }，x/y = 格子中心 = offset + col*cell + cell/2。
//   拿不准的地方：无（算式、字段名、字段顺序都和上面几处一致）。
function get_world_position_from_grid(col, row) {
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
