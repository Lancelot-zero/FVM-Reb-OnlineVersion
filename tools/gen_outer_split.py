# -*- coding: utf-8 -*-
# 一次性：把 native 表（NATIVE_STUBS + NATIVE_IMPL）按"写到哪个文件 + 要不要宿主"分好类，
#   产出 DLL_VM/outer/outer_split.h（每个文件一份名单宏）
#
#   分类原则：
#     转发     没有 C++ 实现 → VM_CallFunc(名字池下标, 实参…)   （= NATIVE_STUBS 那 79 条）
#     容器     数组 / ds_list / ds_map / ds_grid / struct        → 自举
#     实例     instance_* / variable_instance_* / 属性           → 自举
#     派发     run_block / run_event_user / VM_Execute           → 自举
#     内置     数学 / 类型 / 字符串 / 全局变量 / 坐标 / 随机      → 自举
#     宿主     文件 / 音频 / 相机 / 绘制 / 网络 / buffer / 游戏侧 → **不能自举**，留给宿主定
#     game     VM_*（150 条游戏 API，走 OP_CALL）                → 基本都要宿主
import io, re, sys

py = io.open('transformGML2txt/gm2bin.py', encoding='utf-8').read()
def py_list(name):
    m = re.search(name + r'\s*=\s*\[(.*?)\n\]', py, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))

stubs, impl, game = py_list('NATIVE_STUBS'), py_list('NATIVE_IMPL'), py_list('GAME_NAMES')

RE_CONTAINER = re.compile(r'^(array_|ds_list_|ds_map_|ds_grid_|ds_exists$|struct_|variable_struct_remove$)')
RE_INST      = re.compile(r'^(instance_|variable_instance_)')
RE_DISPATCH  = re.compile(r'^(run_block$|run_event_user$|VM_Execute$)')
RE_BUILTIN   = re.compile(
    r'^(floor|round|ceil|abs|max|min|clamp|lerp|sign|sqr|sqrt|power|exp|ln|log2|log10|frac|mean|median|'
    r'sin|cos|tan|arcsin|arccos|arctan|arctan2|degtorad|radtodeg|dot_product|triangle_area|'
    r'lengthdir_x|lengthdir_y|point_direction|point_distance|'
    r'random|random_range|irandom|irandom_range|choose|randomize|random_set_seed|random_get_seed|'
    r'is_real|is_string|is_array|is_struct|is_undefined|is_bool|is_numeric|is_int32|is_int64|is_method|is_callable|'
    r'real$|int$|int64|bool|typeof|'
    r'string$|string_length|string_copy|string_delete|string_pos|string_replace|string_replace_all|string_count|'
    r'string_upper|string_lower|string_letters|string_digits|string_lettersdigits|string_starts_with|string_ends_with|'
    r'string_char_at|string_ord_at|string_byte_at|string_byte_length|string_format|string_repeat|string_join|string_split|'
    r'chr|ord|json_stringify|json_parse|'
    r'variable_global_exists|variable_global_get|variable_global_set|variable_global_names|'
    r'merge_colour|merge_color|make_colour_rgb|make_color_rgb|make_colour_hsv|make_color_hsv|'
    r'colour_get_red|color_get_red|colour_get_green|color_get_green|colour_get_blue|color_get_blue|'
    r'irandom$|random$|clamp$|lerp$|sign$|real$|frac$|floor$|round$|ceil$|abs$|max$|min$|sin$|cos$|degtorad$)')
# ── 分类规则（真引擎侧 / 自己能算 / 项目业务 / 归业务层）──
RE_ENGINE = [
    ('绘制/几何',      r'^(draw_|string_width|string_height)'),
    ('表面/GPU/纹理',  r'^(surface_|gpu_|texture_)'),
    ('窗口/显示',      r'^(window_|display_)'),
    ('输入',           r'^(keyboard_|mouse_|device_)'),
    ('音频',           r'^audio_'),
    ('相机/视图',      r'^(camera_|view_)'),
    ('房间',           r'^room_'),
    ('精灵(引擎资源)', r'^sprite_'),
    ('网络收发',       r'^(network_send_raw|network_destroy|http_request|url_open|network_broadcast_active_skill)'),
    ('系统/IME/对话框', r'^(native_|show_question|game_end|game_set_speed|object_is_ancestor)'),
]
RE_REAL = [
    ('文件/目录', r'^(file_|directory_)'),
    ('缓冲区',    r'^buffer_'),
    ('日期',      r'^date_'),
    ('ini',       r'^ini_'),
]
# 归业务层的那 4 条（网格数学 3 + 伤害 1）
BIZ_NATIVE = ['get_grid_position_from_world', 'get_world_position_from_grid',
              'calculate_plant_depth', 'damage_enemy']


# ── 读 B1 状态区（契约 host/host_layout.h）的那几条 ──
#   它们和 host_engine 那批的差别：**不需要引擎回调 / 函数指针**，只要读引擎每帧写好的
#   那块共享内存（host_state_base() 给的槽数组）。所以不跟 host_engine 混在一起生成为空壳，
#   单独进 outer/host_state_in.cpp 真实现（名单 = HOST_STATE_LIST）。
#   ⚠ keyboard_get_string **故意不在**这里：B1 契约里没有"本帧输入的文本"这个字段
#     （槽 40-63 是预留，JSON 注释提了"输入法文本"但没定义字段）—— 没有字段就没法读，
#     硬编一个槽号出来 = 自己造约定，那正是这套接口唯一真正危险的错。所以它留在空壳名单里。
HOST_STATE_NAMES = [
    'keyboard_check_pressed',
    'mouse_check_button', 'mouse_check_button_pressed', 'mouse_check_button_released',
    'device_mouse_x_to_gui', 'device_mouse_y_to_gui',
]
for _n in HOST_STATE_NAMES:                 # 名字拼错会"静默少一条"，在这儿先拦住
    assert _n in impl, ('HOST_STATE_NAMES 里这条不在 NATIVE_IMPL 表里：' + _n)

groups = {'forward': list(stubs), 'container': [], 'inst': [], 'dispatch': [],
          'builtin': [], 'biz_native': [], 'host_real': [], 'host_state': [],
          'host_engine': [], 'host_biz': []}
for n in impl:
    if   RE_DISPATCH.match(n):  groups['dispatch'].append(n)
    elif RE_CONTAINER.match(n): groups['container'].append(n)
    elif RE_INST.match(n):      groups['inst'].append(n)
    elif RE_BUILTIN.match(n):   groups['builtin'].append(n)
    elif n in BIZ_NATIVE:                            groups['biz_native'].append(n)
    elif n in HOST_STATE_NAMES:                      groups['host_state'].append(n)
    elif any(re.match(p, n) for _, p in RE_REAL):    groups['host_real'].append(n)
    elif any(re.match(p, n) for _, p in RE_ENGINE):  groups['host_engine'].append(n)
    else:                                            groups['host_biz'].append(n)

order = ('forward','container','inst','dispatch','builtin','biz_native','host_real',
         'host_state','host_engine','host_biz')
tot = sum(len(groups[k]) for k in order)
for k in order:
    print('%-12s %3d' % (k, len(groups[k])))
print('合计 %d（表 %d + %d = %d）' % (tot, len(stubs), len(impl), len(stubs)+len(impl)))
assert tot == len(stubs) + len(impl)

def fmt(names, per_line=4):
    out = []
    for i in range(0, len(names), per_line):
        out.append('    ' + ' '.join('X(%s)' % n for n in names[i:i+per_line]) + ' \\')
    return '\n'.join(out)

title = {'forward':'转发：没有 C++ 实现的 native → VM_CallFunc(名字池下标, 实参…)',
         'container':'容器：数组 / ds_list / ds_map / ds_grid / struct   [自举]',
         'inst':'实例：instance_* / variable_instance_*            [自举]',
         'dispatch':'派发：run_block / run_event_user / VM_Execute      [自举]',
         'builtin':'内置：数学 / 类型 / 字符串 / 全局 / 坐标 / 随机     [自举]',
         'biz_native':'归业务层：网格数学 3 + 伤害 1',
         'host_real':'自己能算：文件 / 目录 / 缓冲区 / 日期 / ini        [已实现，见 host_real.cpp]',
         'host_state':'读 B1 状态区：B1 共享内存（契约 host/host_layout.h）  [已实现，见 host_state_in.cpp]',
         'host_engine':'**真引擎侧**：绘制 / 输入 / 音频 / 窗口 / 相机 / 房间 / 精灵 / 网络收发 / 系统',
         'host_biz':'项目业务：平台已有同名 callbin（或照 GML 搬）→ 转发'}
def macro(key):
    return key.upper() + '_LIST'

body = ['// ============================================================',
        '// outer_split.h — 外层函数按"写到哪个文件 / 要不要宿主"分组',
        '//   ⚠ 这些分组只决定**写在哪个 .cpp**，不决定表里的顺序：',
        '//     表的顺序永远由 core/const_str.h 的 NATIVE_FN_LIST / NATIVE_IMPL_LIST 说了算',
        '//     每个名字必须**恰好**被定义一次（少一个 = 链接报错，多一个 = 重复符号）',
        '// ============================================================',
        '#pragma once', '']
for k in order:
    body.append('// %s（%d 条）' % (title[k], len(groups[k])))
    body.append('#define %s(X) \\' % macro(k))
    body.append(fmt(groups[k]))
    body.append('')
# host_fn.cpp 那份空壳名单 = 真引擎侧 + 项目业务
#   ⚠ 读 B1 的那批（HOST_STATE_LIST）**不在**里头：它们有自己的实现文件 host_state_in.cpp，
#     加进来就会和那边的定义**重复符号**（两边各定义一次 = 链接报错）。
body.append('// host_fn.cpp 里生成空壳的那批 = 真引擎侧 + 项目业务（%d 条）；'
            '读 B1 的那 %d 条有自己的实现文件，**不在**这里'
            % (len(groups['host_engine']) + len(groups['host_biz']), len(groups['host_state'])))
body.append('#define HOST_LIST(X) \\')
body.append('    HOST_ENGINE_LIST(X) \\')
body.append('    HOST_BIZ_LIST(X)')
body.append('')
io.open('DLL_VM/outer/outer_split.h', 'w', encoding='utf-8', newline='\n').write('\n'.join(body))
print('\n写出 DLL_VM/outer/outer_split.h')
print('\n真引擎侧（%d 条）：' % len(groups['host_engine']))
print('  ' + ' '.join(groups['host_engine']))
print('\n自己能算（%d 条，已实现）：' % len(groups['host_real']))
print('  ' + ' '.join(groups['host_real']))
print('\n读 B1 状态区（%d 条，已实现）：' % len(groups['host_state']))
print('  ' + ' '.join(groups['host_state']))
print('\n归业务层（%d 条）：%s' % (len(groups['biz_native']), ' '.join(groups['biz_native'])))
