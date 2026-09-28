#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# fill_and_sync.py — 迭代补缺接口，并把结果同步进 C++ 表
#
#   1) 循环：跑 gm2bin → 收集 "has no builtin" 的名字 → 追加到 NATIVE_IMPL → 再跑
#      直到这一轮没有新名字（每个对象只报第一个错，所以要一层层剥）
#   2) 把 gm2bin 的 native 段顺序同步回 names.h：
#        PLACEHOLDER_FN_LIST = NATIVE_IMPL[99:] 去掉 irandom（它已经有真实现）
#      REST_NAME_LIST 里同名的一律删掉（一个名字只能有一个 id）
#      NATIVE_FN_COUNT = 79 + len(NATIVE_IMPL)
#   3) 重新编译 + 重跑转换，打印最终结果
# ============================================================

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GM = os.path.join(HERE, "gm2bin.py")
NAMES_H = os.path.join(HERE, "names.h")
VM_H = os.path.join(HERE, "vm.h")
LOG = os.path.join(HERE, "convert.log")
REAL_IMPL = {"irandom", "chr", "string_pos", "file_exists", "camera_get_view_x"}   # 有真实现


def run():
    """在 kernel/ 上跑（两遍都会走到：函数 callbin + 对象 bin），并带上 callbin 目录，
       这样已经编好的 callbin 名字也能被认到（否则会误报 has no builtin）。"""
    subprocess.run([sys.executable, GM, os.path.join(os.path.dirname(HERE), "kernel"),
                    "--recursive", "--out", os.path.join(os.path.dirname(HERE), "kernel"),
                    "--callbins", os.path.join(os.path.dirname(HERE), "kernel", "function")],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, cwd=HERE)


def last_stops():
    text = open(LOG, encoding="utf-8", errors="replace").read()
    last = re.split(r'={20,}\n(?=\d{4}-\d\d-\d\d)', text)[-1]
    return [l for l in last.split("\n") if " STOP " in l]


def impl_names():
    s = open(GM, encoding="utf-8").read()
    m = re.search(r'NATIVE_IMPL = \[(.*?)\n\]', s, re.S)
    return re.findall(r'"([^"]+)"', m.group(1))


def append_impl(names):
    s = open(GM, encoding="utf-8").read()
    m = re.search(r'(NATIVE_IMPL = \[)(.*?)(\n\])', s, re.S)
    body = re.sub(r',?\s*$', '', m.group(2))
    s = s[:m.start()] + m.group(1) + body + "".join(',\n    "%s"' % n for n in names) + m.group(3) + s[m.end():]
    open(GM, "w", encoding="utf-8", newline="\n").write(s)


def macro_body(src, name):
    return re.search(r'#define %s\(X\) \\\n(.*?)\n\n' % name, src, re.S)


def rewrite_macro(src, name, items):
    m = macro_body(src, name)
    body = "\n".join("    X(%s)%s" % (n, " \\" if i < len(items) - 1 else "")
                     for i, n in enumerate(items))
    return src[:m.start()] + "#define %s(X) \\\n%s\n\n" % (name, body) + src[m.end():]


def main():
    found = []
    for _ in range(8):
        run()
        cur = set(impl_names())
        new = sorted({re.search(r'function "([A-Za-z_]\w*)" has no builtin', l).group(1)
                      for l in last_stops()
                      if re.search(r'function "([A-Za-z_]\w*)" has no builtin', l)} - cur)
        print("  轮次：新缺接口 %d 个%s" % (len(new), ("  " + ", ".join(new[:8])) if new else "  （收敛）"))
        if not new:
            break
        append_impl(new)
        found += new

    impl = impl_names()
    assert impl[:99] == impl_names()[:99]
    tail = impl[99:]
    placeholders = [n for n in tail if n not in REAL_IMPL]

    s = open(NAMES_H, encoding="utf-8").read()
    s = rewrite_macro(s, "PLACEHOLDER_FN_LIST", placeholders)
    rest = re.findall(r'X\(([A-Za-z_][A-Za-z0-9_]*)\)', macro_body(s, "REST_NAME_LIST").group(1))
    kill = set(placeholders)
    kept = [n for n in rest if n not in kill]
    if len(kept) != len(rest):
        s = rewrite_macro(s, "REST_NAME_LIST", kept)
    open(NAMES_H, "w", encoding="utf-8", newline="\n").write(s)

    total = 79 + len(impl)
    v = open(VM_H, encoding="utf-8").read()
    v = re.sub(r"static const int NATIVE_FN_COUNT = \d+;",
               "static const int NATIVE_FN_COUNT = %d;" % total, v, count=1)
    open(VM_H, "w", encoding="utf-8", newline="\n").write(v)

    print("\n本轮新补接口 %d 个：%s" % (len(found), ", ".join(found) if found else "无"))
    print("native 段：79 + %d = %d（NATIVE_FN_COUNT=%d）；占位 %d + 真实现 %d"
          % (len(impl), total, total, len(placeholders), len(REAL_IMPL)))
    print("REST_NAME_LIST: %d → %d" % (len(rest), len(kept)))


if __name__ == "__main__":
    main()
