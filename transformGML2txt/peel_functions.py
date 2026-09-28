#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# peel_functions.py — 迭代剥出"函数侧"还缺哪些接口
#
#   循环：跑 gm2bin kernel/function --recursive → 收集 "has no builtin" 的名字
#         → 追加到 gm2bin 的 NATIVE_IMPL → 再跑……直到没有新名字。
#   最后分两类打印：GM 内建（该在 C++ 实现）/ 项目脚本（该进 function/ 闭包）。
#
#   ⚠ 只改 gm2bin 的名字表（让转换能过）；名字进了表但 C++ 没有真实现的话，
#     运行时会走"转发给 VM_CallFunc"，所以这一步是**测量**，不是交付。
# ============================================================

import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
GM = os.path.join(HERE, "gm2bin.py")
BACKUP = os.path.join(os.environ.get("TEMP", "."), "gm2bin.peel_fn.bak")


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


def run():
    return subprocess.run([sys.executable, GM, os.path.join(ROOT, "kernel", "function"),
                           "--recursive", "--out", os.path.join(ROOT, "kernel")],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=ROOT)


def script_funcs():
    out = set()
    for root, dirs, files in os.walk(os.path.join(ROOT, "scripts")):
        for f in files:
            if f.endswith(".gml"):
                src = open(os.path.join(root, f), encoding="utf-8-sig", errors="replace").read()
                out |= set(re.findall(r'(?m)^\s*function\s+(\w+)\s*\(', src))
    return out


def main():
    shutil.copyfile(GM, BACKUP)
    found = []
    for rnd in range(1, 9):
        r = run()
        txt = r.stdout.decode("utf-8", "replace")
        cur = set(impl_names())
        new = sorted({m.group(1) for m in re.finditer(r'function "([A-Za-z_]\w*)" has no builtin', txt)} - cur)
        cb = len(re.findall(r'callbin\s', txt))
        print("第 %d 轮：新缺接口 %d 个%s" % (rnd, len(new), ("  " + ", ".join(new[:10])) if new else "（收敛）"))
        if not new:
            break
        append_impl(new)
        found += new
    shutil.copyfile(BACKUP, GM)          # 还原（这一步只是测量）

    scripts = script_funcs()
    gm = [n for n in found if n not in scripts]
    pr = [n for n in found if n in scripts]
    print("\n=== 函数侧一共要补 %d 个接口 ===" % len(found))
    print("\n-- A. GM 内建 / 引擎函数（该在 C++ 里实现）%d 个 --" % len(gm))
    line = ""
    for n in sorted(gm):
        if len(line) + len(n) + 2 > 120:
            print("   " + line); line = ""
        line += n + "  "
    if line:
        print("   " + line)
    print("\n-- B. 项目脚本（该进 kernel/function 闭包）%d 个 --" % len(pr))
    line = ""
    for n in sorted(pr):
        if len(line) + len(n) + 2 > 120:
            print("   " + line); line = ""
        line += n + "  "
    if line:
        print("   " + line)
    print("\ngm2bin.py 已还原。")


if __name__ == "__main__":
    main()
