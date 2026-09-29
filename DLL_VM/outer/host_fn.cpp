// ============================================================
// host_fn.cpp — 宿主类（180 条）[**不能自举**，留给宿主定]
//
//   这批要游戏引擎 / 平台 / 文件系统 / 图形 / 音频 / 网络，VM 自己算不出来：
//     文件 file_* / 目录 directory_* / 缓冲区 buffer_* / 日期 date_* / ini
//     音频 audio_* / 相机 camera_* / 绘制 draw_* / 表面 surface_* / 着色器 shader_*
//     输入 keyboard_/mouse_/device_* / 窗口 window_* / 房间 room_* / 精灵 sprite_*
//     网络 network_* / 存档 save_* / 任务 task_* / 解锁 unlock_* / 界面 gui_* / …
//
//   现在：**留空壳**，一被调到就置 VM_E_NOT_IMPL（a1 = 函数名池下标）+ 返回 undefined。
//   这样跑起来能立刻看见"卡在哪几条函数上"，而不是静默算错。
//
// ── 按"要什么"分组（143 条 = 真引擎侧 71 + 项目业务 72）──────────
//   ⚠ 原来在"真引擎侧"里的那 6 条输入查询（keyboard_check_pressed / mouse_check_button* /
//     device_mouse_*_to_gui）已经挪去 outer/host_state_in.cpp：它们只是**读**引擎每帧写好的
//     B1 共享内存，不需要宿主填函数指针，不该在这儿当空壳。
//   这只是**给人看的分类**：表里的顺序和条数由 core/const_str.h 的名单说了算，
//   而"哪些生成本文件的空壳"由 outer_split.h 的 HOST_LIST 说了算
//   （HOST_LIST = HOST_ENGINE_LIST + HOST_BIZ_LIST）——
//   这里少写 / 写错一条都不影响编译（名单才是唯一真源）。
//   分组由 tools/gen_host_comment.py 从名单生成，**别手改这一块**。
// >>> HOST_GROUPS_BEGIN
// ── ① 【真引擎侧】这 71 条**必须有引擎/系统**（下面按"要什么"分组）──
//   绘制/几何 20 条       draw_set_color draw_set_alpha draw_set_font draw_set_halign draw_set_valign draw_rectangle 
//                         draw_rectangle_color draw_roundrect string_width_ext string_height_ext string_width 
//                         draw_notices draw_sprite string_height draw_clear_alpha draw_line draw_surface 
//                         draw_text_ext draw_text_ext_transformed draw_enable_drawevent
//   表面/GPU/纹理 7 条    surface_free texture_prefetch surface_exists surface_create surface_set_target 
//                         surface_reset_target gpu_set_tex_filter
//   窗口/显示 6 条        window_get_fullscreen window_has_focus window_set_cursor window_set_fullscreen 
//                         window_handle window_enable_borderless_fullscreen
//   输入 1 条             keyboard_get_string
//   音频 12 条            audio_play_sound audio_group_set_gain audio_is_paused audio_is_playing audio_pause_all 
//                         audio_resume_sound audio_sound_gain audio_pause_sound audio_resume_all 
//                         audio_group_get_gain audio_exists audio_create_stream
//   相机/视图 6 条        view_get_camera camera_set_view_pos camera_get_view_x camera_get_view_y 
//                         camera_get_view_width camera_get_view_height
//   房间 3 条             room_restart room_goto room_exists
//   精灵(引擎资源) 3 条   sprite_get_width sprite_get_height sprite_get_name
//   网络收发 5 条         network_broadcast_active_skill http_request url_open network_destroy network_send_raw
//   系统/IME/对话框 8 条  native_enable_ime native_disable_ime show_question native_stop_mouse_limit 
//                         native_start_mouse_limit game_end game_set_speed object_is_ancestor
//
// ── ② 项目业务 72 条：平台**已有同名 callbin**（或只有同名 GML）→ 函数体就是转发 ──
//   任务/解锁 14 条       is_card_unlocked is_level_completed is_weapon_unlocked is_gem_unlocked upgrade_card_shape 
//                         is_attire_unlocked is_task_unlocked upgrade_card unlock_card unlock_task unlock_weapon 
//                         unlock_gem unlock_attire complete_level
//   存档/卡组 10 条       deck_slot_count load_custom_deck save_file save_to_custom_deck deck_slot_is_empty 
//                         deck_ensure_size deck_slot_max add_to_deck remove_from_deck sort_plants_in_grid
//   商店/材料 1 条        shop_list_recharge
//   图鉴/装备 8 条        get_card_attire_list get_card_craft_rule get_gem_max_level get_gem_craft_rule 
//                         get_load_audio get_plant_shape_data edit_gem_max_level edit_gem_level
//   界面/横幅 8 条        refresh_cookbook_list refresh_level_button refresh_task_list update_notices 
//                         refresh_reward_button gui_stack_pop refresh_task_progress gui_stack_to
//   铲子/其它 6 条        mod_card_vm_deck_refresh VM_FlushHooks version_greater get_map_object_data 
//                         mod_sync_props_tick create_battle_slots
//   网络/连接 3 条        add_net_id network_broadcast_shovel_remove disconnect
//   调试/消息 2 条        send_message show_debug_message
//   其他业务 20 条        clear_deck get_material_amount get_task_data get_task_state add_material_amount 
//                         add_task_progress edit_task_progress edit_task_state equip_cookbook equip_attire 
//                         get_task_progress unequip_cookbook remove_task unequip_attire get_material_info 
//                         get_cookbook_data select_shovel deselect_shovel reset_task_state sh_disconnect
//
// ── 不在本文件里的几批（别在这儿找）──
//   ③ 自己能算 27 条 → outer/host_real.cpp（文件/目录/缓冲区/日期/ini，已真实现）
//        buffer_load buffer_read ini_open buffer_delete ini_write_real ini_close file_exists file_text_open_write file_text_write_string file_text_close directory_exists directory_create buffer_create buffer_write file_text_open_append file_text_open_read buffer_get_size date_current_datetime file_text_eof buffer_seek date_datetime_string file_text_read_string buffer_tell file_text_readln file_text_writeln buffer_copy buffer_poke
//   ④ 归业务层 4 条 → business/native_biz.cpp（网格数学 3 + 伤害 1）
//        damage_enemy calculate_plant_depth get_grid_position_from_world get_world_position_from_grid
//   ⑤ 读 B1 状态区 6 条 → outer/host_state_in.cpp（引擎每帧写好的共享内存，见 host_state_in.cpp）
//        mouse_check_button_released mouse_check_button device_mouse_x_to_gui keyboard_check_pressed device_mouse_y_to_gui mouse_check_button_pressed
// >>> HOST_GROUPS_END
//
//   ⚠ 等宿主这边定了怎么接（函数指针表？回调？平台把 callbin 塞进来？），
//     再把这张表接到宿主实现上 —— 表里的**顺序和条数不动**（契约）。
// ============================================================
#include "outer.h"

// ── 真引擎侧那 71 条：实现要等宿主 → 先空壳（一调到就置 VM_E_NOT_IMPL）──
#define STUB(NAME) \
    void NAME(Value* mem, const uint8_t* args, int argc, int dst) { \
        (void)args; (void)argc; \
        vm_set_err(VM_E_NOT_IMPL, S_##NAME, 0); \
        wr_v(mem, dst, mk_undef()); \
    }
HOST_ENGINE_LIST(STUB)
#undef STUB

// ── 项目业务那 72 条：**不写实现**，走"名字 → 实现"的统一查找（fn_dll.cpp）──
//   ① DLL 函数表命中 → 直通（谁注册了同名函数谁说了算）
//   ② 否则 callbin 表（平台扫到的同名 callbin —— 仓库里现成就有 62 个）
//   ③ 都没有 → VM_E_FN_NOT_FOUND（响亮，不静默）
//   所以给某条写 C++ 实现 = VM_RegisterFunc("名字", fn)，**这里的表顺序一个字不用动**
#define BIZ_FWD(NAME) \
    void NAME(Value* mem, const uint8_t* args, int argc, int dst) { \
        native_forward(mem, args, argc, dst, S_##NAME); \
    }
HOST_BIZ_LIST(BIZ_FWD)
#undef BIZ_FWD
