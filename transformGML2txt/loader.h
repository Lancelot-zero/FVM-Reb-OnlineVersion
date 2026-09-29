#pragma once
#include <cstdint>
#include <string>
#include "vm.h"
// bin 解析 / 字符串重映射（装载层；内层不认识它）
int  op_fixed_len(int op);           // 操作码定长（core 定义，非 static）
bool remap_block_strings(Vm& vm);
bool parse_bin_body(Vm& vm, const uint8_t* d, size_t n, int* out_strs);

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
