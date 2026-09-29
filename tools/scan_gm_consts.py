# -*- coding: utf-8 -*-
# 扫全项目 .gml：找出"用到了、但项目自己没定义、前缀像 GM 内建常量"的标识符
#   → 这就是"哪些常量"（翻译器把它们编成了读变量，我们这边没人给值）
import io, re, glob, os, json, collections

# 1) 项目自己定义的名字：函数（kernel/function、scripts 里的 function xxx）、
#    宏/enum（gml_consts.json 里那两本字典的 key）
defined = set()
for p in glob.glob('kernel/**/*.gml', recursive=True) + glob.glob('scripts/**/*.gml', recursive=True):
    t = io.open(p, encoding='utf-8', errors='replace').read()
    defined.update(re.findall(r'\bfunction\s+([A-Za-z_]\w*)', t))
    defined.update(re.findall(r'\benum\s+([A-Za-z_]\w*)', t))
    # 宏： #macro NAME value
    defined.update(re.findall(r'#macro\s+([A-Za-z_]\w*)', t))
try:
    consts = json.loads(io.open('transformGML2txt/gml_consts.json', encoding='utf-8').read())
    for d in consts:
        for k in d:
            defined.add(k)
            defined.add(k.split('.')[0])          # CARD_STATE.IDLE → CARD_STATE
except Exception as e:
    print('consts 读取失败:', e)

# 2) 项目里用到的标识符
used = collections.Counter()
files = glob.glob('kernel/**/*.gml', recursive=True) + glob.glob('scripts/**/*.gml', recursive=True)
for p in files:
    t = io.open(p, encoding='utf-8', errors='replace').read()
    t = re.sub(r'//[^\n]*', ' ', t)               # 去行注释
    t = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)  # 去块注释
    for m in re.findall(r'\b([A-Za-z_]\w*)\b', t):
        used[m] += 1

# 2b) 函数名（native / game / callbin）都不算常量
gm_src = io.open('transformGML2txt/gm2bin.py', encoding='utf-8').read()
for blk in ('NATIVE_STUBS', 'NATIVE_IMPL', 'GAME_NAMES'):
    m = re.search(blk + r'\s*=\s*\[(.*?)\n\]', gm_src, re.S)
    if m:
        defined.update(re.findall(r'"([^"]+)"', m.group(1)))
for p in glob.glob('kernel/**/*.callbin', recursive=True):
    defined.add(os.path.basename(p)[:-len('.callbin')])

# 3) GM 内建常量的前缀族（够覆盖项目用到的）
FAMILIES = ['buffer_', 'vk_', 'c_', 'mb_', 'gp_', 'ds_type_', 'fa_', 'pr_', 'os_', 'ev_',
            'audio_', 'tile_', 'phy_', 'matrix_', 'display_', 'window_', 'network_',
            'cr_', 'pt_', 'bm_', 'cmpfunc_', 'asset_', 'application_surface', 'room_speed',
            'gamespeed_', 'game_speed', 'pi', 'infinity', 'NaN', 'noone', 'all', 'self', 'other',
            'global', 'local', 'undefined', 'pointer_', 'steam_', 'device_', 'of_', 'choose',
            'draw_', 'texture_', 'surface_', 'shader_', 'gpu_', 'font_', 'sprite_', 'sound_',
            'ef_', 'part_', 'particle_', 'se_', 'tile_', 'layer_', 'sequence_', 'anim_']
hit = collections.defaultdict(list)
for name, cnt in used.items():
    if name in defined:
        continue
    for fam in FAMILIES:
        if name.startswith(fam) or name == fam:
            hit[fam].append((cnt, name))
            break

print('扫了 %d 个 .gml；项目自己定义 %d 个名字' % (len(files), len(defined)))
print('\n"用到但项目没定义、前缀像 GM 内建常量"的，按族列（次数倒序）：')
total = 0
for fam in FAMILIES:
    lst = hit.get(fam)
    if not lst:
        continue
    lst.sort(reverse=True)
    total += len(lst)
    show = ', '.join('%s(%d)' % (n, c) for c, n in lst[:12])
    print('  %-18s %3d 个   %s%s' % (fam, len(lst), show, ' …' if len(lst) > 12 else ''))
print('\n合计（去重后）%d 个候选' % total)
