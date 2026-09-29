# -*- coding: utf-8 -*-
# 只看**我们真翻译的那批**（kernel/）：被调用、但不是项目自己的函数、也不在两张表里
#   = 我们"还缺的内建函数"
import io, re, glob, os, collections

sp = io.open('DLL_VM/core/const_str.h', encoding='utf-8').read()
def lst(name):
    m = re.search(r'#define ' + name + r'\(X\)(.*?)(?=\n#)', sp, re.S)
    return set(re.findall(r'X\(([^)]*)\)', m.group(1))) if m else set()
table = lst('NATIVE_FN_LIST') | lst('NATIVE_IMPL_LIST') | lst('GAME_FN_LIST')
cb = {os.path.basename(p)[:-len('.callbin')] for p in glob.glob('kernel/**/*.callbin', recursive=True)}

# 翻译器**特殊处理**的名字（不当函数调用编，走专门的分支）—— 不算缺
SPECIAL = set('''event_inherited event_user with repeat switch case default if else while for do
until return exit break continue var new delete throw try catch static globalvar function'''.split())

files = glob.glob('kernel/**/*.gml', recursive=True)
defined = set()
for p in files:
    t = io.open(p, encoding='utf-8', errors='replace').read()
    defined |= set(re.findall(r'\bfunction\s+([A-Za-z_]\w*)', t))
    defined |= set(re.findall(r'\b([A-Za-z_]\w*)\s*=\s*function', t))

calls, where = collections.Counter(), {}
for p in files:
    t = io.open(p, encoding='utf-8', errors='replace').read()
    t = re.sub(r'//[^\n]*', ' ', t)
    t = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)
    for m in re.finditer(r'(?<![.\w])([A-Za-z_]\w*)\s*\(', t):
        calls[m.group(1)] += 1
        where.setdefault(m.group(1), p.replace('\\', '/'))

miss = sorted(((c, n) for n, c in calls.items()
               if n not in table and n not in cb and n not in defined and n not in SPECIAL), reverse=True)
print('kernel/ 里 %d 个 .gml；项目自定义函数 %d | 表 508 | callbin %d' % (len(files), len(defined), len(cb)))
print('缺的内建函数：%d 个' % len(miss))
for c, n in miss[:40]:
    print('  %-32s %4d 次   %s' % (n, c, where[n]))
