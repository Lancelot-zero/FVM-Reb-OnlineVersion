#!/usr/bin/env python3
# ============================================================
# gm2bin.py — GML → 字节码（.bin）转换器
#
# 三件套：词法 → 语法树 → 遍历生成
#   Parser 完全不碰字节码；控制流降级、调用、属性读写全在 Cg 那一趟做。
#
# 用法：gm2bin.py <对象目录或单个.gml> <输出.bin>
#       不给参数就跑扫描模式，见下面「扫描模式」那一段
#
# 每次运行都往 convert.log 追加一段：输入哪个目录、哪个事件文件过了、
# 卡在哪个文件的第几行、为什么、结果写没写出来。
#
# 内置 id 表在本文件里显式维护（NATIVE_STUBS / NATIVE_IMPL / GAME_NAMES），
# 不再从 vm.h / compiler_defs.h 解析 —— 这样 gm2bin 能独立跑（不依赖 C++ 源码）。
# id = 下标，只能往尾部追加；C++ 那边加了函数，就往对应列表尾部补一行。
# ============================================================

import contextlib
import io
import os
import re
import struct
import sys
import time

OP_ASSIGN, OP_COPY, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD = 1, 2, 3, 4, 5, 6, 7
OP_EQ, OP_NEQ, OP_GT, OP_GTE, OP_LT, OP_LTE = 8, 9, 10, 11, 12, 13
OP_CALL, OP_IF, OP_JMP, OP_HALT, OP_NATIVE, OP_LOAD_ELEM = 14, 15, 16, 17, 18, 19
OP_BAND, OP_BOR, OP_BXOR, OP_BNOT, OP_SHL, OP_SHR = 20, 21, 22, 23, 24, 25

MEM_INT, MEM_FLOAT, MEM_STRING = 0, 1, 2
MEM_UNDEFINED = 6     # GML 的 undefined（跟 vm.h 对齐；is_undefined 认这个类型）
RET_SLOT = 0          # 函数返回值固定写这个槽（调用方 run_block 完就读它）

# 槽位布局（照原版编译器那套：字面量池和临时槽分成两段，绝不重叠）
#   0            RET_SLOT
#   1..4095      字面量池（_VM_CONST_INIT 一次性写入）
#   4096 起       块的局部/临时槽，每个块各占一段（按实际用量分段，见 compile_object）
# 每个块一段是必须的：run_block 调进来时调用方的局部槽还活着，
# 共用一段就会互相踩。
LOCAL_BASE = 4096
LOCAL_MAX = 32768     # 和解释器的 MEM_SIZE 一致
BIN_OP = {"+": OP_ADD, "-": OP_SUB, "*": OP_MUL, "/": OP_DIV, "%": OP_MOD,
          "==": OP_EQ, "!=": OP_NEQ, ">": OP_GT, ">=": OP_GTE,
          "<": OP_LT, "<=": OP_LTE, "=": OP_EQ, "mod": OP_MOD, "div": OP_DIV,
          "&": OP_BAND, "|": OP_BOR, "^": OP_BXOR, "<<": OP_SHL, ">>": OP_SHR}

BIN_PREC = {"*": 90, "/": 90, "%": 90,
            "+": 80, "-": 80,
            "<<": 75, ">>": 75,
            "<": 70, ">": 70, "<=": 70, ">=": 70,
            "==": 60, "!=": 60, "=": 60,
            "&": 55, "^": 54, "|": 53,
            "&&": 51, "||": 50, "mod": 85, "div": 85}

HERE = os.path.dirname(os.path.abspath(__file__))


# ============================================================
# 内置函数 id 表 —— gm2bin 自己维护，唯一真源
#   id = 列表下标，**只能往尾部追加**：中间插、删、重排都会让老 bin 里
#   烘的数字 id 整体错位（旧产物会静默调错函数，不报错、不崩溃）。
#   · NATIVE_STUBS —— vm.h 的 NATIVE_FN_LIST(X)，宏生成的占位（已冻结）
#   · NATIVE_IMPL  —— vm_native.cpp 尾部显式列出的实现（新函数追加在这里）
#   · GAME_NAMES   —— LabMapCompiler/compiler_defs.h 的 FUNC_DEFS 顺序
#   这三份是快照，不和 C++ 源码对账；C++ 那边加了函数就往对应列表尾部补。
# ============================================================

NATIVE_STUBS = [
    "_find_right_cell", "alarm_get", "armor_drop", "array_deep_copy", "asset_get_index",
    "audio_stop_sound", "boss_array_shuffle", "boss_random", "bresenham", "buffer_exists",
    "bullet_screen_add_Ex", "bullet_screen_add_Exs", "Camera_Shock", "can_hit",
    "can_place_at_position", "can_target_on", "card_created", "card_equipped_attire_id",
    "clear_train_body", "connectOnesToTwos", "create_train_body", "create_train_body_order",
    "create_train_body_pos", "deck_get_card_data", "draw_self", "draw_set_colour",
    "draw_sprite_ext", "draw_text", "effect_create_above", "event_perform",
    "find_priority_enemy", "get_attire_info", "get_card_info_simple", "get_gem_index",
    "get_gem_info", "get_gem_level", "get_load_sprite", "get_plant_data_with_skill",
    "get_weapon_info", "homing_bullet_add", "instance_position", "into_act",
    "is_cookbook_equipped", "mod_base_tick", "mod_enemy_init", "mod_inst_add", "mod_inst_del",
    "move_towards_point", "network_apply_plant_level", "network_spawn_enemy",
    "object_get_name", "place_meeting", "point_in_rectangle", "position_meeting",
    "remove_train_body", "screen_flash", "set_net_id", "shader_get_uniform", "shader_reset",
    "shader_set", "show_notice", "sprite_add", "sprite_collision_mask", "sprite_exists",
    "sprite_get_number", "sprite_set_offset", "variable_struct_exists", "vm_hook_run",
    "card_destroyed", "get_nearest_grid_position", "collision_other", "inst_array_read",
    "inst_array_assign", "inst_array_index", "inst_array_delete", "inst_array_pop",
    "inst_array_resize", "inst_array_shuffle", "inst_array_create",
]

NATIVE_IMPL = [
    "run_block", "ds_list_create", "ds_list_destroy", "ds_list_clear", "ds_list_add",
    "ds_list_delete", "ds_list_size", "ds_list_find_value", "ds_list_find_index",
    "array_literal", "array_get", "array_set", "array_pop", "array_clear", "array_contains",
    "ds_map_create", "ds_map_set", "ds_map_delete", "ds_map_size", "ds_map_clear",
    "ds_map_keys_to_array", "ds_map_destroy", "struct_get", "struct_set", "struct_exists",
    "array_push", "ds_map_exists", "ds_map_find_value", "VM_Execute", "ds_grid_create",
    "ds_grid_set", "ds_grid_get", "merge_colour", "ds_grid_width", "ds_grid_height",
    "ds_grid_clear", "ds_grid_destroy", "array_length", "array_get_index", "array_delete",
    "array_resize", "array_create", "array_shuffle", "ds_exists", "instance_exists",
    "instance_number", "instance_create_depth", "instance_destroy", "variable_instance_exists",
    "variable_instance_get", "variable_instance_set", "floor", "round", "abs", "max", "min",
    "clamp", "lerp", "random", "random_range", "irandom_range", "sin", "cos", "degtorad",
    "lengthdir_x", "lengthdir_y", "point_direction", "point_distance", "is_real", "is_string",
    "is_array", "is_struct", "is_undefined", "string", "json_stringify",
    "variable_global_exists", "variable_global_get", "merge_color", "damage_enemy",
    "calculate_plant_depth", "get_grid_position_from_world", "get_world_position_from_grid",
    "audio_play_sound", "send_message", "show_debug_message", "run_event_user", "ceil",
    "make_color_rgb", "draw_set_color", "draw_set_alpha", "draw_set_font", "draw_set_halign",
    "draw_set_valign", "draw_rectangle", "draw_rectangle_color", "draw_roundrect",
    "string_width_ext", "string_height_ext", "network_broadcast_active_skill",
]

GAME_NAMES = [
    "VM_BanCard", "VM_SetCardLevelCap", "VM_SetMaxSlots", "VM_ShellPrint", "VM_ShowNotice",
    "VM_CreatePlatform", "VM_SpawnPlant", "VM_SpawnEnemy", "VM_SpawnBoss", "VM_LoadSprite",
    "VM_SpawnObject", "VM_SetProp", "VM_GetWave", "VM_GetSubwave", "VM_GetProp",
    "VM_GetLastBoss", "VM_GetLastCreatedEnemy", "VM_GetLastKilledEnemy",
    "VM_GetLastCreatedCard", "VM_GetLastDestroyedCard", "VM_GetFlame", "VM_SetFlame",
    "VM_SetTerrain", "VM_ClearPlants", "VM_Random", "VM_GetLastIdlePlatform",
    "VM_SetPlatformParams", "VM_SetMapBackground", "VM_SetDrawSlot", "VM_GetLoadedSpriteName",
    "VM_GameWin", "VM_GameLose", "VM_SetDrawSlot_front", "VM_SpawnCats", "VM_ClearMapObjects",
    "VM_BanGem", "VM_SetRowFeature", "VM_ClearPlantsByType", "VM_WakePlants", "VM_SetCardProp",
    "VM_SetEnemyProp", "VM_ShowNoticeDur", "VM_SetEventEnabled", "VM_RefreshPlatformSnapshots",
    "VM_GetMouseX", "VM_GetMouseY", "VM_GetMouseCol", "VM_GetMouseRow", "VM_GetTerrain",
    "VM_GetMousePressed", "VM_GetKeyDown", "VM_GetKeyPressed", "VM_GetEnemyCount",
    "VM_GetPlantCount", "VM_GetPlantCountAt", "VM_PlaySound", "VM_GetPlantAt", "VM_SwapPlants",
    "VM_SwapPlantRects", "VM_CompactColumn", "VM_CompactRow", "VM_CompactColumnRev",
    "VM_CompactRowRev", "VM_SpawnPlantsRandom", "VM_CreateButton", "VM_GetLastClickedButton",
    "VM_LoadSpriteFrames", "VM_ApplyPlantLevel", "VM_LoadSpritePerm", "VM_FreeSpritePerm",
    "VM_SetCardSlotProp", "VM_CalcCardSlotProp", "VM_GetCardSlotCount", "VM_GetPreviewCard",
    "VM_AliasSprite", "VM_LoadSpriteFrames_Ex", "VM_GetLastBossStateChangeId",
    "VM_GetLastBossOldState", "VM_GetLastBossNewState", "VM_ArrayGet", "VM_ArraySet",
    "VM_ArrayDel", "VM_ArrayADD", "VM_ArraySize", "VM_ArrayClear", "VM_ArrayClearAll",
    "VM_SetNoticeStyle", "VM_conveyor_belt_able", "VM_Slot_add", "VM_BanAllCard",
    "VM_CannelBanCard", "VM_BanWeapon", "VM_BanSuperWeapon", "VM_BanShield",
    "VM_SetCardShapeCap", "VM_SetCardSkillCap", "VM_GetKilledProp", "VM_IsUndefined",
    "VM_IsDestroyed", "VM_LoadSound", "VM_Floor", "VM_Ceil", "VM_SpawnBatMouse",
    "VM_GetTimeLimit", "VM_SetTimeLimit", "VM_SetDrawSlotEx", "VM_SetDrawSlotEx_front",
    "VM_SetWaveAuto", "VM_SetWave", "VM_GetCurCard", "VM_EnemyInRange", "VM_GetHomingTarget",
    "VM_GetInstancesInRange", "VM_CreateInstance", "VM_LoadSpritePerm_Ex",
    "VM_SetShovelFlameRate", "VM_BulletScreenAdd", "VM_DrawSpriteExt", "VM_RunStep",
    "VM_DestroyInstance", "VM_GetCardProp", "VM_CanPlace", "VM_CallFunc", "VM_FuncExists",
    "VM_FuncDesc", "VM_SpriteExists", "VM_AliasSpritePerm", "VM_ArrayExists", "VM_CellCount",
    "VM_CellItem", "VM_CellContains", "VM_ArrayContains", "VM_InstArrayExists",
    "VM_InstArraySize", "VM_InstArrayItem", "VM_InstArraySet", "VM_InstArrayAdd",
    "VM_InstArrayDel", "VM_InstArrayClear", "VM_InstArrayContains", "VM_HomingBulletAdd",
    "VM_DamageEnemy", "VM_DamageEnemyAsh", "VM_BulletScreenAdd_Ex", "VM_BulletScreenAdd_Exs",
    "VM_GetInfo", "VM_CatInRow", "VM_MapObj", "VM_GetInstanceCount", "VM_GetInstanceAt",
]


def _id_table(names, what):
    """名字 -> id（= 下标）；重名直接报错，别让两次运行给出不同 id"""
    tbl = {}
    for i, n in enumerate(names):
        if n in tbl:
            raise SystemExit("%s id 表里名字重复：%s（下标 %d 和 %d）" % (what, n, tbl[n], i))
        tbl[n] = i
    return tbl


NATIVE_NAMES = NATIVE_STUBS + NATIVE_IMPL
NATIVE_ID = _id_table(NATIVE_NAMES, "native")
GAME_ID = _id_table(GAME_NAMES, "game")

FID_GETPROP = GAME_ID["VM_GetProp"]
FID_SETPROP = GAME_ID["VM_SetProp"]
FID_GETCUR = GAME_ID["VM_GetCurCard"]


# ============================================================
# 常量表： GML 源里 enum  #macro，缓存成 json
# ============================================================
CONST_CACHE = os.path.join(HERE, "gml_consts.json")


def scan_consts():
    enum_re = re.compile(r"\benum\s+([A-Za-z_]\w*)\s*\{([^}]*)\}", re.S)
    macro_re = re.compile(r"(?m)^\s*#macro\s+([A-Za-z_]\w*)\s+(-?\w+)\s*$")
    macros = {}
    consts = {}
    roots = [os.path.join(HERE, "..", "objects"), os.path.join(HERE, "..", "scripts")]
    files = []
    for r in roots:
        for base, _d, fs in os.walk(r):
            for fn in fs:
                if fn.endswith(".gml"):
                    files.append(os.path.join(base, fn))
    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                src = f.read()
        except OSError:
            continue
        for m in macro_re.finditer(src):
            try:
                macros[m.group(1)] = int(m.group(2), 0)
            except ValueError:
                pass
        for m in enum_re.finditer(src):
            ename, body = m.group(1), m.group(2)
            body = re.sub(r"//[^\n]*", "", body)
            val = 0
            for part in body.split(","):
                part = part.strip()
                if not part:
                    continue
                if "=" in part:
                    k, _, v = part.partition("=")
                    k = k.strip()
                    v = v.strip()
                    got = None
                    try:
                        got = int(v, 0)
                    except ValueError:
                        got = consts.get(ename + "." + v, macros.get(v))
                    if got is None:
                        continue
                    val = got
                else:
                    k = part
                if not re.fullmatch(r"[A-Za-z_]\w*", k):
                    continue
                consts[ename + "." + k] = val
                val += 1
    return macros, consts


def load_consts():
    try:
        if os.path.exists(CONST_CACHE):
            import json
            with open(CONST_CACHE, "r", encoding="utf-8") as f:
                d = json.load(f)
            return d[0], d[1]
    except Exception:
        pass
    macros, consts = scan_consts()
    try:
        import json
        with open(CONST_CACHE, "w", encoding="utf-8") as f:
            json.dump([macros, consts], f)
    except Exception:
        pass
    return macros, consts


MACROS, CONSTS = load_consts()


def load_object_names():
    """工程里的对象资源名（从 .yyp 读）。用来把裸的 obj_xxx 认成"对象名"而不是变量名。"""
    yyp = os.path.abspath(os.path.join(HERE, "..", "FVM-reborn.yyp"))
    names = set()
    try:
        with open(yyp, "rb") as f:
            src = f.read().decode("utf-8-sig", "replace")
        names = set(re.findall(r'"path"\s*:\s*"objects/([^/"]+)/', src))
    except OSError:
        pass
    return names


OBJECT_NAMES = load_object_names()

class Stop(Exception):
    pass


g_file = ""
g_line = 0
STMT_LINE = {}   # id(语句节点)  (行号, 节点)：连节点丢起存，免得被回收 id 重用

# 每次运行都往这里追加丢段结果，跑完可以
LOG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "convert.log")
LAST_BAIL = ("", 0, "", "", "")


def dump_log(lines):
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as fp:
            for s in lines:
                fp.write(s + "\n")
    except OSError as e:
        print("log write failed: %s" % e)


def bail(what, why, hint=""):
    global LAST_BAIL
    LAST_BAIL = (g_file, g_line, what, why, hint)
    print("")
    print("========================================================")
    print("  STOP : %s:%d" % (g_file, g_line))
    print("--------------------------------------------------------")
    print("  FOUND: %s" % what)
    print("  WHY  : %s" % why)
    if hint:
        print("  FIX  : %s" % hint)
    print("========================================================")
    print("")
    raise Stop()


# ============================================================
# 词法
# ============================================================
TK_EOF, TK_NUM, TK_STR, TK_ID, TK_PUNCT, TK_NL, TK_TMPL = range(7)

NO_END = set(["+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=",
              "&&", "||", "!", ",", "(", "[", "{", ".", "?", ":", "+=", "-=",
              "*=", "/=", "else", "return", "var"])
CONT_OPS = ("||", "&&", "==", "!=", "<=", ">=")
CONT_CH = "+-*/%<>"
TWO_CH = ("==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "++", "--",
          "<<", ">>")


class Tok(object):
    __slots__ = ("k", "s", "num", "line", "isf")

    def __init__(self, k, s="", num=0.0, line=1, isf=False):
        self.k = k
        self.s = s
        self.num = num
        self.line = line
        self.isf = isf


def lex(src):
    out = []
    i = 0
    line = 1
    depth = 0
    last = ""
    n = len(src)

    while i < n:
        ch = src[i]

        if ch == "\n" or ch == "\r":
            if ch == "\r" and i + 1 < n and src[i + 1] == "\n":
                i += 1
            i += 1
            before = line
            line += 1
            if depth == 0 and last and last not in NO_END:
                j = i
                while j < n:
                    c2 = src[j]
                    if c2 in " \t\r\n":
                        j += 1
                        continue
                    if c2 == "/" and j + 1 < n and src[j + 1] == "/":
                        while j < n and src[j] != "\n":
                            j += 1
                        continue
                    break
                if src[j:j + 2] in CONT_OPS or src[j:j + 1] in CONT_CH:
                    continue
                out.append(Tok(TK_NL, "", 0.0, before))
                last = ""
            continue

        if ch in " \t":
            i += 1
            continue

        if ch == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue

        if ch == "/" and i + 1 < n and src[i + 1] == "*":
            i += 2
            while i + 1 < n and not (src[i] == "*" and src[i + 1] == "/"):
                if src[i] == "\n":
                    line += 1
                i += 1
            i += 2
            continue

        if ch == "#":
            j = i + 1
            while j < n and src[j] in "0123456789abcdefABCDEF":
                j += 1
            if j > i + 1:
                out.append(Tok(TK_NUM, "color", float(int(src[i + 1:j], 16)), line))
                last = "1"
                i = j
                continue
            bail("'#' literal", "only #RRGGBB color literals are handled")

        if ch.isdigit() or (ch == "." and i + 1 < n and src[i + 1].isdigit()):
            j = i
            isf = False
            while j < n and (src[j].isdigit() or src[j] == "." or src[j] in "xX" or
                             src[j] in "abcdefABCDEF"):
                if src[j] == ".":
                    isf = True
                j += 1
            txt = src[i:j]
            try:
                v = float(int(txt, 16)) if (len(txt) > 2 and txt[0] == "0" and txt[1] in "xX") else float(txt)
            except ValueError:
                v = 0.0
            out.append(Tok(TK_NUM, "", v, line, isf))
            last = "1"
            i = j
            continue

        # 模板字符串 $"...{expr}..." —— GML 2.3 的字符串插值
        if ch == "$" and i + 1 < n and src[i + 1] == '"':
            j = i + 2
            depth = 0
            buf = []
            while j < n:
                c = src[j]
                if c == "\n":
                    line += 1
                if depth == 0:
                    if c == '"':
                        break
                    if c == "{":
                        depth = 1
                    buf.append(c)
                    j += 1
                    continue
                # {} 里面：原样收，跳过其中的字符串和嵌套花括号
                if c == '"':
                    buf.append(c)
                    j += 1
                    while j < n and src[j] != '"':
                        if src[j] == "\\" and j + 1 < n:
                            buf.append(src[j])
                            buf.append(src[j + 1])
                            j += 2
                            continue
                        buf.append(src[j])
                        j += 1
                    if j < n:
                        buf.append('"')
                        j += 1
                    continue
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                buf.append(c)
                j += 1
            if j >= n:
                bail("unterminated template string", "no closing quote", "check the quotes")
            i = j + 1
            out.append(Tok(TK_TMPL, "".join(buf), 0.0, line))
            last = "s"
            continue

        if ch in "\"'":
            q = ch
            i += 1
            buf = []
            while i < n and src[i] != q:
                if src[i] == "\\" and i + 1 < n:
                    e = src[i + 1]
                    buf.append({"n": "\n", "t": "\t", "\\": "\\", '"': '"'}.get(e, e))
                    i += 2
                    continue
                if src[i] == "\n":
                    line += 1
                buf.append(src[i])
                i += 1
            if i >= n:
                bail("unterminated string", "no closing quote", "check the quotes")
            i += 1
            out.append(Tok(TK_STR, "".join(buf), 0.0, line))
            last = "s"
            continue

        if ch.isalpha() or ch == "_":
            j = i
            while j < n and (src[j].isalnum() or src[j] == "_"):
                j += 1
            nm = src[i:j]
            if nm in ('and', 'or', 'not', 'mod', 'div'):
                sym = {'and': '&&', 'or': '||', 'not': '!', 'mod': '%', 'div': '/'}[nm]
                out.append(Tok(TK_PUNCT, sym, 0.0, line))
                last = sym
                i = j
                continue
            out.append(Tok(TK_ID, nm, 0.0, line))
            last = nm
            i = j
            continue

        two = src[i:i + 2]
        if two in TWO_CH:
            out.append(Tok(TK_PUNCT, two, 0.0, line))
            last = two
            i += 2
        else:
            out.append(Tok(TK_PUNCT, ch, 0.0, line))
            last = ch
            i += 1
        if out[-1].s in ("(", "["):
            depth += 1
        elif out[-1].s in (")", "]"):
            depth -= 1

    out.append(Tok(TK_EOF, "", 0.0, line))
    return out


# ============================================================
# 语法树（元组
#   表达式：('num',v) ('float',v) ('str',s) ('id',n) ('self',) ('other',)
#           ('global',n) ('member',b,n) ('index',b,i) ('lidx',b,i)
#           ('midx',b,k) ('sidx',b,k) ('call',n,args) ('un',op,e)
#           ('bin',op,a,b) ('logic',op,a,b) ('tern',c,a,b)
#           ('arr',elems) ('struct',pairs)
#   语句 'block',ss) ('expr',e) ('var',n,init) ('assign',tgt,op,val)
#         ('if',c,t,e) ('while',c,b) ('for',i,c,s,b)
#         ('switch',subj,cases,default) ('with',tgt,b)
#         ('break',) ('continue',) ('exit',) ('return',e)
#         ('func',name,params,body)
# ============================================================
ASSIGN_OPS = ("=", "+=", "-=", "*=", "/=", "++", "--")

# 这些数组函数，如果目标是实例/全局变量上的数组，就走 VM_InstArray* / inst_array_*
ARRAY_FNS = ("array_length", "array_clear", "array_contains", "array_push",
             "array_get_index", "array_delete", "array_deep_copy",
             "array_pop", "array_resize", "array_shuffle")


def is_assign_start(toks, p):
    if toks[p].k != TK_ID:
        return False
    j = p + 1
    while True:
        while toks[j].k == TK_PUNCT and toks[j].s == ".":
            j += 1
            if toks[j].k == TK_ID:
                j += 1
            else:
                return False
        if toks[j].k == TK_PUNCT and toks[j].s == "[":
            d = 0
            while j < len(toks):
                if toks[j].k == TK_PUNCT and toks[j].s == "[":
                    d += 1
                elif toks[j].k == TK_PUNCT and toks[j].s == "]":
                    d -= 1
                    if d == 0:
                        j += 1
                        break
                j += 1
            continue
        break
    return toks[j].k == TK_PUNCT and toks[j].s in ASSIGN_OPS


class Parser(object):
    def __init__(self, toks):
        self.t = toks
        self.p = 0

    def cur(self):
        return self.t[self.p]

    def isP(self, s):
        return self.cur().k == TK_PUNCT and self.cur().s == s

    def isK(self, s):
        return self.cur().k == TK_ID and self.cur().s == s

    def adv(self):
        if self.p + 1 < len(self.t):
            self.p += 1

    def skip_nl(self):
        while self.cur().k == TK_NL:
            self.adv()

    def want(self, s):
        global g_line
        if not self.isP(s):
            g_line = self.cur().line
            bail("missing '%s'" % s, "expected '%s' but got '%s'" % (s, self.cur().s))
        self.adv()

    def expr(self, minp=0):
        lhs = self.unary()
        while True:
            t = self.cur()
            isop = (t.k == TK_PUNCT) or (t.k == TK_ID and t.s in ("mod", "div"))
            if isop and t.s in BIN_PREC and BIN_PREC[t.s] >= minp:
                op = t.s
                prec = BIN_PREC[op]
                global g_line
                g_line = t.line
                self.adv()
                rhs = self.expr(prec + 1)
                lhs = ("logic", op, lhs, rhs) if op in ("&&", "||") else ("bin", op, lhs, rhs)
                continue
            break
        if minp == 0 and self.isP("?"):
            self.adv()
            self.skip_nl()
            a1 = self.expr()
            self.skip_nl()
            self.want(":")
            self.skip_nl()
            b1 = self.expr()
            return ("tern", lhs, a1, b1)
        return lhs

    def unary(self):
        if self.isP("!"):
            self.adv()
            return ("un", "!", self.unary())
        if self.isP("-"):
            self.adv()
            return ("un", "-", self.unary())
        if self.isP("~"):
            self.adv()
            return ("un", "~", self.unary())
        return self.postfix(self.primary())

    def postfix(self, base):
        while True:
            if self.isP("."):
                self.adv()
                if self.cur().k != TK_ID:
                    global g_line
                    g_line = self.cur().line
                    bail("'.' not followed by a name", "property name must be an identifier")
                nm = self.cur().s
                self.adv()
                base = ("member", base, nm)
                continue
            if self.isP("["):
                self.adv()
                kind = "index"
                if self.isP("|"):
                    kind = "lidx"
                    self.adv()
                elif self.isP("?"):
                    kind = "midx"
                    self.adv()
                elif self.isP("$"):
                    kind = "sidx"
                    self.adv()
                self.skip_nl()
                idx = self.expr()
                self.skip_nl()
                self.want("]")
                base = (kind, base, idx)
                continue
            return base

    def tmpl_parts(self, raw):
        """$"a{expr}b" 的内容拆成 [("str","a"), 表达式, ("str","b")]"""
        parts = []
        cur = ""
        i = 0
        n = len(raw)
        while i < n:
            c = raw[i]
            if c != "{":
                cur += c
                i += 1
                continue
            j = i + 1
            depth = 1
            while j < n and depth:
                if raw[j] == '"':
                    j += 1
                    while j < n and raw[j] != '"':
                        if raw[j] == "\\":
                            j += 1
                        j += 1
                elif raw[j] == "{":
                    depth += 1
                elif raw[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if cur:
                parts.append(("str", cur))
                cur = ""
            sub = Parser(lex(raw[i + 1:j]))
            sub.skip_nl()
            parts.append(sub.expr())
            i = j + 1
        if cur:
            parts.append(("str", cur))
        return parts

    def primary(self):
        global g_line
        g_line = self.cur().line
        t = self.cur()

        if t.k == TK_NUM:
            self.adv()
            if t.s == "color":
                return ("num", int(t.num))
            return ("float", t.num) if t.isf else ("num", int(t.num))

        if t.k == TK_STR:
            self.adv()
            return ("str", t.s)

        if t.k == TK_TMPL:
            self.adv()
            return self.postfix(("tmpl", self.tmpl_parts(t.s)))

        if self.isP("("):
            self.adv()
            self.skip_nl()
            e = self.expr()
            self.skip_nl()
            self.want(")")
            return self.postfix(e)

        if self.isP("["):
            self.adv()
            elems = []
            self.skip_nl()
            if not self.isP("]"):
                while True:
                    self.skip_nl()
                    elems.append(self.expr())
                    self.skip_nl()
                    if self.isP(","):
                        self.adv()
                        self.skip_nl()
                        if self.isP("]"):
                            break
                        continue
                    break
            self.want("]")
            return self.postfix(("arr", elems))

        if self.isP("{"):
            self.adv()
            pairs = []
            self.skip_nl()
            while not self.isP("}"):
                self.skip_nl()
                if self.cur().k in (TK_STR, TK_ID):
                    key = self.cur().s
                    self.adv()
                elif self.cur().k == TK_NUM:
                    key = str(int(self.cur().num))
                    self.adv()
                else:
                    bail("struct key must be a name or a string", "got '%s'" % self.cur().s)
                self.skip_nl()
                if not self.isP(":"):
                    bail("struct entry without ':'", "expected key : value")
                self.adv()
                self.skip_nl()
                pairs.append((key, self.expr()))
                self.skip_nl()
                if self.isP(","):
                    self.adv()
                    self.skip_nl()
            self.want("}")
            return self.postfix(("struct", pairs))

        if t.k == TK_ID:
            nm = t.s
            # 对象名（obj_xxx，且确实是工程里的对象资源）：一律当字符串用，
            # 和 with obj_xxx / VM_GetInstanceCount 那套一个口径。
            # 注意只认 .yyp 里真实存在的对象名 —— obj_type、obj_flame 这种变量名不在表里，照旧当变量。
            if nm in OBJECT_NAMES:
                self.adv()
                return self.postfix(("str", nm))
            # 枚举成员（BOSS_STATE.APPEAR 这种）：常量表里存的是全名，直接按全名查。
            # 注意不能先拿裸名 nm 去查 —— 枚举名本身不在表里，那样永远命中不了。
            n1 = self.t[self.p + 1] if self.p + 1 < len(self.t) else None
            n2 = self.t[self.p + 2] if self.p + 2 < len(self.t) else None
            if (n1 is not None and n1.k == TK_PUNCT and n1.s == "."
                    and n2 is not None and n2.k == TK_ID):
                full = nm + "." + n2.s
                if full in CONSTS:
                    self.adv(); self.adv(); self.adv()
                    return ("num", CONSTS[full])
            if nm in MACROS:
                self.adv()
                return ("num", MACROS[nm])
            self.adv()
            if nm == "true":
                return ("num", 1)
            if nm == "false":
                return ("num", 0)
            if nm == "noone":
                return ("num", -4)
            if nm == "undefined":
                return ("undef",)
            if nm in ("self", "id"):
                return self.postfix(("self",))
            if nm == "other":
                return self.postfix(("other",))
            if nm == "event_inherited" and self.isP("("):
                self.adv()
                if self.isP(")"):
                    self.adv()
                return ("num", 0)
            if nm == "event_user" and self.isP("("):
                self.adv()
                if self.cur().k != TK_NUM:
                    bail("event_user argument must be a number literal",
                         "got '%s'" % self.cur().s)
                k = int(self.cur().num)
                self.adv()
                self.want(")")
                return ("call", "__event_user", [("num", k)])
            if nm == "global" and self.isP("."):
                self.adv()
                prop = self.cur().s
                self.adv()
                return self.postfix(("global", prop))
            if self.isP("("):
                self.adv()
                self.skip_nl()
                args = []
                if not self.isP(")"):
                    while True:
                        self.skip_nl()
                        args.append(self.expr())
                        self.skip_nl()
                        if self.isP(","):
                            self.adv()
                            continue
                        break
                self.want(")")
                return self.postfix(("call", nm, args))
            return self.postfix(("id", nm))

        bail("unparsable expression start '%s'" % t.s, "this token cannot start an expression")

    def stmts(self, stop):
        out = []
        while True:
            self.skip_nl()
            if self.cur().k == TK_EOF or (stop and self.isP(stop)):
                return out
            out.append(self.stmt())

    def body(self):
        self.skip_nl()
        if self.isP("{"):
            self.adv()
            st = self.stmts("}")
            self.want("}")
            return ("block", st)
        return self.stmt()

    def end_stmt(self):
        global g_line
        if self.isP(";"):
            self.adv()
            self.skip_nl()
            return
        if self.cur().k == TK_NL:
            self.skip_nl()
            return
        # 语句后面紧跟一个裸块（`x = f() { ... }`）也算结束：块是**下一条语句**，
        # 交给 stmt_inner 的裸块分支去解析。GMS2 接受这种写法，块照常执行。
        #   （出处：obj_cateye_gem/Mouse_4.gml:23 那种"赋值后面多一个 {"）
        if self.isP("{") or self.isP("}") or self.cur().k == TK_EOF:
            return
        g_line = self.cur().line
        bail("trailing tokens after statement",
             "expected newline or ';' but got '%s'" % self.cur().s)

    def assign_or_expr(self):
        if is_assign_start(self.t, self.p):
            tgt = self.primary()
            op = self.cur().s
            self.adv()
            if op in ("++", "--"):
                return ("assign", tgt, op, None)
            val = self.expr()
            return ("assign", tgt, op, val)
        return ("expr", self.expr())

    def for_part(self):
        if self.isK("var"):
            self.adv()
            out = []
            while True:
                if self.cur().k != TK_ID:
                    bail("var not followed by a name", "a name is required")
                nm = self.cur().s
                self.adv()
                init = None
                if self.isP("="):
                    self.adv()
                    init = self.expr()
                out.append(("var", nm, init))
                if self.isP(","):
                    self.adv()
                    continue
                break
            return ("block", out)
        return self.assign_or_expr()
    def case_body(self):
        out = []
        while True:
            self.skip_nl()
            if (self.cur().k == TK_EOF or self.isP("}") or self.isK("case")
                    or self.isK("default")):
                return ("block", out)
            out.append(self.stmt())

    def stmt(self):
        global g_line
        g_line = self.cur().line
        start = g_line
        n = self.stmt_inner()
        if isinstance(n, tuple):
            STMT_LINE[id(n)] = (start, n)
        return n

    def stmt_inner(self):
        global g_line
        g_line = self.cur().line

        if self.isP(";"):
            self.adv()
            return ("block", [])

        # 裸块 `{ ... }` 当一条语句：GML 允许（不引入新作用域，块照常执行）。
        # 常见于"赋值后面直接跟一个块"，见 end_stmt 里的说明。
        if self.isP("{"):
            self.adv()
            st = self.stmts("}")
            self.want("}")
            return ("block", st)

        if self.isK("var"):
            self.adv()
            out = []
            while True:
                if self.cur().k != TK_ID:
                    bail("var not followed by a name", "a name is required")
                nm = self.cur().s
                self.adv()
                init = None
                if self.isP("="):
                    self.adv()
                    init = self.expr()
                out.append(("var", nm, init))
                if self.isP(","):
                    self.adv()
                    continue
                break
            self.end_stmt()
            return ("block", out)

        if self.isK("if"):
            self.adv()
            c = self.expr()
            th = self.body()
            self.skip_nl()
            el = None
            if self.isK("else"):
                self.adv()
                el = self.stmt() if self.isK("if") else self.body()
            return ("if", c, th, el)

        if self.isK("while"):
            self.adv()
            c = self.expr()
            return ("while", c, self.body())

        if self.isK("for"):
            self.adv()
            self.want("(")
            self.skip_nl()
            init = None if self.isP(";") else self.for_part()
            self.want(";")
            self.skip_nl()
            cond = None if self.isP(";") else self.expr()
            self.want(";")
            self.skip_nl()
            step = None if self.isP(")") else self.for_part()
            self.want(")")
            return ("for", init, cond, step, self.body())

        if self.isK("switch"):
            self.adv()
            subj = self.expr()
            self.skip_nl()
            self.want("{")
            cases = []
            default = None
            while True:
                self.skip_nl()
                if self.isP("}") or self.cur().k == TK_EOF:
                    break
                if self.isK("case"):
                    self.adv()
                    v = self.expr()
                    self.want(":")
                    cases.append((v, self.case_body()))
                    continue
                if self.isK("default"):
                    self.adv()
                    self.want(":")
                    default = self.case_body()
                    continue
                bail("switch body must be case/default", "got '%s'" % self.cur().s)
            self.want("}")
            return ("switch", subj, cases, default)

        if self.isK("with"):
            self.adv()
            if self.isP("("):
                self.adv()
                self.skip_nl()
                tgt = self.expr()
                self.skip_nl()
                self.want(")")
            else:
                tgt = self.expr()
            return ("with", tgt, self.body())

        if self.isK("function"):
            self.adv()
            if self.cur().k != TK_ID:
                bail("function without a name", "a name is required")
            nm = self.cur().s
            self.adv()
            self.want("(")
            params = []
            if not self.isP(")"):
                while True:
                    if self.cur().k != TK_ID:
                        bail("function parameter must be a name", "got '%s'" % self.cur().s)
                    params.append(self.cur().s)
                    self.adv()
                    if self.isP("="):
                        bail("function parameter with a default value", "not supported")
                    if self.isP(","):
                        self.adv()
                        continue
                    break
            self.want(")")
            return ("func", nm, params, self.body())

        if self.isK("try") or self.isK("catch"):
            bail("try/catch", "GML exceptions have no counterpart")

        if self.isK("exit") or self.isK("halt"):
            self.adv()
            self.end_stmt()
            return ("exit",)

        if self.isK("return"):
            self.adv()
            e = None if (self.cur().k == TK_NL or self.isP(";")) else self.expr()
            self.end_stmt()
            return ("return", e)

        if self.isK("break"):
            self.adv()
            self.end_stmt()
            return ("break",)

        if self.isK("continue"):
            self.adv()
            self.end_stmt()
            return ("continue",)

        if self.isK("else"):
            bail("stray else", "no matching if before it")

        if self.isP("{"):
            self.adv()
            st = self.stmts("}")
            self.want("}")
            return ("block", st)

        s = self.assign_or_expr()
        self.end_stmt()
        return s


# ============================================================
# 代码生成
# ============================================================
class Gen(object):
    def __init__(self):
        self.lits = []
        self.lit_index = {}
        self.pool = []
        self.pool_index = {}
        self.const_init = bytearray()
        self.block_no = 0      # 第几个块，用来给它分槽段
        self.block_bases = {}  # 第二遍：块号 → 段基址
        self.block_use = {}    # 第一遍：块号 → 用了多少槽
        # 槽位基址：普通 bin 是 字面量从 1、局部从 4096；
        #   callbin 是 槽0=返回值、槽1..argc=实参，所以字面量从 argc+1 起、局部紧跟着字面量
        self.lit_base = 1
        self.local_base = LOCAL_BASE
        # callbin 专用：参数名 → 实参槽（函数体里写参数名就是读这个槽，不走实例变量）
        self.param_slots = {}

    def note_block(self, blk):
        self.block_use[blk.no] = blk.next_slot - blk.slot_base

    def lit_int(self, v):
        k = "i:%d" % v
        if k not in self.lit_index:
            self.lits.append((MEM_INT, int(v), 0.0, ""))
            self.lit_index[k] = self.lit_base + len(self.lits) - 1
        return self.lit_index[k]

    def lit_float(self, v):
        k = "f:%.9g" % v
        if k not in self.lit_index:
            self.lits.append((MEM_FLOAT, 0, float(v), ""))
            self.lit_index[k] = self.lit_base + len(self.lits) - 1
        return self.lit_index[k]

    def lit_str(self, s):
        k = "s:" + s
        if k not in self.lit_index:
            self.lits.append((MEM_STRING, 0, 0.0, s))
            self.lit_index[k] = self.lit_base + len(self.lits) - 1
        return self.lit_index[k]

    def str_index(self, s):
        if s not in self.pool_index:
            self.pool.append(s)
            self.pool_index[s] = len(self.pool) - 1
        return self.pool_index[s]

    def build_const_init(self):
        # 字面量要落在 [lit_base, local_base) 里（最后一个字面量槽 = lit_base+个数-1）
        if len(self.lits) > self.local_base - self.lit_base:
            bail("too many literals (%d)" % len(self.lits),
                 "literal pool must stay below slot %d" % self.local_base,
                 "raise LOCAL_BASE / LOCAL_STRIDE or split the object")
        for i, (ty, iv, fv, sv) in enumerate(self.lits):
            b = self.const_init
            b.append(OP_ASSIGN)
            b += struct.pack("<i", i + self.lit_base)
            b.append(ty)
            if ty == MEM_STRING:
                b += struct.pack("<H", self.str_index(sv))
            elif ty == MEM_FLOAT:
                b += struct.pack("<f", fv)
            else:
                b += struct.pack("<i", iv)


class Block(object):
    def __init__(self, gen):
        self.code = bytearray()
        self.locals = {}
        # 段基址：第一遍先都从 LOCAL_BASE 起（只量大小），第二遍按量出来的大小分段
        self.slot_base = gen.block_bases.get(gen.block_no, gen.local_base)
        self.next_slot = self.slot_base
        self.slot_end = LOCAL_MAX
        self.no = gen.block_no
        gen.block_no += 1
        self.self_stack = []
        self.is_func = False
        self.is_collision = False   # 碰撞事件块：顶层 other = 碰到的那个实例
        self.other_slot = -1        # 碰撞对方的槽（取一次缓存）
        self.breaks = []
        self.continues = []


FUNC_BLOCKS = set()


class Cg(object):
    def __init__(self, gen, blk):
        self.g = gen
        self.b = blk

    def alloc(self):
        s = self.b.next_slot
        if s >= self.b.slot_end:
            bail("object uses too many slots (> %d)" % LOCAL_MAX,
                 "slot %d is past the memory end" % s)
        self.b.next_slot += 1
        return s

    def here(self):
        return len(self.b.code)

    def patch(self, pos, val):
        struct.pack_into("<i", self.b.code, pos, val)

    def call_op(self, opc, fid, args, dst):
        # 唯一的发射点：opc 只能是 OP_CALL（游戏函数）或 OP_NATIVE（内部函数）
        c = self.b.code
        c.append(opc)
        c += struct.pack("<H", fid)
        c.append(len(args))
        c += struct.pack("<i", dst)
        for a in args:
            c += struct.pack("<i", a)

    # 四个入口：机器码各自写死，没有开关，就不会出现"忘了传"的情况
    def game_call(self, fid, args, dst=None):          # 游戏函数，要返回值
        if dst is None:
            dst = self.alloc()
        self.call_op(OP_CALL, fid, args, dst)
        return dst

    def game_void(self, fid, args):                    # 游戏函数，无返回值
        self.call_op(OP_CALL, fid, args, -1)

    def native_call(self, fid, args, dst=None):        # 内部函数，要返回值
        if dst is None:
            dst = self.alloc()
        self.call_op(OP_NATIVE, fid, args, dst)
        return dst

    def native_void(self, fid, args):                  # 内部函数，无返回值
        self.call_op(OP_NATIVE, fid, args, -1)

    def op(self, opc, a, b, dst=None):
        if dst is None:
            dst = self.alloc()
        self.b.code.append(opc)
        self.b.code += struct.pack("<iii", dst, a, b)
        return dst

    def op_un(self, opc, a, dst=None):
        # 一元操作码：只有 [操作码][dst][a]
        if dst is None:
            dst = self.alloc()
        self.b.code.append(opc)
        self.b.code += struct.pack("<ii", dst, a)
        return dst

    def copy(self, src, dst=None):
        if dst is None:
            dst = self.alloc()
        self.b.code.append(OP_COPY)
        self.b.code += struct.pack("<ii", dst, src)
        return dst

    def set_int(self, dst, v):
        c = self.b.code
        c.append(OP_ASSIGN)
        c += struct.pack("<i", dst)
        c.append(MEM_INT)
        c += struct.pack("<i", v)

    def set_undef(self, dst=None):
        # 写一个 undefined 进槽（type = MEM_UNDEFINED）
        if dst is None:
            dst = self.alloc()
        c = self.b.code
        c.append(OP_ASSIGN)
        c += struct.pack("<i", dst)
        c.append(MEM_UNDEFINED)
        c += struct.pack("<i", 0)
        return dst

    def jif(self, cond, t, f):
        c = self.b.code
        c.append(OP_IF)
        c += struct.pack("<iii", cond, t, f)
        return len(c) - 8, len(c) - 4

    def jmp(self):
        c = self.b.code
        c.append(OP_JMP)
        c += struct.pack("<i", 0)
        return len(c) - 4

    def jmp_to(self, target):
        c = self.b.code
        c.append(OP_JMP)
        c += struct.pack("<i", target)

    def nat(self, name, args):
        if name not in NATIVE_ID:
            bail('function "%s" has no builtin' % name,
                 "not in gm2bin 的 NATIVE_IMPL 列表",
                 "加到 NATIVE_IMPL 末尾，并在 C++ 侧同位置补上实现")
        return self.native_call(NATIVE_ID[name], args)

    def nat_void(self, name, args):
        if name not in NATIVE_ID:
            bail('function "%s" has no builtin' % name, "not in gm2bin 的 NATIVE_IMPL 列表")
        self.native_void(NATIVE_ID[name], args)

    def self_slot(self):
        if not self.b.self_stack:
            self.b.self_stack.append(self.game_call(FID_GETCUR, []))
        return self.b.self_stack[-1]

    def get_prop(self, inst, name):
        return self.game_call(FID_GETPROP, [inst, self.g.lit_str(name)])

    def set_prop(self, inst, name, val):
        self.game_void(FID_SETPROP, [inst, self.g.lit_str(name), val])

    def ex(self, n):
        k = n[0]
        if k == "num":
            return self.g.lit_int(n[1])
        if k == "float":
            return self.g.lit_float(n[1])
        if k == "str":
            return self.g.lit_str(n[1])
        if k == "undef":
            return self.set_undef()
        if k == "self":
            return self.self_slot()
        if k == "other":
            if len(self.b.self_stack) >= 2:
                return self.b.self_stack[-2]
            # 碰撞事件（_OBJECT_COLLISION_*）顶层：GML 里 other = 碰到的那个实例，
            # 由运行时在派发这个块时告诉 VM（collision_other 去取）
            if self.b.is_collision:
                return self.collision_other()
            bail("other outside a with", "no outer instance to refer to")
        if k == "id":
            nm = n[1]
            if nm in self.b.locals:
                return self.b.locals[nm]
            if nm in self.g.param_slots:       # callbin：参数就是那个实参槽
                return self.g.param_slots[nm]
            return self.get_prop(self.self_slot(), nm)
        if k == "global":
            return self.get_prop(0, n[1])
        if k == "member":
            return self.get_prop(self.ex(n[1]), n[2])
        if k in ("index", "lidx"):
            base = self.inst_array_ref(n[1])
            if base is not None:
                return self.game_call(GAME_ID["VM_InstArrayItem"],
                                      [base[0], self.g.lit_str(base[1]), self.ex(n[2])])
            return self.nat("array_get", [self.ex(n[1]), self.ex(n[2])])
        if k == "midx":
            return self.nat("ds_map_find_value", [self.ex(n[1]), self.ex(n[2])])
        if k == "sidx":
            return self.nat("struct_get", [self.ex(n[1]), self.ex(n[2])])
        if k == "arr":
            # 先建空的外层，再把元素逐个加进去
            a = self.nat("array_literal", [])
            for e in n[1]:
                self.nat_void("array_push", [a, self.ex(e)])
            return a
        if k == "tmpl":
            # $"a{expr}b" → "a" + string(expr) + "b"
            res = None
            for p in n[1]:
                if p[0] == "str":
                    piece = self.g.lit_str(p[1])
                else:
                    piece = self.nat("string", [self.ex(p)])
                res = piece if res is None else self.op(OP_ADD, res, piece)
            return res if res is not None else self.g.lit_str("")
        if k == "struct":
            m = self.nat("ds_map_create", [])
            for key, val in n[1]:
                self.nat_void("ds_map_set", [m, self.g.lit_str(key), self.ex(val)])
            return m
        if k == "un":
            if n[1] == "!":
                return self.op(OP_EQ, self.ex(n[2]), self.g.lit_int(0))
            if n[1] == "~":
                return self.op_un(OP_BNOT, self.ex(n[2]))
            return self.op(OP_SUB, self.g.lit_int(0), self.ex(n[2]))
        if k == "bin":
            if n[1] in ("mod", "%"):
                # mod 只接整数：字面量是浮点直接报停（变量靠解释器运行时拦）
                for side in (n[2], n[3]):
                    if side[0] == "float":
                        bail("mod with a float literal",
                             "%s mod %s" % (self._ftxt(n[2]), self._ftxt(n[3])),
                             "fix the GML: both operands of mod must be integers")
            return self.op(BIN_OP[n[1]], self.ex(n[2]), self.ex(n[3]))
        if k == "logic":
            res = self.alloc()
            if n[1] == "&&":
                self.set_int(res, 0)
                _, f1 = self.jif(self.ex(n[2]), -1, 0)
                _, f2 = self.jif(self.ex(n[3]), -1, 0)
                self.set_int(res, 1)
                end = self.here()
                self.patch(f1, end)
                self.patch(f2, end)
            else:
                self.set_int(res, 1)
                t1, _ = self.jif(self.ex(n[2]), 0, -1)
                t2, _ = self.jif(self.ex(n[3]), 0, -1)
                self.set_int(res, 0)
                end = self.here()
                self.patch(t1, end)
                self.patch(t2, end)
            return res
        if k == "tern":
            res = self.alloc()
            _, f = self.jif(self.ex(n[1]), -1, 0)
            self.copy(self.ex(n[2]), res)
            j = self.jmp()
            self.patch(f, self.here())
            self.copy(self.ex(n[3]), res)
            self.patch(j, self.here())
            return res
        if k == "call":
            nm = n[1]
            if nm == "__run_block":
                return self.nat("run_block", [self.ex(a) for a in n[2]])
            if nm == "__event_user":
                # with 别人的块里 → 发给那一格实例；否则就是发给自己
                n_ev = n[2][0][1]
                if len(self.b.self_stack) >= 2:
                    return self.nat("run_event_user",
                                    [self.b.self_stack[-1], self.g.lit_int(n_ev)])
                return self.nat("run_block",
                                [self.g.lit_str("_OBJECT_USER_EVENT_%d" % n_ev)])
            if nm in FUNC_BLOCKS:
                self.nat_void("run_block", [self.g.lit_str("_FUNC_" + nm)])
                return self.copy(RET_SLOT)
            # ── callbin（外部函数）：编成 VM_CallFunc("名字", 实参…) ──
            #   名字来自 CALLBIN_NAMES（扫 --callbins 目录下的 .callbin）
            if nm in CALLBIN_NAMES:
                return self.game_call(GAME_ID["VM_CallFunc"],
                                      [self.g.lit_str(nm)] + [self.ex(a) for a in n[2]])
            # ── 数组函数：目标是"实例/全局变量上的数组"就走实例那套 ──
            if nm in ARRAY_FNS and n[2]:
                base = self.inst_array_ref(n[2][0])
                if base is not None:
                    return self.inst_array_fn(nm, base, n[2][1:])
            if nm in GAME_ID:
                return self.game_call(GAME_ID[nm], [self.ex(a) for a in n[2]])
            return self.nat(nm, [self.ex(a) for a in n[2]])
        bail("unhandled expression '%s'" % k, "codegen gap")

    def _ftxt(self, node):
        """报错时把操作数写成人能看的（只用于拼消息）"""
        if node[0] in ("num", "float"):
            return str(node[1])
        if node[0] == "str":
            return '"%s"' % node[1]
        if node[0] == "id":
            return node[1]
        return node[0]

    def collision_other(self):
        # 碰撞对方：一个块里取一次就够（跟 self_slot 一个套路）
        if self.b.other_slot < 0:
            self.b.other_slot = self.nat("collision_other", [])
        return self.b.other_slot

    # ── 实例数组（VM_InstArray* / inst_array_*）──
    def ia(self, name, args, want_value=True):
        """实例数组那批游戏 API：按名字取 id 再发调用"""
        fid = GAME_ID[name]
        return self.game_call(fid, args) if want_value else self.game_void(fid, args)

    def inst_array_ref(self, node):
        """这个表达式是"实例/全局变量上的数组"就返回 (实例槽, 变量名)；是局部或别的就返回 None。

        判据只看语法（局部变量不在 locals 里、不是局部就走实例）—— 静态没有类型信息，
        数组函数/下标访问这些"一看就是数组"的位置才调它。
        """
        k = node[0]
        if k == "id":
            if node[1] in self.b.locals:
                return None                       # 局部 → 还是走 VM 的池
            return (self.self_slot(), node[1])
        if k == "global":
            return (self.g.lit_int(0), node[1])
        if k == "member":
            return (self.ex(node[1]), node[2])
        return None

    def inst_array_fn(self, name, base, rest):
        """数组函数在实例数组上的写法（rest 是实参 AST，不含数组本身）"""
        inst = base[0]
        ins  = self.g.lit_str(base[1])
        a0 = [inst, ins]
        if name == "array_length":
            return self.game_call(GAME_ID["VM_InstArraySize"], a0)
        if name == "array_clear":
            self.ia("VM_InstArrayClear", a0, False)
            return self.g.lit_int(0)
        if name == "array_contains":
            return self.game_call(GAME_ID["VM_InstArrayContains"], a0 + [self.ex(rest[0])])
        if name == "array_push":
            dst = None                          # 可以一次推多个
            for e in rest:
                dst = self.game_call(GAME_ID["VM_InstArrayAdd"], a0 + [self.ex(e)])
            return dst if dst is not None else self.g.lit_int(0)
        if name == "array_get_index":
            return self.nat("inst_array_index", a0 + [self.ex(rest[0])])
        if name == "array_delete":
            n = self.ex(rest[1]) if len(rest) > 1 else self.g.lit_int(1)
            return self.nat("inst_array_delete", a0 + [self.ex(rest[0]), n])
        if name == "array_deep_copy":
            # 读进池里就是一份拷贝；要放回实例变量由赋值那边用 inst_array_assign 接
            return self.nat("inst_array_read", a0)
        if name == "array_pop":
            return self.nat("inst_array_pop", a0)
        if name == "array_resize":
            return self.nat("inst_array_resize", a0 + [self.ex(rest[0])])
        if name == "array_shuffle":
            return self.nat("inst_array_shuffle", a0)
        bail("%s on an instance array" % name,
             "no instance-array equivalent",
             "hand-edit the GML or add an API")
        return None


    def store(self, tgt, val):
        k = tgt[0]
        if k == "id":
            nm = tgt[1]
            if nm in self.b.locals:
                self.copy(val, self.b.locals[nm])
            else:
                self.set_prop(self.self_slot(), nm, val)
            return
        if k == "global":
            self.set_prop(0, tgt[1], val)
            return
        if k == "member":
            self.set_prop(self.ex(tgt[1]), tgt[2], val)
            return
        if k in ("index", "lidx"):
            base = self.inst_array_ref(tgt[1])
            if base is not None:
                self.ia("VM_InstArraySet",
                        [base[0], self.g.lit_str(base[1]), self.ex(tgt[2]), val], False)
                return
            self.nat_void("array_set", [self.ex(tgt[1]), self.ex(tgt[2]), val])
            return
        if k == "midx":
            self.nat_void("ds_map_set", [self.ex(tgt[1]), self.ex(tgt[2]), val])
            return
        if k == "sidx":
            self.nat_void("struct_set", [self.ex(tgt[1]), self.ex(tgt[2]), val])
            return
        bail("cannot assign to '%s'" % k, "not an lvalue")

    def st(self, n):
        global g_line
        ent = STMT_LINE.get(id(n))
        if ent is not None:
            g_line = ent[0]
        k = n[0]

        if k == "block":
            for s in n[1]:
                self.st(s)
            return

        if k == "expr":
            self.ex(n[1])
            return

        if k == "var":
            slot = self.alloc()
            self.b.locals[n[1]] = slot
            if n[2] is not None:
                self.copy(self.ex(n[2]), slot)
            return

        if k == "assign":
            tgt, op, val = n[1], n[2], n[3]
            # 数组字面量赋给实例/全局变量 → 走实例数组：先建空的，再逐个加进去
            # （GML 里 x = [a,b] 就是把这个实例变量变成数组）
            if op == "=" and val is not None and val[0] == "arr":
                base = self.inst_array_ref(tgt)
                if base is not None:
                    inst, ins = base[0], self.g.lit_str(base[1])
                    self.ia("VM_InstArrayClear", [inst, ins], False)
                    for e in val[1]:
                        self.ia("VM_InstArrayAdd", [inst, ins, self.ex(e)], False)
                    return
            # x = array_create(n [,v])：目标是实例/全局变量 → 走实例数组的建
            if (op == "=" and val is not None and val[0] == "call"
                    and val[1] == "array_create" and val[2]):
                base = self.inst_array_ref(tgt)
                if base is not None:
                    args = [base[0], self.g.lit_str(base[1]), self.ex(val[2][0])]
                    if len(val[2]) > 1:
                        args.append(self.ex(val[2][1]))
                    self.nat("inst_array_create", args)
                    return
            if op == "++" or op == "--":
                cur = self.ex(tgt)
                v = self.op(OP_ADD if op == "++" else OP_SUB, cur, self.g.lit_int(1))
            elif op == "=":
                v = self.ex(val)
            else:
                cur = self.ex(tgt)
                v = self.op(BIN_OP[op[:-1]], cur, self.ex(val))
            self.store(tgt, v)
            return

        if k == "if":
            _, f = self.jif(self.ex(n[1]), -1, 0)
            self.st(n[2])
            if n[3] is not None:
                j = self.jmp()
                self.patch(f, self.here())
                self.st(n[3])
                self.patch(j, self.here())
            else:
                self.patch(f, self.here())
            return

        if k == "while":
            head = self.here()
            _, f = self.jif(self.ex(n[1]), -1, 0)
            self.b.breaks.append([])
            self.b.continues.append([])
            self.st(n[2])
            self.jmp_to(head)
            end = self.here()
            self.patch(f, end)
            for p in self.b.breaks.pop():
                self.patch(p, end)
            for p in self.b.continues.pop():
                self.patch(p, head)
            return

        if k == "for":
            if n[1] is not None:
                self.st(n[1])
            head = self.here()
            f = None
            if n[2] is not None:
                _, f = self.jif(self.ex(n[2]), -1, 0)
            self.b.breaks.append([])
            self.b.continues.append([])
            self.st(n[4])
            step_pos = self.here()
            if n[3] is not None:
                self.st(n[3])
            self.jmp_to(head)
            end = self.here()
            if f is not None:
                self.patch(f, end)
            for p in self.b.breaks.pop():
                self.patch(p, end)
            for p in self.b.continues.pop():
                self.patch(p, step_pos)
            return

        if k == "switch":
            subj = self.ex(n[1])
            self.b.breaks.append([])
            fall = []
            for cv, _b in n[2]:
                cond = self.op(OP_EQ, subj, self.ex(cv))
                _, f = self.jif(cond, -1, 0)
                fall.append(f)
            dj = self.jmp()
            for i, (_cv, body) in enumerate(n[2]):
                self.patch(fall[i], self.here())
                self.st(body)
            self.patch(dj, self.here())
            if n[3] is not None:
                self.st(n[3])
            end = self.here()
            for p in self.b.breaks.pop():
                self.patch(p, end)
            return

        if k == "with":
            tgt = n[1]
            # 先把"当前实例"这一格压进栈：体里用 other 指的就是它（GML 里 other = 跑 with 的实例）
            self.self_slot()
            if (tgt[0] == "id" and tgt[1].startswith("obj_")) or tgt[0] == "str":
                oname = self.g.lit_str(tgt[1])
                kk = self.alloc()
                self.set_int(kk, 0)
                head = self.here()
                # 每轮重新取实例数：体里可能销毁实例，计数会变
                cnt = self.game_call(GAME_ID["VM_GetInstanceCount"], [oname])
                cond = self.op(OP_LT, kk, cnt)
                _, f = self.jif(cond, -1, 0)
                inst = self.game_call(GAME_ID["VM_GetInstanceAt"], [oname, kk])
                self.b.breaks.append([])
                self.b.continues.append([])
                self.b.self_stack.append(inst)
                self.st(n[2])
                self.b.self_stack.pop()
                cont = self.here()
                # 当前这格还活着才往下挪一格；被销毁了说明后面那个顶上来了，不能挪
                alive = self.nat("instance_exists", [inst])
                _, nf = self.jif(alive, -1, 0)
                self.op(OP_ADD, kk, self.g.lit_int(1), kk)
                self.patch(nf, self.here())
                self.jmp_to(head)
                end = self.here()
                self.patch(f, end)
                for p in self.b.breaks.pop():
                    self.patch(p, end)
                for p in self.b.continues.pop():
                    self.patch(p, cont)
                return
            inst = self.ex(tgt)
            self.b.breaks.append([])
            self.b.continues.append([])
            self.b.self_stack.append(inst)
            self.st(n[2])
            self.b.self_stack.pop()
            end = self.here()
            for p in self.b.breaks.pop():
                self.patch(p, end)
            for p in self.b.continues.pop():
                self.patch(p, end)
            return

        if k == "break":
            if not self.b.breaks:
                bail("break outside a loop", "no enclosing loop or switch")
            self.b.breaks[-1].append(self.jmp())
            return

        if k == "continue":
            if not self.b.continues:
                bail("continue outside a loop", "no enclosing loop")
            self.b.continues[-1].append(self.jmp())
            return

        if k == "exit":
            self.b.code.append(OP_HALT)
            return

        if k == "return":
            # 事件顶层（不是 function 里）的 return：GML 里就是"结束这个事件"，
            # 跟 exit 同一个意思 → 直接 HALT。带返回值的 return 只可能出现在函数里，
            # 事件里出现就是没写清楚，照规矩停住。
            #   （出处：obj_bomb_gem/Mouse_4.gml:6、obj_starlight_gem/Mouse_4.gml:6）
            if not self.b.is_func:
                if n[1] is not None:
                    bail("return with a value in an event",
                         "an event can only use a bare 'return' (it means 'exit')",
                         "drop the value, or move the code into a function")
                self.b.code.append(OP_HALT)
                return
            if n[1] is not None:
                self.copy(self.ex(n[1]), RET_SLOT)
            self.b.code.append(OP_HALT)
            return

        if k == "func":
            bail("nested function definition", "functions must be at the top level")

        bail("unhandled statement '%s'" % k, "codegen gap")


# ============================================================
# 事件文件 
# ============================================================
def event_to_block(fname):
    if fname == "Create_0.gml":
        return "_OBJECT_CREATE"
    if fname == "Step_0.gml":
        return "_OBJECT_STEP"
    if fname == "Step_2.gml":
        return "_OBJECT_STEP_END"
    if fname in ("Destroy_0.gml", "CleanUp_0.gml"):
        return "_OBJECT_DESTROY"
    if fname == "Draw_0.gml":
        return "_OBJECT_DRAW"
    if fname.startswith("Other_"):
        try:
            nn = int(fname[6:].split(".")[0])
        except ValueError:
            nn = -1
        if nn >= 10:
            return "_OBJECT_USER_EVENT_%d" % (nn - 10)
        if nn >= 0:
            return "_OBJECT_OTHER_%d" % nn          # 0..9：Room Start/End、动画结束…
    # 鼠标事件：平台派发用的是语义名（宿主对象里就是这几个字符串），要跟它对上
    if fname.endswith(".gml") and fname.startswith("Mouse_"):
        tail = fname[len("Mouse_"):-len(".gml")]
        for num, bname in (("4", "_OBJECT_CLICK"),
                           ("10", "_OBJECT_MOUSE_ENTER"),
                           ("11", "_OBJECT_MOUSE_LEAVE")):
            if tail == num:
                return bname
        if tail:
            return "_OBJECT_MOUSE_" + tail
    # 闹钟 / GUI 绘制 / 按键
    for pre, tag in (("Alarm_", "_OBJECT_ALARM_"), ("Draw_", "_OBJECT_DRAW_"),
                     ("KeyPress_", "_OBJECT_KEYPRESS_"),
                     ("KeyRelease_", "_OBJECT_KEYRELEASE_")):
        if fname.startswith(pre) and fname.endswith(".gml"):
            tail = fname[len(pre):-len(".gml")]
            if tail:
                return tag + tail
    # 碰撞事件：Collision_<对方对象名>.gml → _OBJECT_COLLISION_<对方对象名>
    # 前缀判定是不是碰撞，后缀就是和哪个对象碰
    if fname.startswith("Collision_") and fname.endswith(".gml"):
        other = fname[len("Collision_"):-len(".gml")]
        if other:
            return "_OBJECT_COLLISION_" + other
    return ""


EVENT_FILES = ["Create_0.gml", "Step_0.gml", "Step_2.gml", "Destroy_0.gml",
               "CleanUp_0.gml", "Draw_0.gml"] + ["Other_%d.gml" % i for i in range(10, 26)]


def event_files_in(dirpath):
    """这个目录下要转的事件文件：固定名（按 EVENT_FILES 顺序）+ 其它能映射成块的"""
    out = []
    taken = set()
    for e in EVENT_FILES:
        f = os.path.join(dirpath, e)
        if os.path.isfile(f):
            out.append(f)
            taken.add(e)
    try:
        names = sorted(os.listdir(dirpath))
    except OSError:
        names = []
    for e in names:
        if e in taken or not e.endswith(".gml"):
            continue
        f = os.path.join(dirpath, e)
        if os.path.isfile(f) and event_to_block(e):
            out.append(f)
    return out


def compile_file(path, gen):
    global g_file, g_line
    fname = os.path.basename(path)
    bname = event_to_block(fname)
    if not bname:
        g_file, g_line = path, 0
        bail("event file %s" % fname, "no block mapping for this event")

    with open(path, "rb") as f:
        src = f.read().decode("utf-8", "replace")

    g_file, g_line = path, 1
    p = Parser(lex(src))
    p.skip_nl()
    stmts = []
    while p.cur().k != TK_EOF:
        stmts.append(p.stmt())
        p.skip_nl()

    out = []
    top = []
    for s in stmts:
        if s[0] == "func":
            if s[2]:
                g_line = 0
                bail("function '%s' has parameters" % s[1],
                     "parameter passing is not supported",
                     "fix the GML: drop the parameters (pass values via instance variables) "
                     "or inline the body at the call site")
            FUNC_BLOCKS.add(s[1])
            fb = Block(gen)
            fb.is_func = True
            Cg(gen, fb).st(s[3])
            fb.code.append(OP_HALT)
            gen.note_block(fb)
            out.append(("_FUNC_" + s[1], fb.code))
        else:
            top.append(s)

    blk = Block(gen)
    blk.is_collision = bname.startswith("_OBJECT_COLLISION_")
    Cg(gen, blk).st(("block", top))
    gen.note_block(blk)
    out.insert(0, (bname, blk.code))
    return out


def compile_object(files):
    """编一个对象的全部事件文件：第一遍只量每块用多少槽，第二遍用真实段基址重编。

    槽位是编进字节码里的常量，所以必须先知道每块多大才能给它们分段；
    分段是必须的：run_block 调进来时调用方的局部槽还活着，共用一段会互相踩。
    """
    global FUNC_BLOCKS
    FUNC_BLOCKS = set()
    g1 = Gen()
    for f in files:
        compile_file(f, g1)

    bases = {}
    cur = g1.local_base
    for no in sorted(g1.block_use):
        bases[no] = cur
        cur += g1.block_use[no]
    if cur > LOCAL_MAX:
        g_file, g_line = files[-1], 0
        bail("object needs %d slots for locals" % cur,
             "local slots must stay below %d" % LOCAL_MAX,
             "raise MEM_SIZE in the interpreter / split the object")

    FUNC_BLOCKS = set()
    g2 = Gen()
    g2.block_bases = bases
    blocks = []
    for f in files:
        blocks.extend(compile_file(f, g2))
    return g2, blocks


def _bin_body_bytes(gen, blocks):
    """池 + 块的字节（普通 bin 和 callbin 共用的那半截）"""
    gen.build_const_init()
    out_blocks = [("_VM_CONST_INIT", gen.const_init)] + blocks
    seen = set()
    uniq = []
    for nm, code in out_blocks:
        if nm in seen:
            continue
        seen.add(nm)
        uniq.append((nm, code))
    for ty, iv, fv, sv in gen.lits:
        if ty == MEM_STRING:
            gen.str_index(sv)
    for nm, _ in uniq:
        gen.str_index(nm)
    out = bytearray()
    out += struct.pack("<i", len(gen.pool))
    for s in gen.pool:
        b = s.encode("utf-8")
        out += struct.pack("<i", len(b)) + b
    for nm, code in uniq:
        out += struct.pack("<i", len(code))
        out += struct.pack("<i", gen.str_index(nm))
        out += bytes(code)
    return bytes(out), len(uniq), len(gen.pool), len(gen.lits)


def emit_bin(out_path, gen, blocks):
    data, nb, ns, nl = _bin_body_bytes(gen, blocks)
    d = os.path.dirname(out_path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out_path, "wb") as fp:
        fp.write(data)
    return nb, ns, nl


# ============================================================
# callbin 模式：一个 .gml 里**只有一个函数** → 一个 .callbin
#   命令行：gm2bin.py --callbin <函数.gml> <输出.callbin>
#   槽布局（只这个模式用）：槽0 = 返回值，槽1..argc = 实参，
#                            字面量从 argc+1 起，局部紧跟着字面量
#   函数体里写参数名 = 读那个实参槽；`return x` 就是写槽 0（跟普通本地函数一个约定）
# ============================================================
CALLBIN_MAGIC = 0x4E424C43              # 'C''L''B''N'（小端读）
CALLBIN_ENTRY = "_CALL_ENTRY"

CALLBIN_NAMES = set()                   # 已知的 callbin 函数名（调用点按它编成 VM_CallFunc）


def callbin_scan_dir(dirpath):
    """扫一个目录下的 *.callbin，把函数名（文件名去掉后缀）收进 CALLBIN_NAMES"""
    CALLBIN_NAMES.clear()
    if not dirpath or not os.path.isdir(dirpath):
        return 0
    for e in sorted(os.listdir(dirpath)):
        if e.endswith(".callbin"):
            CALLBIN_NAMES.add(e[:-len(".callbin")])
    return len(CALLBIN_NAMES)


def callbin_desc_of(src):
    """说明串：文件里第一处 `// @desc xxx` 后面那段"""
    for line in src.splitlines():
        s = line.strip()
        if s.startswith("//"):
            m = re.search(r"@desc\s*(.+)$", s)
            if m:
                return m.group(1).strip()
            continue
        if s == "":
            continue
        break
    return ""


def compile_callbin(path):
    """编一个"只有一个函数"的 .gml → (gen, blocks, 函数名, argc, slots)"""
    global FUNC_BLOCKS, g_file, g_line
    FUNC_BLOCKS = set()
    g_file, g_line = path, 1
    with open(path, "rb") as f:
        src = f.read().decode("utf-8", "replace")

    p = Parser(lex(src))
    p.skip_nl()
    stmts = []
    while p.cur().k != TK_EOF:
        stmts.append(p.stmt())
        p.skip_nl()

    funcs  = [s for s in stmts if s[0] == "func"]
    others = [s for s in stmts if s[0] != "func"]
    if len(funcs) != 1 or others:
        g_line = 0
        bail("callbin 只能编「一个文件一个函数」",
             "这个文件里有 %d 个 function、%d 条别的顶层语句" % (len(funcs), len(others)),
             "一个 .gml 只留一个 function，别的挪出去（callbin 不支持文件里还有别的东西）")
    name, params, body = funcs[0][1], (funcs[0][2] or []), funcs[0][3]
    argc = len(params)
    if argc > 32:
        bail("参数太多（%d 个）" % argc, "跑起来最多收 32 个实参", "拆成两个函数")
    # 函数名必须跟文件名一样：名字就是字典 key，也是调用点名 —— 这样递归/互相调用能自己认出来
    stem = os.path.basename(path)
    if stem.lower().endswith(".gml"):
        stem = stem[:-4]
    if name != stem:
        g_line = 0
        bail("函数名跟文件名不一致（function %s 在 %s 里）" % (name, os.path.basename(path)),
             "一个文件一个函数，名字要能对上",
             "把函数改名成 %s，或者把文件名改成 %s.gml" % (stem, name))
    CALLBIN_NAMES.add(name)                 # 自己也算已知函数（递归、互相调用靠它）

    def build(gen):
        blk = Block(gen)
        blk.is_func = True                      # 允许 return（写槽 0）
        Cg(gen, blk).st(body)                   # body 本身就是 ('block', [...])，别再套一层
        blk.code.append(OP_HALT)
        gen.note_block(blk)
        return blk

    # 第一遍：量字面量个数 + 这个块用多少槽
    g1 = Gen()
    g1.lit_base = argc + 1
    g1.param_slots = dict((nm, 1 + i) for i, nm in enumerate(params))
    b1 = build(g1)

    local_base = argc + 1 + len(g1.lits)        # 局部紧跟着字面量
    # 第二遍：按真实基址重编
    FUNC_BLOCKS = set()
    g2 = Gen()
    g2.lit_base = argc + 1
    g2.local_base = local_base
    g2.block_bases = {b1.no: local_base}
    g2.param_slots = dict((nm, 1 + i) for i, nm in enumerate(params))
    b2 = build(g2)

    slots = local_base + g2.block_use[b2.no]
    return g2, [(CALLBIN_ENTRY, b2.code)], name, argc, slots


def emit_callbin(out_path, gen, blocks, name, desc, argc, slots):
    body, nb, ns, nl = _bin_body_bytes(gen, blocks)
    head = struct.pack("<iii", CALLBIN_MAGIC, slots, argc)
    for s in (name, desc):
        b = s.encode("utf-8")
        head += struct.pack("<i", len(b)) + b
    d = os.path.dirname(out_path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out_path, "wb") as fp:
        fp.write(head)
        fp.write(body)
    return nb, ns, nl


# ============================================================
# 扫描模式：不给参数就跑这个
#   对象按 .yy 里的文件夹归组：Cards / Enemy / Bullet / Map Object
#   bin 分别丢到 bin/<组>/ 下面，一个对象一个 .bin
#   结果一行一个对象记进 convert.log
# ============================================================
GROUP_DIRS = (("Cards", "card"), ("Enemy", "enemy"),
              ("Bullet", "bullet"), ("Map Object", "mapobj"),
              # Player / Effects 也是"内容"（卡牌生成出来的武器/宝石、战斗里生成的特效），
              # 不是 UI —— 按"直接 GML 转 bin、不搬 C++"的口径加进来
              ("Player", "player"), ("Effects", "effects"))

# 这几个是宿主 / 插件对象，不转（走 C++：hot/ 或 battle_kernel/）
SKIP_OBJECTS = {"obj_card_mod", "obj_card_slot", "obj_enemy_mod", "obj_bullet_mod",
                "obj_player_character", "obj_Bullet_Screen_Management",
                "obj_Homing_Bullet_Management",
                # 三个 mod 单位对象：跟上面三个 mod 对象同构（插件 VM_CreateInstance 建、
                # 再 VM_SetProp 写 mod_type），进 C++，不转 bin
                "obj_weapon_mod", "obj_gem_mod", "obj_effect_mod"}

# 白名单：写在这里的对象直接跳过，不转、也不算失败。一行一个名字，加逗号。
WHITELIST = (
    "obj_enemy_parent",
    "obj_card_parent",
    "obj_bullet_parent",
)


def object_group(obj_dir):
    name = os.path.basename(obj_dir.rstrip("/\\"))
    yy = os.path.join(obj_dir, name + ".yy")
    if not os.path.isfile(yy):
        return ""
    with open(yy, "rb") as f:
        src = f.read().decode("utf-8-sig", "replace")
    m = re.search(r'"parent"\s*:\s*\{(.*?)\}', src, re.S)
    if not m:
        return ""
    p = re.search(r'"path"\s*:\s*"([^"]+)"', m.group(1))
    if not p:
        return ""
    parts = p.group(1).split("/")
    if len(parts) < 3:
        return ""
    top = parts[2][:-3] if parts[2].endswith(".yy") else parts[2]
    for gname, gdir in GROUP_DIRS:
        if top == gname:
            return gdir
    return ""


def yyp_objects():
    # 对象清单 .yyp 为准（磁盘上可能有没注册进工程的目录，不转）
    yyp = os.path.abspath(os.path.join(HERE, "..", "FVM-reborn.yyp"))
    if not os.path.isfile(yyp):
        return []
    with open(yyp, "rb") as f:
        src = f.read().decode("utf-8-sig", "replace")
    seen = set()
    out = []
    for nm in re.findall(r'"path"\s*:\s*"objects/([^/"]+)/', src):
        if nm not in seen:
            seen.add(nm)
            out.append(nm)
    return sorted(out)


def scan_mode():
    root = os.path.abspath(os.path.join(HERE, "..", "objects"))
    out_root = os.path.join(HERE, "bin")
    if not os.path.isdir(root):
        print("no objects dir: %s" % root)
        return 1

    jobs = []
    skips = []
    for name in yyp_objects():
        p = os.path.join(root, name)
        if not os.path.isdir(p):
            continue
        g = object_group(p)
        if not g:
            continue
        if name in SKIP_OBJECTS or name in WHITELIST:
            skips.append((g, name))
            continue
        jobs.append((g, name, p))

    log = ["=" * 72,
           "%s  SCAN  %s -> %s" % (time.strftime("%Y-%m-%d %H:%M:%S"), root, out_root),
           "-" * 72]
    stat = dict((gdir, [0, 0, 0]) for _, gdir in GROUP_DIRS)   # [总数,   停]

    for g, name, path in jobs:
        stat[g][0] += 1
        files = event_files_in(path)
        if not files:
            log.append("%-6s %-34s NO EVENT FILES" % (g, name))
            continue

        gen = None
        blocks = []
        buf = io.StringIO()
        failed = False
        try:
            with contextlib.redirect_stdout(buf):
                gen, blocks = compile_object(files)
        except Stop:
            failed = True

        if failed:
            stat[g][2] += 1
            where = os.path.basename(LAST_BAIL[0])
            line = "%-6s %-34s STOP %s:%d  %s | %s" % (
                g, name, where, LAST_BAIL[1], LAST_BAIL[2], LAST_BAIL[3])
        else:
            out_path = os.path.join(out_root, g, name + ".bin")
            nb, ns, nl = emit_bin(out_path, gen, blocks)
            stat[g][1] += 1
            line = "%-6s %-34s OK   %d blocks / %d strings / %d literals" % (
                g, name, nb, ns, nl)
        log.append(line)
        print(line)

    log.append("-" * 72)
    for g, name in skips:
        log.append("%-6s %-34s SKIP (whitelist)" % (g, name))
    log.append("-" * 72)
    log.append("SUMMARY")
    tot = ot = st = 0
    for _, gdir in GROUP_DIRS:
        n, o, s = stat[gdir]
        tot += n
        ot += o
        st += s
        log.append("  %-6s : %4d object(s), ok %4d, stop %4d" % (gdir, n, o, s))
    log.append("  total  : %4d object(s), ok %4d, stop %4d, skip %d" % (tot, ot, st, len(skips)))
    dump_log(log)

    print("")
    for s in log[-len(GROUP_DIRS) - 2:]:
        print(s)
    print("log: %s" % LOG_PATH)
    return 0


def callbin_mode(in_path, out_path):
    """--callbin：一个 .gml（只有一个函数）→ 一个 .callbin"""
    log = ["=" * 64,
           "%s  CALLBIN  %s -> %s" % (time.strftime("%Y-%m-%d %H:%M:%S"), in_path, out_path)]
    try:
        gen, blocks, name, argc, slots = compile_callbin(in_path)
        src = open(in_path, "rb").read().decode("utf-8", "replace")
        desc = callbin_desc_of(src)
        nb, ns, nl = emit_callbin(out_path, gen, blocks, name, desc, argc, slots)
    except Stop:
        f_, l_, what, why, hint = LAST_BAIL
        log.append("  STOP   %s:%d" % (f_, l_))
        log.append("         FOUND: %s" % what)
        log.append("         WHY  : %s" % why)
        if hint:
            log.append("         FIX  : %s" % hint)
        log.append("  result : FAILED, %s not written" % out_path)
        dump_log(log)
        print("no output written.  (log: %s)" % LOG_PATH)
        return 2

    line = "  ok     %s  函数 %s，%d 个参数，一帧 %d 槽，%d 块 / %d 字符串 / %d 字面量" % (
        os.path.basename(in_path), name, argc, slots, nb, ns, nl)
    log.append(line)
    log.append("  result : OK, wrote %s" % out_path)
    dump_log(log)
    print(line)
    print("wrote %s" % out_path)
    return 0


def main():
    args = sys.argv[1:]
    if not args:
        return scan_mode()

    # --callbins <目录>：扫出已知的 callbin 函数名（影响所有调用点怎么编）；可以给多个
    rest = []
    dirs = []
    i = 0
    while i < len(args):
        if args[i] == "--callbins" and i + 1 < len(args):
            dirs.append(args[i + 1]); i += 2; continue
        rest.append(args[i]); i += 1
    for d in dirs:
        n = callbin_scan_dir(d)
        print("[callbin] %s：认得 %d 个函数%s" % (d, n, ("（" + ", ".join(sorted(CALLBIN_NAMES)) + "）") if n and n <= 12 else ""))

    if rest and rest[0] == "--callbin":
        if len(rest) < 3:
            print("usage: gm2bin.py --callbin <函数.gml> <out.callbin>")
            return 1
        return callbin_mode(rest[1], rest[2])

    args = rest
    if len(args) < 2:
        print("usage: gm2bin.py [<gml_dir_or_file> <out.bin>]  [--callbins <dir>]")
        print("       gm2bin.py --callbin <函数.gml> <out.callbin>")
        return 1

    in_path, out_path = args[0], args[1]

    files = []
    if os.path.isdir(in_path):
        files = event_files_in(in_path)
        if not files:
            print("input not found: %s" % in_path)
            return 1
    elif os.path.isfile(in_path):
        files.append(in_path)
    else:
        print("input not found: %s" % in_path)
        return 1

    log = ["=" * 64,
           "%s  %s -> %s" % (time.strftime("%Y-%m-%d %H:%M:%S"), in_path, out_path),
           "  input : %d file(s)" % len(files)]

    gen = None
    blocks = []
    done = []
    try:
        gen, blocks = compile_object(files)
        done = [os.path.basename(f) for f in files]
    except Stop:
        fname = os.path.basename(LAST_BAIL[0])
        for f in files:
            if os.path.basename(f) == fname:
                break
            done.append(os.path.basename(f))
        for f in done:
            log.append("  ok     %s" % f)
        f_, l_, what, why, hint = LAST_BAIL
        log.append("  STOP   %s:%d" % (f_, l_))
        log.append("         FOUND: %s" % what)
        log.append("         WHY  : %s" % why)
        if hint:
            log.append("         FIX  : %s" % hint)
        log.append("  result : FAILED, %s not written" % out_path)
        dump_log(log)
        print("no output written.  (log: %s)" % LOG_PATH)
        return 2

    gen.build_const_init()
    out_blocks = [("_VM_CONST_INIT", gen.const_init)] + blocks

    seen = set()
    uniq = []
    for nm, code in out_blocks:
        if nm in seen:
            continue
        seen.add(nm)
        uniq.append((nm, code))

    for ty, iv, fv, sv in gen.lits:
        if ty == MEM_STRING:
            gen.str_index(sv)
    for nm, _ in uniq:
        gen.str_index(nm)

    with open(out_path, "wb") as fp:
        fp.write(struct.pack("<i", len(gen.pool)))
        for s in gen.pool:
            b = s.encode("utf-8")
            fp.write(struct.pack("<i", len(b)))
            fp.write(b)
        for nm, code in uniq:
            fp.write(struct.pack("<i", len(code)))
            fp.write(struct.pack("<i", gen.str_index(nm)))
            fp.write(bytes(code))

    for f in done:
        log.append("  ok     %s" % f)
    log.append("  result : OK, %d blocks, %d strings, %d literals" %
               (len(uniq), len(gen.pool), len(gen.lits)))
    dump_log(log)

    print("wrote %s: %d blocks, %d strings, %d literals" %
          (out_path, len(uniq), len(gen.pool), len(gen.lits)))
    print("log: %s" % LOG_PATH)
    return 0


if __name__ == "__main__":
    sys.exit(main())
