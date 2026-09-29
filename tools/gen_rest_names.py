# -*- coding: utf-8 -*-
# 把 outer/*.cpp 里"运行时才 intern 的名字"收进 const_str.h 的 REST_NAME_LIST 尾部
#   规则：名字只许往各段**尾部**加；REST 是最后一段 → 追加安全
#   幂等：已有的名字自动跳过（不重复加），下次跑只补新的
#   用法：python tools/gen_rest_names.py          （只报告）
#         python tools/gen_rest_names.py --write  （改写）
import io, re, glob, sys

HP = 'DLL_VM/core/const_str.h'
h = io.open(HP, encoding='utf-8').read()

# 1) 现有名单里所有名字（用来查重）
have = set()
for m in re.finditer(r'#define (\w+_LIST)\(X\)(.*?)(?=\n#)', h, re.S):
    have.update(re.findall(r'X\(([^)]*)\)', m.group(2)))

# 2) outer/*.cpp 里运行时 intern 的字面量
#   直接 intern_str("x") 的，以及各批自己包一层的助手（name_sid("x") / xxx_sid("x")）
pat = re.compile(r'(?:intern_str|intern_once|intern_name|sid_of|name_sid|\w+_sid|\w+_name)'
                 r'\s*\(\s*"([^"]{1,64})"\s*\)')
found = []
for f in sorted(glob.glob('DLL_VM/outer/*.cpp')) + sorted(glob.glob('DLL_VM/outer/**/*.cpp')):
    t = io.open(f, encoding='utf-8').read()
    for name in pat.findall(t):
        found.append((name, f.split('/')[-1].split('\\')[-1]))
uniq = sorted(set(n for n, _ in found))
new = [n for n in uniq if n not in have]
print('运行时 intern 到 %d 个字面量，去重 %d 个；名单里已经有 %d 个，需要补 %d 个'
      % (len(found), len(uniq), len(uniq) - len(new), len(new)))
print('来源：', sorted(set(f for _, f in found)))
print('要补的：', ' '.join(new))

if '--write' not in sys.argv or not new:
    print('\n（没写文件；要写加 --write）')
    raise SystemExit(0)

# 3) 追加到 REST_NAME_LIST 尾部
i = h.index('#define REST_NAME_LIST(X)')
j = h.index('\n\n', i)
seg = h[i:j]
lines = seg.rstrip().split('\n')
assert lines[-1].strip().startswith('X('), lines[-1]
assert not lines[-1].rstrip().endswith('\\'), 'REST 尾行不该带反斜杠：' + lines[-1]
lines[-1] = lines[-1].rstrip() + ' \\'
BLOCK = 4
add = []
for k in range(0, len(new), BLOCK):
    add.append('    ' + ' '.join('X(%s)' % n for n in new[k:k+BLOCK]) + ' \\')
add[-1] = add[-1].rstrip()[:-1].rstrip()          # 最后一行去掉反斜杠
lines.append('    /* ↓ 下面这些是各批实现里运行时用到的名字（脚本补的，只许往后加） */')
lines += add
h = h[:i] + '\n'.join(lines) + h[j:]
io.open(HP, 'w', encoding='utf-8', newline='\n').write(h)
print('\n已把 %d 个名字追加到 REST_NAME_LIST 尾部' % len(new))
