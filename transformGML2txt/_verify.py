import struct
import subprocess
import sys

sys.path.insert(0, "transformGML2txt")
import gm2bin as g

GAME = {v: k for k, v in g.GAME_ID.items()}
NAT = {v: k for k, v in g.NATIVE_ID.items()}


def load(obj):
    subprocess.run([sys.executable, r"transformGML2txt\gm2bin.py", obj, r"transformGML2txt\_v.bin"],
                   capture_output=True)
    d = open(r"transformGML2txt\_v.bin", "rb").read()
    o = 0
    sc = struct.unpack_from("<i", d, o)[0]
    o += 4
    strings = []
    for _ in range(sc):
        n = struct.unpack_from("<i", d, o)[0]
        o += 4
        strings.append(d[o:o + n].decode("utf-8"))
        o += n
    blocks = []
    while o + 8 <= len(d):
        blen, nidx = struct.unpack_from("<ii", d, o)
        o += 8
        blocks.append((strings[nidx], d[o:o + blen]))
        o += blen
    return strings, blocks


def verify(obj):
    strings, blocks = load(obj)
    ci = blocks[0][1]
    p = 0
    lits = {}
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
            lits[dst] = strings[si]
        elif ty == 1:
            lits[dst] = struct.unpack_from("<f", ci, p)[0]
            p += 4
        else:
            lits[dst] = struct.unpack_from("<i", ci, p)[0]
            p += 4
    N = max(lits) if lits else 0
    print("== %s  字面量 %d 个（槽 1..%d）" % (obj, len(lits), N))
    bad = 0
    for name, code in blocks[1:]:
        p = 0
        dsts = set()
        refs = set()
        names_used = []
        while p < len(code):
            op = code[p]
            p += 1
            args = []
            if op == 1:
                dst, ty = struct.unpack_from("<iB", code, p)
                p += 5 + (2 if ty == 2 else 4)
                dsts.add(dst)
            elif op == 2:
                a, b = struct.unpack_from("<ii", code, p)
                p += 8
                dsts.add(a)
                args = [b]
            elif op in (3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13) or op in (20, 21, 22, 24, 25):
                a, b, c = struct.unpack_from("<iii", code, p)
                p += 12
                dsts.add(a)
                args = [b, c]
            elif op in (19, 23):
                a, b = struct.unpack_from("<ii", code, p)
                p += 8
                dsts.add(a)
                args = [b]
            elif op in (14, 18):
                fid, argc = struct.unpack_from("<HB", code, p)
                p += 3
                dst = struct.unpack_from("<i", code, p)[0]
                p += 4
                a = list(struct.unpack_from("<" + "i" * argc, code, p)) if argc else []
                p += argc * 4
                if dst >= 0:
                    dsts.add(dst)
                args = a
                fn = (GAME if op == 14 else NAT).get(fid, "?")
                if fn in ("VM_GetProp", "VM_SetProp") and len(a) >= 2:
                    nm = lits.get(a[1], "?")
                    names_used.append("%s(%s%s)" % (fn, "global" if a[0] == 0 else "inst", "." + str(nm) if fn == "VM_GetProp" else "." + str(nm)))
            elif op == 15:
                a, b, c = struct.unpack_from("<iii", code, p)
                p += 12
                args = [a]
            elif op == 16:
                p += 4
            elif op == 17:
                pass
            else:
                print("   !! 未知操作码 %d" % op)
                bad += 1
                break
            for x in args:
                if x > 0:
                    refs.add(x)
        loc = sorted(x for x in dsts | refs if x >= 4096)
        gap = sorted(x for x in dsts | refs if N < x < 4096)
        over = sorted(x for x in dsts | refs if x >= 32768)
        print("   块 %-22s 局部槽 %s..%s  字面量引用 %d 处  字面量区外/局部区前: %s %s" %
              (name, loc[0] if loc else "-", loc[-1] if loc else "-",
               len([x for x in refs if x <= N]), gap or "无", "越界!" if over else ""))
        if gap or over:
            bad += 1
        if names_used:
            print("      属性读写样例: %s" % ", ".join(names_used[:6]))
    print("   结论: %s" % ("有问题" if bad else "槽位布局没问题"))


for o in sys.argv[1:]:
    verify(o)
