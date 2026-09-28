/// 



function GuiStack() constructor {

    /// @type {Array<Asset.GMRoom>} 栈内存 room 索引（与 room_goto 一致）
    self._rooms = []
    static _push_room = function(_room) {
        array_push(self._rooms, _room)
    }

    static _pop_room = function() {
        array_pop(self._rooms)
    }

    /// @param {Asset.GMRoom} _room 
    /// @returns {Bool} 
    static _stack_contains = function(_room) {
        for (var i = 0; i < array_length(self._rooms); i++) {
            if (self._rooms[i] == _room) {
                return true
            }
        }
        return false
    }

    /// @returns {Struct.Result} 
    static _sync_room_after_mutation = function() {
        var _n = array_length(self._rooms)
        if (_n == 0) {
            return new Result().fail(ErrorCode.GUI_INVALID_ROOM, "invalid room index: -1")
        }
        var _room = self._rooms[_n - 1]
        if (_room == room) {
            return new Result().success()
        }
        if (!room_exists(_room)) {
            return new Result().fail(ErrorCode.GUI_INVALID_ROOM, "invalid room index: " + string(_room))
        }
        room_goto(_room)
        return new Result().success()
    }

    /// @description 若栈内已有该 room 则 pop 到该层，否则压入并 room_goto。
    /// @param {Asset.GMRoom} _room 
    /// @returns {Struct.Result} 
    static to = function(_room) {
        if (!room_exists(_room)) {
            return new Result().fail(ErrorCode.GUI_INVALID_ROOM, "invalid room: " + string(_room))
        }
        if (self._stack_contains(_room)) {
            return self.pop_until(_room)
        }
        self._push_room(_room)
        return self._sync_room_after_mutation()
    }

    /// @param {Asset.GMRoom} _fallback_room 
    /// @returns {Struct.Result} 
    static pop = function(_fallback_room = room_menu) {
        if (array_length(self._rooms) == 0) {
            return new Result().fail(ErrorCode.GUI_STACK_EMPTY, "gui stack is empty")
        }
        self._pop_room()
        if (array_length(self._rooms) == 0) {
            self._push_room(_fallback_room)
        }
        return self._sync_room_after_mutation()
    }



    /// @param {Asset.GMRoom} _room 
    /// @returns {Struct.Result} 
    static pop_until = function(_room) {
        while (array_length(self._rooms) > 0 && self._rooms[array_length(self._rooms) - 1] != _room) {
            self._pop_room()
        }
        if (array_length(self._rooms) == 0) {
            if (!room_exists(_room)) {
                return new Result().fail(ErrorCode.GUI_INVALID_ROOM, "invalid room: " + string(_room))
            }
            self._push_room(_room)
        }
        return self._sync_room_after_mutation()
    }

    /// @returns {Asset.GMRoom|Undefined} 当前栈顶 room，空栈为 undefined
    static get_top = function() {
        var _n = array_length(self._rooms)
        if (_n == 0) {
            return undefined
        }
        return self._rooms[_n - 1]
    }
}


// ============================================================
// 普通函数版（**给 VM 用**：翻译器不支持方法调用 x.y(...)，所以战斗侧改成调这两个）
//   语义和上面的 to / pop 完全一样，只是把栈当参数传进来。
//   改这两处时记得同步改它们——两边行为必须一致。
// ============================================================

/// @description 同 GuiStack.to（**普通函数版**，不返回 Result、内部不用方法调用）
/// @param {Struct.GuiStack} _stack
/// @param {Asset.GMRoom} _room
function gui_stack_to(_stack, _room) {
    if (!room_exists(_room)) return;
    for (var i = 0; i < array_length(_stack._rooms); i++) {
        if (_stack._rooms[i] == _room) {
            gui_stack_pop_until(_stack, _room);
            return;
        }
    }
    array_push(_stack._rooms, _room);
    gui_stack_sync(_stack);
}

/// @description 同 GuiStack.pop（普通函数版；fallback 固定 room_menu，和原默认值一致）
function gui_stack_pop(_stack) {
    if (array_length(_stack._rooms) == 0) return;
    array_pop(_stack._rooms);
    if (array_length(_stack._rooms) == 0) {
        array_push(_stack._rooms, room_menu);
    }
    gui_stack_sync(_stack);
}

/// @description 同 GuiStack.pop_until（普通函数版）
function gui_stack_pop_until(_stack, _room) {
    while (array_length(_stack._rooms) > 0 && _stack._rooms[array_length(_stack._rooms) - 1] != _room) {
        array_pop(_stack._rooms);
    }
    if (array_length(_stack._rooms) == 0) {
        if (!room_exists(_room)) return;
        array_push(_stack._rooms, _room);
    }
    gui_stack_sync(_stack);
}

/// @description 同 GuiStack._sync_room_after_mutation（普通函数版，不返回 Result）
function gui_stack_sync(_stack) {
    var _n = array_length(_stack._rooms);
    if (_n == 0) return;
    var _top = _stack._rooms[_n - 1];
    if (_top == room) return;
    if (!room_exists(_top)) return;
    room_goto(_top);
}

/// @description 同 GuiStack.get_top（普通函数版）；空栈 → undefined
function gui_stack_get_top(_stack) {
    var _n = array_length(_stack._rooms);
    if (_n == 0) return undefined;
    return _stack._rooms[_n - 1];
}
