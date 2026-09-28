#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================
# make_kernel_view.py — 生成 kernel/ 目录树（**构建视图，可重新生成**）
#
#   kernel/<类型>/<对象名>/<事件>.gml    ← 从 objects/ 复制（原文件保留）
#   kernel/function/<函数名>.gml         ← 从 scripts/ 里拆出来（一个文件一个函数）
#
#   收哪些对象：能编成 bin 的 + GM2BIN 明确跳过的（hot 覆盖的 / 白名单父对象）。
#   不收：编不过的 UI/基础设施（menu / laboratory / basic / button / other）。
#
#   ⚠ 这是**视图**：改这里不会改 objects/，重新跑本脚本会覆盖 kernel/。
#     要改真源去改 objects/ 和 scripts/。
# ============================================================

import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
OBJ = os.path.join(ROOT, "objects")
SCRIPTS = os.path.join(ROOT, "scripts")
KERNEL = os.path.join(ROOT, "kernel")

GROUP_DIRS = (("Cards", "card"), ("Enemy", "enemy"), ("Bullet", "bullet"),
              ("Map Object", "mapobj"), ("Player", "player"), ("Effects", "effects"))
# 不需要转的（hot 覆盖 + 父对象白名单）：逻辑已经在 C++ 里，**不收进视图**
SKIP = {"obj_card_mod", "obj_card_slot", "obj_enemy_mod", "obj_bullet_mod",
        "obj_player_character", "obj_Bullet_Screen_Management", "obj_Homing_Bullet_Management",
        "obj_weapon_mod", "obj_gem_mod", "obj_effect_mod", "obj_shell",
        "obj_enemy_parent", "obj_card_parent", "obj_bullet_parent"}
# 不收的对象（UI / 基础设施）
DROP_GROUPS = {"menu", "laboratory", "basic", "button", "other"}


def group_of(name):
    yy = os.path.join(OBJ, name, name + ".yy")
    if not os.path.isfile(yy):
        return ""
    src = open(yy, "rb").read().decode("utf-8-sig", "replace")
    m = re.search(r'"parent"\s*:\s*\{(.*?)\}', src, re.S)
    if not m:
        return ""
    p = re.search(r'"path"\s*:\s*"([^"]+)"', m.group(1))
    if not p:
        return ""
    parts = p.group(1).split("/")
    if len(parts) < 3:
        return ""
    top = parts[2][:-3] if parts[2].endswith(".yy") else parts[2]
    for gname, gdir in GROUP_DIRS:
        if top == gname:
            return gdir
    return re.sub(r"[^a-z0-9]+", "_", top.lower()).strip("_") or "other"


def yyp_objects():
    yyp = os.path.join(ROOT, "FVM-reborn.yyp")
    src = open(yyp, "rb").read().decode("utf-8-sig", "replace")
    seen, out = set(), []
    for nm in re.findall(r'"path"\s*:\s*"objects/([^/"]+)/', src):
        if nm not in seen:
            seen.add(nm); out.append(nm)
    return sorted(out)


def split_functions():
    """把 scripts/ 里每个 function 的源码切出来（大括号配对），返回 {name: 源码}"""
    out = {}
    for root, dirs, files in os.walk(SCRIPTS):
        for f in files:
            if not f.endswith(".gml"):
                continue
            src = open(os.path.join(root, f), encoding="utf-8-sig", errors="replace").read()
            for m in re.finditer(r'(?m)^[ \t]*function\s+(\w+)\s*\(', src):
                name = m.group(1)
                if name in out:
                    continue
                start = src.rfind("\n", 0, m.start()) + 1
                i = src.find("{", m.end())
                if i < 0:
                    continue
                depth, j = 0, i
                while j < len(src):
                    if src[j] == "{":
                        depth += 1
                    elif src[j] == "}":
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                out[name] = src[start:j + 1].rstrip() + "\n"
    return out


def main():
    # 需要的函数名：gm2bin 的 native 占位里、scripts/ 有定义的那些
    names_h = open(os.path.join(HERE, "names.h"), encoding="utf-8").read()
    m = re.search(r'#define PLACEHOLDER_FN_LIST\(X\) \\\n(.*?)\n\n', names_h, re.S)
    ph = set(re.findall(r'X\(([A-Za-z_][A-Za-z0-9_]*)\)', m.group(1)))
    funcs = split_functions()
    want = [n for n in ph if n in funcs]
    # 依赖闭包：函数体里调到的、scripts/ 里也有定义的函数，递归收进来
    seen = set(want)
    queue = list(want)
    while queue:
        n = queue.pop()
        for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', funcs.get(n, "")):
            c = m.group(1)
            if c in funcs and c not in seen:
                seen.add(c); queue.append(c)
    want = sorted(seen)

    if os.path.isdir(KERNEL):
        shutil.rmtree(KERNEL)
    os.makedirs(os.path.join(KERNEL, "function"))

    n_obj = n_file = 0
    for name in yyp_objects():
        src_dir = os.path.join(OBJ, name)
        if not os.path.isdir(src_dir):
            continue
        if name in SKIP:                     # hot 覆盖 / 白名单父对象：不需要转，不收
            continue
        g = group_of(name)
        if not g or g in DROP_GROUPS:        # UI / 基础设施：也不收
            continue
        dst_dir = os.path.join(KERNEL, g, name)
        os.makedirs(dst_dir, exist_ok=True)
        for f in sorted(os.listdir(src_dir)):
            if f.endswith(".gml"):
                shutil.copyfile(os.path.join(src_dir, f), os.path.join(dst_dir, f))
                n_file += 1
        n_obj += 1

    for n in want:
        open(os.path.join(KERNEL, "function", n + ".gml"), "w", encoding="utf-8", newline="\n").write(funcs[n])

    readme = """# kernel/ —— 构建视图（自动生成，别手改）

生成器：`transformGML2txt/make_kernel_view.py`

    kernel/<类型>/<对象名>/<事件>.gml     ← 从 objects/ 复制
    kernel/function/<函数名>.gml          ← 从 scripts/ 拆出来（一个文件一个函数）

⚠ **不是真源**：改这里不会改 objects/scripts；重跑生成器会**整个删掉重建**
（连转换产物 `.bin` / `.callbin` 一起没），要重新转换。改真源请去 `objects/`、`scripts/`。

---

## 一、重新生成视图

```bash
python transformGML2txt/make_kernel_view.py
```

收什么：能编成 bin 的对象 + `function/` 里那批（含**依赖闭包**）；
不收：hot 覆盖的 12 个对象、白名单父对象、UI/基础设施（menu / laboratory / basic / button / other）。

## 二、转换（**两遍**，顺序不能反）

```bash
# 第 1 遍：函数 → .callbin（对象侧编译时要先认得这些名字）
python transformGML2txt/gm2bin.py kernel/function --recursive --out kernel

# 第 2 遍：对象 → .bin，并把 callbin 目录喂进去
#   → 那批函数的调用点会编成 VM_CallFunc(...)，而不是落到 native 占位转发
python transformGML2txt/gm2bin.py kernel --recursive --out kernel --callbins kernel/function
```

⚠ **第 2 遍的 `--callbins` 不能漏** —— 漏了就报一堆 `function "X" has no builtin`，
因为已经编好的 callbin 名字没被认到。

## 三、产物布局（和源放一起，同名）

```
kernel/card/obj_aquarius_elve/
      Create_0.gml  Step_0.gml …          ← 源
      obj_aquarius_elve.bin               ← 产物（一个对象一个）
kernel/function/
      select_shovel.gml                   ← 源（一个函数一个）
      select_shovel.callbin               ← 产物
```

产物不入库：`.gitignore` 里有 `kernel/**/*.bin`、`kernel/**/*.callbin`。

## 四、递归转换模式本身

```bash
gm2bin.py <目录> --recursive [--out <输出目录>] [--callbins <目录>]
```
- 含 **单个 function** 的 .gml → 编成 `.callbin`（文件名 = 函数名）
- 含事件 .gml 的目录 → 编成一个对象 `.bin`（对象名 = 目录名）
- 输出镜像输入树（相对路径）

## 五、常用检查

```bash
transformGML2txt/vm.exe <某个.bin> _VM_CONST_INIT     # 装载空跑一遍
transformGML2txt/vm.exe --selftest-instbox            # 实例容器自检（数组/map 双模式 + 游标）
```
"""
    open(os.path.join(KERNEL, "README.md"), "w", encoding="utf-8", newline="\n").write(readme)
    print("kernel/ 生成完毕：%d 个对象（%d 个 .gml）+ function/ %d 个函数文件（已排除 %d 个不需要转的）"
          % (n_obj, n_file, len(want), len(SKIP)))
    # 按类型统计
    import collections
    c = collections.Counter()
    for g in os.listdir(KERNEL):
        p = os.path.join(KERNEL, g)
        if os.path.isdir(p):
            c[g] = len([d for d in os.listdir(p)]) if g != "function" else len(os.listdir(p))
    print("   " + ", ".join("%s %d" % (k, c[k]) for k in sorted(c)))


if __name__ == "__main__":
    main()
