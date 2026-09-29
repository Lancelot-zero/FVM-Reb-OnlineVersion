# -*- coding: utf-8 -*-
# 把 game_api.cpp 里那 150 条按"要什么"分组，写进文件注释块（两个 >>> GAME_GROUPS_ 标记之间）
#   名单真源是 core/const_str.h 的 GAME_FN_LIST
#   重新生成： python tools/gen_game_comment.py
import io, re

c   = io.open('DLL_VM/core/const_str.h', encoding='utf-8').read()
dst = 'DLL_VM/outer/game_api.cpp'
src = io.open(dst, encoding='utf-8').read()

m = re.search(r'#define GAME_FN_LIST\(X\)(.*?)(?=\n#)', c, re.S)
game = re.findall(r'X\(([^)]*)\)', m.group(1))
assert len(game) == 150, len(game)

# 名字带 VM_、其实只用 VM 自己数据的（不用等宿主）
SELF = """VM_GetProp VM_SetProp VM_IsUndefined VM_IsDestroyed VM_Random VM_Floor VM_Ceil
VM_CreateInstance VM_DestroyInstance VM_GetInstanceCount VM_GetInstanceAt VM_RunStep
VM_ArrayGet VM_ArraySet VM_ArrayDel VM_ArrayADD VM_ArraySize VM_ArrayClear VM_ArrayClearAll
VM_ArrayExists VM_ArrayContains
VM_InstArrayExists VM_InstArraySize VM_InstArrayItem VM_InstArraySet VM_InstArrayAdd
VM_InstArrayDel VM_InstArrayClear VM_InstArrayContains
VM_GetFlame VM_SetFlame VM_GetWave VM_SetWave VM_SetShovelFlameRate VM_SetCardLevelCap
VM_SetCardShapeCap VM_SetCardSkillCap VM_BanWeapon VM_BanSuperWeapon VM_BanShield
VM_GetCurCard VM_GetLastCreatedCard VM_GetLastDestroyedCard VM_GetLastCreatedEnemy
VM_GetLastKilledEnemy VM_GetLastClickedButton
VM_CallFunc VM_FuncExists VM_FuncDesc""".split()
SELF = set(SELF)

RULES = [
    ('刷怪/生成',  r'^VM_(Spawn|CreatePlatform|CreateButton|SpawnPlantsRandom|HomingBulletAdd)'),
    ('卡片/卡组',  r'^VM_(BanCard|BanAllCard|CannelBanCard|SetCardLevelCap|SetCardShapeCap|SetCardSkillCap|'
                   r'SetCardProp|SetCardSlotProp|CalcCardSlotProp|GetCardSlotCount|GetPreviewCard|GetCardProp|'
                   r'GetCurCard|Slot_add|conveyor_belt_able|ApplyPlantLevel)'),
    ('精灵/音效',  r'^VM_(LoadSprite|LoadSound|PlaySound|SetMapBackground|DrawSpriteExt|AliasSprite|'
                   r'FreeSpritePerm|SpriteExists|GetLoadedSpriteName)'),
    ('战场/地形',  r'^VM_(SetTerrain|GetTerrain|ClearPlants|ClearPlantsByType|WakePlants|GetPlant|GetEnemyCount|'
                   r'SwapPlant|Compact|SetRowFeature|CellCount|CellItem|CellContains|CatInRow|MapObj|ClearMapObjects)'),
    ('波次/结算',  r'^VM_(GetWave|SetWave|GetSubwave|GetTimeLimit|SetTimeLimit|GameWin|GameLose|GetInfo|GetKilledProp|'
                   r'DamageEnemy|GetFlame|SetFlame|SetShovelFlameRate|BulletScreenAdd)'),
    ('开关/横幅',  r'^VM_(BanGem|BanWeapon|BanSuperWeapon|BanShield|SetEventEnabled|RefreshPlatformSnapshots|'
                   r'SetMaxSlots|SetPlatformParams|SetDrawSlot|SetNoticeStyle|ShowNotice|ShellPrint)'),
    ('上次事件',   r'^VM_GetLast'),
    ('范围/索敌',  r'^VM_(EnemyInRange|GetHomingTarget|GetInstancesInRange)'),
    ('输入/鼠标',  r'^VM_Get(Mouse|Key)'),
]

host = [n for n in game if n not in SELF]
left = list(host)
groups = []
for label, pat in RULES:
    hit = [n for n in left if re.match(pat, n)]
    if hit:
        groups.append((label, hit))
        left = [n for n in left if n not in hit]
if left:
    groups.append(('其余', left))

def dwidth(s):
    return sum(2 if ord(c) > 0x2E80 else 1 for c in s)

lines = []
NAME_COL = 16
MAXW = 118
for label, names in groups:
    head = '%s %d 条' % (label, len(names))
    pad = ' ' * max(1, NAME_COL - dwidth(head))
    cur = '//   ' + head + pad
    for n in names:
        if dwidth(cur) > NAME_COL and dwidth(cur) + 1 + len(n) > MAXW:
            lines.append(cur)
            cur = '//   ' + ' ' * NAME_COL
        cur += n + ' '
    lines.append(cur.rstrip())

# 自举那批单独一段
self_names = [n for n in game if n in SELF]
lines.append('//')
head = '自举  %d 条' % len(self_names)
pad = ' ' * max(1, NAME_COL - dwidth(head))
cur = '//   ' + head + pad
for n in self_names:
    if dwidth(cur) > NAME_COL and dwidth(cur) + 1 + len(n) > MAXW:
        lines.append(cur)
        cur = '//   ' + ' ' * NAME_COL
    cur += n + ' '
lines.append(cur.rstrip())
lines.append('//   （这一批**已经实现**：impl_VM_* 在 outer/game_self.cpp（46 条）+ outer/callbin_fn.cpp（3 条），')
lines.append('//     启动时由 outer/fn_table.cpp 的 patch_game_fns() 把表项换过去 —— 本文件里对应的空壳是死代码）')

assert sum(len(n) for _, n in groups) + len(self_names) == len(game)

block = '\n'.join(lines)
a = src.index('// >>> GAME_GROUPS_BEGIN')
b = src.index('// >>> GAME_GROUPS_END')
out = src[:a] + '// >>> GAME_GROUPS_BEGIN\n' + block + '\n' + src[b:]
io.open(dst, 'w', encoding='utf-8', newline='\n').write(out)
print('%s：%d 条 = 要宿主 %d + 自举 %d，%d 组'
      % (dst, len(game), len(host), len(self_names), len(groups)))
