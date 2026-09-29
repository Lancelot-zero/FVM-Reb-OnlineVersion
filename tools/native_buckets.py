# -*- coding: utf-8 -*-
# 把 native 那 181 条空壳（HOST_LIST 180 + VM_Execute）分成三堆：
#   ① 真引擎侧：必须有引擎/系统（画、声、输入、窗口、相机、表面、房间、网络收发、IME…）
#   ② 自己能算：文件/目录/缓冲区/日期/ini/纯数学/实例操作
#   ③ 项目业务：平台已有同名 callbin 或 GML → 转发/照搬（不是"引擎原语"）
import io, re, glob, os

h = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()
i = h.index('#define HOST_LIST(X)')
seg = h[i:]
j = seg.find('\n\n')
if j < 0: j = len(seg)
host = re.findall(r'X\(([^)]*)\)', seg[:j]) + ['VM_Execute']

cbs = {os.path.basename(p)[:-len('.callbin')] for p in glob.glob('kernel/**/*.callbin', recursive=True)}
gm  = {os.path.basename(p)[:-len('.gml')]     for p in glob.glob('kernel/**/*.gml', recursive=True)}
gm |= {os.path.basename(p)[:-len('.gml')]     for p in glob.glob('scripts/**/*.gml', recursive=True)}

# 真引擎侧：按前缀/名字
ENGINE_RULES = [
    ('绘制/几何',   r'^(draw_|string_width|string_height)'),
    ('表面/GPU/纹理', r'^(surface_|gpu_|texture_)'),
    ('窗口/显示',   r'^(window_|display_)'),
    ('输入',        r'^(keyboard_|mouse_|device_)'),
    ('音频',        r'^audio_'),
    ('相机/视图',   r'^(camera_|view_)'),
    ('房间',        r'^room_'),
    ('精灵(引擎资源)', r'^sprite_'),
    ('网络收发',    r'^(network_send_raw|network_destroy|http_request|url_open|network_broadcast_active_skill)'),
    ('系统/IME/对话框', r'^(native_|show_question|game_end|game_set_speed|object_is_ancestor)'),
]
# 自己能算
SELF_RULES = [
    ('文件/目录',   r'^(file_|directory_)'),
    ('缓冲区',      r'^buffer_'),
    ('日期',        r'^date_'),
    ('ini',         r'^ini_'),
    ('纯数学/实例', r'^(get_grid_position_from_world|get_world_position_from_grid|calculate_plant_depth|'
                    r'damage_enemy|version_greater|VM_Execute)$'),
]

engine, selfok, biz = [], [], []
for n in host:
    if n in cbs or (n in gm and not re.match(r'^(draw_|surface_|window_|keyboard_|mouse_|audio_|camera_|sprite_)', n)):
        biz.append(n)
        continue
    if any(re.match(p, n) for _, p in SELF_RULES): selfok.append(n)
    elif any(re.match(p, n) for _, p in ENGINE_RULES): engine.append(n)
    else: biz.append(n)

print('native 空壳 %d 条' % len(host))
print('\n=== ① 真引擎侧 %d 条 ===' % len(engine))
for label, pat in ENGINE_RULES:
    hit = [n for n in engine if re.match(pat, n)]
    if hit: print('  %-14s %2d  %s' % (label, len(hit), ' '.join(hit)))
print('\n=== ② 自己能算 %d 条 ===' % len(selfok))
for label, pat in SELF_RULES:
    hit = [n for n in selfok if re.match(pat, n)]
    if hit: print('  %-12s %2d  %s' % (label, len(hit), ' '.join(hit)))
print('\n=== ③ 项目业务（转发/照 GML）%d 条 ===' % len(biz))
print('  ' + ' '.join(biz))
