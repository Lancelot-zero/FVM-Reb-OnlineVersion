// ============================================================
// host_draw.cpp — 绘制控制（外层的 draw_* / surface_* / sprite_* 尺寸 / string_width 落到这儿）
//
//   默认那套 = "没接宿主"：什么都不画、量宽返回 0。不带引擎也能跑（只是看不见东西）。
//   宿主启动时把 g_host_draw 整张换掉。
//
//   ⚠ 画布尺寸（host_draw_width/height）单独给两个函数：它不属于"画"，
//     但 VM_DrawSpriteExt / 屏幕↔GUI 坐标换算要用；默认 0（= 没接）。
// ============================================================
#include "host.h"

static void   d_sprite(int, double, double) {}
static void   d_sprite_ext(int, double, double, double, double, double, double, int, int) {}
static void   d_text(double, double, const std::string&) {}
static void   d_text_ext(double, double, const std::string&, double) {}
static void   d_rectangle(double, double, double, double, bool) {}
static void   d_rectangle_colour(double, double, double, double, int, int, int, int, bool) {}
static void   d_roundrect(double, double, double, double, double, double, bool) {}
static void   d_line(double, double, double, double) {}
static void   d_clear_alpha(int, double) {}
static void   d_set_colour(int) {}
static void   d_set_alpha(double) {}
static void   d_set_font(int) {}
static void   d_set_halign(int) {}
static void   d_set_valign(int) {}
static int    d_surface_create(int, int) { return -1; }
static void   d_surface_free(int) {}
static bool   d_surface_exists(int) { return false; }
static void   d_surface_set_target(int) {}
static void   d_surface_reset_target() {}
static void   d_draw_surface(int, double, double) {}
static void   d_gpu_set_tex_filter(bool) {}
static void   d_texture_prefetch(int) {}
static void   d_enable_drawevent(bool) {}
static int    d_sprite_width(int) { return 0; }
static int    d_sprite_height(int) { return 0; }
static std::string d_sprite_name(int) { return std::string(); }
static int    d_string_width(const std::string&) { return 0; }
static int    d_string_height(const std::string&) { return 0; }
static int    d_string_width_ext(const std::string&, double, double) { return 0; }
static int    d_string_height_ext(const std::string&, double, double) { return 0; }

HostDrawOps g_host_draw = {
    d_sprite, d_sprite_ext, d_text, d_text_ext,
    d_rectangle, d_rectangle_colour, d_roundrect, d_line, d_clear_alpha,
    d_set_colour, d_set_alpha, d_set_font, d_set_halign, d_set_valign,
    d_surface_create, d_surface_free, d_surface_exists, d_surface_set_target,
    d_surface_reset_target, d_draw_surface, d_gpu_set_tex_filter, d_texture_prefetch,
    d_enable_drawevent, d_sprite_width, d_sprite_height, d_sprite_name,
    d_string_width, d_string_height, d_string_width_ext, d_string_height_ext
};

// 画布尺寸：宿主没接 → 0（调用方自己判 0 = 不知道）
static int g_draw_w = 0;
static int g_draw_h = 0;

int host_draw_width()  { return g_draw_w; }
int host_draw_height() { return g_draw_h; }

// 宿主填尺寸（不填就是 0）
void host_draw_set_size(int w, int h) { g_draw_w = w; g_draw_h = h; }
