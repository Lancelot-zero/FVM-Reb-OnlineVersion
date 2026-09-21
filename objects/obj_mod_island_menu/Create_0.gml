// Mod 大地图的小地图选择菜单
state = {
    map_id: "",
    map_name: "",
    levels: [],
    buttons: [],
    positions: [],
    close_btn: undefined
};
depth = -100;

// 参考世界地图：占用 menu_type，让依赖 menu_type == 0 的原版按钮停止响应
if (instance_exists(obj_player_info_ui)) obj_player_info_ui.menu_type = 5;

function destroy_widgets() {
    for (var _i = 0; _i < array_length(self.state.buttons); _i++) {
        var _b = self.state.buttons[_i];
        if (instance_exists(_b)) instance_destroy(_b);
    }
    self.state.buttons = [];
    self.state.positions = [];
    if (!is_undefined(self.state.close_btn) && instance_exists(self.state.close_btn)) {
        instance_destroy(self.state.close_btn);
    }
    self.state.close_btn = undefined;
}

function init(_map_id) {
    self.state.map_id = _map_id;
    if (!variable_global_exists("mod_maps") || !ds_map_exists(global.mod_maps, _map_id)) return self;

    var _map = global.mod_maps[? _map_id];
    self.state.map_name = _map[$ "name"] ?? _map_id;
    self.state.levels = _map[$ "levels"] ?? [];

    var _count = array_length(self.state.levels);
    var _cols = 4;
    var _cell_w = 260;
    var _cell_h = 220;
    var _start_x = room_width / 2 - ((_cols - 1) * _cell_w) / 2;
    var _start_y = room_height / 2 - ((ceil(_count / _cols) - 1) * _cell_h) / 2;

    for (var _i = 0; _i < _count; _i++) {
        var _lv = self.state.levels[_i];
        var _spr = _lv[$ "icon"] ?? spr_levelselect_button;
        if (!sprite_exists(_spr)) _spr = spr_levelselect_button;

        var _x = _start_x + (_i mod _cols) * _cell_w;
        var _y = _start_y + floor(_i / _cols) * _cell_h;

        var _icon_scale = 0.5;
        if (sprite_exists(_spr)) {
            var _sw = sprite_get_width(_spr);
            var _sh = sprite_get_height(_spr);
            if (_sw > 0 && _sh > 0) {
                _icon_scale = min(0.5, 180 / max(_sw, _sh));
            }
        }

        var _btn = instance_create_depth(_x, _y, -101, Button);
        _btn.set_sprite(_spr)
            .set_scale(_icon_scale)
            .set_position(_x, _y)
            .set_on_click(method({map_id: _map_id, level_index: _i}, function() {
                src_mod_map_enter(map_id, level_index);
                if (instance_exists(obj_mod_island_menu)) instance_destroy(obj_mod_island_menu);
            }));

        array_push(self.state.buttons, _btn);
        array_push(self.state.positions, {
            x: _x,
            y: _y,
            name: _lv[$ "name"] ?? ("关卡 " + string(_i + 1))
        });
    }

    self.state.close_btn = instance_create_depth(room_width - 220, 80, -101, Button);
    self.state.close_btn.set_sprite(spr_closemenu_btn)
                      .set_scale(2.0)
                      .set_position(room_width - 220, 80)
                      .set_on_click(function() {
                          instance_destroy(obj_mod_island_menu);
                      });

    return self;
}
