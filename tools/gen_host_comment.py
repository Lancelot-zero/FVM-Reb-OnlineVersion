# -*- coding: utf-8 -*-
# 重写 host_fn.cpp 的注释块：把【真引擎侧】单独列出来，
# 再列【项目业务（转发）】，并指明另外两批（自己能算 27 / 归业务层 4）不在本文件里
#   名单真源 = outer_split.h 的 HOST_ENGINE_LIST / HOST_BIZ_LIST / HOST_REAL_LIST / BIZ_NATIVE_LIST
#   重新生成： python tools/gen_host_comment.py
import io, re

sp  = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()
dst = 'DLL_VM/outer/host_fn.cpp'
src = io.open(dst, encoding='utf-8').read()

def grab(name):
    i = sp.index('#define %s(X)' % name)
    seg = sp[i:]
    j = seg.find('\n\n')
    if j < 0:
        j = len(seg)
    return re.findall(r'X\(([^)]*)\)', seg[:j])

eng  = grab('HOST_ENGINE_LIST')
biz  = grab('HOST_BIZ_LIST')
real = grab('HOST_REAL_LIST')
bnat = grab('BIZ_NATIVE_LIST')
stat = grab('HOST_STATE_LIST')   # 读 B1 状态区的那几条（有自己的实现文件 host_state_in.cpp）

ENGINE_RULES = [
    ('绘制/几何',       r'^(draw_|string_width|string_height)'),
    ('表面/GPU/纹理',   r'^(surface_|gpu_|texture_)'),
    ('窗口/显示',       r'^(window_|display_)'),
    ('输入',            r'^(keyboard_|mouse_|device_)'),
    ('音频',            r'^audio_'),
    ('相机/视图',       r'^(camera_|view_)'),
    ('房间',            r'^room_'),
    ('精灵(引擎资源)',  r'^sprite_'),
    ('网络收发',        r'^(network_send_raw|network_destroy|http_request|url_open|network_broadcast_active_skill)'),
    ('系统/IME/对话框', r'^(native_|show_question|game_end|game_set_speed|object_is_ancestor)'),
]
BIZ_RULES = [
    ('任务/解锁',  r'(is_.*_unlocked|is_level_completed|unlock_|upgrade_|complete_level|task)'),
    ('存档/卡组',  r'(save_|deck_|load_custom|add_to_deck|remove_from_deck|sort_plants)'),
    ('商店/材料',  r'(shop_|material)'),
    ('图鉴/装备',  r'(cookbook|attire|get_card_attire_list|get_card_craft_rule|get_plant_shape_data|'
                   r'get_gem_craft_rule|get_gem_max_level|edit_gem|get_load_audio)'),
    ('界面/横幅',  r'(draw_notices|update_notices|refresh_|gui_stack|notice|reward|level_button)'),
    ('铲子/其它',  r'(shovel|get_map_object_data|keyboard_get_string|version_greater|VM_FlushHooks|'
                   r'create_battle_slots|mod_sync_props_tick|mod_card_vm_deck_refresh)'),
    ('网络/连接',  r'(network_|add_net_id|disconnect)'),
    ('调试/消息',  r'(send_message|show_debug_message)'),
]

def dwidth(s):
    return sum(2 if ord(c) > 0x2E80 else 1 for c in s)

def emit(lines, label, names, head_pad='//   '):
    if not names:
        return
    MAXW, NAME_COL = 118, 22
    head = '%s %d 条' % (label, len(names))
    pad = ' ' * max(1, NAME_COL - dwidth(head))
    cur = head_pad + head + pad
    for n in names:
        if dwidth(cur) > NAME_COL and dwidth(cur) + 1 + len(n) > MAXW:
            lines.append(cur)
            cur = head_pad + ' ' * NAME_COL
        cur += n + ' '
    lines.append(cur.rstrip())

lines = []
lines.append('// ── ① 【真引擎侧】这 %d 条**必须有引擎/系统**（下面按"要什么"分组）──' % len(eng))
left = list(eng)
for label, pat in ENGINE_RULES:
    hit = [n for n in left if re.match(pat, n)]
    emit(lines, label, hit)
    left = [n for n in left if n not in hit]
emit(lines, '其他引擎侧', left)
lines.append('//')
lines.append('// ── ② 项目业务 %d 条：平台**已有同名 callbin**（或只有同名 GML）→ 函数体就是转发 ──' % len(biz))
left = list(biz)
for label, pat in BIZ_RULES:
    hit = [n for n in left if re.match(pat, n)]
    emit(lines, label, hit)
    left = [n for n in left if n not in hit]
emit(lines, '其他业务', left)
lines.append('//')
lines.append('// ── 不在本文件里的几批（别在这儿找）──')
lines.append('//   ③ 自己能算 %d 条 → outer/host_real.cpp（文件/目录/缓冲区/日期/ini，已真实现）' % len(real))
lines.append('//        %s' % ' '.join(real))
lines.append('//   ④ 归业务层 %d 条 → business/native_biz.cpp（网格数学 3 + 伤害 1）' % len(bnat))
lines.append('//        %s' % ' '.join(bnat))
lines.append('//   ⑤ 读 B1 状态区 %d 条 → outer/host_state_in.cpp（引擎每帧写好的共享内存，见 host_state_in.cpp）' % len(stat))
lines.append('//        %s' % ' '.join(stat))

# 180 = 宿主类总数：真引擎侧 + 项目业务 + 自己能算 27 + 归业务层 4 + 读 B1 的那几条
assert len(eng) + len(biz) + len(stat) == 180 - 27 - 4, (len(eng), len(biz), len(stat))
block = '\n'.join(lines)
a = src.index('// >>> HOST_GROUPS_BEGIN')
b = src.index('// >>> HOST_GROUPS_END')
out = src[:a] + '// >>> HOST_GROUPS_BEGIN\n' + block + '\n' + src[b:]
io.open(dst, 'w', encoding='utf-8', newline='\n').write(out)
print('%s：真引擎侧 %d + 项目业务 %d；另几批 %d + %d + %d'
      % (dst, len(eng), len(biz), len(real), len(bnat), len(stat)))
