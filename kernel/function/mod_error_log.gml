function mod_error_log(_msg) {
    var _m = string(_msg);

    // 连续重复的同一个错误不刷屏
    if (variable_global_exists("_mod_err_last") && _m == global._mod_err_last) {
        global._mod_err_dup += 1;
        if ((global._mod_err_dup mod 60) != 0) return;
    } else {
        global._mod_err_last = _m;
        global._mod_err_dup  = 0;
    }

    var _dir = working_directory + "mod/";
    if (!directory_exists(_dir)) return;
    var _path = _dir + "mod_errors.log";

    // 读旧内容（GM 没有追加模式，只能读出来再整体写回）
    var _lines = [];
    if (file_exists(_path)) {
        var _r = file_text_open_read(_path);
        if (_r != -1) {
            while (!file_text_eof(_r)) {
                array_push(_lines, file_text_read_string(_r));
                file_text_readln(_r);
            }
            file_text_close(_r);
        }
    }
    array_push(_lines, date_datetime_string(date_current_datetime()) + "  " + _m);
    if (array_length(_lines) > 2000) {
        var _keep = [];
        for (var _i = array_length(_lines) - 2000; _i < array_length(_lines); _i++) array_push(_keep, _lines[_i]);
        _lines = _keep;
    }

    var _w = file_text_open_write(_path);
    if (_w == -1) return;
    for (var _i = 0; _i < array_length(_lines); _i++) {
        file_text_write_string(_w, _lines[_i]);
        file_text_writeln(_w);
    }
    file_text_close(_w);
}
