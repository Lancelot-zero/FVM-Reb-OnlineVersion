#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# align_native_tables.py — 把 names.h 的 native 段和 gm2bin 的名单对齐
#
#   起因：有 7 个名字既在 REST_NAME_LIST（当字符串用）又被 gm2bin 当 native。
#   一个名字在池里只能有一个 id，两边就会错位（bin 里烘的 native id 对不上 C++ 表）。
#
#   规矩（对齐后两边必须一样长、同序）：
#     C++  native 段 = NATIVE_FN_LIST(79) + NATIVE_TAIL_LIST(99)
#                      + PLACEHOLDER_FN_LIST(123) + NATIVE_EXTRA_LIST(1)
#     gm2bin        = NATIVE_STUBS(79) + NATIVE_IMPL(99 + 123 + 1)
# ============================================================

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
NAMES_H = os.path.join(HERE, "names.h")
VM_H = os.path.join(HERE, "vm.h")

# 顺序真源 = gm2bin NATIVE_IMPL 去掉前 99 条
NAMES = """audio_group_set_gain audio_is_paused audio_is_playing buffer_load clear_deck deck_slot_count device_mouse_x_to_gui ds_map_add
get_card_attire_list get_card_craft_rule get_cookbook_data get_material_amount get_task_data get_task_state is_card_unlocked
is_level_completed is_weapon_unlocked keyboard_check_pressed keyboard_get_string load_custom_deck mod_card_vm_deck_refresh
refresh_cookbook_list refresh_level_button refresh_task_list save_file save_to_custom_deck shop_list_recharge sprite_get_width
string_width surface_free texture_prefetch update_notices view_get_camera window_get_fullscreen window_has_focus window_set_cursor
VM_FlushHooks add_material_amount add_task_progress array_copy audio_pause_all audio_resume_sound audio_sound_gain buffer_read
camera_set_view_pos device_mouse_y_to_gui draw_notices draw_sprite draw_text_ext_transformed edit_task_progress edit_task_state
equip_cookbook http_request ini_open is_gem_unlocked mouse_check_button_pressed sprite_get_height string_height string_length
surface_exists upgrade_card_shape window_set_fullscreen audio_pause_sound audio_resume_all buffer_delete equip_attire
get_task_progress ini_write_real instance_log_enable is_attire_unlocked is_task_unlocked json_parse mouse_check_button_released
string_copy surface_create surface_set_target unequip_cookbook upgrade_card add_net_id audio_group_get_gain deck_slot_is_empty
draw_clear_alpha draw_line get_gem_max_level ini_close refresh_reward_button remove_task room_restart string_replace_all
surface_reset_target unequip_attire unlock_card unlock_task window_handle deck_ensure_size draw_surface get_gem_craft_rule
get_load_audio get_material_info get_plant_shape_data gpu_set_tex_filter mouse_check_button native_enable_ime ord sprite_get_name
unlock_weapon version_greater audio_exists deck_slot_max draw_text_ext edit_gem_max_level instance_log_disable native_disable_ime
show_question unlock_gem window_enable_borderless_fullscreen add_to_deck edit_gem_level native_stop_mouse_limit unlock_attire
url_open native_start_mouse_limit remove_from_deck""".split()


def macro_body(src, name):
    m = re.search(r'#define %s\(X\) \\\n(.*?)\n\n' % name, src, re.S)
    return m


def main():
    s = open(NAMES_H, encoding="utf-8").read()

    # ① PLACEHOLDER_FN_LIST ← 全部 123 个（按 NAMES 顺序）
    m = macro_body(s, "PLACEHOLDER_FN_LIST")
    assert m, "找不到 PLACEHOLDER_FN_LIST"
    body = "\n".join("    X(%s)%s" % (n, " \\" if i < len(NAMES) - 1 else "")
                     for i, n in enumerate(NAMES))
    s = s[:m.start()] + "#define PLACEHOLDER_FN_LIST(X) \\\n" + body + "\n\n" + s[m.end():]
    print("PLACEHOLDER_FN_LIST 重写为 %d 条（NAHES 顺序）" % len(NAMES))

    # ② 这 7 个从 REST_NAME_LIST 里删掉（它们已经是 native 了）
    m = macro_body(s, "REST_NAME_LIST")
    rest = re.findall(r'X\(([A-Za-z_][A-Za-z0-9_]*)\)', m.group(1))
    kill = set(NAMES)
    kept = [n for n in rest if n not in kill]
    removed = [n for n in rest if n in kill]
    body = "\n".join("    X(%s)%s" % (n, " \\" if i < len(kept) - 1 else "")
                     for i, n in enumerate(kept))
    s = s[:m.start()] + "#define REST_NAME_LIST(X) \\\n" + body + "\n\n" + s[m.end():]
    print("REST_NAME_LIST 删掉 %d 个（%s）→ 剩 %d" % (len(removed), ", ".join(removed), len(kept)))
    open(NAMES_H, "w", encoding="utf-8", newline="\n").write(s)

    # ③ NATIVE_FN_COUNT = 79 + 99 + len(NAMES) + 1
    v = open(VM_H, encoding="utf-8").read()
    total = 79 + 99 + len(NAMES) + 1
    v2 = re.sub(r"static const int NATIVE_FN_COUNT = \d+;",
                "static const int NATIVE_FN_COUNT = %d;" % total, v, count=1)
    open(VM_H, "w", encoding="utf-8", newline="\n").write(v2)
    print("NATIVE_FN_COUNT = %d" % total)


if __name__ == "__main__":
    main()
