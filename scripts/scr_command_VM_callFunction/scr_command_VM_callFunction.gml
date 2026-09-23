// ============================================================
// VM_CallFunc 的「按名字调用」清单 —— 常用数学函数
//
//   VM 脚本里这样调：VM_CallFunc("clamp", v, 0, 100)
//   加函数不用改编译器、不用占 VM 函数号、不用跑 sync_vmfuncs.py
//   raw: true → 普通 GML 函数，收真值（VM_CallFunc 内部已用 vm_arg 把地址转成值）
//
//   生效：全局初始化之后调一次 scr_command_VM_callFunction()（重复调安全，会覆盖）
//
//   ⚠️ VM_CallFunc 走字典的动态调用，比直接调 VM 函数慢不少（查字典 + 参数地址转值 + 动态调用）。
//      这批函数适合低频场景（开局摆地图、一次性配置、偶发事件），
//      别放进 _VM_FRAME / _OBJECT_STEP 这种每帧每实例都跑的地方；高频逻辑请直接写成 VM 函数。
// ============================================================

/// @function vm_math_abs(x)
/// @desc 绝对值
function vm_math_abs(x) {
    if (!is_real(x)) return undefined;
    return abs(x);
}

/// @function vm_math_min(a, b)
/// @desc 两个数里小的那个
function vm_math_min(a, b) {
    if (!is_real(a) || !is_real(b)) return undefined;
    return min(a, b);
}

/// @function vm_math_max(a, b)
/// @desc 两个数里大的那个
function vm_math_max(a, b) {
    if (!is_real(a) || !is_real(b)) return undefined;
    return max(a, b);
}

/// @function vm_math_clamp(v, lo, hi)
/// @desc 把 v 夹在 [lo, hi] 之间
function vm_math_clamp(v, lo, hi) {
    if (!is_real(v) || !is_real(lo) || !is_real(hi)) return undefined;
    return clamp(v, lo, hi);
}

/// @function vm_math_sign(x)
/// @desc 符号：x<0 → -1，x==0 → 0，x>0 → 1
function vm_math_sign(x) {
    if (!is_real(x)) return undefined;
    return sign(x);
}

/// @function vm_math_round(x)
/// @desc 四舍五入到整数
function vm_math_round(x) {
    if (!is_real(x)) return undefined;
    return round(x);
}

/// @function vm_math_sqrt(x)
/// @desc 平方根（x<0 返回 0）
function vm_math_sqrt(x) {
    if (!is_real(x)) return undefined;
    if (x < 0) return 0;
    return sqrt(x);
}

/// @function vm_math_pow(a, b)
/// @desc a 的 b 次方
function vm_math_pow(a, b) {
    if (!is_real(a) || !is_real(b)) return undefined;
    return power(a, b);
}

/// @function vm_math_lerp(a, b, t)
/// @desc 线性插值：t=0 → a，t=1 → b（不夹取，t 可以超范围）
function vm_math_lerp(a, b, t) {
    if (!is_real(a) || !is_real(b) || !is_real(t)) return undefined;
    return lerp(a, b, t);
}

/// @function vm_math_approach(cur, target, step)
/// @desc 每步朝目标逼近 step（不超过目标）：平滑移动 / 冷却条常用
function vm_math_approach(cur, target, step) {
    if (!is_real(cur) || !is_real(target) || !is_real(step)) return undefined;
    if (step <= 0) return cur;
    if (cur < target) return min(cur + step, target);
    if (cur > target) return max(cur - step, target);
    return cur;
}

/// @function vm_math_dist(x1, y1, x2, y2)
/// @desc 两点距离（像素）
function vm_math_dist(x1, y1, x2, y2) {
    if (!is_real(x1) || !is_real(y1) || !is_real(x2) || !is_real(y2)) return undefined;
    return point_distance(x1, y1, x2, y2);
}

/// @function vm_math_angle(x1, y1, x2, y2)
/// @desc 从 (x1,y1) 指向 (x2,y2) 的角度（度，同 point_direction）
function vm_math_angle(x1, y1, x2, y2) {
    if (!is_real(x1) || !is_real(y1) || !is_real(x2) || !is_real(y2)) return undefined;
    return point_direction(x1, y1, x2, y2);
}

/// @function vm_math_deg(rad)
/// @desc 弧度 → 度
function vm_math_deg(rad) {
    if (!is_real(rad)) return undefined;
    return radtodeg(rad);
}

/// @function vm_math_rad(deg)
/// @desc 度 → 弧度
function vm_math_rad(deg) {
    if (!is_real(deg)) return undefined;
    return degtorad(deg);
}

// ============================================================
// 通用工具（和游戏内部无关，纯 GML 逻辑；入参不是对应类型就返回 undefined）
// ============================================================

// ---- 位运算（VM 语言没有位运算符，mask / flag 只能靠这几个）----
function vm_bit_and(a, b)    { if (!is_real(a) || !is_real(b)) return undefined; return a & b; }
function vm_bit_or(a, b)     { if (!is_real(a) || !is_real(b)) return undefined; return a | b; }
function vm_bit_xor(a, b)    { if (!is_real(a) || !is_real(b)) return undefined; return a ^ b; }
function vm_bit_not(a)       { if (!is_real(a)) return undefined; return ~a; }
function vm_shl(a, n)        { if (!is_real(a) || !is_real(n)) return undefined; return a << n; }
function vm_shr(a, n)        { if (!is_real(a) || !is_real(n)) return undefined; return a >> n; }
function vm_bit_test(a, n)   { if (!is_real(a) || !is_real(n)) return undefined; return (a >> n) & 1; }
function vm_bit_set(a, n)    { if (!is_real(a) || !is_real(n)) return undefined; return a | (1 << n); }
function vm_bit_clear(a, n)  { if (!is_real(a) || !is_real(n)) return undefined; return a & ~(1 << n); }
function vm_bit_toggle(a, n) { if (!is_real(a) || !is_real(n)) return undefined; return a ^ (1 << n); }

// ---- 数值杂项 ----
function vm_mod_pos(a, b) {
    if (!is_real(a) || !is_real(b) || b == 0) return undefined;
    return ((a % b) + b) % b;
}
function vm_wrap(v, lo, hi) {
    if (!is_real(v) || !is_real(lo) || !is_real(hi) || hi <= lo) return undefined;
    return lo + vm_mod_pos(v - lo, hi - lo);
}
function vm_pingpong(v, len) {
    if (!is_real(v) || !is_real(len) || len <= 0) return undefined;
    var _m = vm_mod_pos(v, len * 2);
    return (_m <= len) ? _m : len * 2 - _m;
}
function vm_smooth(cur, target, f) {
    if (!is_real(cur) || !is_real(target) || !is_real(f)) return undefined;
    return cur + (target - cur) * f;
}
function vm_clamp01(x)  { if (!is_real(x)) return undefined; return clamp(x, 0, 1); }
function vm_is_even(x)  { if (!is_real(x)) return undefined; return (x mod 2 == 0) ? 1 : 0; }
function vm_is_odd(x)   { if (!is_real(x)) return undefined; return (x mod 2 != 0) ? 1 : 0; }
function vm_fmt(v, digits) {
    if (!is_real(v) || !is_real(digits)) return undefined;
    return string_format(v, 0, digits);
}

// ---- 字符串（下标一律 0 起，找不到返回 -1，和 VM 的数组/查询函数一个口径）----
function vm_str_len(s) { if (!is_string(s)) return undefined; return string_length(s); }
function vm_str_sub(s, start, count) {
    if (!is_string(s) || !is_real(start) || !is_real(count)) return undefined;
    if (start < 0 || count <= 0) return "";
    return string_copy(s, start + 1, count);
}
function vm_str_find(s, sub) {
    if (!is_string(s) || !is_string(sub)) return undefined;
    var _p = string_pos(sub, s);
    return (_p <= 0) ? -1 : _p - 1;
}
function vm_str_upper(s) { if (!is_string(s)) return undefined; return string_upper(s); }
function vm_str_lower(s) { if (!is_string(s)) return undefined; return string_lower(s); }
function vm_str_replace(s, find, rep) {
    if (!is_string(s) || !is_string(find) || !is_string(rep)) return undefined;
    return string_replace(s, find, rep);
}
function vm_str_replace_all(s, find, rep) {
    if (!is_string(s) || !is_string(find) || !is_string(rep)) return undefined;
    return string_replace_all(s, find, rep);
}
function vm_str_repeat(s, n) {
    if (!is_string(s) || !is_real(n)) return undefined;
    if (n <= 0) return "";
    var _out = "";
    for (var _i = 0; _i < n; _i++) _out += s;
    return _out;
}
function vm_str_trim(s) {
    if (!is_string(s)) return undefined;
    var _a = 0;
    var _b = string_length(s) - 1;
    while (_a <= _b) {
        var _c1 = string_char_at(s, _a + 1);
        if (_c1 != " " && _c1 != "\t" && _c1 != "\n" && _c1 != "\r") break;
        _a++;
    }
    while (_b >= _a) {
        var _c2 = string_char_at(s, _b + 1);
        if (_c2 != " " && _c2 != "\t" && _c2 != "\n" && _c2 != "\r") break;
        _b--;
    }
    if (_b < _a) return "";
    return string_copy(s, _a + 1, _b - _a + 1);
}

// ---- 随机 ----
function vm_rand_i(minv, maxv) { if (!is_real(minv) || !is_real(maxv)) return undefined; return irandom_range(minv, maxv); }
function vm_rand_f(minv, maxv) { if (!is_real(minv) || !is_real(maxv)) return undefined; return random_range(minv, maxv); }
function vm_chance(p)          { if (!is_real(p)) return undefined; return (random(1) < p) ? 1 : 0; }

/// @function scr_command_VM_callFunction()
/// @desc 把上面这些数学函数登记进 global._VM_call_dict（VM_CallFunc 按名字调用）。
///       在全局初始化之后调一次即可；重复调用安全（ds_map_replace 覆盖）。
/// @return 登记了几个
function scr_command_VM_callFunction() {
    if (!variable_global_exists("_VM_call_dict")) {
        show_debug_message("[callFunction] global._VM_call_dict 还不存在，登记跳过");
        return 0;
    }
    var _d = global._VM_call_dict;
    var _list = [
        ["abs",      vm_math_abs,      "abs(x) → 绝对值"],
        ["min",      vm_math_min,      "min(a,b) → 小的那个"],
        ["max",      vm_math_max,      "max(a,b) → 大的那个"],
        ["clamp",    vm_math_clamp,    "clamp(v,lo,hi) → 夹在区间内"],
        ["sign",     vm_math_sign,     "sign(x) → -1 / 0 / 1"],
        ["round",    vm_math_round,    "round(x) → 四舍五入"],
        ["sqrt",     vm_math_sqrt,     "sqrt(x) → 平方根"],
        ["pow",      vm_math_pow,      "pow(a,b) → a 的 b 次方"],
        ["lerp",     vm_math_lerp,     "lerp(a,b,t) → 线性插值"],
        ["approach", vm_math_approach, "approach(cur,target,step) → 每步逼近"],
        ["dist",     vm_math_dist,     "dist(x1,y1,x2,y2) → 两点距离"],
        ["angle",    vm_math_angle,    "angle(x1,y1,x2,y2) → 两点角度(度)"],
        ["deg",      vm_math_deg,      "deg(rad) → 弧度转度"],
        ["rad",      vm_math_rad,      "rad(deg) → 度转弧度"],
        // ---- 位运算（VM 语言没有位运算符）----
        ["bit_and",     vm_bit_and,     "bit_and(a,b) → 按位与"],
        ["bit_or",      vm_bit_or,      "bit_or(a,b) → 按位或"],
        ["bit_xor",     vm_bit_xor,     "bit_xor(a,b) → 按位异或"],
        ["bit_not",     vm_bit_not,     "bit_not(a) → 按位取反"],
        ["shl",         vm_shl,         "shl(a,n) → 左移 n 位"],
        ["shr",         vm_shr,         "shr(a,n) → 右移 n 位"],
        ["bit_test",    vm_bit_test,    "bit_test(a,n) → 第 n 位是 1 就返回 1，否则 0"],
        ["bit_set",     vm_bit_set,     "bit_set(a,n) → 置第 n 位"],
        ["bit_clear",   vm_bit_clear,   "bit_clear(a,n) → 清第 n 位"],
        ["bit_toggle",  vm_bit_toggle,  "bit_toggle(a,n) → 翻转第 n 位"],
        // ---- 数值杂项 ----
        ["mod_pos",  vm_mod_pos,  "mod_pos(a,b) → 结果恒为正的取模"],
        ["wrap",     vm_wrap,     "wrap(v,lo,hi) → 超界绕回另一端"],
        ["pingpong", vm_pingpong, "pingpong(v,len) → 0..len 来回"],
        ["smooth",   vm_smooth,   "smooth(cur,target,f) → 指数平滑（f 0~1）"],
        ["clamp01",  vm_clamp01,  "clamp01(x) → 夹到 0~1"],
        ["is_even",  vm_is_even,  "is_even(x) → 偶数返回 1"],
        ["is_odd",   vm_is_odd,   "is_odd(x) → 奇数返回 1"],
        ["fmt",      vm_fmt,      "fmt(v,digits) → 保留几位小数的字符串"],
        // ---- 字符串（下标 0 起，找不到返回 -1）----
        ["str_len",         vm_str_len,         "str_len(s) → 长度"],
        ["str_sub",         vm_str_sub,         "str_sub(s,start,count) → 截取（start 从 0 起）"],
        ["str_find",        vm_str_find,        "str_find(s,sub) → 下标，没有返回 -1"],
        ["str_upper",       vm_str_upper,       "str_upper(s) → 转大写"],
        ["str_lower",       vm_str_lower,       "str_lower(s) → 转小写"],
        ["str_replace",     vm_str_replace,     "str_replace(s,find,rep) → 只换第一处"],
        ["str_replace_all", vm_str_replace_all, "str_replace_all(s,find,rep) → 全换"],
        ["str_repeat",      vm_str_repeat,      "str_repeat(s,n) → 重复 n 次"],
        ["str_trim",        vm_str_trim,        "str_trim(s) → 去掉首尾空白"],
        // ---- 随机 ----
        ["rand_i", vm_rand_i, "rand_i(min,max) → 整数，含两端"],
        ["rand_f", vm_rand_f, "rand_f(min,max) → 浮点"],
        ["chance", vm_chance, "chance(p) → 以概率 p（0~1）返回 1"],
    ];
    var _n = 0;
    for (var _i = 0; _i < array_length(_list); _i++) {
        var _e = _list[_i];
        ds_map_replace(_d, _e[0], { fn: _e[1], raw: true, desc: _e[2] });
        _n++;
    }
    return _n;
}
