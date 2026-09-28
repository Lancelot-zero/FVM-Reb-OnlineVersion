#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# fill_interfaces.py — 迭代补齐 gm2bin 缺的接口，看还剩什么不能转
#
#   为什么要迭代：每个对象**只报第一个错**，补完一批接口，同一批对象会
#   撞到第二批缺接口。所以只能"补 → 重跑 → 再补"，直到这一轮没有新名字。
#
#   ⚠ 这是**实验**：只改 gm2bin 的名字表（让转换能过），不动 C++。
#     跑完会把 gm2bin.py 还原，并重跑一次转换把 bin 恢复成一致状态。
# ============================================================

import io
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GM = os.path.join(HERE, "gm2bin.py")
LOG = os.path.join(HERE, "convert.log")
BACKUP = os.path.join(os.environ.get("TEMP", "."), "gm2bin.fill_iface.bak")
MAX_ROUNDS = 10


def last_run_stops():
    text = open(LOG, encoding="utf-8", errors="replace").read()
    last = re.split(r'={20,}\n(?=\d{4}-\d\d-\d\d)', text)[-1]
    return [l for l in last.split("\n") if " STOP " in l]


def run_convert():
    subprocess.run([sys.executable, GM], stdout=subprocess.DEVNULL,
                   stderr=subprocess.STDOUT, cwd=HERE)


def missing_builtins(stops):
    out = []
    for l in stops:
        m = re.search(r'function "([A-Za-z_]\w*)" has no builtin', l)
        if m:
            out.append(m.group(1))
    return out


def append_to_native_impl(names):
    s = open(GM, encoding="utf-8").read()
    m = re.search(r'(NATIVE_IMPL = \[)(.*?)(\n\])', s, re.S)
    body = re.sub(r',?\s*$', '', m.group(2))
    add = "".join(',\n    "%s"' % n for n in names)
    s = s[:m.start()] + m.group(1) + body + add + m.group(3) + s[m.end():]
    open(GM, "w", encoding="utf-8", newline="\n").write(s)


def main():
    shutil.copyfile(GM, BACKUP)
    added = []
    for rnd in range(1, MAX_ROUNDS + 1):
        run_convert()
        stops = last_run_stops()
        new = sorted(set(missing_builtins(stops)) - set(added))
        print("第 %d 轮：STOP %d 条，其中缺接口 %d 个（新 %d 个）"
              % (rnd, len(stops), len(missing_builtins(stops)), len(new)))
        if not new:
            break
        added += new
        append_to_native_impl(new)
        print("     新补: %s" % ", ".join(new[:12]) + (" …" if len(new) > 12 else ""))

    # 最后一轮的 STOP（此时缺接口已全部补掉）
    stops = last_run_stops()
    rest = [l for l in stops if "has no builtin" not in l]
    print("\n=== 补完全部 %d 个接口后，仍然不能转的 %d 个 ===" % (len(added), len(rest)))
    for l in rest:
        m = re.search(r'STOP (\S+?):(\d+)\s+(.*)', l)
        print("   %-30s %-26s %s" % (l.split()[1], "%s:%s" % (m.group(1), m.group(2)),
                                     m.group(3).split("|")[0].strip()[:56]))

    # 还原 + 重跑，把 bin 恢复成"没有实验改动"的一致状态
    shutil.copyfile(BACKUP, GM)
    run_convert()
    print("\n=== 本轮补接口累计需要的名字（%d 个，按发现顺序）===" % len(added))
    line = ""
    for n in added:
        if len(line) + len(n) + 2 > 140:
            print("   " + line); line = ""
        line += n + "  "
    if line:
        print("   " + line)
    print("\ngm2bin.py 已还原，bin 已重跑恢复。")


if __name__ == "__main__":
    main()
