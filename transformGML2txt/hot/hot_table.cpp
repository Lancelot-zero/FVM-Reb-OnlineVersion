// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"
#include "hot_objects.h"


// 表项按**池下标**存（S_xxx）：查表是两次整数比较，不再比字符串
struct HotBlock { int obj_sid; int block_sid; bool (*fn)(Vm&); };

const HotBlock kHotBlocks[] = {
    { S_obj_card_parent, S__OBJECT_CREATE,       hot_card_create         },
    { S_obj_card_parent, S__OBJECT_STEP,         hot_card_step           },
    { S_obj_card_parent, S__OBJECT_DESTROY,      hot_card_destroy        },
    { S_obj_card_parent, S__OBJECT_DRAW,         hot_card_draw           },
    { S_obj_card_parent, S__OBJECT_DRAW_64,      hot_card_draw_64        },
    { S_obj_card_parent, S__OBJECT_USER_EVENT_0, hot_card_user_event_0   },
    { S_obj_card_parent, S__OBJECT_USER_EVENT_2, hot_card_user_event_2   },
    { S_obj_enemy_parent, S__OBJECT_CREATE,       hot_enemy_create        },
    { S_obj_enemy_parent, S__OBJECT_STEP,         hot_enemy_step          },
    // ⚠ 块名要跟翻译器 event_to_block() 对齐：Step_2.gml → _OBJECT_STEP_END（不是 _OBJECT_STEP_2）
    { S_obj_enemy_parent, S__OBJECT_STEP_END,     hot_enemy_step_2        },
    { S_obj_enemy_parent, S__OBJECT_DESTROY,      hot_enemy_destroy       },
    { S_obj_enemy_parent, S__OBJECT_DRAW,         hot_enemy_draw          },
    { S_obj_enemy_parent, S__OBJECT_DRAW_64,      hot_enemy_draw_64       },
    { S_obj_enemy_parent, S__OBJECT_USER_EVENT_0, hot_enemy_user_event_0  },
    { S_obj_Homing_Bullet_Management, S__OBJECT_CREATE, hot_homing_create },
    { S_obj_Homing_Bullet_Management, S__OBJECT_STEP,   hot_homing_step   },
    { S_obj_Homing_Bullet_Management, S__OBJECT_DRAW,   hot_homing_draw   },
    // obj_enemy_mod：7 个事件 + Create 里的本地函数
    { S_obj_enemy_mod, S__OBJECT_CREATE,         hot_enemymod_create      },
    { S_obj_enemy_mod, S__OBJECT_STEP,           hot_enemymod_step        },
    { S_obj_enemy_mod, S__OBJECT_DRAW,           hot_enemymod_draw        },
    { S_obj_enemy_mod, S__OBJECT_DESTROY,        hot_enemymod_destroy     },
    { S_obj_enemy_mod, S__OBJECT_CLICK,          hot_enemymod_click       },
    { S_obj_enemy_mod, S__OBJECT_MOUSE_ENTER,    hot_enemymod_mouse_enter },
    { S_obj_enemy_mod, S__OBJECT_MOUSE_LEAVE,    hot_enemymod_mouse_leave },
    { S_obj_enemy_mod, S__FUNC_mod_enemy_init,   hot_enemymod_func_init   },
    // obj_player_character（父对象是 obj_card_parent，但它不调 event_inherited）
    { S_obj_player_character, S__OBJECT_CREATE,        hot_player_create        },
    { S_obj_player_character, S__OBJECT_STEP,          hot_player_step          },
    { S_obj_player_character, S__OBJECT_STEP_END,      hot_player_step_end      },
    { S_obj_player_character, S__OBJECT_DRAW,          hot_player_draw          },
    { S_obj_player_character, S__OBJECT_MOUSE_53,      hot_player_mouse_53      },
    { S_obj_player_character, S__OBJECT_USER_EVENT_1,  hot_player_user_event_1  },
    { S_obj_player_character, S__OBJECT_USER_EVENT_2,  hot_player_user_event_2  },
    // obj_card_mod（id 变量 = plant_id，注册表 mod_card_vms）
    { S_obj_card_mod, S__OBJECT_CREATE,       hot_cardmod_create       },
    { S_obj_card_mod, S__OBJECT_STEP,         hot_cardmod_step         },
    { S_obj_card_mod, S__OBJECT_DRAW,         hot_cardmod_draw         },
    { S_obj_card_mod, S__OBJECT_DESTROY,      hot_cardmod_destroy      },
    { S_obj_card_mod, S__OBJECT_CLICK,        hot_cardmod_click        },
    { S_obj_card_mod, S__OBJECT_MOUSE_ENTER,  hot_cardmod_mouse_enter  },
    { S_obj_card_mod, S__OBJECT_MOUSE_LEAVE,  hot_cardmod_mouse_leave  },
    // obj_bullet_mod（没有父对象；id 变量 = mod_type，注册表 mod_bullet_vms）
    { S_obj_bullet_mod, S__OBJECT_CREATE,       hot_bulletmod_create      },
    { S_obj_bullet_mod, S__OBJECT_STEP,         hot_bulletmod_step        },
    { S_obj_bullet_mod, S__OBJECT_DRAW,         hot_bulletmod_draw        },
    { S_obj_bullet_mod, S__OBJECT_DESTROY,      hot_bulletmod_destroy     },
    { S_obj_bullet_mod, S__OBJECT_CLICK,        hot_bulletmod_click       },
    { S_obj_bullet_mod, S__OBJECT_MOUSE_ENTER,  hot_bulletmod_mouse_enter },
    { S_obj_bullet_mod, S__OBJECT_MOUSE_LEAVE,  hot_bulletmod_mouse_leave },
    { S_obj_bullet_mod, S__OBJECT_COLLISION_obj_enemy_parent, hot_bulletmod_collision },
    // obj_Bullet_Screen_Management（全场一个实例，子弹是结构体不是实例）
    { S_obj_Bullet_Screen_Management, S__OBJECT_CREATE,  hot_bulletscreen_create  },
    { S_obj_Bullet_Screen_Management, S__OBJECT_STEP,    hot_bulletscreen_step    },
    { S_obj_Bullet_Screen_Management, S__OBJECT_DRAW,    hot_bulletscreen_draw    },
    { S_obj_Bullet_Screen_Management, S__OBJECT_DESTROY, hot_bulletscreen_destroy },
    // obj_card_slot（卡槽 UI，没有父对象；两个本地函数也是热块）
    { S_obj_card_slot, S__OBJECT_CREATE,        hot_cardslot_create           },
    { S_obj_card_slot, S__OBJECT_STEP,          hot_cardslot_step             },
    { S_obj_card_slot, S__OBJECT_STEP_END,      hot_cardslot_step_end         },
    { S_obj_card_slot, S__OBJECT_DRAW,          hot_cardslot_draw             },
    { S_obj_card_slot, S__OBJECT_DRAW_73,       hot_cardslot_draw_73          },
    { S_obj_card_slot, S__OBJECT_USER_EVENT_0,  hot_cardslot_user_event_0     },
    { S_obj_card_slot, S__FUNC_select_slot,     hot_cardslot_func_select_slot },
    { S_obj_card_slot, S__FUNC_try_place_once,  hot_cardslot_func_try_place_once },
};

// 某个对象名认不认得（对 GML 的 asset_get_index(name) != -1）
//   判据是"这个对象存在"，**不是**"它有 Create 块"：
//     1) 有装了这个对象的 Vm（g_vm_of_object 里有）
//     2) 或者它是硬编码对象（热块表里有它）
bool has_hot_object_sid(int obj_sid) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (kHotBlocks[i].obj_sid == obj_sid) return true;
    return false;
}

bool has_hot_object(const std::string& obj) {
    int sid = str_find(obj);
    return (sid >= 0) && has_hot_object_sid(sid);
}

bool obj_known(Vm& vm, const std::string& obj) {
    (void)vm;
    if (vm_of_object(obj)) return true;
    return has_hot_object(obj);
}


bool run_hot_block_sid(Vm& vm, int obj_sid, int block_sid) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (kHotBlocks[i].obj_sid == obj_sid && kHotBlocks[i].block_sid == block_sid)
            return kHotBlocks[i].fn(vm);
    return false;                                        // 没硬编码 → 让解释器去跑
}

bool run_hot_block(Vm& vm, const std::string& obj, const std::string& block) {
    int o = str_find(obj), b = str_find(block);
    return (o >= 0 && b >= 0) && run_hot_block_sid(vm, o, b);
}


// 只问"有没有"，不跑。run_block_on 派发事件前问一下：
//   父对象不在 VM 的块表里（它们是硬编码的），别把"没有块"当成错
bool has_hot_block_sid(int obj_sid, int block_sid) {
    for (size_t i = 0; i < sizeof(kHotBlocks) / sizeof(kHotBlocks[0]); i++)
        if (kHotBlocks[i].obj_sid == obj_sid && kHotBlocks[i].block_sid == block_sid) return true;
    return false;
}

bool has_hot_block(const std::string& obj, const std::string& block) {
    int o = str_find(obj), b = str_find(block);
    return (o >= 0 && b >= 0) && has_hot_block_sid(o, b);
}
