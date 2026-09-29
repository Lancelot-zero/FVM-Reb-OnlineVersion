# -*- coding: utf-8 -*-
# 生成 transformGML2txt/gm_builtin_consts.json —— GM 内建常量 → 数值
#   用途：gm2bin.py 翻译时查这张表，把 buffer_u8 这类**直接内联成数字**（和 GM 编译器一样）
#   只收"项目真的用到"的那些（tools/scan_gm_consts.py 扫出来的）
#   值来源：GM 规范/Win32 键码；拿不准的都列进了文件里的 "_uncertain"，要跟平台核一遍
#   重新生成： python tools/gen_gm_builtin_consts.py
import io, json

C = {}

# ── buffer 类型（和 outer/host_real.cpp 顶部的枚举一致）──
C.update(buffer_u8=1, buffer_s8=2, buffer_u16=3, buffer_s16=4, buffer_u32=5, buffer_s32=6,
         buffer_f16=7, buffer_f32=8, buffer_f64=9, buffer_bool=10, buffer_string=11,
         buffer_text=12, buffer_u64=13)
# ── buffer 用法 ──
C.update(buffer_fixed=0, buffer_grow=1, buffer_wrap=2, buffer_fast=3)
# ── buffer seek 基准 ──
C.update(buffer_seek_start=0, buffer_seek_relative=1, buffer_seek_end=2)

# ── 鼠标键（GM：mb_any=0, mb_left=1, mb_middle=2, mb_right=3）──
C.update(mb_any=0, mb_none=-1, mb_left=1, mb_middle=2, mb_right=3)

# ── 容器类型（照老 C++ vm_container.cpp 的 ds_exists switch：0=list / 1=map / 3=grid）──
C.update(ds_type_list=0, ds_type_map=1, ds_type_grid=3)
# ── 颜色（GM 的 BGR 打包：0xBBGGRR）──
C.update(c_aqua=16776960, c_black=0, c_blue=16711680, c_dkgray=4210752, c_fuchsia=16711935,
         c_gray=8421504, c_green=32768, c_lime=65280, c_ltgray=12632256, c_maroon=128,
         c_navy=8388608, c_olive=32896, c_orange=4235519, c_purple=8388736, c_red=255,
         c_silver=12632256, c_teal=8421376, c_white=16777215, c_yellow=65535)

# ── 文字对齐（GM：halign fa_left=0/fa_center=1/fa_right=2；valign fa_top=0/fa_middle=1/fa_bottom=2）──
C.update(fa_left=0, fa_center=1, fa_right=2, fa_top=0, fa_middle=1, fa_bottom=2,
         fa_justify=3, fa_none=4, fa_centre=1)
# ── 文件属性（GM 位掩码）──
C.update(fa_readonly=1, fa_hidden=2, fa_sysfile=4, fa_volumeid=8, fa_directory=16, fa_archive=32)

# ── 图元类型 ──
C.update(pr_pointlist=1, pr_linelist=2, pr_linestrip=3, pr_trianglelist=4,
         pr_trianglestrip=5, pr_trianglefan=6)

# ── 事件类型 / step 子类型（老代码里块名 _OBJECT_STEP/_STEP_END 的对应关系）──
C.update(ev_create=0, ev_destroy=1, ev_alarm=2, ev_step=3, ev_collision=4, ev_keyboard=5,
         ev_mouse=6, ev_other=7, ev_draw=8, ev_keypress=9, ev_keyrelease=10, ev_trigger=11,
         ev_step_normal=0, ev_step_begin=1, ev_step_end=2)

# ── 帧率 ──
C.update(gamespeed_fps=0, gamespeed_microseconds=1)

# ── 平台 ──
C.update(os_windows=0, os_macosx=1, os_ios=2, os_android=3, os_linux=4, os_winphone=5,
         os_tizen=6, os_win8native=7, os_wiiu=8, os_3ds=9, os_psvita=10, os_bb10=11,
         os_ps4=12, os_xboxone=13, os_ps3=14, os_xbox360=15, os_uwp=16, os_tvos=17,
         os_switch=18, os_xboxseriesxs=19, os_ps5=20, os_operagx=21, os_browser=22)

# ── 语言级 ──
C.update(infinity=100000000, NaN=0)          # NaN 另见 _uncertain

# ── 键盘：Win32 虚拟键码（GM 直接用这套）──
vk_nokey=0
for k in range(1, 255):                       # 0x01..0xFE 的常见键单独列，其余按码给
    pass
C.update(
    vk_backspace=8, vk_tab=9, vk_enter=13, vk_return=13, vk_shift=16, vk_control=17,
    vk_alt=18, vk_pause=19, vk_escape=27, vk_space=32, vk_pageup=33, vk_pagedown=34,
    vk_end=35, vk_home=36, vk_left=37, vk_up=38, vk_right=39, vk_down=40,
    vk_printscreen=44, vk_insert=45, vk_delete=46,
)
for i in range(10):                           # 主键盘数字 0..9
    C['vk_%d' % i] = 48 + i
for i in range(26):                           # A..Z
    C['vk_%s' % chr(ord('a') + i)] = 65 + i
for i in range(12):                           # F1..F12
    C['vk_f%d' % (i + 1)] = 112 + i
for i in range(10):                           # 小键盘 0..9
    C['vk_numpad%d' % i] = 96 + i
C.update(vk_multiply=106, vk_add=107, vk_subtract=109, vk_decimal=110, vk_divide=111,
         vk_lshift=160, vk_rshift=161, vk_lcontrol=162, vk_rcontrol=163, vk_lalt=164, vk_ralt=165)

# 拿不准的：值可能和 GM 规范有出入，等和平台核一遍
UNCERTAIN = ['c_orange', 'fa_none', 'buffer_fast', 'NaN', 'vk_numpad0']

lines = json.dumps(C, indent=0, ensure_ascii=False, sort_keys=True).replace('\n', '')
out = '{\n  "_note": "GM 内建常量 → 值。gm2bin 翻译时查这张表做内联（GM 编译器也是这么干的）。",\n'
out += '  "_uncertain": %s,\n' % json.dumps(UNCERTAIN, ensure_ascii=False)
out += '  "consts": ' + json.dumps(C, ensure_ascii=False, sort_keys=True, indent=1) + '\n}\n'
io.open('transformGML2txt/gm_builtin_consts.json', 'w', encoding='utf-8', newline='\n').write(out)
print('写出 transformGML2txt/gm_builtin_consts.json：%d 个内建常量（拿不准 %d 个）' % (len(C), len(UNCERTAIN)))
