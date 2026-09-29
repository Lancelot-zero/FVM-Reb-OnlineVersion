# -*- coding: utf-8 -*-
# 一次性：核对"编译器认的 native id 顺序"和"C++ kNativeFns 表的顺序"
import re, io

s = io.open('transformGML2txt/gm2bin.py', encoding='utf-8').read()
h = io.open('DLL_VM/core/const_str.h', encoding='utf-8').read()

def grab_py(name):
    m = re.search(name + r'\s*=\s*\[(.*?)\n\]', s, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))

def grab_c(name):
    m = re.search(r'#define ' + name + r'\(X\)(.*?)(?=\n#)', h, re.S)
    return re.findall(r'X\(([^)]*)\)', m.group(1)) if m else []

st, im = grab_py('NATIVE_STUBS'), grab_py('NATIVE_IMPL')
fn, tail = grab_c('NATIVE_FN_LIST'), grab_c('NATIVE_TAIL_LIST')
ph, ex = grab_c('PLACEHOLDER_FN_LIST'), grab_c('NATIVE_EXTRA_LIST')

print('compiler: NATIVE_STUBS=%d  NATIVE_IMPL=%d  -> total %d' % (len(st), len(im), len(st) + len(im)))
print('lists:    FN=%d TAIL=%d PLACEHOLDER=%d EXTRA=%d -> total %d'
      % (len(fn), len(tail), len(ph), len(ex), len(fn) + len(tail) + len(ph) + len(ex)))
print('stubs == FN_LIST ?', st == fn)
print('impl  == TAIL    ?', im == tail)
print('compiler first 3:', st[:3], im[:3])
print('tail     first 3:', tail[:3])
print('impl[95:105] :', im[95:105])
print('impl[170:185]:', im[170:185])
print('impl[240:250]:', im[240:250])
print('impl tail   :', im[-5:])
print('impl contains PH?', im[99:99+144] == ph)
print('impl contains EX?', im[99+144:99+144+19] == ex)
print('impl[0:99]==tail?', im[:99] == tail)
print('impl extra beyond 262:', im[262:])
# 集合对比：编译器 358 是否覆盖 C++ 那 341 条
allc = fn + tail + ph + ex
allcp = st + im
missing = [n for n in allc if n not in allcp]     # C++ 有、编译器没有（没有 fid）
extra   = [n for n in allcp if n not in allc]     # 编译器有、C++ 没有（没实现名额）
print('C++ 名单里但编译器没有的（没有 fid）: %d %s' % (len(missing), missing))
print('编译器里有但 C++ 名单没有的: %d %s' % (len(extra), extra))
# 找到 impl 里各段的位置
for name, L in (('TAIL', tail), ('PH', ph), ('EX', ex)):
    idx = None
    for i in range(len(im) - len(L) + 1):
        if im[i:i+len(L)] == L:
            idx = i; break
    print('  %s starts at %s' % (name, idx))
# C++ 表里 tail 的起始下标（表序：FN | PLACEHOLDER | EXTRA | TAIL）
print('C++ table: TAIL starts at fid', len(fn) + len(ph) + len(ex))
print('compiler : TAIL starts at id ', len(st))
print('PLACEHOLDER first/last:', ph[:2], ph[-2:])
print('EXTRA first/last:', ex[:2], ex[-2:])
