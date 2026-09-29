# -*- coding: utf-8 -*-
# 把"不能自举"的接口按"要什么"分组列出来（标签用 ASCII，名字原样）
import io, re

c = io.open('DLL_VM/core/const_str.h', encoding='utf-8').read()
s = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()

def grab(text, name):
    m = re.search(r'#define ' + name + r'\(X\)(.*?)(?=\n#)', text, re.S)
    return re.findall(r'X\(([^)]*)\)', m.group(1)) if m else []

def grab_block(text, name):
    i = text.index('#define ' + name + '(X)')
    seg = text[i:]
    j = seg.find('\n\n')
    if j < 0:
        j = len(seg)
    return re.findall(r'X\(([^)]*)\)', seg[:j])

host = grab_block(s, 'HOST_LIST')
game = grab(c, 'GAME_FN_LIST')

# —— 游戏 API 里其实自举得了的（不用宿主）——
SELF_OK = set("""VM_IsUndefined VM_IsDestroyed VM_GetProp VM_SetProp VM_GetInstanceCount VM_GetInstanceAt
VM_CreateInstance VM_DestroyInstance VM_Random VM_RandomRange VM_Floor VM_Ceil VM_RunStep
VM_ArrayGet VM_ArraySet VM_ArrayDel VM_ArrayADD VM_ArraySize VM_ArrayClear VM_ArrayClearAll
VM_ArrayContains VM_ArrayExists VM_InstArrayExists VM_InstArraySize VM_InstArrayItem VM_InstArraySet
VM_InstArrayAdd VM_InstArrayDel VM_InstArrayClear VM_InstArrayContains
VM_CallFunc VM_FuncExists VM_FuncDesc VM_IsInstance VM_GetInstancesInRange""".split())
# VM_GetInstancesInRange 要碰撞/位置，其实要宿主 → 拿掉
SELF_OK.discard('VM_GetInstancesInRange')
SELF_OK.discard('VM_IsInstance')

need_host_game = [n for n in game if n not in SELF_OK]
can_self_game = [n for n in game if n in SELF_OK]

def group(names, rules):
    out, left = {}, list(names)
    for label, pat in rules:
        hit = [n for n in left if re.match(pat, n)]
        if hit:
            out[label] = hit
            left = [n for n in left if n not in hit]
    if left:
        out['MISC'] = left
    return out

HOST_RULES = [
    ('FILE/DIR',        r'^(file_|directory_)'),
    ('BUFFER',          r'^buffer_'),
    ('DATE',            r'^date_'),
    ('INI',             r'^ini_'),
    ('AUDIO',           r'^audio_'),
    ('CAMERA/VIEW',     r'^(camera_|view_)'),
    ('DRAW/GPU/TEX',    r'^(draw_|gpu_|texture_|shader_|surface_)'),
    ('DISPLAY/WINDOW',  r'^(display_|window_)'),
    ('INPUT',           r'^(keyboard_|mouse_|device_)'),
    ('NETWORK/HTTP',    r'^(network_|http_|url_|sh_disconnect|disconnect)'),
    ('ROOM',            r'^room_'),
    ('SPRITE/FONT/TEXT',r'^(sprite_|font_|string_width|string_height)'),
    ('GAME/PLATFORM',   r'^(game_|native_|show_question|version_|mod_sync|object_is_ancestor|VM_FlushHooks)'),
    ('SAVE/DECK/CARD',  r'^(save_|deck_|load_custom|add_to_deck|remove_from_deck|sort_plants)'),
    ('TASK/UNLOCK',     r'^(task|get_task|edit_task|add_task|remove_task|reset_task|refresh_task|unlock_|upgrade_|complete_level|is_.*_unlocked|is_.*_completed)'),
    ('SHOP/MATERIAL',   r'^(shop_|material|add_material|get_material)'),
    ('COOKBOOK/ATTIRE/GEAR', r'(cookbook|attire|gem|weapon|card|plant_shape|map_object|get_load_audio)'),
    ('GUI/NOTICE',      r'^(gui_|notice|reward|show_debug|send_message|screen_flash)'),
    ('GRID/COLLISION',  r'^(get_grid_position|get_world_position|calculate_plant_depth|sort_plants|damage_enemy|can_hit|can_place|can_target|into_act|homing_bullet|effect_create)'),
]

print('===== A. NEEDS HOST (%d) =====' % len(host))
for k, v in group(host, HOST_RULES).items():
    print('-- %s (%d)' % (k, len(v)))
    print('   ' + ' '.join(v))
print()
print('===== B. GAME API: needs host (%d) =====' % len(need_host_game))
print('   ' + ' '.join(need_host_game))
print()
print('===== C. GAME API: can self-host (%d) =====' % len(can_self_game))
print('   ' + ' '.join(can_self_game))
