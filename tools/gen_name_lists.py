# -*- coding: utf-8 -*-
# 一次性：核对 / 重建 DLL_VM/core/const_str.h 里的两张名单
#   真源 = gm2bin.py（它编出 bin，bin 里的 fid 就是它的下标）
#     NATIVE_STUBS + NATIVE_IMPL  → native 表顺序（OP_NATIVE 的 fid）
#     GAME_NAMES                  → game 表顺序（OP_CALL 的 fid）
#   用法： python tools/gen_name_lists.py          （核对，只打印）
#          python tools/gen_name_lists.py --write  （改写 const_str.h）
import io, re, sys

py = io.open('transformGML2txt/gm2bin.py', encoding='utf-8').read()
hpath = 'DLL_VM/core/const_str.h'
h = io.open(hpath, encoding='utf-8').read()

def py_list(name):
    m = re.search(name + r'\s*=\s*\[(.*?)\n\]', py, re.S)
    if not m:
        raise SystemExit('gm2bin.py 里找不到 ' + name)
    return re.findall(r'"([^"]+)"', m.group(1))

def c_list(text, name):
    m = re.search(r'#define ' + name + r'\(X\)(.*?)(?=\n#)', text, re.S)
    return re.findall(r'X\(([^)]*)\)', m.group(1)) if m else []

stubs = py_list('NATIVE_STUBS')
impl  = py_list('NATIVE_IMPL')
game  = py_list('GAME_NAMES')

c_fn   = c_list(h, 'NATIVE_FN_LIST')
c_tail = c_list(h, 'NATIVE_TAIL_LIST')
c_ph   = c_list(h, 'PLACEHOLDER_FN_LIST')
c_ex   = c_list(h, 'NATIVE_EXTRA_LIST')
c_game = c_list(h, 'GAME_FN_LIST')

print('native: 编译器 %d + %d = %d' % (len(stubs), len(impl), len(stubs) + len(impl)))
print('native: 现在 C++ 名单 %d + %d + %d + %d = %d'
      % (len(c_fn), len(c_tail), len(c_ph), len(c_ex), len(c_fn) + len(c_tail) + len(c_ph) + len(c_ex)))
print('  编译器 FN  == C++ NATIVE_FN_LIST ?', stubs == c_fn)
print('  编译器前 79+99 条 == C++ FN+TAIL ?', impl[:99] == c_tail)
print('game  : 编译器 %d  现在 C++ 名单 %d  顺序一致 ? %s' % (len(game), len(c_game), game == c_game))
if game != c_game:
    for i in range(max(len(game), len(c_game))):
        a = game[i] if i < len(game) else '<无>'
        b = c_game[i] if i < len(c_game) else '<无>'
        if a != b:
            print('  第一个不同的位置 %d: 编译器=%s  C++=%s' % (i, a, b)); break
    print('  只差顺序？集合相同 ?', set(game) == set(c_game))

if '--write' not in sys.argv:
    print('\n(核对模式，没改文件；要改加 --write)')
    raise SystemExit(0)

# ── 重写名单：把"三张名单本体"那一整段重建 ──
#    native = NATIVE_FN_LIST(79) + NATIVE_IMPL_LIST(279)，顺序照 gm2bin.py
#    game   = GAME_FN_LIST(150)，顺序照 gm2bin.py（本来就已经一致）
#    INNER_FIELD_LIST 原样保留（19 个内置变量名）
def fmt(names, per_line):
    out = []
    for i in range(0, len(names), per_line):
        out.append('    ' + ' '.join('X(%s)' % n for n in names[i:i+per_line]) + ' \\')
    return '\n'.join(out)

inner = c_list(h, 'INNER_FIELD_LIST')
assert len(inner) == 19, inner

block = (
    '// ── 名单本体（顺序 = 契约：池下标 = native / game 的表下标）──\n'
    '//   真源是编译器 gm2bin.py（它编出的 bin 里烘的就是这些下标）：\n'
    '//     native = NATIVE_STUBS + NATIVE_IMPL\n'
    '//     game   = GAME_NAMES（= LabMapCompiler 的 FUNC_DEFS）\n'
    '//   ⚠ 只能往各段**尾部**加；中间插 / 删 / 重排 → 已编译的 bin 会静默调错函数\n'
    '#define INNER_FIELD_LIST(X) \\\n' + fmt(inner, 4) + '\n\n'
    '#define NATIVE_FN_LIST(X) \\\n' + fmt(stubs, 4) + '\n\n'
    '// native 表尾（后加的都在这里）\n'
    '#define NATIVE_IMPL_LIST(X) \\\n' + fmt(impl, 4) + '\n\n'
    '#define GAME_FN_LIST(X) \\\n' + fmt(game, 4) + '\n\n')

# 先切掉旧 NAME_LIST 里的四段引用（它在新块之后，所以要先把新块拼好再换 NAME_LIST）
h = re.sub(r'    NATIVE_FN_LIST\(X\) \\\s*\n    NATIVE_TAIL_LIST\(X\) \\\s*\n'
           r'    PLACEHOLDER_FN_LIST\(X\) \\\s*\n    NATIVE_EXTRA_LIST\(X\) \\',
           lambda m: '    NATIVE_FN_LIST(X) \\\n    NATIVE_IMPL_LIST(X) \\', h, count=1)
assert '    NATIVE_TAIL_LIST(X) \\' not in h, 'NAME_LIST 那几段没换掉'

# 整段替换
a = h.index('// ── 三张名单本体')
b = h.index('// ── 其余（hot/ 用到的字符串')
h = h[:a] + block + h[b:]
io.open(hpath, 'w', encoding='utf-8', newline='\n').write(h)
print('\n已改写 %s：native %d + %d，game %d，字段 %d'
      % (hpath, len(stubs), len(impl), len(game), len(inner)))

