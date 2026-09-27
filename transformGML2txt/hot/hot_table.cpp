// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


struct HotBlock { const char* obj; const char* block; bool (*fn)(Vm&); };

const HotBlock kHotBlocks[] = {
    { "obj_card_parent",  "_OBJECT_CREATE",       hot_card_create         },
    { "obj_card_parent",  "_OBJECT_STEP",         hot_card_step           },
    { "obj_card_parent",  "_OBJECT_DESTROY",      hot_card_destroy        },
    { "obj_card_parent",  "_OBJECT_DRAW",         hot_card_draw           },
    { "obj_card_parent",  "_OBJECT_DRAW_64",      hot_card_draw_64        },
    { "obj_card_parent",  "_OBJECT_USER_EVENT_0", hot_card_user_event_0   },
    { "obj_card_parent",  "_OBJECT_USER_EVENT_2", hot_card_user_event_2   },
    { "obj_enemy_parent", "_OBJECT_CREATE",       hot_enemy_create        },
    { "obj_enemy_parent", "_OBJECT_STEP",         hot_enemy_step          },
    // ⚠ 块名要跟翻译器 event_to_block() 对齐：Step_2.gml → _OBJECT_STEP_END（不是 _OBJECT_STEP_2）
    { "obj_enemy_parent", "_OBJECT_STEP_END",     hot_enemy_step_2        },
    { "obj_enemy_parent", "_OBJECT_DESTROY",      hot_enemy_destroy       },
    { "obj_enemy_parent", "_OBJECT_DRAW",         hot_enemy_draw          },
    { "obj_enemy_parent", "_OBJECT_DRAW_64",      hot_enemy_draw_64       },
    { "obj_enemy_parent", "_OBJECT_USER_EVENT_0", hot_enemy_user_event_0  },
    { "obj_Homing_Bullet_Management", "_OBJECT_CREATE", hot_homing_create },
    { "obj_Homing_Bullet_Management", "_OBJECT_STEP",   hot_homing_step   },
    { "obj_Homing_Bullet_Management", "_OBJECT_DRAW",   hot_homing_draw   },
    // obj_enemy_mod：7 个事件 + Create 里的本地函数
    { "obj_enemy_mod", "_OBJECT_CREATE",         hot_enemymod_create      },
    { "obj_enemy_mod", "_OBJECT_STEP",           hot_enemymod_step        },
    { "obj_enemy_mod", "_OBJECT_DRAW",           hot_enemymod_draw        },
    { "obj_enemy_mod", "_OBJECT_DESTROY",        hot_enemymod_destroy     },
    { "obj_enemy_mod", "_OBJECT_CLICK",          hot_enemymod_click       },
    { "obj_enemy_mod", "_OBJECT_MOUSE_ENTER",    hot_enemymod_mouse_enter },
    { "obj_enemy_mod", "_OBJECT_MOUSE_LEAVE",    hot_enemymod_mouse_leave },
    { "obj_enemy_mod", "_FUNC_mod_enemy_init",   hot_enemymod_func_init   },
    // obj_player_character（父对象是 obj_card_parent，但它不调 event_inherited）
    { "obj_player_character", "_OBJECT_CREATE",        hot_player_create        },
    { "obj_player_character", "_OBJECT_STEP",          hot_player_step          },
    { "obj_player_character", "_OBJECT_STEP_END",      hot_player_step_end      },
    { "obj_player_character", "_OBJECT_DRAW",          hot_player_draw          },
    { "obj_player_character", "_OBJECT_MOUSE_53",      hot_player_mouse_53      },
    { "obj_player_character", "_OBJECT_USER_EVENT_1",  hot_player_user_event_1  },
    { "obj_player_character", "_OBJECT_USER_EVENT_2",  hot_player_user_event_2  },
    // obj_card_mod（id 变量 = plant_id，注册表 mod_card_vms）
    { "obj_card_mod", "_OBJECT_CREATE",       hot_cardmod_create       },
    { "obj_card_mod", "_OBJECT_STEP",         hot_cardmod_step         },
    { "obj_card_mod", "_OBJECT_DRAW",         hot_cardmod_draw         },
    { "obj_card_mod", "_OBJECT_DESTROY",      hot_cardmod_destroy      },
    { "obj_card_mod", "_OBJECT_CLICK",        hot_cardmod_click        },
    { "obj_card_mod", "_OBJECT_MOUSE_ENTER",  hot_cardmod_mouse_enter  },
    { "obj_card_mod", "_OBJECT_MOUSE_LEAVE",  hot_cardmod_mouse_leave  },
    // obj_bullet_mod（没有父对象；id 变量 = mod_type，注册表 mod_bullet_vms）
    { "obj_bullet_mod", "_OBJECT_CREATE",       hot_bulletmod_create      },
    { "obj_bullet_mod", "_OBJECT_STEP",         hot_bulletmod_step        },
    { "obj_bullet_mod", "_OBJECT_DRAW",         hot_bulletmod_draw        },
    { "obj_bullet_mod", "_OBJECT_DESTROY",      hot_bulletmod_destroy     },
    { "obj_bullet_mod", "_OBJECT_CLICK",        hot_bulletmod_click       },
    { "obj_bullet_mod", "_OBJECT_MOUSE_ENTER",  hot_bulletmod_mouse_enter },
    { "obj_bullet_mod", "_OBJECT_MOUSE_LEAVE",  hot_bulletmod_mouse_leave },
    { "obj_bullet_mod", "_OBJECT_COLLISION_obj_enemy_parent", hot_bulletmod_collision },
    // obj_Bullet_Screen_Management（全场一个实例，子弹是结构体不是实例）
    { "obj_Bullet_Screen_Management", "_OBJECT_CREATE",  hot_bulletscreen_create  },
    { "obj_Bullet_Screen_Management", "_OBJECT_STEP",    hot_bulletscreen_step    },
    { "obj_Bullet_Screen_Management", "_OBJECT_DRAW",    hot_bulletscreen_draw    },
    { "obj_Bullet_Screen_Management", "_OBJECT_DESTROY", hot_bulletscreen_destroy },
    // obj_card_slot（卡槽 UI，没有父对象；两个本地函数也是热块）
    { "obj_card_slot", "_OBJECT_CREATE",        hot_cardslot_create           },
    { "obj_card_slot", "_OBJECT_STEP",          hot_cardslot_step             },
    { "obj_card_slot", "_OBJECT_STEP_END",      hot_cardslot_step_end         },
    { "obj_card_slot", "_OBJECT_DRAW",          hot_cardslot_draw             },
    { "obj_card_slot", "_OBJECT_DRAW_73",       hot_cardslot_draw_73          },
    { "obj_card_slot", "_OBJECT_USER_EVENT_0",  hot_cardslot_user_event_0     },
    { "obj_card_slot", "_FUNC_select_slot",     hot_cardslot_func_select_slot },
    { "obj_card_slot", "_FUNC_try_place_once",  hot_cardslot_func_try_place_once },
};

// 某个对象名认不认得（对 GML 的 asset_get_index(name) != -1）
//   判据是"这个对象存在"，**不是**"它有 Create 块"：
//     1) 有装了这个对象的 Vm（g_vm_of_object 里有）
//     2) 或者它是硬编码对象（热块表里有它）
bool obj_known(Vm& vm, const std::string& obj) {
    (void)vm;
    if (vm_of_object(obj)) return true;
    return has_hot_object(obj);
}


bool run_hot_block(Vm& vm, const std::string& obj, const std::string& block) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (obj == kHotBlocks[i].obj && block == kHotBlocks[i].block)
            return kHotBlocks[i].fn(vm);
    return false;                                        // 没硬编码 → 让解释器去跑
}


// 只问"有没有"，不跑。run_block_on 派发事件前问一下：
//   父对象不在 VM 的块表里（它们是硬编码的），别把"没有块"当成错
bool has_hot_block(const std::string& obj, const std::string& block) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (obj == kHotBlocks[i].obj && block == kHotBlocks[i].block) return true;
    return false;
}


// 热块表里有没有这个对象（任何块都算）—— obj_known 用它判"硬编码对象认不认得"
bool has_hot_object(const std::string& obj) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (obj == kHotBlocks[i].obj) return true;
    return false;
}
