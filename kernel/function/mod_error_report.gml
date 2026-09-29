function mod_error_report(vm, name, ip, _err, _pos_is_byte = false) {
    var _dir = (is_struct(vm) && variable_struct_exists(vm, "mod_dir")) ? string(vm[$ "mod_dir"]) : "";
    // 出错的到底是哪个 bin（src_lines 是 dir+id+".lines"，换成 .bin 就是它）
    //   ⚠ 原来是 string_replace(...)：那个内建函数我们表里没有（见 gm2bin 的 CALL_BY_NAME 说明），
    //     这里用 string_pos / string_copy / string_delete 拼出"替换第一处"，语义等价
    var _binp = "";
    if (is_struct(vm) && variable_struct_exists(vm, "src_lines")) {
        var _sl = string(vm[$ "src_lines"]);
        var _pi = string_pos(".lines", _sl);
        if (_pi > 0) _binp = string_copy(_sl, 1, _pi - 1) + ".bin" + string_delete(_sl, _pi, 6);
        else         _binp = _sl;
    }
    var _nm  = "";
    if (is_struct(vm) && variable_struct_exists(vm, "card_data") && is_struct(vm.card_data)
     && variable_struct_exists(vm.card_data, "name")) _nm = string(vm.card_data.name);

    // 源码行号（VM_LineOf 返回 "err_test.txt:15"，拿不到就是 ""）
    var _loc = VM_LineOf(vm, name, ip, _pos_is_byte);
    var _ln  = -1;
    if (_loc != "") _ln = real(string_delete(_loc, 1, string_pos(":", _loc)));

    // 源码那一行的原文（.txt 就在 bin 旁边：src_lines 是 .lines，换成 .txt）
    var _srctext = "";
    if (_ln > 0 && is_struct(vm) && variable_struct_exists(vm, "src_lines")) {
        var _sl2 = string(vm[$ "src_lines"]);
        var _pi2 = string_pos(".lines", _sl2);
        var _tp = (_pi2 > 0) ? (string_copy(_sl2, 1, _pi2 - 1) + ".txt" + string_delete(_sl2, _pi2, 6)) : _sl2;
        if (file_exists(_tp)) {
            var _fr = file_text_open_read(_tp);
            if (_fr != -1) {
                var _k = 0;
                while (!file_text_eof(_fr)) {
                    var _s = file_text_read_string(_fr);
                    file_text_readln(_fr);
                    _k++;
                    if (_k == _ln) { _srctext = _s; break; }
                }
                file_text_close(_fr);
            }
        }
    }

    // GML 的原始错误信息
    var _escript = "", _emsg = "", _elong = "", _estack = "";
    if (is_struct(_err)) {
        if (variable_struct_exists(_err, "script"))      _escript = string(_err[$ "script"]);
        if (variable_struct_exists(_err, "message"))     _emsg    = string(_err[$ "message"]);
        if (variable_struct_exists(_err, "longMessage")) _elong   = string(_err[$ "longMessage"]);
        if (variable_struct_exists(_err, "stacktrace") && is_array(_err[$ "stacktrace"])) {
            var _st = _err[$ "stacktrace"];
            for (var _i = 0; _i < array_length(_st); _i++) _estack += "      " + string(_st[_i]) + "\n";
        }
    } else {
        _emsg = string(_err);
    }

    var _o = "\n========== mod 运行出错 ==========\n";
    _o += "  文件   : " + _binp + "\n";
    _o += "  名称   : " + _nm + "\n";
    _o += "  VM块   : " + string(name) + "   指令位置 " + string(ip) + "\n";
    if (_ln > 0) _o += "  源码行 : 第 " + string(_ln) + " 行   （" + _loc + "）\n";
    else         _o += "  源码行 : 拿不到（那个 bin 里没有行号表？）\n";
    if (_srctext != "") _o += "  那一行 : " + _srctext + "\n";   // 原来是 string_trim(...)：我们表里没这个内建，且这里只是显示，不去空白
    _o += "  ---- GML 原始错误 ----\n";
    _o += "  script : " + _escript + "\n";
    _o += "  message: " + _emsg + "\n";
    if (_elong != "") _o += "  long   : " + _elong + "\n";
    if (_estack != "") _o += "  调用栈 :\n" + _estack;
    _o += "==================================\n";
    mod_error_log(_o);

    // shell 只给一行摘要：哪个文件、哪张卡、哪个块、第几行；详细内容看日志
    shell_print("[mod 出错] " + _binp + "   块 " + string(name) + "   名称 " + _nm
              + ((_ln > 0) ? ("   第 " + string(_ln) + " 行") : "   行号未知"));
    shell_print("详情见 错误日志：mod/mod_errors.log");
    return _loc;
}
