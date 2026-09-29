// ============================================================
// host_input.cpp — 输入控制（外层的 keyboard_* / mouse_* / device_mouse_* 落到这儿）
//
//   默认那套 = "没接宿主"：键全没按、鼠标在 (0,0)。这样不带引擎也能跑起来
//   （只是没输入），不会崩也不会乱报错。
//   宿主启动时把自己的实现填进 g_host_in 即可（一整张表替换，不用改这里）。
// ============================================================
#include "host.h"

static bool        d_key_down   (int) { return false; }
static bool        d_key_pressed(int) { return false; }
static std::string d_key_string ()    { return std::string(); }
static bool        d_mouse_down   (int) { return false; }
static bool        d_mouse_pressed(int) { return false; }
static bool        d_mouse_released(int) { return false; }
static double      d_mouse_x   () { return 0.0; }
static double      d_mouse_y   () { return 0.0; }
static double      d_mouse_x_gui() { return 0.0; }
static double      d_mouse_y_gui() { return 0.0; }

HostInputOps g_host_in = {
    d_key_down, d_key_pressed, d_key_string,
    d_mouse_down, d_mouse_pressed, d_mouse_released,
    d_mouse_x, d_mouse_y, d_mouse_x_gui, d_mouse_y_gui
};
