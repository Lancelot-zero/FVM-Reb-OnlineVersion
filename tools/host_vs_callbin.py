# -*- coding: utf-8 -*-
# 对账：host_fn 那 180 条里，哪些平台已经有"同名 callbin"或"同名 GML 脚本"
#   → 有同名的，说明平台侧已经有一份实现，VM 这边可以走转发（callbin_call）而不是等 host 原语
import io, re, glob, os

h = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()
i = h.index('#define HOST_LIST(X)')
seg = h[i:]
j = seg.find('\n\n')
if j < 0: j = len(seg)
host = re.findall(r'X\(([^)]*)\)', seg[:j])
print('host 名单 %d 条' % len(host))

cbs = {os.path.basename(p)[:-len('.callbin')] for p in glob.glob('kernel/**/*.callbin', recursive=True)}
gm  = {os.path.basename(p)[:-len('.gml')]     for p in glob.glob('kernel/**/*.gml', recursive=True)}
gm |= {os.path.basename(p)[:-len('.gml')]     for p in glob.glob('scripts/**/*.gml', recursive=True)}
print('平台 callbin %d 个，GML 脚本/函数 %d 个' % (len(cbs), len(gm)))

both = [n for n in host if n in cbs]
only_gml = [n for n in host if n not in cbs and n in gm]
neither = [n for n in host if n not in cbs and n not in gm]
print('\nA 有同名 callbin（可直接转发）%d：%s' % (len(both), ' '.join(both)))
print('\nB 只有同名 GML（可以在 C++ 里照搬或让宿主用 GML）%d：%s' % (len(only_gml), ' '.join(only_gml)))
print('\nC 两者都没有 %d：%s' % (len(neither), ' '.join(neither)))

# 顺带看一眼转发那 79 条有多少真的有 callbin
s = io.open('DLL_VM/outer/outer_split.h', encoding='utf-8').read()
i2 = s.index('#define FORWARD_LIST(X)')
seg2 = s[i2:]; j2 = seg2.find('\n\n')
if j2 < 0: j2 = len(seg2)
fwd = re.findall(r'X\(([^)]*)\)', seg2[:j2])
have = [n for n in fwd if n in cbs]
print('\n（参照）转发类 79 条里，平台已有同名 callbin 的 %d 条：%s' % (len(have), ' '.join(have[:20])))
