# -*- coding: utf-8 -*-
# 把 game_api 里"要宿主"的那 101 条，拿 GML 侧参考实现过一遍：
#   只看它调了什么 —— 只碰 global.__vm / 数组 / ds_map / 字符串的 → 自举候选
#   调引擎的（instance_create/draw_/audio_/file_/…）→ 真宿主
import io, re, glob, os, collections

# 1) 读出 game_api.cpp 注释块里"要宿主"那批（= 非"自举"段落里的 VM_*）
g = io.open('DLL_VM/outer/game_api.cpp', encoding='utf-8').read()
i = g.index('// >>> GAME_GROUPS_BEGIN'); j = g.index('// >>> GAME_GROUPS_END')
block = g[i:j]
self_part = block[block.index('自举'):]
self_names = set(re.findall(r'(VM_\w+)', self_part))
all_names = set(re.findall(r'(VM_\w+)', block))
host = sorted(all_names - self_names)
print('要宿主 %d 条' % len(host))

# 2) GML 参考实现
gml_dir = 'kernel/function'
have = {}
for n in host:
    p = os.path.join(gml_dir, n + '.gml')
    if os.path.exists(p):
        have[n] = io.open(p, encoding='utf-8').read()

ENGINE = re.compile(r'\b(instance_create\w*|instance_destroy|instance_position|place_meeting|'
                    r'draw_\w+|sprite_\w+|surface_\w+|shader_\w+|texture_\w+|gpu_\w+|display_\w+|'
                    r'audio_\w+|file_\w+|directory_\w+|buffer_\w+|date_\w+|ini_\w+|window_\w+|'
                    r'room_\w+|tile_\w+|camera_\w+|view_\w+|keyboard_\w+|mouse_\w+|device_\w+|'
                    r'network_\w+|http_\w+|show_debug_message|show_message|screen_\w+|'
                    r'effect_create\w*|part_\w+|physics_\w+|os_\w+|get_string|game_end|game_set_speed)\b')

pure, mixed, no_gml = [], [], []
for n in host:
    t = have.get(n)
    if t is None:
        no_gml.append(n); continue
    hits = sorted(set(ENGINE.findall(t)))
    (pure if not hits else mixed).append((n, hits))

print('\n有 GML 参考 %d 条，没找到 %d 条' % (len(have), len(no_gml)))
print('\n=== A. 只碰 VM/全局/数组（自举候选）%d 条 ===' % len(pure))
for n, _ in pure: print('   ', n)
print('\n=== B. 调了引擎 %d 条 ===' % len(mixed))
for n, h in mixed[:200]: print('   %-32s %s' % (n, ' '.join(h[:6])))
print('\n=== C. 没找到 GML 参考 %d 条 ===' % len(no_gml))
print('   ', ' '.join(no_gml))
