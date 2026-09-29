# -*- coding: utf-8 -*-
# 把 game_api.cpp 注释块里"要宿主"的那 101 条按分组切成 3 个批次，
# 生成 outer/game_a.cpp、game_b.cpp、game_c.cpp 的骨架（名单摆在文件头，方便对账）
#   批次 A：实例 / 战场 / 索敌 / 生成 / 上次事件   —— 多半只差已有内层·外层 + 自举数学
#   批次 B：卡片 / 精灵 / 波次 / 结算 / 输入        —— 要 host 表
#   批次 C：开关 / 横幅 / 其余                      —— 要 host 表
import io, re

g = io.open('DLL_VM/outer/game_api.cpp', encoding='utf-8').read()
i = g.index('// >>> GAME_GROUPS_BEGIN'); j = g.index('// >>> GAME_GROUPS_END')
block = g[i:j]
self_part = block[block.index('自举'):]
self_names = set(re.findall(r'(VM_\w+)', self_part))

groups = []          # [(label, [names])]
cur = None
for line in block.split('\n'):
    if not line.startswith('//   '):
        continue
    body = line[5:]                           # '//   ' 是 5 个字符，别多切
    m = re.match(r'(\S+)\s+(\d+) 条\s+(.*)$', body)
    if m:
        label = m.group(1).strip()
        if label == '自举':
            break
        cur = (label, re.findall(r'(VM_\w+)', m.group(3)))
        groups.append(cur)
    elif cur is not None:                     # 折行续行：名字接着上一组
        cur[1].extend(re.findall(r'(VM_\w+)', body))

for label, names in groups:                    # 去掉已自举的
    names[:] = [n for n in names if n not in self_names]
groups = [(l, n) for l, n in groups if n]

print('分组：', [(l, len(n)) for l, n in groups])
host_total = sum(len(n) for _, n in groups)
print('要宿主合计', host_total)

A_LABELS = ('刷怪/生成', '战场/地形', '范围/索敌', '其余', '上次事件')
B_LABELS = ('卡片/卡组', '精灵/音效', '波次/结算', '输入/鼠标')
C_LABELS = ('开关/横幅',)
batch = {'a': [], 'b': [], 'c': []}
for label, names in groups:
    if label in A_LABELS:   batch['a'] += names
    elif label in B_LABELS: batch['b'] += names
    elif label in C_LABELS: batch['c'] += names
    else:                   batch['a'] += names        # 兜底
assert sum(len(v) for v in batch.values()) == host_total
assert len(set(batch['a'] + batch['b'] + batch['c'])) == host_total, '有重名'

TITLE = {'a': '批次 A：实例 / 战场 / 索敌 / 生成 / 上次事件',
         'b': '批次 B：卡片 / 精灵·音效 / 波次·结算 / 输入',
         'c': '批次 C：开关 / 横幅'}
HINT = {'a': '多半只差"已有的内层·外层函数 + 自举数学"（inst_* / inst_var_* / ds_* / global.* / 网格换算）',
        'b': '要 host 表（g_host_draw / g_host_in / g_host_net），精灵·音效那几条还要 global.* 里的映射表',
        'c': '要 host 表 + 少量全局开关'}

for k in 'abc':
    names = batch[k]
    lines = []
    for x in range(0, len(names), 4):
        lines.append('    ' + ' '.join('X(%s)' % n for n in names[x:x+4]) + ' \\')
    body = '\n'.join(lines)
    src = '''// ============================================================
// game_%s.cpp — 游戏 API（%s，%d 条）
//
//   A 为什么有这些文件：game_api.cpp 只生成 150 条空壳（名字/顺序是契约），
//   实现写成 impl_XXX 放在这儿，再由 patch_game_fns_%s() 挂进 kGameFns
//   （fn_table.cpp 的 patch_game_fns() 会按顺序调这三个钩子）。
//
//   本批要什么：%s
//
//   写法（照 game_self.cpp 的样子）：
//     static void impl_XXX(Value* mem, const uint8_t* args, int argc, int dst) { … }   // static：本文件私有
//     void patch_game_fns_%s() { kGameFns[S_VM_XXX - SID_GAME_BASE] = impl_XXX; … }
//
//   ⚠ 只改本文件！批次之间、和 outer.h / fn_table.cpp / 表名单都别动（会互相踩）。
//   ⚠ 实参就是**槽位**（不是内存地址）：arg_v(mem, args, k) 取值，argslot(args, k) 只取槽号。
//   ⚠ 语义真源是 GML 参考：scripts/scr_command_VM/scr_command_VM.gml（那 150 条的正版实现），
//     老 C++ 有实现的在 transformGML2txt/vm_api.cpp。两条不一致时以 GML 参考为准。
//
//   本批名单（%d 条；实现一条对一条）：
// ============================================================
#include "outer.h"

#define GAME_%s_LIST(X) \\
%s

// TODO(%s)：实现本批函数体，并在下面挂表
void patch_game_fns_%s() {
}
''' % (k, TITLE[k], len(names), k, HINT[k], k, len(names), k.upper(), body, TITLE[k], k)
    io.open('DLL_VM/outer/game_%s.cpp' % k, 'w', encoding='utf-8', newline='\n').write(src)
    print('写出 game_%s.cpp：%d 条' % (k, len(names)))
