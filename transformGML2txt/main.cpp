#include "vm.h"
#include "loader.h"
#include "selftest.h"

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
    if (argc >= 2 && std::string(argv[1]) == "--selftest-instbox") return selftest_instbox();
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
            printf("  [%2zu] %-32s %zu bytes\n", i, f.str_at(blocks[i].name_sid).c_str(), f.blocks[i].code.size());
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
            if (sl2 != std::string::npos) vm.kind_sid = intern_str(d.substr(sl2 + 1));
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
