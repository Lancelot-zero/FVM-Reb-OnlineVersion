function shell_log_write(_msg) {
    if (!variable_global_exists("sudo_authed") || !global.sudo_authed) return;

    var _dir = working_directory + "mod/";
    if (!directory_exists(_dir)) return;
    var _path = _dir + "shell.log";

    var _w = file_text_open_append(_path);
    if (_w == -1) return;
    file_text_write_string(_w, date_datetime_string(date_current_datetime()) + "  " + string(_msg));
    file_text_writeln(_w);
    file_text_close(_w);

    if (!variable_global_exists("_shell_log_writes")) global._shell_log_writes = 0;
    global._shell_log_writes += 1;
    if ((global._shell_log_writes mod 200) != 0) return;

    // 定期裁剪：读出来只留最后 2000 行再整体写回
    var _lines = [];
    var _r = file_text_open_read(_path);
    if (_r == -1) return;
    while (!file_text_eof(_r)) {
        array_push(_lines, file_text_read_string(_r));
        file_text_readln(_r);
    }
    file_text_close(_r);
    var _n = array_length(_lines);
    if (_n <= 2000) return;
    var _wt = file_text_open_write(_path);
    if (_wt == -1) return;
    for (var _i = _n - 2000; _i < _n; _i++) {
        file_text_write_string(_wt, _lines[_i]);
        file_text_writeln(_wt);
    }
    file_text_close(_wt);
}
