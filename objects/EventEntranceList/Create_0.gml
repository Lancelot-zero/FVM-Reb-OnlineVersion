///

self.state = {
    left: 0,
    top: 0,
    /// @type {Array<Asset.GMObject.Button>}
    elements: [],
    width: 300,
    height: 300,
    grid_list: undefined,
    mod_grid_list: undefined
}

function create_widgets() {
    var _items = []
    var _mod_items = []

    /// @type {Asset.GMObject.Button}
    var _laboratory_entrance = instance_create_depth(0, 0, -3, Button)
    _laboratory_entrance.set_sprite(spr_laboratory_icon)
                        .set_scale(0.9)
                        .set_on_click(function() {
                            global.gui_stack.to(room_laboratory)
                        })
                        .set_should_correspond(method({gui_state: self.state}, function() {
                            var _has_float_layer = instance_exists(obj_config_menu) ||
                                                   instance_exists(obj_edit_menu) ||
                                                   instance_exists(obj_world_map_menu) ||
                                                   instance_exists(obj_package_bg) ||
                                                   instance_exists(obj_info_island_bg) ||
                                                   instance_exists(obj_shop_bg) ||
                                                   instance_exists(obj_task_bg) ||
                                                   instance_exists(obj_craft_bg) ||
                                                   instance_exists(obj_tower_cake_bg) ||
                                                   instance_exists(obj_quit_confirm) ||
                                                   instance_exists(obj_mod_island_menu)
                            return !_has_float_layer
                        }))

    array_push(_items, _laboratory_entrance)

    // 每个 mod 大地图生成一个入口按钮，放在实验室按钮右边
    if (variable_global_exists("mod_maps") && ds_map_size(global.mod_maps) > 0 &&
        variable_global_exists("mod_map_order")) {
        for (var _mi = 0; _mi < array_length(global.mod_map_order); _mi++) {
            var _map_id = global.mod_map_order[_mi];
            var _map = global.mod_maps[? _map_id];
            var _spr = _map[$ "icon"] ?? -1;
            if (!sprite_exists(_spr)) _spr = src_mod_default_map_icon();
            if (!sprite_exists(_spr)) _spr = spr_laboratory_icon;

            // 入口按钮高度尽量和实验室按钮一致，避免排列参差不齐
            var _icon_scale = 0.9;
            if (sprite_exists(_spr)) {
                var _sh = sprite_get_height(_spr);
                var _target_h = _laboratory_entrance.get_height();
                if (_sh > 0 && _target_h > 0) {
                    _icon_scale = _target_h / _sh;
                }
            }

            var _mod_entrance = instance_create_depth(0, 0, -3, Button)
            _mod_entrance.set_sprite(_spr)
                          .set_scale(_icon_scale)
                          .set_on_click(method({map_id: _map_id}, function() {
                              instance_create_depth(0, 0, -100, obj_mod_island_menu).init(map_id);
                          }))
                          .set_should_correspond(method({gui_state: self.state}, function() {
                              var _has_float_layer = instance_exists(obj_config_menu) ||
                                                     instance_exists(obj_edit_menu) ||
                                                     instance_exists(obj_world_map_menu) ||
                                                     instance_exists(obj_package_bg) ||
                                                     instance_exists(obj_info_island_bg) ||
                                                     instance_exists(obj_shop_bg) ||
                                                     instance_exists(obj_task_bg) ||
                                                     instance_exists(obj_craft_bg) ||
                                                     instance_exists(obj_tower_cake_bg) ||
                                                     instance_exists(obj_quit_confirm) ||
                                                     instance_exists(obj_mod_island_menu)
                              return !_has_float_layer
                          }))

            array_push(_mod_items, _mod_entrance)
        }
    }

    /// @type {Asset.GMObject.GridList}
    self.state.grid_list = instance_create_layer(self.state.left, self.state.top, "Instances", GridList)
    self.state.grid_list.set_viewport(self.state.left, self.state.top, self.state.width, self.state.height)
                        .set_items(_items)
                        .set_grid_x(8)

    // mod 大地图入口单独一个网格，整体比实验室按钮高一点
    if (array_length(_mod_items) > 0) {
        self.state.mod_grid_list = instance_create_layer(self.state.left + 90, self.state.top - 10, "Instances", GridList)
        self.state.mod_grid_list.set_viewport(self.state.left + 90, self.state.top - 10, 900, 300)
                            .set_items(_mod_items)
                            .set_grid_x(8)
    }
}

/// @param {Real} _left
/// @param {Real} _top
/// @returns {Asset.GMObject.EventEntranceList}
function set_position(_left, _top) {
    self.state.left = _left
    self.state.top = _top
    self.state.grid_list.set_viewport(self.state.left, self.state.top, self.state.width, self.state.height)
    if (!is_undefined(self.state.mod_grid_list) && instance_exists(self.state.mod_grid_list)) {
        self.state.mod_grid_list.set_viewport(self.state.left + 90, self.state.top - 10, 900, 300)
    }
    return self
}

function set_size(_width, _height) {
    self.state.width = _width
    self.state.height = _height
    self.state.grid_list.set_viewport(self.state.left, self.state.top, self.state.width, self.state.height)
    if (!is_undefined(self.state.mod_grid_list) && instance_exists(self.state.mod_grid_list)) {
        self.state.mod_grid_list.set_viewport(self.state.left + 90, self.state.top - 10, 900, 300)
    }
    return self
}

/// @description Events
function on_create() {
    self.state.left = x
    self.state.top = y
    create_widgets()
}

function on_step() {

}

function on_draw() {

}

on_create()