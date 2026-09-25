#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sync_vmfuncs.py — 把 vmfuncs_spec.json 里声明的新 VM 函数同步到编辑器各处：

    compiler_defs.h          — 编译器函数表（ID 自动递增）
    linter.go                — 函数检测签名表 + 悬停中文描述
    linter.go                — 悬停函数原型 funcSigs（整块从 help.md 自动重抽）
    frontend/src/lablang.js  — 函数高亮与自动补全
    allfuncs_test.go         — 全函数编译测试用例

以下两个文件由人工另行维护，本脚本不处理：
    help.md                  — 帮助文档函数表（各小节格式灵活）
    app.go                   — 启动更新日志（更新提示）

注意：funcSigs 是「整块重新生成」的，来源是 help.md 里函数表行的
`VM_X(参数名...)`，所以**先更新 help.md 再跑本脚本**，否则新函数没有原型。
help.md 里没写成函数表行的函数不会进 funcSigs，悬停时退回显示参数个数。

用法（在项目根目录）:
    python sync_vmfuncs.py                # 实际写入
    python sync_vmfuncs.py --dry-run      # 只打印计划，不写入
    python sync_vmfuncs.py 其他声明.json  # 指定声明文件

幂等：已同步过的函数自动跳过；修改已有函数的 desc 后重跑，会更新
linter.go 的悬停描述与 compiler_defs.h 的注释（签名本身不会改动）。
参数类型: int / float / string / any
"""

import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

CPP_TYPE = {"int": "PT_INT", "float": "PT_FLOAT", "string": "PT_STRING", "any": "PT_ANY"}
GO_TYPE = {"int": "ptInt", "float": "ptFloat", "string": "ptString", "any": "ptAny"}
TEST_VAL = {"int": "1", "float": "0.5", "string": '"s"', "any": "1"}
FUNC_ID_RE = re.compile(r"// (\d+) —")


# ---- 基础工具 ----

def pad(s, n):
    """右侧补空格到宽度 n"""
    return s + " " * max(0, n - len(s))


def read_lines(path):
    with open(path, encoding="utf-8") as f:
        return f.read().split("\n")


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def fail(msg):
    print("[sync_vmfuncs] 错误: %s" % msg, file=sys.stderr)
    sys.exit(1)


def insert_before(lines, idx, entries):
    return lines[:idx] + entries + lines[idx:]


def find_block_close(lines, mark):
    """从包含 mark 的行开始按 {}[] 计数，返回深度归零那一行（块的结束行）"""
    depth, started = 0, False
    for i, ln in enumerate(lines):
        if not started:
            if mark in ln:
                started = True
            else:
                continue
        depth += ln.count("{") - ln.count("}")
        depth += ln.count("[") - ln.count("]")
        if depth == 0:
            return i
    return -1


def find_backtick_close(lines, mark):
    """从包含 mark 的行开始，返回其后第一个含反引号的行（Go 原始字符串结束行）"""
    started = False
    for i, ln in enumerate(lines):
        if not started:
            if mark in ln:
                started = True
            continue
        if "`" in ln:
            return i
    return -1


# ---- 各文件内容生成 ----

def cpp_entry(fs, fid):
    """compiler_defs.h 的 FUNC_DEFS 条目"""
    count = len(fs.get("args", []))
    if fs.get("maxArgs") == -1:
        count = -1
    args = fs.get("args", [])
    ret = fs.get("ret", "")
    line = pad('    {"%s",' % fs["name"], 34)
    if args or ret:
        line += pad(str(count) + ",", 6)
    else:
        line += pad(str(count), 6)
    if count != -1 and args:
        ts = "{" + ", ".join(CPP_TYPE[t] for t in args) + "},"
        line += pad(ts, 38)
    elif ret:
        line += pad("{},", 38)
    if ret:
        line += pad(CPP_TYPE[ret] + ",", 31)
    line = line.rstrip() + "},"
    return pad(line, 74) + "      // %d — %s" % (fid, fs["desc"])


def linter_table_entry(fs):
    """linter.go funcTable 条目"""
    mn = fs.get("minArgs", len(fs.get("args", [])))
    mx = fs.get("maxArgs", len(fs.get("args", [])))
    if fs.get("args") and mx != -1:
        types_part = "[]paramType{" + ", ".join(GO_TYPE[t] for t in fs["args"]) + "}"
    else:
        types_part = "nil"
    return pad('"%s":' % fs["name"], 26) + "{%d, %d, %s, nil}," % (mn, mx, types_part)


def linter_desc_entry(fs):
    """linter.go funcDescs 条目"""
    return pad('"%s":' % fs["name"], 30) + '"%s",' % fs["desc"]


def test_call_line(fs):
    """allfuncs_test.go 的调用行"""
    call = fs.get("testCall")
    if not call:
        if fs.get("args"):
            call = "%s(%s)" % (fs["name"], ", ".join(TEST_VAL[t] for t in fs["args"]))
        else:
            call = "%s()" % fs["name"]
    return "    " + call


def next_func_id(lines):
    """从 compiler_defs.h 现有注释中取最大函数 ID + 1"""
    mx = 0
    for ln in lines:
        for m in FUNC_ID_RE.finditer(ln):
            mx = max(mx, int(m.group(1)))
    return mx + 1


# ---- 各文件补丁 ----

def patch_compiler_defs(lines, funcs):
    fid = next_func_id(lines)
    close_idx = find_block_close(lines, "FUNC_DEFS")
    out, log = [], []
    nid = fid      # 只对「本次新增」的条目自增。
                   # 不能用 enumerate 的下标：funcs 里已经存在的函数也占下标，
                   # 那样一次加多个新函数时编号会越飘越远（曾经 VM_SpriteExists 被写成 134）。
    for fs in funcs:
        idx = next((j for j, ln in enumerate(lines) if '{"%s",' % fs["name"] in ln), -1)
        if idx >= 0:
            m = FUNC_ID_RE.search(lines[idx])
            if m and fs.get("desc"):
                new_line = lines[idx][:m.start()] + "// %s — %s" % (m.group(1), fs["desc"])
                if new_line == lines[idx]:
                    log.append("  %-20s 跳过（已存在）" % fs["name"])
                else:
                    lines[idx] = new_line
                    log.append("  %-20s 更新描述注释" % fs["name"])
            else:
                log.append("  %-20s 跳过（已存在）" % fs["name"])
            continue
        out.append(cpp_entry(fs, nid))
        log.append("  %-20s 新增 ID=%d" % (fs["name"], nid))
        nid += 1
    if out:
        lines = insert_before(lines, close_idx, out)
    return lines, log


def patch_linter_table(lines, funcs):
    return _patch_go_map(lines, funcs, "var funcTable = map[string]fnSpec{", linter_table_entry)


def patch_linter_descs(lines, funcs):
    return _patch_go_map(lines, funcs, "var funcDescs = map[string]string{", linter_desc_entry, update=True)


def _patch_go_map(lines, funcs, mark, entry, update=False):
    start_idx = next(i for i, ln in enumerate(lines) if mark in ln)
    close_idx = find_block_close(lines, mark)
    block_lines = lines[start_idx:close_idx + 1]
    out, log = [], []
    for fs in funcs:
        idx = next((i for i, ln in enumerate(block_lines) if '"%s":' % fs["name"] in ln), -1)
        if idx >= 0:
            if not update:
                log.append("  %-20s 跳过（已存在）" % fs["name"])
                continue
            want = "\t" + entry(fs)
            if block_lines[idx].strip() == want.strip():
                log.append("  %-20s 跳过（描述未变）" % fs["name"])
            else:
                lines[start_idx + idx] = want
                log.append("  %-20s 更新描述" % fs["name"])
            continue
        out.append("\t" + entry(fs))
        log.append("  %-20s 新增" % fs["name"])
    if out:
        lines = insert_before(lines, close_idx, out)
    return lines, log


def patch_lablang(lines, funcs):
    names, log = [], []
    for fs in funcs:
        if fs["name"] in "\n".join(lines):
            log.append("  %-20s 跳过（已存在）" % fs["name"])
            continue
        names.append('"%s"' % fs["name"])
        log.append("  %-20s 新增" % fs["name"])
    if names:
        close_idx = find_block_close(lines, "FUNC_NAMES = [")
        lines = insert_before(lines, close_idx, ["  " + ", ".join(names) + ","])
    return lines, log


def patch_test_file(lines, funcs):
    close_idx = find_backtick_close(lines, "code := ")
    # 测试代码串的最后是事件块的右花括号，调用必须插进块内（linter 要求 _VM_* 块内调用）
    block_end = max(i for i in range(close_idx) if lines[i].strip() == "}")
    close_idx = block_end
    out, log = [], []
    for fs in funcs:
        if fs["name"] in "\n".join(lines):
            log.append("  %-20s 跳过（已存在）" % fs["name"])
            continue
        out.append(test_call_line(fs))
        log.append("  %-20s 新增 %s" % (fs["name"], test_call_line(fs)))
    if out:
        lines = insert_before(lines, close_idx, out)
    return lines, log


# ---- 悬停函数原型（来源：help.md 函数表） ----

SIGS_MARK = "var funcSigs = map[string]string{"
# 函数表行：| `VM_X(参数名...)` | ...    只认「代码段紧接着就是表格竖线」的行，
# 避免把正文举例（例如 VM_SetProp(self, "current_level", nl)）当成原型。
SIG_ROW_RE = re.compile(r"^\|\s*`(VM_\w+)\((.*)\)`\s*\|")


def help_sigs():
    """从 help.md 的函数表抽 `VM_X(参数名...)` 原型；同一个函数取第一次出现"""
    sigs = {}
    for ln in read_lines("help.md"):
        m = SIG_ROW_RE.match(ln)
        if not m:
            continue
        name, inner = m.group(1), m.group(2)
        if name not in sigs:
            sigs[name] = "%s(%s)" % (name, inner)
    return sigs


def patch_linter_sigs(lines, funcs):
    """整块重新生成 funcSigs（幂等：内容不变就不动文件）"""
    del funcs  # 原型只认 help.md，和本次新增的函数列表无关
    sigs = help_sigs()
    if not sigs:
        fail("help.md 里没抽到任何函数原型，检查 help.md 是否在项目根目录")

    body = ['\t"%s": %s,' % (n, json.dumps(sigs[n], ensure_ascii=False)) for n in sorted(sigs)]

    start_idx = next((i for i, ln in enumerate(lines) if ln.startswith(SIGS_MARK)), -1)
    if start_idx < 0:
        # 首次运行：插在 funcDescs 块后面
        anchor = next((i for i, ln in enumerate(lines) if ln.startswith("var funcDescs = map[string]string{")), -1)
        if anchor < 0:
            fail("linter.go 里找不到 funcDescs，无法插入 funcSigs")
        close_idx = next(i for i in range(anchor + 1, len(lines)) if lines[i].strip() == "}")
        block = [
            "",
            "// funcSigs 函数原型（由 sync_vmfuncs.py 从 help.md 函数表自动提取）",
            "// 悬停提示的标题行，例如 VM_GetProp(实例ID, \"属性名\")",
            SIGS_MARK,
        ] + body + ["}", ""]
        log = ["  funcSigs 新建：%d 个原型" % len(sigs)]
        return insert_before(lines, close_idx + 1, block), log

    end_idx = next((i for i in range(start_idx + 1, len(lines)) if lines[i].strip() == "}"), -1)
    if end_idx < 0:
        fail("linter.go 里 funcSigs 块没有收尾的 }")
    old = lines[start_idx + 1:end_idx]
    if old == body:
        return lines, []
    new_lines = lines[:start_idx + 1] + body + lines[end_idx:]
    return new_lines, ["  funcSigs 重新生成：%d 个原型（%d 行 → %d 行）" % (len(sigs), len(old), len(body))]


# ---- 主流程 ----

def main():
    dry = False
    spec_path = "vmfuncs_spec.json"
    for a in sys.argv[1:]:
        if a == "--dry-run":
            dry = True
        else:
            spec_path = a

    with open(spec_path, encoding="utf-8") as f:
        spec = json.load(f)
    funcs = spec.get("functions", [])

    # 声明校验
    for fs in funcs:
        for t in fs.get("args", []):
            if t not in CPP_TYPE:
                fail("函数 %s 参数类型非法: %r（应为 int/float/string/any）" % (fs["name"], t))
        ret = fs.get("ret", "")
        if ret and ret not in CPP_TYPE:
            fail("函数 %s 返回类型非法: %r（应为 int/float/string/any）" % (fs["name"], ret))
    if not funcs:
        print("声明文件中没有函数，无事可做。")
        return

    targets = [
        ("compiler_defs.h", patch_compiler_defs),
        ("linter.go", patch_linter_table),
        ("linter.go", patch_linter_descs),
        ("frontend/src/lablang.js", patch_lablang),
        ("allfuncs_test.go", patch_test_file),
        ("linter.go", patch_linter_sigs),
    ]

    print("函数 %d 个%s" % (len(funcs), "，dry-run 模式（不写入）" if dry else ""))
    for path, patch in targets:
        lines = read_lines(path)
        orig = "\n".join(lines)
        new_lines, log = patch(lines, funcs)
        if "\n".join(new_lines) == orig:
            continue
        print("更新 " + path)
        for l in log:
            print(l)
        if not dry:
            write_lines(path, new_lines)
    if dry:
        print("\n未写入任何文件（--dry-run）。去掉参数即可实际写入。")


if __name__ == "__main__":
    main()
