# -*- coding: utf-8 -*-
# 一次性：在 gm2bin.py 里加一张 CALL_BY_NAME 表 + 在调用点接线
#   表里的函数名 → 编成 OP_CALL VM_CallFunc("名字", 实参…)（不走 native 表）
#   预填：项目业务那批里"平台已经有同名 callbin"的（两种编法落到同一个 callbin，行为等价，
#         所以这一步是安全的迁移；剩下没 callbin 的先留在 native 那条路）
import io, re, glob, os

GM = 'transformGML2txt/gm2bin.py'
src = io.open(GM, encoding='utf-8').read()

# 1) 名字清单：HOST_BIZ_LIST ∩ 仓库里的 callbin
sp = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()
i = sp.index('#define HOST_BIZ_LIST(X)')
seg = sp[i:]
j = seg.find('\n\n')
if j < 0: j = len(seg)
biz = re.findall(r'X\(([^)]*)\)', seg[:j])
cbs = {os.path.basename(p)[:-len('.callbin')] for p in glob.glob('kernel/**/*.callbin', recursive=True)}
names = sorted(n for n in biz if n in cbs)

# 2) 表本身
anchor = 'CALLBIN_NAMES = set()                   # 已知的 callbin 函数名（调用点按它编成 VM_CallFunc）'
assert src.count(anchor) == 1, '锚点不对'
lines = []
for k in range(0, len(names), 4):
    lines.append('    ' + ' '.join('"%s",' % n for n in names[k:k+4]))
block = (
    anchor + '\n\n'
    '# ── 按名字调用（**不进 native 表**）──\n'
    '#   命中就编成  OP_CALL VM_CallFunc("名字", 实参…)  —— 名字作为字符串常量进 _VM_CONST_INIT。\n'
    '#   运行时由 C++ 侧接住：① VM_RegisterFunc 注册的 DLL 函数表 ② 扫到的 callbin 表（DLL 表优先）。\n'
    '#   好处：**native 表不用再加条目** → 已编译 bin 的 fid 顺序永远不会被新函数挤动。\n'
    '#   ⚠ 只影响**新编**的 bin；老 bin 里烘的还是 OP_NATIVE + fid，照旧能跑。\n'
    '#   ⚠ 加名字之前先确认那个名字"运行时真能被接住"（有同名 callbin，或插件会 RegisterFunc），\n'
    '#     否则它从"响亮报 no builtin"变成"运行时响亮报 VM_E_FN_NOT_FOUND"（两种都是响亮失败）。\n'
    'CALL_BY_NAME = set([\n' + '\n'.join(lines) + '\n])\n')
src = src.replace(anchor, block, 1)

# 3) 调用点接线
old_check = ('            # ── callbin（外部函数）：编成 VM_CallFunc("名字", 实参…) ──\n'
             '            #   名字来自 CALLBIN_NAMES（扫 --callbins 目录下的 .callbin）\n'
             '            if nm in CALLBIN_NAMES:\n')
new_check = ('            # ── 按名字调用：编成 VM_CallFunc("名字", 实参…)，不进 native 表 ──\n'
             '            #   两张来源：CALL_BY_NAME（脚本里写死的）+ CALLBIN_NAMES（扫 --callbins 扫到的）\n'
             '            if nm in CALL_BY_NAME or nm in CALLBIN_NAMES:\n')
assert src.count(old_check) == 1, '调用点锚点不对'
src = src.replace(old_check, new_check, 1)

io.open(GM, 'w', encoding='utf-8', newline='\n').write(src)
print('gm2bin.py：CALL_BY_NAME 预填 %d 个（业务 %d 里已有 callbin 的那些）' % (len(names), len(biz)))
print('  ' + ' '.join(names))
