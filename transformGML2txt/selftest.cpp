#include "vm.h"

// ============================================================
// InstBox 自检：vm --selftest-instbox
//   覆盖：数组模式顺序/删中间；跨 16 转 map；map 顺序/反向/随机/重复取；
//         删游标那个 / 删首 / 删尾 / 清空；增删后 at(k) 与 count 一致
// ============================================================
int selftest_instbox() {
    int fail = 0;
    {   // ① 数组模式（< 16）
        Vm v;
        for (int i = 1; i <= 5; i++) v.instances.add(100 + i, i);
        if (v.instances.count() != 5)             { printf("  FAIL 数组 count=%d\n", v.instances.count()); fail++; }
        if (v.instances.use_map)                  { printf("  FAIL 5 个不该转 map\n"); fail++; }
        for (int k = 0; k < 5; k++)
            if (v.instances.at(k) != 101 + k)     { printf("  FAIL 数组 at(%d)=%d\n", k, v.instances.at(k)); fail++; }
        v.instances.erase(103);                   // 删中间（应位移保序）
        if (v.instances.count() != 4 || v.instances.at(0) != 101 || v.instances.at(1) != 102
            || v.instances.at(2) != 104 || v.instances.at(3) != 105)
            { printf("  FAIL 数组删中间后顺序\n"); fail++; }
    }
    {   // ② map 模式（跨过 16）
        Vm v;
        for (int i = 1; i <= 20; i++) v.instances.add(1000 + i, i);
        if (!v.instances.use_map)                 { printf("  FAIL 20 个没转 map\n"); fail++; }
        if (v.instances.count() != 20)            { printf("  FAIL map count=%d\n", v.instances.count()); fail++; }
        for (int k = 0; k < 20; k++)        // 顺序（游标一路 ++）
            if (v.instances.at(k) != 1001 + k)    { printf("  FAIL map 顺序 at(%d)=%d\n", k, v.instances.at(k)); fail++; }
        for (int r = 0; r < 3; r++)         // 反复取同一个 k（游标不动）
            if (v.instances.at(7) != 1008)        { printf("  FAIL map 重复 at(7)=%d\n", v.instances.at(7)); fail++; }
        for (int k = 19; k >= 0; k--)       // 反向（游标一路 --）
            if (v.instances.at(k) != 1001 + k)    { printf("  FAIL map 反向 at(%d)=%d\n", k, v.instances.at(k)); fail++; }
        int ks[6] = { 0, 19, 5, 12, 1, 18 };   // 随机跳（应从最近一端重来）
        for (int i = 0; i < 6; i++)
            if (v.instances.at(ks[i]) != 1001 + ks[i]) { printf("  FAIL map 跳 at(%d)\n", ks[i]); fail++; }
        if (v.instances.at(5) != 1006)            { printf("  FAIL 删前 at(5)\n"); fail++; }
        v.instances.erase(1006);                  // 删的正是游标那个 → 游标应落到下一个
        if (v.instances.count() != 19)            { printf("  FAIL 删游标 count=%d\n", v.instances.count()); fail++; }
        if (v.instances.at(5) != 1007)            { printf("  FAIL 删游标后 at(5)=%d（应 1007）\n", v.instances.at(5)); fail++; }
        v.instances.erase(1001); v.instances.erase(1020);   // 删首 / 删尾
        if (v.instances.count() != 17)            { printf("  FAIL 删首尾 count=%d\n", v.instances.count()); fail++; }
        if (v.instances.at(0) != 1002 || v.instances.at(16) != 1019)
            { printf("  FAIL 删首尾顺序 at(0)=%d at(16)=%d\n", v.instances.at(0), v.instances.at(16)); fail++; }
        if (v.instances.at(16) != 1019)           { printf("  FAIL 末尾 at(16)\n"); fail++; }
        v.instances.erase(1019);                  // 删的又是游标（末尾）→ 游标应退到前一个
        if (v.instances.count() != 16 || v.instances.at(15) != 1018)
            { printf("  FAIL 删末尾后 at(15)=%d\n", v.instances.at(15)); fail++; }
        v.instances.clear();
        if (v.instances.count() != 0 || v.instances.use_map) { printf("  FAIL clear\n"); fail++; }
    }
    printf("[selftest-instbox] %s%s\n", fail ? "FAIL " : "PASS", fail ? "" : "");
    if (fail) printf("[selftest-instbox] %d 处不一致\n", fail);
    return fail ? 1 : 0;
}
