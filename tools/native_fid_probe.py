# -*- coding: utf-8 -*-
# 一次性：从真实 bin 里解出 OP_NATIVE 的 fid，看它们落到哪个名字上
#   · 编译器顺序（gm2bin.py 的 NATIVE_STUBS + NATIVE_IMPL）→ 名字 A
#   · 老 C++ 表顺序（FN | PLACEHOLDER | EXTRA | TAIL）→ 名字 B
#   bin 是 gm2bin.py 编的，所以 A 才是"作者的本意"；B 若不同 = 老 C++ 表是错位/陈旧的
import re, io, glob, struct, collections

py = io.open('transformGML2txt/gm2bin.py', encoding='utf-8').read()
c  = io.open('DLL_VM/core/const_str.h', encoding='utf-8').read()

def py_list(name):
    m = re.search(name + r'\s*=\s*\[(.*?)\n\]', py, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))

def c_list(name):
    m = re.search(r'#define ' + name + r'\(X\)(.*?)(?=\n#)', c, re.S)
    return re.findall(r'X\(([^)]*)\)', m.group(1)) if m else []

compiler = py_list('NATIVE_STUBS') + py_list('NATIVE_IMPL')
oldcpp   = c_list('NATIVE_FN_LIST') + c_list('PLACEHOLDER_FN_LIST') + c_list('NATIVE_EXTRA_LIST') + c_list('NATIVE_TAIL_LIST')
print('compiler ids=%d   old-cpp table=%d' % (len(compiler), len(oldcpp)))

OP_NATIVE = 18
FIXED = {2:8, 3:12,4:12,5:12,6:12,7:12, 8:12,9:12,10:12,11:12,12:12,13:12,
         15:12, 16:4, 17:0, 19:8, 23:8}
for i in range(20, 26): FIXED[i] = 12

def blocks_of(d, o):
    sc, = struct.unpack_from('<i', d, o); o += 4
    pool = []
    for _ in range(sc):
        L, = struct.unpack_from('<i', d, o); o += 4
        pool.append(d[o:o+L].decode('utf-8','replace')); o += L
    out = []
    while o + 8 <= len(d):
        bl, = struct.unpack_from('<i', d, o); o += 4
        ni, = struct.unpack_from('<i', d, o); o += 4
        out.append((pool[ni] if 0 <= ni < len(pool) else '?', d[o:o+bl])); o += bl
    return out

def walk(code, on_native):
    p, n = 0, len(code)
    while p < n:
        op = code[p]; p += 1
        if op == 1:
            ty = code[p+4]; p += 5
            p += 2 if ty == 2 else 4
        elif op in (14, 18):
            fid, = struct.unpack_from('<H', code, p); p += 2
            argc = code[p]; p += 1
            if op == OP_NATIVE: on_native(fid)
            p += 4 + argc*4
        else:
            k = FIXED.get(op)
            if k is None: return
            p += k

used = collections.Counter()
nb = 0
for path in glob.glob('kernel/**/*.bin', recursive=True):
    nb += 1
    d = io.open(path,'rb').read()
    for name, code in blocks_of(d, 0):
        if name == '_VM_CONST_INIT': continue
        walk(code, lambda fid: used.update([fid]))

print('bins=%d  用到的 native fid 种类=%d  max=%d' % (nb, len(used), max(used) if used else -1))
print('fid >= 178 的调用数:', sum(v for k, v in used.items() if k >= 178))
print('fid 在 79..177（两套顺序解释不同的那一段）的调用数:', sum(v for k, v in used.items() if 79 <= k <= 177))
print()
print('%-6s %-28s %-28s %s' % ('fid', '编译器顺序说的名字', '老C++表顺序说的名字', '次数'))
for fid, cnt in sorted(used.items(), key=lambda kv: -kv[1])[:18]:
    a = compiler[fid] if fid < len(compiler) else '<越界>'
    b = oldcpp[fid] if fid < len(oldcpp) else '<越界>'
    print('%-6d %-28s %-28s %d' % (fid, a, b, cnt))
