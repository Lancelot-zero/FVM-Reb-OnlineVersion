// ============================================================
// vm.cpp — 执行循环 + 装载 + 入口
//   声明都在 vm.h，游戏 API 在 vm_api.cpp，内部函数在 vm_native.cpp，
//   容器/二维表在 vm_container.cpp
// ============================================================

#include "vm.h"

// ============================================================
// 块表
// ============================================================
const Block* Vm::find_block_sid(int sid) const {
    std::unordered_map<int, Block>::const_iterator it = blocks.find(sid);
    return (it == blocks.end()) ? NULL : &it->second;
}

// 手上只有名字：先换池下标（不在池里 = 这个 VM 不可能有这个名字的块）
const Block* Vm::find_block(const std::string& name) const {
    int sid = str_find(name);
    return (sid < 0) ? NULL : find_block_sid(sid);
}

bool Vm::run_named(const std::string& name) {
    bool ok;
    if (run_hot_block(*this, obj, name)) {
        ok = true;                                       // 有硬编码的块 → 走 C++ 版
    } else {
        const Block* b = find_block(name);
        if (!b) {
            printf("[vm] block not found: %s / %s\n", obj.c_str(), name.c_str());
            return false;
        }
        ok = run(*b);
    }
    return ok;
}

// 同 run_named，但池下标已经在手上（字节码 run_block 那条路）
bool Vm::run_named_sid(int sid) {
    int obj_sid = str_find(obj);           // 硬编码块的 key 是"对象 + 块"两个池下标
    if (obj_sid >= 0 && run_hot_block_sid(*this, obj_sid, sid)) return true;
    const Block* b = find_block_sid(sid);
    if (!b) {
        printf("[vm] block not found: %s / %s\n", obj.c_str(), str_at(sid).c_str());
        return false;
    }
    return run(*b);
}

// ============================================================
// 执行一个块
// ============================================================
bool Vm::run(const Block& blk) {
    const uint8_t* base = blk.code.data();
    const uint8_t* p    = base;
    const uint8_t* end  = base + blk.code.size();
    int* m = (int*)mem;

    while (p < end) {
        // 强制结束旗子（callbin 触上限时置上）：立刻收手，一层层 false 传回去
        if (g_call_abort) return false;
        switch (*p++) {

        case OP_ASSIGN: {
            int     dst = ld32(p); p += 4;
            uint8_t ty  = *p++;
            Value v; v.t = ty; v.v = 0;
            if (ty == MEM_STRING) {
                v.v = (int)ld16(p); p += 2;
            } else if (ty == MEM_FLOAT) {
                float f = ldf(p); p += 4;
                memcpy(&v.v, &f, 4);
            } else {
                v.v = ld32(p); p += 4;
            }
            wr_v(*this, dst, v);      // 走 wr_v 才会把 dst 里原来那个容器引用放掉
            break;
        }

        case OP_COPY: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            wr_v(*this, dst, rd_v(*this, src));   // 整 8 字节搬家 + 引用计数
            break;
        }

        case OP_ADD: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x + y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] + m[b * 2 + 1];
            }
            break;
        }

        case OP_SUB: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x - y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] - m[b * 2 + 1];
            }
            break;
        }

        case OP_MUL: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x * y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] * m[b * 2 + 1];
            }
            break;
        }

        case OP_DIV: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = (y == 0.0f) ? 0.0f : (x / y);
            } else {
                int x = m[a * 2 + 1], y = m[b * 2 + 1];
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = (y != 0) ? (x / y) : 0;   // 整型除法：向零截断
            }
            break;
        }

        case OP_MOD: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            // mod 只接整数（和 GML 一致）：任何一个操作数是浮点就当错误，停掉这个块
            if (m[a * 2] == MEM_FLOAT || m[b * 2] == MEM_FLOAT) {
                printf("[vm] mod with a non-integer operand (block %s)\n", blk.name.c_str());
                return false;
            }
            int x = m[a * 2 + 1], y = m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (y != 0) ? (x % y) : 0;
            break;
        }

        // 比的是槽里的值：数字比数值，字符串比字符串池下标
        case OP_EQ: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x == y) ? 1 : 0;
            break;
        }

        case OP_NEQ: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x != y) ? 1 : 0;
            break;
        }

        case OP_GT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x > y) ? 1 : 0;
            break;
        }

        case OP_GTE: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x >= y) ? 1 : 0;
            break;
        }

        case OP_LT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x < y) ? 1 : 0;
            break;
        }

        case OP_LTE: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x <= y) ? 1 : 0;
            break;
        }

        case OP_CALL: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < GAME_FN_COUNT) kGameFns[fid](*this, args, argc, dst);
            else printf("[vm] unknown game function id = %d\n", fid);
            break;
        }

        case OP_NATIVE: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < NATIVE_FN_COUNT) kNativeFns[fid](*this, args, argc, dst);
            else printf("[vm] unknown native function id = %d\n", fid);
            break;
        }

        case OP_IF: {
            int cond = ld32(p); p += 4;
            int tip  = ld32(p); p += 4;
            int fip  = ld32(p); p += 4;
            int ty = m[cond * 2];
            bool t;
            // GML 的真值：<= 0.5 为假、> 0.5 为真（手册 Boolean 一节）
            if (ty == MEM_INT)        t = (m[cond * 2 + 1] > 0);
            else if (ty == MEM_FLOAT) t = (*(float*)(m + cond * 2 + 1) > 0.5f);
            else                      t = false;    // 非数字：GML 的 is_real 为假 → 当 0
            int tgt = t ? tip : fip;
            if (tgt != -1) p = base + tgt;          // -1 = 不跳转，往下走
            break;
        }

        case OP_JMP:
            p = base + ld32(p);
            break;

        case OP_HALT:
            return true;

        // 取地址处的值：src 槽里存的是内存地址，把那个地址的 8 字节读到 dst
        case OP_LOAD_ELEM: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            int a   = m[src * 2 + 1];
            if (a >= 0 && a + 8 <= MEM_SIZE * 8) memcpy(m + dst * 2, mem + a, 8);
            break;
        }

        // ── 位运算：都在槽的「值」段上做，结果一律是 INT ──
        case OP_BAND: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] & m[b * 2 + 1];
            break;
        }

        case OP_BOR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] | m[b * 2 + 1];
            break;
        }

        case OP_BXOR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] ^ m[b * 2 + 1];
            break;
        }

        case OP_BNOT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = ~m[a * 2 + 1];
            break;
        }

        // 移位量先按 32 位取模：槽是 int32，移 32 位以上在 C++ 里是未定义行为
        case OP_SHL: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] << (m[b * 2 + 1] & 31);
            break;
        }

        case OP_SHR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] >> (m[b * 2 + 1] & 31);
            break;
        }
        default:
            printf("[vm] unknown opcode %d (block %s)\n", (int)*(p - 1), blk.name.c_str());
            return false;
        }
    }
    return true;
}

// ============================================================
// 装载期：把 bin 自带的字符串池并进全局池，并把字节码里内联的字符串常量
//   改写成全局下标。
//   只有 OP_ASSIGN 会内联字符串下标（[dst i32][ty u8][idx u16]），别的指令只带槽号。
//   这里走一遍指令流：没正好走到块尾就说明指令布局和这份表对不上 → 报错返回 false。
//   宁可装载失败，也不能让错位的下标进池 —— 那是静默改语义。
// ============================================================
static int op_fixed_len(uint8_t op) {
    switch (op) {
    case OP_COPY: return 8;
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
    case OP_EQ: case OP_NEQ: case OP_GT: case OP_GTE: case OP_LT: case OP_LTE:
    case OP_IF:
    case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR: return 12;
    case OP_JMP:     return 4;
    case OP_HALT:    return 0;
    case OP_LOAD_ELEM: case OP_BNOT: return 8;
    default:         return -1;      // 未知，或变长（OP_ASSIGN / OP_CALL / OP_NATIVE）
    }
}

bool remap_block_strings(std::vector<Block>& blocks,
                         const std::vector<std::string>& local_pool) {
    // 先把局部池并进全局池，拿到"局部下标 → 全局下标"
    std::vector<int> remap(local_pool.size());
    for (size_t i = 0; i < local_pool.size(); i++) {
        remap[i] = intern_str(local_pool[i]);
        if (remap[i] > 0xFFFF) {
            printf("[vm] 全局字符串池已超过 65535，OP_ASSIGN 的 u16 装不下\n");
            return false;
        }
    }

    for (size_t bi = 0; bi < blocks.size(); bi++) {
        std::vector<uint8_t>& c = blocks[bi].code;
        const size_t n = c.size();
        size_t p = 0;
        while (p < n) {
            uint8_t op = c[p++];
            if (op == OP_ASSIGN) {
                if (p + 5 > n) { printf("[vm] 块 %s：指令被截断\n", blocks[bi].name.c_str()); return false; }
                p += 4;                                   // dst
                uint8_t ty = c[p++];
                if (ty == MEM_STRING) {
                    if (p + 2 > n) { printf("[vm] 块 %s：字符串下标被截断\n", blocks[bi].name.c_str()); return false; }
                    int idx = (int)ld16(&c[p]);
                    if (idx < 0 || idx >= (int)remap.size()) {
                        printf("[vm] 块 %s：字符串下标 %d 越界（本 bin 池只有 %zu 条）\n",
                               blocks[bi].name.c_str(), idx, remap.size());
                        return false;
                    }
                    uint16_t g = (uint16_t)remap[(size_t)idx];
                    c[p]     = (uint8_t)(g & 0xFF);
                    c[p + 1] = (uint8_t)(g >> 8);
                    p += 2;
                } else {
                    p += 4;                               // 浮点 / 整数
                }
            } else if (op == OP_CALL || op == OP_NATIVE) {
                if (p + 3 > n) { printf("[vm] 块 %s：调用指令被截断\n", blocks[bi].name.c_str()); return false; }
                p += 2;                                   // fid u16
                int argc = (int)c[p++];
                if (p + 4 + (size_t)argc * 4 > n) {
                    printf("[vm] 块 %s：调用指令的实参被截断\n", blocks[bi].name.c_str());
                    return false;
                }
                p += 4 + (size_t)argc * 4;                // dst + args
            } else {
                int k = op_fixed_len(op);
                if (k < 0 || p + (size_t)k > n) {
                    printf("[vm] 块 %s：未知操作码 %d（字符串下标重写中止）\n",
                           blocks[bi].name.c_str(), (int)op);
                    return false;
                }
                p += (size_t)k;
            }
        }
    }
    return true;
}

// ============================================================
// .bin / .callbin 共用的那半截：从 d[off] 开始读"字符串池 + 块表"
//   callbin 的头在它前面，body 完全一样，所以两边共用这一段
// ============================================================
bool parse_bin_body(const uint8_t* d, size_t size, size_t o,
                    std::vector<std::string>& strings, std::vector<Block>& blocks) {
    if (o + 4 > size) return false;
    int32_t sc = ld32(d + o); o += 4;
    if (sc < 0) return false;
    for (int i = 0; i < sc && o + 4 <= size; i++) {
        int32_t len = ld32(d + o); o += 4;
        if (len < 0 || o + (size_t)len > size) return false;
        strings.push_back(std::string((const char*)d + o, (size_t)len));
        o += (size_t)len;
    }
    while (o + 8 <= size) {
        int32_t blen = ld32(d + o); o += 4;
        int32_t nidx = ld32(d + o); o += 4;
        if (blen < 0 || o + (size_t)blen > size) return false;
        Block b;
        b.name = (nidx >= 0 && nidx < (int)strings.size()) ? strings[(size_t)nidx]
                                                           : std::string("<?>");
        b.code.assign(d + o, d + o + (size_t)blen);
        o += (size_t)blen;
        blocks.push_back(b);
    }
    return true;
}

// ============================================================
// .bin 装载
// ============================================================
struct BinFile {
    std::vector<std::string> strings;
    std::vector<Block>       blocks;

    bool load(const std::string& path) {
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) { printf("[vm] cannot open %s\n", path.c_str()); return false; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<uint8_t> d((size_t)(sz > 0 ? sz : 0));
        if (sz > 0 && fread(d.data(), 1, (size_t)sz, fp) != (size_t)sz) {
            fclose(fp);
            printf("[vm] read failed %s\n", path.c_str());
            return false;
        }
        fclose(fp);
        return parse_bin_body(d.data(), d.size(), 0, strings, blocks);
    }

    int find(const std::string& name) const {
        for (size_t i = 0; i < blocks.size(); i++)
            if (blocks[i].name == name) return (int)i;
        return -1;
    }
};

// ============================================================
// 入口
// ============================================================
// ── 命令行解析实参用的小工具（只给 --callbin 测试入口用）──
static Value cli_num(const std::string& s) {
    double d = atof(s.c_str());
    Value v;
    if (d == (double)(int)d) { v.t = MEM_INT; v.v = (int)d; }
    else { v.t = MEM_FLOAT; float f = (float)d; memcpy(&v.v, &f, 4); }
    return v;
}
static Value cli_undef() { Value v; v.t = MEM_UNDEFINED; v.v = 0; return v; }

int main(int argc, char** argv) {
    // ── callbin 测试入口：vm --callbin <文件.callbin> <函数名> [实参…] ──
    //   （开发用：装进字典 → 调它 → 打印返回值。实参按整数/浮点/字符串解析）
    if (argc >= 4 && std::string(argv[1]) == "--callbin") {
        if (!callbin_load(argv[2])) return 1;
        std::string fname = argv[3];
        std::vector<Value> av;
        Vm host;                                  // 当调用方（它有自己的 mem）
        for (int i = 4; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--load" && i + 1 < argc) { callbin_load(argv[++i]); continue; }   // 再多装几个（互相调用用）
            bool isstr = a.size() >= 2 && a.front() == '"' && a.back() == '"';
            if (isstr) {
                Value v; v.t = MEM_STRING; v.v = intern_str(a.substr(1, a.size() - 2));
                av.push_back(v);
            } else {
                av.push_back(cli_num(a));
            }
        }
        Value ret = cli_undef();
        bool ok = callbin_invoke(host, intern_str(fname), av.empty() ? NULL : &av[0], (int)av.size(), &ret);
        if (!ok) { printf("[vm] 调用失败（返回值 undefined）\n"); return 1; }
        if (ret.t == MEM_UNDEFINED)                  printf("=> undefined\n");
        else if (ret.t == MEM_STRING)                printf("=> \"%s\"\n", str_at(ret.v).c_str());
        else if (ret.t == MEM_FLOAT) { float f; memcpy(&f, &ret.v, 4); printf("=> %g\n", (double)f); }
        else                                         printf("=> %d\n", ret.v);
        release(ret);                            // callbin_invoke 转交过来的那一份（非容器空操作）
        callbin_clear();
        return 0;
    }

    if (argc < 2) {
        printf("usage: vm <file.bin> [block] [--list] [--load x.callbin]\n");
        printf("       vm --callbin <file.callbin> <func> [args...]\n");
        return 1;
    }
    BinFile f;
    if (!f.load(argv[1])) return 1;

    bool list = false;
    std::string block;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--list")      list = true;
        else if (a == "--load" && i + 1 < argc) callbin_load(argv[++i]);   // 先装 callbin（VM_CallFunc 要用）
        else if (block.empty()) block = a;
    }

    printf("%s: %zu blocks, %zu strings\n", argv[1], f.blocks.size(), f.strings.size());

    if (list || block.empty()) {
        for (size_t i = 0; i < f.blocks.size(); i++)
            printf("  [%2zu] %-32s %zu bytes\n", i, f.blocks[i].name.c_str(), f.blocks[i].code.size());
        if (block.empty()) return 0;
    }

    Vm vm;
    // 把本 bin 的字符串池并进全局池，并把字节码里的字符串常量换成全局下标
    if (!remap_block_strings(f.blocks, f.strings)) return 1;

    // 对象名从文件名取：.../obj_obstacle.bin → obj_obstacle
    // （.bin 本身没存对象名，只有块名）
    std::string objname = argv[1];
    size_t sl = objname.find_last_of("/\\");
    if (sl != std::string::npos) objname = objname.substr(sl + 1);
    if (objname.size() > 4 && objname.compare(objname.size() - 4, 4, ".bin") == 0)
        objname = objname.substr(0, objname.size() - 4);
    vm.load_object(objname, f.blocks);

    // Vm::kind：bin 里没存类型，按目录名猜（bin/card/… → "card"）。宿主也可以自己赋。
    {
        std::string dir = argv[1];
        size_t sl = dir.find_last_of("/\\");
        if (sl != std::string::npos) {
            std::string d = dir.substr(0, sl);
            size_t sl2 = d.find_last_of("/\\");
            if (sl2 != std::string::npos) vm.kind = d.substr(sl2 + 1);
        }
    }

    // _VM_CONST_INIT 是字面量池初始化块，读入即执行
    if (vm.find_block_sid(S__VM_CONST_INIT)) {
        if (!vm.run_named_sid(S__VM_CONST_INIT)) return 2;
    }

    if (!vm.run_named(block)) return 1;

    printf("[vm] done %s\n", block.c_str());
    return 0;
}
