#!/usr/bin/env python3
# ============================================================
# dump_bin.py — 把 .bin 反汇编成人能读的样子
#
#   · 不反编译、不还原 GML，就是把字节码一条条摊开
#   · 槽号写成 T<n>：T0 = 全局/返回槽，T1..4095 = 字面量池，>=4096 = 块的局部槽
#   · 字面量引用直接写出值（字符串、整数、浮点）
#   · 跳转目标算成标签 L0000，IF/JMP 直接指标签
#   · 常见函数给个"人话"备注（读属性/写属性/实例数组/实例个数…）
#
# 用法：
#   python dump_bin.py <file.bin>              # 全部块
#   python dump_bin.py <file.bin> _OBJECT_STEP # 只看某个块
#   python dump_bin.py <file.bin> --stats      # 末尾附每个块调了哪些函数
# ============================================================

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gm2bin as g

GAME_NAME = dict((v, k) for k, v in g.GAME_ID.items())
NAT_NAME = dict((v, k) for k, v in g.NATIVE_ID.items())

MEM_TY = {0: "int", 1: "float", 2: "string", 3: "array", 4: "map", 5: "grid",
          6: "undefined", 7: "list"}
BIN_OP = {3: "+", 4: "-", 5: "*", 6: "/", 7: "%",
          8: "==", 9: "!=", 10: ">", 11: ">=", 12: "<", 13: "<=",
          20: "&", 21: "|", 22: "^", 24: "<<", 25: ">>"}


def load(path):
    d = open(path, "rb").read()
    o = 0
    sc = struct.unpack_from("<i", d, o)[0]
    o += 4
    strings = []
    for _ in range(sc):
        n = struct.unpack_from("<i", d, o)[0]
        o += 4
        strings.append(d[o:o + n].decode("utf-8", "replace"))
        o += n
    blocks = []
    while o + 8 <= len(d):
        blen, nidx = struct.unpack_from("<ii", d, o)
        o += 8
        name = strings[nidx] if 0 <= nidx < len(strings) else "<?>"
        blocks.append((name, d[o:o + blen]))
        o += blen
    return strings, blocks


def read_lits(ci, strings):
    """_VM_CONST_INIT 块 → {槽号: 值文本}"""
    out = {}
    p = 0
    while p < len(ci):
        op = ci[p]
        p += 1
        if op != 1:
            break
        dst, ty = struct.unpack_from("<iB", ci, p)
        p += 5
        if ty == 2:
            si = struct.unpack_from("<H", ci, p)[0]
            p += 2
            out[dst] = '"%s"' % (strings[si] if si < len(strings) else "?")
        elif ty == 1:
            out[dst] = repr(struct.unpack_from("<f", ci, p)[0])
            p += 4
        else:
            out[dst] = str(struct.unpack_from("<i", ci, p)[0])
            p += 4
    return out


class Dec(object):
    def __init__(self, code, lits):
        self.code = code
        self.lits = lits
        self.items = []      # (offset, kind, text, targets)
        self.labels = set()
        self.stats = {}

    def t(self, a):
        """槽 → 文本"""
        if a == 0:
            return "T0"
        if a in self.lits:
            return "%s(%s)" % (self.lits[a], "T%d" % a)
        return "T%d" % a

    def decode(self):
        code = self.code
        p = 0
        while p < len(code):
            off = p
            op = code[p]
            p += 1
            tg = []
            txt = "?%d" % op
            if op == 1:
                dst, ty = struct.unpack_from("<iB", code, p)
                p += 5
                if ty == 2:
                    si = struct.unpack_from("<H", code, p)[0]
                    p += 2
                    val = '"%s"' % (val_string(self, si))
                elif ty == 1:
                    val = repr(struct.unpack_from("<f", code, p)[0])
                    p += 4
                else:
                    val = str(struct.unpack_from("<i", code, p)[0])
                    p += 4
                txt = "T%-5d = %s   [%s]" % (dst, val, MEM_TY.get(ty, "?"))
            elif op == 2:
                a, b = struct.unpack_from("<ii", code, p)
                p += 8
                txt = "T%-5d = %s" % (a, self.t(b))
            elif op in BIN_OP:
                a, b, c = struct.unpack_from("<iii", code, p)
                p += 12
                txt = "T%-5d = %s %s %s" % (a, self.t(b), BIN_OP[op], self.t(c))
            elif op == 23:
                a, b = struct.unpack_from("<ii", code, p)
                p += 8
                txt = "T%-5d = ~%s" % (a, self.t(b))
            elif op == 19:
                a, b = struct.unpack_from("<ii", code, p)
                p += 8
                txt = "T%-5d = mem[%s]          [load from address]" % (a, self.t(b))
            elif op in (14, 18):
                fid, argc = struct.unpack_from("<HB", code, p)
                p += 3
                dst = struct.unpack_from("<i", code, p)[0]
                p += 4
                args = list(struct.unpack_from("<" + "i" * argc, code, p)) if argc else []
                p += argc * 4
                name = (GAME_NAME if op == 14 else NAT_NAME).get(fid, "?")
                self.stats[name] = self.stats.get(name, 0) + 1
                txt = self.annotate(op, name, dst, args)
            elif op == 15:
                c, ti, fi = struct.unpack_from("<iii", code, p)
                p += 12
                tg = [x for x in (ti, fi) if x >= 0]
                tt = ("L%04d" % ti) if ti >= 0 else "(fall through)"
                ff = ("L%04d" % fi) if fi >= 0 else "(fall through)"
                txt = "IF %s ? %s : %s" % (self.t(c), tt, ff)
            elif op == 16:
                ip = struct.unpack_from("<i", code, p)[0]
                p += 4
                tg = [ip]
                txt = "JMP L%04d" % ip
            elif op == 17:
                txt = "HALT                    [return: T0 holds the return value]"
            else:
                txt = "?? unknown opcode %d" % op
            for x in tg:
                self.labels.add(x)
            self.items.append((off, txt))

    def prop(self, slot):
        """字面量槽里的属性名（去掉引号，读着顺）"""
        s = self.lits.get(slot, "?")
        return s[1:-1] if len(s) > 1 and s[0] == '"' else s

    def annotate(self, op, name, dst, args):
        """常见函数写成人话；其余就是 名字(参数)"""
        tag = "CALL  " if op == 14 else "NATIVE"
        a = [self.t(x) for x in args]
        if name == "VM_GetProp" and len(args) == 2:
            who = "global" if args[0] == 0 else ("T%d" % args[0])
            return "T%-5d = get %s.%s" % (dst, who, self.prop(args[1]))
        if name == "VM_SetProp" and len(args) == 3:
            who = "global" if args[0] == 0 else ("T%d" % args[0])
            return "         set %s.%s = %s" % (who, self.prop(args[1]), a[2])
        if name == "VM_GetCurCard":
            return "T%-5d = this instance (self)" % dst
        if name == "VM_GetInstanceCount":
            return "T%-5d = instance count(%s)" % (dst, a[0] if a else "?")
        if name == "VM_GetInstanceAt":
            return "T%-5d = instance #%s of(%s)" % (dst, a[1] if len(a) > 1 else "?", a[0] if a else "?")
        if name.startswith("VM_InstArray") or name.startswith("inst_array_"):
            tail = name[3:] if name.startswith("VM_") else name
            return "T%-5d = %s(%s)" % (dst, tail, ", ".join(a))
        if name == "VM_CreateInstance":
            return "T%-5d = create instance(%s)" % (dst, ", ".join(a))
        if dst < 0:
            return "         %s(%s)   [%s]" % (name, ", ".join(a), tag.strip())
        return "T%-5d = %s(%s)   [%s]" % (dst, name, ", ".join(a), tag.strip())

    def dump(self):
        for off, txt in self.items:
            mark = "L%04d:" % off if off in self.labels else "       "
            print("  %s %5d  %s" % (mark, off, txt))


def val_string(dec, si):
    return dec.strings[si] if si < len(dec.strings) else "?"


def main():
    if len(sys.argv) < 2:
        print("usage: dump_bin.py <file.bin> [block] [--stats]")
        return 1
    path = sys.argv[1]
    want = None
    stats = False
    for a in sys.argv[2:]:
        if a == "--stats":
            stats = True
        else:
            want = a

    strings, blocks = load(path)
    print("file   : %s" % path)
    print("strings: %d    blocks: %d" % (len(strings), len(blocks)))
    print("")
    print("--- string pool ---")
    for i, s in enumerate(strings):
        print("  #%-4d %s" % (i, s))
    print("")
    if blocks:
        lits = read_lits(blocks[0][1], strings)
        print("--- literal pool (slot -> value, written by _VM_CONST_INIT) ---")
        for slot in sorted(lits):
            print("  T%-5d = %s" % (slot, lits[slot]))
        print("")

    for i, (name, code) in enumerate(blocks):
        if want and name != want:
            continue
        print("=== [%d] %s  (%d bytes) ===" % (i, name, len(code)))
        if name == "_VM_CONST_INIT":
            print("  (literal init, see the table above)")
            print("")
            continue
        dec = Dec(code, lits)
        dec.strings = strings
        dec.decode()
        dec.dump()
        if stats:
            print("  --- functions used ---")
            for fn, c in sorted(dec.stats.items(), key=lambda kv: -kv[1]):
                print("     %-30s %d" % (fn, c))
        print("")
    return 0


if __name__ == "__main__":
    sys.exit(main())
