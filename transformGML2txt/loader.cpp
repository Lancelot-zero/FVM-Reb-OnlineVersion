#include "vm.h"
#include "loader.h"

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
                if (p + 5 > n) { printf("[vm] 块 %s：指令被截断\n", str_at(blocks[bi].name_sid).c_str()); return false; }
                p += 4;                                   // dst
                uint8_t ty = c[p++];
                if (ty == MEM_STRING) {
                    if (p + 2 > n) { printf("[vm] 块 %s：字符串下标被截断\n", str_at(blocks[bi].name_sid).c_str()); return false; }
                    int idx = (int)ld16(&c[p]);
                    if (idx < 0 || idx >= (int)remap.size()) {
                        printf("[vm] 块 %s：字符串下标 %d 越界（本 bin 池只有 %zu 条）\n",
                               str_at(blocks[bi].name_sid).c_str(), idx, remap.size());
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
                if (p + 3 > n) { printf("[vm] 块 %s：调用指令被截断\n", str_at(blocks[bi].name_sid).c_str()); return false; }
                p += 2;                                   // fid u16
                int argc = (int)c[p++];
                if (p + 4 + (size_t)argc * 4 > n) {
                    printf("[vm] 块 %s：调用指令的实参被截断\n", str_at(blocks[bi].name_sid).c_str());
                    return false;
                }
                p += 4 + (size_t)argc * 4;                // dst + args
            } else {
                int k = op_fixed_len(op);
                if (k < 0 || p + (size_t)k > n) {
                    printf("[vm] 块 %s：未知操作码 %d（字符串下标重写中止）\n",
                           str_at(blocks[bi].name_sid).c_str(), (int)op);
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
        b.name_sid = intern_str((nidx >= 0 && nidx < (int)strings.size()) ? strings[(size_t)nidx]
                                                           : std::string("<?>"));
        b.code.assign(d + o, d + o + (size_t)blen);
        o += (size_t)blen;
        blocks.push_back(b);
    }
    return true;
}

// ============================================================
// .bin 装载
// ============================================================

