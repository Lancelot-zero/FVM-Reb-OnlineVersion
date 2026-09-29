// ============================================================
// selftest.cpp — 内层自检（测试层；只被 main 调，内层不认识它）
//
//   验四件事：
//     1 字符串池：预置段的池下标 == 枚举值
//     2 装载：现在导出的 .bin / .callbin **能原样读进来**（全部文件）
//     3 跑：解释循环（算术 / 浮点 / 跳转 / 返回槽 0）
//     4 实例：建 / 取 / 存活 / 第 k 个 / 变量 / 槽位复用 / ≥16 转集合
//
//   用法：selftest.exe [kernel 目录]
// ============================================================
#include "../core/vm_def.h"
#include "../outer/outer.h"
#include "../host/host.h"
#include "../host/host_layout.h"      // 共享内存布局契约（生成物；见 tools/gen_host_layout.py）
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

static int g_fail = 0;

static void chk(bool ok, const char* what) {
    printf("%s %s\n", ok ? "[ok]  " : "[FAIL]", what);
    if (!ok) g_fail++;
}

// vm_bin 里是**定长内存**（32 位 256KB+ / 64 位 512KB+）→ 测试里一律堆上开，别放栈上
static vm_bin& new_vm() { return *(new vm_bin()); }

// ============================================================
// 1 字符串池
// ============================================================
static void test_pool() {
    pool_seed();
    chk(str_at(S_x) == "x",                    "pool: S_x 就是 \"x\"");
    chk(str_at(S_image_blend) == "image_blend", "pool: 字段段尾对得上");
    chk(str_find("VM_BanCard") == S_VM_BanCard, "pool: game 段头对得上");
    chk(str_find("__no_such_name__") == -1,     "pool: 没有的名字 → -1");

    int a = intern_str("__selftest_name");
    int b = intern_str("__selftest_name");
    chk(a == b && a >= S_LAST_, "pool: intern 幂等");
}

// ============================================================
// 2 装载（现有 bin / callbin 要能直接读进来用）
// ============================================================
static bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
    FILE* fp = fopen(p.string().c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize((size_t)(sz > 0 ? sz : 0));
    bool ok = (sz <= 0) || (fread(out.data(), 1, (size_t)sz, fp) == (size_t)sz);
    fclose(fp);
    return ok;
}

static void test_load(const fs::path& kernel) {
    int nb = 0, nb_ok = 0, nc = 0, nc_ok = 0;
    int const_left = 0;             // 还有 _VM_CONST_INIT 留在 blocks 里的（应该 0）
    int bins_with_lit = 0;          // 字面量槽里真有字符串的 bin（说明表跑过了）
    std::vector<std::string> bad;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(kernel, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;
        std::string ext = it->path().extension().string();
        bool is_bin = (ext == ".bin"), is_cb = (ext == ".callbin");
        if (!is_bin && !is_cb) continue;

        std::vector<uint8_t> d;
        if (!read_file(it->path(), d) || d.empty()) { bad.push_back(it->path().string() + " (read)"); continue; }

        Value r;
        if (is_bin) {
            nb++;
            vm_bin& vm = new_vm();
            r = VM_BIN_Load(d.data(), d.size(), vm);
            if (r.t == MEM_INT && r.v == 1 && !vm.blocks.empty()) nb_ok++;
            else bad.push_back(it->path().filename().string() + " bin err=" + std::to_string(g_vm_err));
            if (vm.blocks.count(S__VM_CONST_INIT)) const_left++;
            for (int s = 1; s < 4096; s++)
                if (vm.mem[s].t == MEM_STRING && str_ok(vm.mem[s].v)) { bins_with_lit++; break; }
        } else {
            nc++;
            vm_callbin cb;
            r = VM_CALLBIN_LOAD(d.data(), d.size(), cb);
            if (r.t == MEM_INT && r.v == 1 && !cb.block.code.empty() && cb.value > 0) nc_ok++;
            else bad.push_back(it->path().filename().string() + " callbin err=" + std::to_string(g_vm_err));
        }
    }

    printf("      bin %d/%d   callbin %d/%d   字面量跑过的 bin %d   残留字面量表 %d\n",
           nb_ok, nb, nc_ok, nc, bins_with_lit, const_left);
    for (size_t i = 0; i < bad.size() && i < 10; i++) printf("      bad: %s\n", bad[i].c_str());
    chk(nb > 0 && nb_ok == nb, "load: 所有 .bin 都读进来了");
    chk(nc > 0 && nc_ok == nc, "load: 所有 .callbin 都读进来了");
    chk(const_left == 0 && bins_with_lit > 0, "load: 字面量表读入即执行、且不留进 blocks");
}

// ============================================================
// 3 跑一个手写的块
// ============================================================
static void put32(std::vector<uint8_t>& c, int v) {
    uint32_t u = (uint32_t)v;
    c.push_back((uint8_t)(u & 0xFF)); c.push_back((uint8_t)((u >> 8) & 0xFF));
    c.push_back((uint8_t)((u >> 16) & 0xFF)); c.push_back((uint8_t)((u >> 24) & 0xFF));
}
// OP_ASSIGN 里的字符串下标是 **u16**（文件格式，见 vm_def.h 的指令布局），不是 4 字节
static void put16(std::vector<uint8_t>& c, int v) {
    c.push_back((uint8_t)(v & 0xFF)); c.push_back((uint8_t)((v >> 8) & 0xFF));
}
static void put_assign_int(std::vector<uint8_t>& c, int dst, int v) {
    c.push_back(OP_ASSIGN); put32(c, dst); c.push_back(MEM_INT); put32(c, v);
}
// 字符串字面量那条：定义在 §8（"装载 .bin 时字符串池怎么并"那节），这里先用
static void put_assign_str(std::vector<uint8_t>& c, int dst, int sid);
// 调一条 native：OP_NATIVE 的布局是 fid(u16) argc(u8) dst(s32) arg(s32)*argc
static void put_native(std::vector<uint8_t>& c, int fid, int dst, std::initializer_list<int> argslots) {
    c.push_back(OP_NATIVE); put16(c, fid); c.push_back((uint8_t)argslots.size()); put32(c, dst);
    for (int s : argslots) put32(c, s);
}
// 写入一个"槽入的文件字面量"：**文件格式里浮点永远是 f32**，跟槽宽（VM_FLOAT）无关
static void put_assign_float(std::vector<uint8_t>& c, int dst, float f) {
    c.push_back(OP_ASSIGN); put32(c, dst); c.push_back(MEM_FLOAT);
    int i; memcpy(&i, &f, 4); put32(c, i);
}

static void test_run() {
    // 3.1 整数加：2 + 3 = 5（结果进槽 0）
    {
        std::vector<uint8_t> c;
        put_assign_int(c, 1, 2);
        put_assign_int(c, 2, 3);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        Block b; b.name_sid = S_x; b.code = c;
        vm_bin& vm = new_vm();
        Value r = VM_RUN(b, vm.mem);
        chk(r.t == MEM_INT && r.v == 5, "run: 2 + 3 = 5（槽 0）");
    }

    // 3.2 浮点加：1.5 + 2.25 = 3.75
    {
        std::vector<uint8_t> c;
        put_assign_float(c, 1, 1.5f);
        put_assign_float(c, 2, 2.25f);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        Block b; b.name_sid = S_x; b.code = c;
        vm_bin& vm = new_vm();
        Value r = VM_RUN(b, vm.mem);
        chk(r.t == MEM_FLOAT && f_of(r.v) == 3.75f, "run: 1.5 + 2.25 = 3.75（浮点）");
    }

    // 3.3 条件跳转：cond > 0 → 跳到 HALT，下面那句不许执行
    {
        std::vector<uint8_t> c;
        put_assign_int(c, 1, 1);
        size_t ip_if = c.size();
        c.push_back(OP_IF); put32(c, 1);
        size_t at_tip = c.size(); put32(c, 0); put32(c, -1);      // 先占位
        put_assign_int(c, 0, 99);                                 // 不该走到这
        size_t ip_halt = c.size();
        c.push_back(OP_HALT);
        // 回填 tip = halt 的偏移
        int tip = (int)ip_halt;
        c[at_tip]     = (uint8_t)(tip & 0xFF);
        c[at_tip + 1] = (uint8_t)((tip >> 8) & 0xFF);
        c[at_tip + 2] = (uint8_t)((tip >> 16) & 0xFF);
        c[at_tip + 3] = (uint8_t)((tip >> 24) & 0xFF);
        (void)ip_if;
        Block b; b.name_sid = S_x; b.code = c;
        vm_bin& vm = new_vm();
        Value r = VM_RUN(b, vm.mem);
        chk(r.t == MEM_INT && r.v == 0, "run: IF 真 → 跳过去（槽 0 还是 0）");
    }

    // 3.4 循环：i = 0; while (i < 5) i++; → slot1 = 5
    {
        std::vector<uint8_t> c;
        put_assign_int(c, 1, 0);                                  // i = 0
        size_t ip_top = c.size();
        put_assign_int(c, 2, 5);                                  // 常数 5
        c.push_back(OP_LT); put32(c, 3); put32(c, 1); put32(c, 2);  // 3 = i < 5
        c.push_back(OP_IF); put32(c, 3); put32(c, -1); put32(c, 0); // 真 → 往下走（循环体），假 → 跳出去
        size_t at_fip = c.size() - 4;
        // 循环体：i = i + 1
        put_assign_int(c, 4, 1);
        c.push_back(OP_ADD); put32(c, 1); put32(c, 1); put32(c, 4);
        c.push_back(OP_JMP); put32(c, (int)ip_top);
        size_t ip_halt = c.size();
        c.push_back(OP_HALT);
        int fip = (int)ip_halt;
        c[at_fip]     = (uint8_t)(fip & 0xFF);
        c[at_fip + 1] = (uint8_t)((fip >> 8) & 0xFF);
        c[at_fip + 2] = (uint8_t)((fip >> 16) & 0xFF);
        c[at_fip + 3] = (uint8_t)((fip >> 24) & 0xFF);
        Block b; b.name_sid = S_x; b.code = c;
        vm_bin& vm = new_vm();
        VM_RUN(b, vm.mem);
        chk(rd_i(vm.mem, 1) == 5, "run: 循环 i 加到 5");
    }
}

// ============================================================
// 3b 字符串拼接：OP_ADD 的字符串路径
//   · 任一侧是字符串 → 真拼接（不是"两边都是字符串"才拼），结果进池、同文本同 sid
//   · 数字 + 数字 → 行为不变（回归；原来就是浮点走浮点、整数走整数）
//   · 数字 → 文本只有一份实现（内层 value_to_text）：所以 `"f=" + 1.5` 必须
//     **逐字节等于** `"f=" + string(1.5)`，这条就是拿来钉死"只有一份"的
//   · 超长拼接 → 响亮报错（错误码 + 块停住），不静默截断、不把池撑爆
// ============================================================
static Value run_code(vm_bin& vm, std::vector<uint8_t>& c) {
    Block b; b.name_sid = S_x; b.code = c;
    return VM_RUN(b, vm.mem);
}

static void test_str_concat(const fs::path& kernel) {
    // 3b.1 "abc" + "def" → "abcdef"，而且结果**是字符串**
    //      （原来这条会走整数分支：把两个池下标当数字加，静默错）
    {
        std::vector<uint8_t> c;
        put_assign_str(c, 1, intern_str("abc"));
        put_assign_str(c, 2, intern_str("def"));
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        g_vm_err = VM_OK;
        Value r = run_code(vm, c);
        chk(r.t == MEM_STRING && str_at(r.v) == "abcdef",
            "concat: \"abc\" + \"def\" = \"abcdef\"（t == MEM_STRING）");
        chk(g_vm_err == VM_OK, "concat: 纯字符串相加没出错");
    }

    // 3b.2 "n=" + 42 → "n=42"；7 + "x" → "7x"（顺序不能反）
    {
        std::vector<uint8_t> c;
        put_assign_str(c, 1, intern_str("n="));
        put_assign_int(c, 2, 42);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);   // 字符串在左
        put_assign_int(c, 3, 7);
        put_assign_str(c, 4, intern_str("x"));
        c.push_back(OP_ADD); put32(c, 5); put32(c, 3); put32(c, 4);   // 字符串在右
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        run_code(vm, c);
        chk(vm.mem[0].t == MEM_STRING && str_at(vm.mem[0].v) == "n=42",
            "concat: \"n=\" + 42 = \"n=42\"");
        chk(vm.mem[5].t == MEM_STRING && str_at(vm.mem[5].v) == "7x",
            "concat: 7 + \"x\" = \"7x\"（顺序没反）");
    }

    // 3b.3 "f=" + 1.5f  必须等于  "f=" + string(1.5f)
    //      —— 两侧走的是同一份"值 → 文本"，浮点格式不可能漂
    {
        std::vector<uint8_t> a;
        put_assign_str(a, 1, intern_str("f="));
        put_assign_float(a, 2, 1.5f);
        a.push_back(OP_ADD); put32(a, 0); put32(a, 1); put32(a, 2);
        a.push_back(OP_HALT);
        vm_bin& va = new_vm();
        run_code(va, a);

        std::vector<uint8_t> b;
        put_assign_str(b, 1, intern_str("f="));
        put_assign_float(b, 2, 1.5f);
        put_native(b, (int)(S_string - SID_NATIVE_BASE), 3, { 2 });   // 槽3 = string(槽2)
        b.push_back(OP_ADD); put32(b, 0); put32(b, 1); put32(b, 3);
        b.push_back(OP_HALT);
        vm_bin& vb = new_vm();
        run_code(vb, b);

        chk(va.mem[0].t == MEM_STRING && vb.mem[0].t == MEM_STRING
            && str_at(va.mem[0].v) == str_at(vb.mem[0].v) && str_at(va.mem[0].v) == "f=1.50",
            "concat: \"f=\" + 1.5 完全等于 \"f=\" + string(1.5)（只有一份格式化实现）");
        chk(va.mem[0].v == vb.mem[0].v, "concat: 同文本 → 同一个 poolsid（池去重）");
    }

    // 3b.4 池规范化：同样的拼接做两次 → 同一个 sid
    {
        std::vector<uint8_t> c;
        put_assign_str(c, 1, intern_str("abc"));
        put_assign_str(c, 2, intern_str("def"));
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_ADD); put32(c, 3); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        run_code(vm, c);
        chk(vm.mem[0].v == vm.mem[3].v && str_at(vm.mem[0].v) == "abcdef",
            "concat: 拼两次 → 同一个 sid（intern 按文本去重）");
    }

    // 3b.5 数字 + 数字：行为不变（回归）—— 尤其**不能**被字符串分支带偏
    {
        std::vector<uint8_t> c;
        put_assign_int(c, 1, 2);
        put_assign_int(c, 2, 3);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        put_assign_float(c, 4, 1.5f);
        put_assign_float(c, 5, 2.25f);
        c.push_back(OP_ADD); put32(c, 6); put32(c, 4); put32(c, 5);
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        run_code(vm, c);
        chk(vm.mem[0].t == MEM_INT && vm.mem[0].v == 5, "concat: 2 + 3 还是整数 5（回归）");
        chk(vm.mem[6].t == MEM_FLOAT && f_of(vm.mem[6].v) == 3.75f,
            "concat: 1.5 + 2.25 还是浮点 3.75（回归）");
    }

    // 3b.6 字符串 + 非数字：照**平台 string() 的口径**转文本（不是静默给个数字）
    //      "undefined" / "[array]" 都来自内层 value_to_text（和老实现 vm_builtin.cpp 一致）
    {
        std::vector<uint8_t> c;
        put_assign_str(c, 1, intern_str("u="));
        c.push_back(OP_ASSIGN); put32(c, 2); c.push_back(MEM_UNDEFINED); put32(c, 0);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        run_code(vm, c);
        chk(vm.mem[0].t == MEM_STRING && str_at(vm.mem[0].v) == "u=undefined",
            "concat: \"u=\" + undefined = \"u=undefined\"（照 string() 口径）");
    }
    {
        int ai = arr_new();
        std::vector<uint8_t> c;
        put_assign_str(c, 1, intern_str("c="));
        c.push_back(OP_ASSIGN); put32(c, 2); c.push_back(MEM_ARRAY); put32(c, ai);
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        run_code(vm, c);
        chk(vm.mem[0].t == MEM_STRING && str_at(vm.mem[0].v) == "c=[array]",
            "concat: \"c=\" + 数组 = \"c=[array]\"（照 string() 口径，不是数字）");
        arr_free(ai);                      // 测试直接塞槽，账自己结（句柄就地归还）
    }

    // 3b.7 超长拼接 → 响亮报错 + 停块（不截断、不撑爆池）
    {
        int big = intern_str(std::string((size_t)(1024 * 1024), 'a'));   // 1 MB
        std::vector<uint8_t> c;
        put_assign_str(c, 1, big);
        put_assign_str(c, 2, big);         // 1 MB + 1 MB = 2 MB > 上限 1 MB
        c.push_back(OP_ADD); put32(c, 0); put32(c, 1); put32(c, 2);
        put_assign_int(c, 4, 7);           // 报错后这条**不该**跑
        c.push_back(OP_HALT);
        vm_bin& vm = new_vm();
        g_vm_err = VM_OK;
        Value r = run_code(vm, c);
        chk(g_vm_err == VM_E_STR_TOO_LONG && r.t == MEM_UNDEFINED,
            "concat: 超过上限 → VM_E_STR_TOO_LONG（响亮报错，不静默截断）");
        chk(vm.mem[4].t == MEM_INT && vm.mem[4].v == 0,
            "concat: 报错后块确实停住了（后面的指令没跑）");
    }

    // 3b.8 真 kernel GML：src_mod_save_path 里就是 "saves/" + _rel 这种真拼串
    //      （翻译器发的是 OP_ADD —— 这份 .callbin 的 _CALL_ENTRY 里就一条 ADD；
    //        这里顺带证明"编出来的 bin 跑得过、结果对"）
    {
        fs::path p = kernel / "function" / "src_mod_save_path.callbin";
        std::vector<uint8_t> d;
        if (!read_file(p, d) || d.empty()) {
            printf("[skip] 没找到 %s\n", p.string().c_str());
        } else {
            vm_callbin cb;
            Value okv = VM_CALLBIN_LOAD(d.data(), d.size(), cb);
            chk(okv.t == MEM_INT && okv.v == 1, "concat/kernel: src_mod_save_path.callbin 装载成功");
            global_var_set_sid(intern_str("mod_enabled"), mk_int(0));    // 走 "saves/" + _rel 那条
            Value argv[1] = { mk_str_sid(intern_str("abc")) };
            g_vm_err = VM_OK;
            Value out = callbin_call(intern_str("src_mod_save_path"), argv, 1);
            chk(out.t == MEM_STRING && str_at(out.v) == "saves/abc",
                "concat/kernel: src_mod_save_path(\"abc\") = \"saves/abc\"");
            chk(g_vm_err == VM_OK, "concat/kernel: 调用全程没出错");
        }
    }
}

// ============================================================
// 4 实例
// ============================================================
static void test_inst() {
    vm_bin& vm = new_vm();
    int obj = intern_str("__selftest_obj");
    g_vm_of_object[obj] = &vm;

    int id1 = inst_new(vm, obj);
    int id2 = inst_new(vm, obj);
    chk(inst_alive(id1) && inst_alive(id2), "inst: 建出来的都活着");
    chk(obj_count(obj) == 2 && vm.inst_count == 2, "inst: 个数对得上");
    int k0 = inst_kth(vm, 0), k1 = inst_kth(vm, 1);
    chk((k0 == id1 && k1 == id2) || (k0 == id2 && k1 == id1), "inst: 第 0 / 第 1 个就是这两个");
    chk(inst_kth(vm, 2) == -1, "inst: 越界 → -1");

    // 变量：脚本动态的走 vars，内置的走 attr
    int sid_atk = intern_str("atk");
    inst_var_set_sid(id1, sid_atk, mk_int(7));
    Value* v = inst_var_sid(id1, sid_atk);
    chk(v && v->t == MEM_INT && v->v == 7, "inst: vars 读写");
    inst_var_set_sid(id1, S_x, mk_int(42));
    Instance* in = inst_get(id1);
    chk(in && in->attr[F_x].v == 42, "inst: 内置字段 x 直接落 attr");
    chk(inst_var_sid(id1, intern_str("__nope")) == NULL, "inst: 没设过的属性 → NULL(undefined)");

    // 属性用**实例引用**读写（省一次 id → 槽位查找）
    chk(in != NULL, "inst: 拿到实例引用");
    inst_prop_set(*in, sid_atk, mk_int(11));
    chk(inst_prop(*in, sid_atk) && inst_prop(*in, sid_atk)->v == 11, "inst: inst_prop 读 / 写（引用版）");
    inst_prop_set(*in, S_y, mk_int(5));
    chk(inst_prop(*in, S_y)->v == 5 && in->attr[F_y].v == 5, "inst: inst_prop 内置字段走 attr");
    chk(inst_prop(*in, intern_str("__nope2")) == NULL, "inst: inst_prop 没设过 → NULL");

    // 回收：id 不复用，槽位复用
    inst_kill(id2);
    chk(!inst_alive(id2) && inst_get(id2) == NULL, "inst: 死了就查不到");
    chk(obj_count(obj) == 1 && vm.inst_count == 1, "inst: 死了个数 -1");
    int id3 = inst_new(vm, obj);
    chk(inst_alive(id3) && id3 > id2, "inst: id 只增不减（槽位可以复用）");

    // ≥16 → 转集合 + 游标，转了还能取第 k 个
    vm_bin& vm2 = new_vm();
    int obj2 = intern_str("__selftest_obj2");
    g_vm_of_object[obj2] = &vm2;
    std::vector<int> ids;
    for (int i = 0; i < 20; i++) ids.push_back(inst_new(vm2, obj2));
    chk(vm2.mp.size() == 20 && vm2.arr.empty(), "inst: 20 个 → 转到集合了");
    chk(obj_count(obj2) == 20, "inst: 集合模式个数对");
    bool all = true;
    for (int i = 0; i < 20; i++) {
        int got = inst_kth(vm2, i);
        bool hit = false;
        for (size_t j = 0; j < ids.size(); j++) if (ids[j] == got) hit = true;
        if (!hit) all = false;
    }
    chk(all, "inst: 集合模式第 k 个都能取到（跑游标）");
    chk(inst_kth(vm2, 20) == -1, "inst: 集合模式越界 → -1");

    // Destroy 事件：没装这个块 → 什么都不跑，直接回收
    int id4 = inst_new(vm2, obj2);
    inst_destroy(id4);
    chk(!inst_alive(id4) && obj_count(obj2) == 20, "inst: inst_destroy 唯一入口（没块也回收）");

    // 收尾：清掉注册，免得影响别的用例
    g_vm_of_object.erase(obj);
    g_vm_of_object.erase(obj2);
}

// ============================================================
// 5 实例的数组（挂在实例身上，按名字索引）
// ============================================================
static void test_arr() {
    vm_bin& vm = new_vm();
    int obj = intern_str("__selftest_arr_obj");
    g_vm_of_object[obj] = &vm;
    int id = inst_new(vm, obj);
    Instance* in = inst_get(id);
    chk(in != NULL, "arr: 拿到实例引用");

    int sid = intern_str("__selftest_arr");
    chk(!inst_arr_exists(*in, sid) && inst_arr_size(*in, sid) == 0, "arr: 没建过 → 不存在 / 长度 0");

    inst_arr_create(*in, sid, 3, mk_int(0));
    chk(inst_arr_size(*in, sid) == 3 && inst_arr_item(*in, sid, 2).v == 0, "arr: create 建出 3 个 0");

    inst_arr_set(*in, sid, 1, mk_int(7));
    chk(inst_arr_item(*in, sid, 1).v == 7 && inst_arr_item(*in, sid, 0).v == 0, "arr: set / item");
    chk(inst_arr_item(*in, sid, 99).t == MEM_UNDEFINED, "arr: 越界读 → undefined");

    inst_arr_add(*in, sid, mk_int(9));
    chk(inst_arr_size(*in, sid) == 4 && inst_arr_item(*in, sid, 3).v == 9, "arr: add 追到尾部");
    chk(inst_arr_index(*in, sid, mk_int(9)) == 3, "arr: index 找到下标");
    chk(inst_arr_index(*in, sid, mk_int(12345)) == -1, "arr: index 没有 → -1");

    Value popped = inst_arr_pop(*in, sid);
    chk(popped.v == 9 && inst_arr_size(*in, sid) == 3, "arr: pop 弹出最后一个");
    release(popped);                                   // pop 转交过来的引用，用完放掉

    inst_arr_delete(*in, sid, 0, 1);
    chk(inst_arr_size(*in, sid) == 2 && inst_arr_item(*in, sid, 0).v == 7, "arr: delete 删一个并前移");

    inst_arr_resize(*in, sid, 5);
    chk(inst_arr_size(*in, sid) == 5 && inst_arr_item(*in, sid, 4).v == 0, "arr: resize 变长填 0");
    inst_arr_set(*in, sid, 7, mk_int(3));              // 跳着写 → 中间补 0
    chk(inst_arr_size(*in, sid) == 8 && inst_arr_item(*in, sid, 5).v == 0
        && inst_arr_item(*in, sid, 7).v == 3, "arr: 跳着 set 自动补 0");

    // 打乱：长度不变、元素还是那一堆
    inst_arr_create(*in, sid, 5, mk_int(0));
    for (int k = 0; k < 5; k++) inst_arr_set(*in, sid, k, mk_int(k + 1));
    inst_arr_shuffle(*in, sid);
    int sum = 0;
    for (int k = 0; k < 5; k++) sum += inst_arr_item(*in, sid, k).v;
    chk(inst_arr_size(*in, sid) == 5 && sum == 15, "arr: shuffle 只换位置（和还是 15）");

    inst_arr_clear(*in, sid);
    chk(inst_arr_size(*in, sid) == 0, "arr: clear 清空");

    // 引用计数：容器句柄存进实例数组要 retain，删掉要 release
    int a = arr_new();
    Value h{MEM_ARRAY, a};
    chk(g_arr_refs[(size_t)a] == 1, "arr: 池里新建的容器引用计数 = 1");
    inst_arr_add(*in, sid, h);
    chk(g_arr_refs[(size_t)a] == 2, "arr: 存进实例数组 → +1 引用");
    inst_arr_clear(*in, sid);
    chk(g_arr_refs[(size_t)a] == 1, "arr: 清掉 → -1 引用");
    release(h);
    chk(g_arr_refs[(size_t)a] == 0, "arr: 放掉最后一份 → 回池");

    g_vm_of_object.erase(obj);
}

// ============================================================
// 6 callbin：装进字典 → 借公共内存的帧跑 → 回值 → 偏移还回去
//   这里手写一个最小 callbin：
//     _VM_CONST_INIT  槽5 = 10（字面量）
//     _CALL_ENTRY     槽0 = 槽1(实参) + 槽5
// ============================================================
static void put_header_str(std::vector<uint8_t>& c, const std::string& s) {
    put32(c, (int)s.size());
    for (size_t i = 0; i < s.size(); i++) c.push_back((uint8_t)s[i]);
}

static std::vector<uint8_t> make_callbin(const char* name, int slots, int argc) {
    std::vector<uint8_t> c;
    put32(c, 0x4E424C43);                 // 'CLBN'
    put32(c, slots);
    put32(c, argc);
    put_header_str(c, name);
    put_header_str(c, "");                // desc

    std::vector<std::string> pool;
    pool.push_back("_VM_CONST_INIT");
    pool.push_back("_CALL_ENTRY");
    put32(c, (int)pool.size());
    for (size_t i = 0; i < pool.size(); i++) put_header_str(c, pool[i]);

    std::vector<uint8_t> init;            // 槽5 = 10; HALT
    put_assign_int(init, 5, 10);
    init.push_back(OP_HALT);
    put32(c, (int)init.size()); put32(c, 0);
    c.insert(c.end(), init.begin(), init.end());

    std::vector<uint8_t> body;            // 槽0 = 槽1 + 槽5; HALT
    body.push_back(OP_ADD); put32(body, 0); put32(body, 1); put32(body, 5);
    body.push_back(OP_HALT);
    put32(c, (int)body.size()); put32(c, 1);
    c.insert(c.end(), body.begin(), body.end());
    return c;
}

static void test_callbin() {
    std::vector<uint8_t> d = make_callbin("__selftest_fn", 16, 1);
    vm_callbin cb;
    Value r = VM_CALLBIN_LOAD(d.data(), d.size(), cb);
    chk(r.t == MEM_INT && r.v == 1, "callbin: 装载成功");
    chk(!cb.block.code.empty() && !cb.init.code.empty() && cb.value == 16,
        "callbin: 两个块 + 需要的内存大小都读出来了");

    int sid = intern_str("__selftest_fn");
    Value argv[1] = { mk_int(7) };
    chk(g_call_off == 0, "callbin: 没调用时偏移 = 0");
    Value out = callbin_call(sid, argv, 1);
    chk(out.t == MEM_INT && out.v == 17, "callbin: 7 + 字面量 10 = 17（槽0）");
    chk(g_call_off == 0, "callbin: 跑完偏移退回来 = 0");

    // 连调两次：帧是复用的，init 必须每次重填字面量
    Value out2 = callbin_call(sid, argv, 1);
    chk(out2.v == 17, "callbin: 第二次调用结果一样（帧复用 + 重填字面量）");
    chk(g_call_off == 0, "callbin: 第二次跑完偏移还是 0");

    // 找不到的函数 → undefined + 错误码
    g_vm_err = VM_OK;
    Value nf = callbin_call(intern_str("__no_such_fn"), argv, 1);
    chk(nf.t == MEM_UNDEFINED && g_vm_err == VM_E_FN_NOT_FOUND, "callbin: 没这个函数 → undefined + 错误码");
}

// ============================================================
// 7 取参 / 回值 的机制（外层写实现函数用的就是这几条）
// ============================================================
static void test_handles() {
    Value mem[64];
    for (int i = 0; i < 64; i++) mem[i] = mk_int(0);

    int a = arr_new();
    mem[5] = Value{MEM_ARRAY, a};              // 槽 5 = 一个数组句柄
    mem[6] = mk_int(3);
    uint8_t args[8];
    args[0] = 5; args[1] = 0; args[2] = 0; args[3] = 0;   // 第 0 个实参 = 槽 5
    args[4] = 6; args[5] = 0; args[6] = 0; args[7] = 0;   // 第 1 个实参 = 槽 6

    chk(arg_v(mem, args, 1).v == 3, "arg: arg_v 取到第 1 个实参");
    chk(arg_i(mem, args, 1) == 3, "arg: arg_i");
    chk(arg_num(mem, args, 1) == 3.0f, "arg: arg_num 整数当数");

    chk(handle_arg(mem, args, 0, MEM_ARRAY) == a, "arg: handle_arg 认出数组句柄");
    chk(handle_arg(mem, args, 0, MEM_MAP) == -1, "arg: 类型不对 → -1");

    // 回值：dst >= 0 槽接手（净剩 1 份引用）；dst < 0 没人接 → 放掉创造者那份
    Value mem2[8];
    for (int i = 0; i < 8; i++) mem2[i] = mk_int(0);
    int b = arr_new();
    chk(g_arr_refs[(size_t)b] == 1, "handle: 刚建出来 = 1 份引用");
    give_handle(mem2, 2, Value{MEM_ARRAY, b});
    chk(g_arr_refs[(size_t)b] == 1 && mem2[2].v == b, "handle: give_handle 给槽（净剩 1）");
    int c2 = arr_new();
    give_handle(mem2, -1, Value{MEM_ARRAY, c2});
    chk(g_arr_refs[(size_t)c2] == 0, "handle: give_handle 没人接 → 直接回收");

    // 逆序拒收：只准"老容器里放新容器"，反着来/自己塞自己 → 不收
    int old_a = arr_new();
    int new_a = arr_new();
    Value vo{MEM_ARRAY, old_a}, vn{MEM_ARRAY, new_a};
    chk(allow_store(vn, vo) && !allow_store(vo, vn) && !allow_store(vo, vo),
        "handle: 逆序 / 自己塞自己都拒收");
    release(vo); release(vn);
    release(Value{MEM_ARRAY, a});
    release(Value{MEM_ARRAY, b});
}

// ============================================================
// 8 装载 .bin 时字符串池怎么并（重复取 id，新的追加）+ 字面量表读入即执行
// ============================================================
static void put_assign_str(std::vector<uint8_t>& c, int dst, int idx) {
    c.push_back(OP_ASSIGN); put32(c, dst); c.push_back(MEM_STRING);
    c.push_back((uint8_t)(idx & 0xFF)); c.push_back((uint8_t)((idx >> 8) & 0xFF));
}

// 手写一个 bin：[字符串池] + [块表]
static std::vector<uint8_t> make_bin(const std::vector<std::string>& pool,
                                     const std::vector<std::pair<int, std::vector<uint8_t>>>& blocks) {
    std::vector<uint8_t> c;
    put32(c, (int)pool.size());
    for (size_t i = 0; i < pool.size(); i++) put_header_str(c, pool[i]);
    for (size_t i = 0; i < blocks.size(); i++) {
        put32(c, (int)blocks[i].second.size());
        put32(c, blocks[i].first);
        c.insert(c.end(), blocks[i].second.begin(), blocks[i].second.end());
    }
    return c;
}

static void test_bin_load() {
    std::vector<std::string> pool;
    pool.push_back("_VM_CONST_INIT");
    pool.push_back("_OBJECT_STEP");
    pool.push_back("__selftest_strA");

    std::vector<uint8_t> init;              // 字面量槽 7 = 本文件池的第 2 条
    put_assign_str(init, 7, 2);
    std::vector<uint8_t> step;              // 事件块：什么都不干
    step.push_back(OP_HALT);

    std::vector<std::pair<int, std::vector<uint8_t>>> blks;
    blks.push_back(std::make_pair(0, init));
    blks.push_back(std::make_pair(1, step));
    std::vector<uint8_t> d = make_bin(pool, blks);

    size_t pool_before = g_strings.size();
    vm_bin& vm = new_vm();
    Value r = VM_BIN_Load(d.data(), d.size(), vm);
    chk(r.t == MEM_INT && r.v == 1, "bin: 装载成功");
    chk(vm.blocks.count(S__OBJECT_STEP) == 1, "bin: 事件块进 blocks");
    chk(vm.blocks.count(S__VM_CONST_INIT) == 0, "bin: 字面量表**不**进 blocks（不是事件）");
    bool lit_ok = (vm.mem[7].t == MEM_STRING
                   && str_at(vm.mem[7].v) == "__selftest_strA"
                   && vm.mem[7].v == intern_str("__selftest_strA"));
    chk(lit_ok, "bin: 字面量表读入即执行 + 串下标已换成全局下标");

    // 再装一个带同一条字符串的 bin：全局池**不该**变长（重复取 id）
    vm_bin& vm2 = new_vm();
    Value r2 = VM_BIN_Load(d.data(), d.size(), vm2);
    chk(r2.v == 1 && vm2.mem[7].v == vm.mem[7].v, "bin: 同一个字符串 → 同一个全局下标");
    chk(g_strings.size() == pool_before + 1, "bin: 全局池只多了那 1 条（没重复加）");
}

// ============================================================
// 9 外层：两张表的契约 + 转发那条路（转发 → VM_CallFunc → callbin）
// ============================================================
static void test_outer() {
    outer_init();

    // 段基址 / 表长：这三个数一漂，已编译的 bin 就整体调错函数
    chk(SID_NATIVE_BASE == 1,    "outer: native 段头 = 1");
    chk(SID_GAME_BASE == 359,    "outer: game 段头 = 359（native 表 358 条）");
    chk(NATIVE_FN_COUNT == 358 && GAME_FN_COUNT == 150, "outer: 两张表条数 = 契约");
    chk(kNativeFnCount == NATIVE_FN_COUNT && kGameFnCount == GAME_FN_COUNT, "outer: 表本体条数对得上");

    // 池下标 → 函数：array_get 是"表尾段"里的（老代码那张表在这一段是错位的）
    chk(fn_of_sid(S_array_get) == array_get, "outer: fn_of_sid(array_get)");
    chk(fn_of_sid(S_run_block) == run_block, "outer: fn_of_sid(run_block)");
    chk(fn_of_sid(S_x) == NULL,              "outer: 字段名不是 native 函数");

    // 转发：boss_random 没有 C++ 实现 → 交给 VM_CallFunc → 打中字典里的同名 callbin
    std::vector<uint8_t> d = make_callbin("boss_random", 16, 1);
    vm_callbin cb;
    VM_CALLBIN_LOAD(d.data(), d.size(), cb);

    Value* mem = new Value[MEM_SIZE]();        // 转发要占外层临时槽，得给全长内存
    mem[3] = mk_int(41);
    uint8_t args[4] = { 3, 0, 0, 0 };          // 第 0 个实参 = 槽 3
    boss_random(mem, args, 1, 5);              // dst = 槽 5
    chk(mem[5].t == MEM_INT && mem[5].v == 51, "outer: 转发 → VM_CallFunc → callbin（41 + 10 = 51）");
    chk(g_call_off == 0, "outer: 转发完公共内存偏移归零");
    delete[] mem;
}

// ============================================================
// 10 表体检：每条函数用**空参数**跑一遍
//   · 数出还有多少条是空壳（置 VM_E_NOT_IMPL 的）
//   · 顺便冒烟：参数不对时不许崩
//   转发那 79 条会去打函数字典（没这个 callbin）→ 置的是"函数不存在"，不算空壳 ✓
// ============================================================
static void test_tables_smoke() {
    Value* mem = new Value[MEM_SIZE]();
    uint8_t args[64] = { 0 };                  // 实参表全 0：都指槽 0（值 = INT 0）

    //   ⚠ 判"这条自己是空壳"要拿**自己的**名字比：池顺序 = 表顺序，
    //     所以第 i 条的名字就是 str_at(段基址 + i)。不然"内部调到了别的空壳"会被误判（踩过）。
    int nat_stub = 0, game_stub = 0;
    for (int i = 0; i < kNativeFnCount; i++) {
        g_vm_err = VM_OK;
        kNativeFns[i](mem, args, 0, -1);
        if (g_vm_err == VM_E_NOT_IMPL && g_vm_err_a1 == SID_NATIVE_BASE + i) {
            nat_stub++;
            if (nat_stub <= 6) printf("      native 空壳: %s\n", str_at(g_vm_err_a1).c_str());
        }
    }
    for (int i = 0; i < kGameFnCount; i++) {
        g_vm_err = VM_OK;
        kGameFns[i](mem, args, 0, -1);
        if (g_vm_err == VM_E_NOT_IMPL && g_vm_err_a1 == SID_GAME_BASE + i) {
            game_stub++;
            if (game_stub <= 10) printf("      game 空壳: %s\n", str_at(g_vm_err_a1).c_str());
        }
    }
    printf("      native %d 条里空壳 %d，game %d 条里空壳 %d（合计 %d/%d）\n",
           kNativeFnCount, nat_stub, kGameFnCount, game_stub,
           nat_stub + game_stub, kNativeFnCount + kGameFnCount);
    chk(nat_stub < kNativeFnCount && game_stub < kGameFnCount, "tables: 表里有真实现（不是全空壳）");
    chk(game_stub == 0, "tables: game 那 150 条全部有实现（自举 49 + A 42 + B 46 + C 13）");
    delete[] mem;
}

// ============================================================
// 11 容器 / 实例 / 内置：真功能（按池下标调，顺便验 sid → fid 的映射）
// ============================================================
static void nat(Value* mem, VM_INT sid, int dst, std::vector<int> argslots) {
    std::vector<int32_t> tbl(argslots.begin(), argslots.end());
    kNativeFns[sid - SID_NATIVE_BASE](mem, (const uint8_t*)tbl.data(), (int)tbl.size(), dst);
}
static void gset(Value* mem, int slot, Value v) { mem[slot] = v; }   // 测试直接塞槽（不讲究引用计数）
// 调一条 game API（OP_CALL 那张表）：按池下标算 fid，和 nat 一个用法
static void gm(Value* mem, VM_INT sid, int dst, std::vector<int> argslots) {
    std::vector<int32_t> tbl(argslots.begin(), argslots.end());
    kGameFns[sid - SID_GAME_BASE](mem, (const uint8_t*)tbl.data(), (int)tbl.size(), dst);
}

static void test_self_hosted() {
    Value* mem = new Value[MEM_SIZE]();
    g_vm_err = VM_OK;

    // ── 容器：数组 ──
    gset(mem, 1, mk_int(3)); gset(mem, 2, mk_int(7));
    nat(mem, S_array_create, 3, { 1, 2 });                 // array_create(3, 7)
    nat(mem, S_array_length, 4, { 3 });
    chk(rd_i(mem, 4) == 3, "arr fn: array_create(3,7) → 长度 3");
    gset(mem, 5, mk_int(0));
    nat(mem, S_array_get, 6, { 3, 5 });
    chk(rd_i(mem, 6) == 7, "arr fn: array_get → 7");
    gset(mem, 7, mk_int(1)); gset(mem, 8, mk_int(9));
    nat(mem, S_array_set, 9, { 3, 7, 8 });
    nat(mem, S_array_get, 10, { 3, 7 });
    chk(rd_i(mem, 10) == 9, "arr fn: array_set / array_get");
    nat(mem, S_array_push, 11, { 3, 8 });                  // 数组现在是 [7, 9, 7, 9]
    nat(mem, S_array_length, 12, { 3 });
    chk(rd_i(mem, 12) == 4, "arr fn: array_push → 长度 4");
    nat(mem, S_array_get_index, 13, { 3, 8 });             // 9 第一次出现在下标 1
    chk(rd_i(mem, 13) == 1, "arr fn: array_get_index(9) → 1");
    gset(mem, 14, mk_int(0)); gset(mem, 15, mk_int(1));
    nat(mem, S_array_delete, 16, { 3, 14, 15 });
    nat(mem, S_array_length, 17, { 3 });
    chk(rd_i(mem, 17) == 3, "arr fn: array_delete → 长度 3");
    gset(mem, 18, mk_int(1));
    nat(mem, S_array_resize, 19, { 3, 18 });
    nat(mem, S_array_length, 20, { 3 });
    chk(rd_i(mem, 20) == 1, "arr fn: array_resize → 长度 1");
    nat(mem, S_array_clear, 21, { 3 });
    nat(mem, S_array_length, 22, { 3 });
    chk(rd_i(mem, 22) == 0, "arr fn: array_clear → 长度 0");

    // ── 容器：ds_list / ds_map / ds_grid ──
    nat(mem, S_ds_list_create, 30, {});
    gset(mem, 31, mk_int(42));
    nat(mem, S_ds_list_add, 32, { 30, 31 });
    nat(mem, S_ds_list_size, 33, { 30 });
    chk(rd_i(mem, 33) == 1, "list fn: ds_list_add / ds_list_size");
    gset(mem, 35, mk_int(0));                              // ds_list_find_value(list, **位置**)
    nat(mem, S_ds_list_find_value, 34, { 30, 35 });
    chk(rd_i(mem, 34) == 42, "list fn: ds_list_find_value(位置 0) = 42");

    nat(mem, S_ds_map_create, 40, {});
    int sid_key = intern_str("__selftest_key");
    gset(mem, 41, mk_str_sid(sid_key));
    gset(mem, 42, mk_int(123));
    nat(mem, S_ds_map_set, 43, { 40, 41, 42 });
    nat(mem, S_ds_map_size, 44, { 40 });
    chk(rd_i(mem, 44) == 1, "map fn: ds_map_set / ds_map_size");
    nat(mem, S_ds_map_find_value, 45, { 40, 41 });
    chk(rd_i(mem, 45) == 123, "map fn: ds_map_find_value");
    nat(mem, S_ds_map_exists, 46, { 40, 41 });
    chk(rd_i(mem, 46) == 1, "map fn: ds_map_exists");

    nat(mem, S_ds_grid_create, 50, { 1, 2 });              // 2 列 3 行？照 GML：宽 2 高 3
    gset(mem, 51, mk_int(0)); gset(mem, 52, mk_int(0));
    gset(mem, 53, mk_int(77));
    nat(mem, S_ds_grid_set, 54, { 50, 51, 52, 53 });
    nat(mem, S_ds_grid_get, 55, { 50, 51, 52 });
    chk(rd_i(mem, 55) == 77, "grid fn: ds_grid_set / ds_grid_get");

    // ── 实例 ──
    vm_bin& vm = new_vm();
    int obj = intern_str("__selftest_outer_obj");
    g_vm_of_object[obj] = &vm;
    gset(mem, 60, mk_int(10)); gset(mem, 61, mk_int(20)); gset(mem, 62, mk_int(0));
    gset(mem, 63, mk_str_sid(obj));
    nat(mem, S_instance_create_depth, 64, { 60, 61, 62, 63 });
    VM_INT iid = rd_i(mem, 64);
    chk(inst_alive(iid) && inst_get(iid)->obj == obj, "inst fn: instance_create_depth 建出来了");
    nat(mem, S_instance_exists, 65, { 64 });
    chk(rd_i(mem, 65) == 1, "inst fn: instance_exists");
    nat(mem, S_instance_number, 66, { 63 });
    chk(rd_i(mem, 66) == 1, "inst fn: instance_number");
    nat(mem, S_instance_find, 67, { 63, 62 });
    chk(rd_i(mem, 67) == iid, "inst fn: instance_find(0) = 那个实例");
    int sid_atk = intern_str("__selftest_atk");
    gset(mem, 68, mk_int(55));
    gset(mem, 69, mk_str_sid(sid_atk));
    nat(mem, S_variable_instance_set, 70, { 64, 69, 68 });
    nat(mem, S_variable_instance_get, 71, { 64, 69 });
    chk(rd_i(mem, 71) == 55, "inst fn: variable_instance_set / get");
    nat(mem, S_variable_instance_exists, 72, { 64, 69 });
    chk(rd_i(mem, 72) == 1, "inst fn: variable_instance_exists");
    nat(mem, S_instance_destroy, 73, { 64 });
    chk(!inst_alive(iid), "inst fn: instance_destroy 回收了");
    g_vm_of_object.erase(obj);

    // ── 内置：数学 / 类型 / 字符串 / 全局 / 颜色 ──
    gset(mem, 80, Value{MEM_FLOAT, i_of((VM_FLOAT)2.75)});
    nat(mem, S_floor, 81, { 80 });
    chk(rd_i(mem, 81) == 2, "builtin: floor(2.75) = 2");
    nat(mem, S_ceil, 82, { 80 });
    chk(rd_i(mem, 82) == 3, "builtin: ceil(2.75) = 3");
    gset(mem, 83, mk_int(-5));
    nat(mem, S_abs, 84, { 83 });
    chk(rd_i(mem, 84) == 5, "builtin: abs(-5) = 5");
    gset(mem, 85, mk_int(3)); gset(mem, 86, mk_int(9));
    nat(mem, S_max, 87, { 85, 86 });
    chk(rd_i(mem, 87) == 9, "builtin: max(3,9) = 9");
    nat(mem, S_min, 88, { 85, 86 });
    chk(rd_i(mem, 88) == 3, "builtin: min(3,9) = 3");
    gset(mem, 89, mk_int(0)); gset(mem, 90, mk_int(10));
    nat(mem, S_clamp, 91, { 83, 89, 90 });
    chk(rd_i(mem, 91) == 0, "builtin: clamp(-5,0,10) = 0");

    gset(mem, 92, mk_int(1));
    nat(mem, S_is_real, 93, { 92 });
    chk(rd_i(mem, 93) == 1, "builtin: is_real(1) = 1");
    int sid_abc = intern_str("__selftest_abc");
    gset(mem, 94, mk_str_sid(sid_abc));
    nat(mem, S_is_string, 95, { 94 });
    chk(rd_i(mem, 95) == 1, "builtin: is_string(\"…\") = 1");
    nat(mem, S_is_undefined, 96, { 92 });
    chk(rd_i(mem, 96) == 0, "builtin: is_undefined(1) = 0");

    nat(mem, S_string_length, 100, { 94 });
    chk(rd_i(mem, 100) == 14, "builtin: string_length(\"__selftest_abc\") = 14");
    gset(mem, 101, mk_int(0)); gset(mem, 102, mk_int(1));
    nat(mem, S_ord, 103, { 94 });
    int ch0 = rd_i(mem, 103);
    nat(mem, S_chr, 104, { 103 });
    chk(rd_t(mem, 104) == MEM_STRING && ch0 == (int)'_', "builtin: ord / chr 往返");

    gset(mem, 110, mk_int(1)); gset(mem, 111, mk_int(2)); gset(mem, 112, mk_int(3));
    nat(mem, S_make_color_rgb, 113, { 110, 111, 112 });
    chk(rd_i(mem, 113) == 1 + (2 << 8) + (3 << 16), "builtin: make_color_rgb(1,2,3)");

    int sid_g = intern_str("__selftest_global");
    gset(mem, 120, mk_str_sid(sid_g)); gset(mem, 121, mk_int(8080));
    nat(mem, S_variable_global_set, 122, { 120, 121 });
    nat(mem, S_variable_global_get, 123, { 120 });
    chk(rd_i(mem, 123) == 8080, "builtin: variable_global_set / get");
    nat(mem, S_variable_global_exists, 124, { 120 });
    chk(rd_i(mem, 124) == 1, "builtin: variable_global_exists");

    gset(mem, 130, mk_int(0)); gset(mem, 131, mk_int(5));
    nat(mem, S_irandom_range, 132, { 130, 131 });
    chk(rd_i(mem, 132) >= 0 && rd_i(mem, 132) <= 5, "builtin: irandom_range 落在区间内");

    chk(g_vm_err == VM_OK, "self-hosted: 全程没出错");
    delete[] mem;
}

// ============================================================
// 12 自举的 game API（46 条，outer/game_self.cpp）：走 kGameFns
//   只挑语义有分支的那几条验：undefined / 实例建销 / 属性读写（实例 + 全局）/
//   命名数组（含"同对象两个实例共用一份"）/ 实例数组 / 全局读写 / RunStep
// ============================================================
static void test_game_self() {
    outer_init();                              // 幂等：保证空壳已经换成 impl_*（test_outer 已调过）
    Value* mem = new Value[MEM_SIZE]();
    g_vm_err = VM_OK;

    // ── 纯计算 ──
    gset(mem, 1, Value{MEM_FLOAT, i_of((VM_FLOAT)2.75)});
    gm(mem, S_VM_Floor, 2, { 1 });
    chk(rd_i(mem, 2) == 2, "game: VM_Floor(2.75) = 2");
    gm(mem, S_VM_Ceil, 3, { 1 });
    chk(rd_i(mem, 3) == 3, "game: VM_Ceil(2.75) = 3");
    gset(mem, 4, mk_int(10));
    gm(mem, S_VM_Random, 5, { 4 });
    double rv = (rd_t(mem, 5) == MEM_FLOAT) ? (double)f_of(mem[5].v) : (double)rd_i(mem, 5);
    chk(rv >= 0.0 && rv <= 10.0, "game: VM_Random(10) 落在 [0,10]");

    // ── VM_IsUndefined ──
    gset(mem, 6, mk_undef());
    gm(mem, S_VM_IsUndefined, 7, { 6 });
    chk(rd_i(mem, 7) == 1, "game: VM_IsUndefined(undefined) = 1");
    gset(mem, 8, mk_int(5));
    gm(mem, S_VM_IsUndefined, 9, { 8 });
    chk(rd_i(mem, 9) == 0, "game: VM_IsUndefined(5) = 0");

    // ── 实例：建 / 数 / 取第 k 个 / 销毁 ──
    vm_bin& vm = new_vm();
    int obj = intern_str("__selftest_game_obj");
    g_vm_of_object[obj] = &vm;
    gset(mem, 10, mk_str_sid(obj));
    gm(mem, S_VM_CreateInstance, 11, { 10 });
    VM_INT id1 = rd_i(mem, 11);
    chk(inst_alive(id1) && inst_get(id1) && inst_get(id1)->obj == obj, "game: VM_CreateInstance 建出来了");
    chk(vm.inst_count == 1, "game: 新实例挂在**对象自己的 VM** 上");
    gm(mem, S_VM_GetInstanceCount, 12, { 10 });
    chk(rd_i(mem, 12) == 1, "game: VM_GetInstanceCount = 1");
    gset(mem, 13, mk_int(0));
    gm(mem, S_VM_GetInstanceAt, 14, { 10, 13 });
    chk(rd_i(mem, 14) == id1, "game: VM_GetInstanceAt(0) = 那个实例");
    gm(mem, S_VM_IsDestroyed, 15, { 11 });
    chk(rd_i(mem, 15) == 0, "game: VM_IsDestroyed(活着的) = 0");
    gm(mem, S_VM_DestroyInstance, -1, { 11 });
    chk(!inst_alive(id1), "game: VM_DestroyInstance 回收了");
    gm(mem, S_VM_IsDestroyed, 16, { 11 });
    chk(rd_i(mem, 16) == 1, "game: VM_IsDestroyed(死了的) = 1");
    gset(mem, 17, mk_int(0));                          // 不是对象名
    gm(mem, S_VM_CreateInstance, 18, { 17 });
    chk(rd_i(mem, 18) == -4, "game: VM_CreateInstance(非对象名) = noone(-4)");

    // ── 属性读写（实例 + 全局 0 号 + 内置字段）──
    VM_INT ia = inst_new(vm, obj), ib = inst_new(vm, obj);
    int sid_atk = intern_str("__selftest_game_atk");
    gset(mem, 20, mk_int(ia)); gset(mem, 21, mk_str_sid(sid_atk)); gset(mem, 22, mk_int(77));
    gm(mem, S_VM_SetProp, -1, { 20, 21, 22 });
    gm(mem, S_VM_GetProp, 23, { 20, 21 });
    chk(rd_i(mem, 23) == 77, "game: VM_SetProp / VM_GetProp（实例）");
    gset(mem, 24, mk_str_sid(S_x));
    gm(mem, S_VM_GetProp, 25, { 20, 24 });
    chk(rd_i(mem, 25) == 0, "game: VM_GetProp 内置字段 x 走 attr");
    gset(mem, 26, mk_int(0));                          // 0 = 全局
    int sid_g = intern_str("__selftest_game_global");
    gset(mem, 27, mk_str_sid(sid_g)); gset(mem, 28, mk_int(9));
    gm(mem, S_VM_SetProp, -1, { 26, 27, 28 });
    gm(mem, S_VM_GetProp, 29, { 26, 27 });
    chk(rd_i(mem, 29) == 9, "game: VM_SetProp / GetProp 的 0 号目标 = 全局变量");
    gset(mem, 30, mk_str_sid(intern_str("__selftest_game_none")));
    gm(mem, S_VM_GetProp, 31, { 20, 30 });
    chk(rd_t(mem, 31) == MEM_UNDEFINED, "game: VM_GetProp 没设过 → undefined");
    gm(mem, S_VM_GetProp, 32, { 11, 21 });
    chk(rd_t(mem, 32) == MEM_UNDEFINED, "game: VM_GetProp(死实例) → undefined");

    // ── 命名数组：一个 VM 一份 → 同一个对象的两个实例看到的是同一份 ──
    int nar = intern_str("__selftest_game_named");
    run_push(vm, ia);                                  // 命名数组认"当前是哪个 VM"
    gset(mem, 40, mk_str_sid(nar));
    gm(mem, S_VM_ArrayExists, 41, { 40 });
    chk(rd_i(mem, 41) == 0, "game: VM_ArrayExists 没有 → 0");
    gset(mem, 42, mk_int(0)); gset(mem, 43, mk_int(123));
    gm(mem, S_VM_ArraySet, -1, { 40, 42, 43 });
    gm(mem, S_VM_ArraySize, 44, { 40 });
    chk(rd_i(mem, 44) == 1, "game: VM_ArraySet / VM_ArraySize");
    gm(mem, S_VM_ArrayGet, 45, { 40, 42 });
    chk(rd_i(mem, 45) == 123, "game: VM_ArrayGet = 123");
    gm(mem, S_VM_ArrayContains, 46, { 40, 43 });
    chk(rd_i(mem, 46) == 1, "game: VM_ArrayContains 有 = 1");
    gset(mem, 47, mk_int(999));
    gm(mem, S_VM_ArrayContains, 48, { 40, 47 });
    chk(rd_i(mem, 48) == 0, "game: VM_ArrayContains 没有 = 0");
    gm(mem, S_VM_ArrayExists, 49, { 40 });
    chk(rd_i(mem, 49) == 1, "game: VM_ArrayExists 有了 = 1");
    gset(mem, 50, mk_int(7));
    gm(mem, S_VM_ArrayADD, -1, { 40, 50 });
    gm(mem, S_VM_ArraySize, 51, { 40 });
    chk(rd_i(mem, 51) == 2, "game: VM_ArrayADD → 长度 2");
    run_pop();
    run_push(vm, ib);                                  // 同一个对象的**另一个实例**
    gm(mem, S_VM_ArrayGet, 52, { 40, 42 });
    chk(rd_i(mem, 52) == 123, "game: 同类型两个实例共用一份命名数组（读得到同一个值）");
    gm(mem, S_VM_ArraySize, 53, { 40 });
    chk(rd_i(mem, 53) == 2, "game: 另一个实例看到的长度也是 2");
    gset(mem, 54, mk_int(0));
    gm(mem, S_VM_ArrayDel, -1, { 40, 54 });
    gm(mem, S_VM_ArraySize, 55, { 40 });
    chk(rd_i(mem, 55) == 1, "game: VM_ArrayDel → 长度 1");
    gm(mem, S_VM_ArrayClear, -1, { 40 });
    gm(mem, S_VM_ArraySize, 56, { 40 });
    chk(rd_i(mem, 56) == 0, "game: VM_ArrayClear → 长度 0");
    gm(mem, S_VM_ArrayExists, 57, { 40 });
    chk(rd_i(mem, 57) == 1, "game: clear 之后数组还在（Exists = 1）");
    gm(mem, S_VM_ArrayClearAll, -1, { 40 });
    gm(mem, S_VM_ArrayExists, 58, { 40 });
    chk(rd_i(mem, 58) == 0, "game: VM_ArrayClearAll 把它删了");
    run_pop();

    // ── 实例数组（VM_InstArray*）：落在 Instance::arrays ──
    int iar = intern_str("__selftest_game_iarr");
    gset(mem, 60, mk_int(ia)); gset(mem, 61, mk_str_sid(iar));
    gm(mem, S_VM_InstArrayExists, 62, { 60, 61 });
    chk(rd_i(mem, 62) == 0, "game: VM_InstArrayExists 没有 = 0");
    gm(mem, S_VM_InstArraySize, 63, { 60, 61 });
    chk(rd_i(mem, 63) == -1, "game: VM_InstArraySize 没有 = -1");
    gm(mem, S_VM_InstArrayClear, -1, { 60, 61 });      // `x = []` 走它：不存在也当建成空数组
    gm(mem, S_VM_InstArrayExists, 64, { 60, 61 });
    chk(rd_i(mem, 64) == 1, "game: VM_InstArrayClear 把数组建出来了");
    gset(mem, 65, mk_int(5));
    gm(mem, S_VM_InstArrayAdd, 66, { 60, 61, 65 });
    chk(rd_i(mem, 66) == 1, "game: VM_InstArrayAdd = 1");
    gm(mem, S_VM_InstArraySize, 67, { 60, 61 });
    chk(rd_i(mem, 67) == 1, "game: VM_InstArraySize = 1");
    gset(mem, 68, mk_int(0));
    gm(mem, S_VM_InstArrayItem, 69, { 60, 61, 68 });
    chk(rd_i(mem, 69) == 5, "game: VM_InstArrayItem(0) = 5");
    gset(mem, 70, mk_int(9));
    gm(mem, S_VM_InstArraySet, 71, { 60, 61, 68, 70 });
    chk(rd_i(mem, 71) == 1, "game: VM_InstArraySet = 1");
    gm(mem, S_VM_InstArrayItem, 72, { 60, 61, 68 });
    chk(rd_i(mem, 72) == 9, "game: VM_InstArrayItem 改后 = 9");
    gm(mem, S_VM_InstArrayContains, 73, { 60, 61, 70 });
    chk(rd_i(mem, 73) == 1, "game: VM_InstArrayContains 有 = 1");
    gset(mem, 74, mk_int(1));                          // 越界
    gm(mem, S_VM_InstArrayItem, 75, { 60, 61, 74 });
    chk(rd_i(mem, 75) == -1, "game: VM_InstArrayItem 越界 = -1");
    gm(mem, S_VM_InstArraySet, 76, { 60, 61, 74, 70 });
    chk(rd_i(mem, 76) == -1, "game: VM_InstArraySet 越界 = -1（实例那支不自动扩容）");
    gm(mem, S_VM_InstArrayDel, 77, { 60, 61, 68 });
    chk(rd_i(mem, 77) == 1, "game: VM_InstArrayDel = 1");
    gm(mem, S_VM_InstArraySize, 78, { 60, 61 });
    chk(rd_i(mem, 78) == 0, "game: VM_InstArrayDel 之后长度 0");
    gm(mem, S_VM_InstArrayContains, 79, { 60, 61, 70 });
    chk(rd_i(mem, 79) == 0, "game: VM_InstArrayContains 空数组 = 0");
    gset(mem, 80, mk_str_sid(intern_str("__selftest_game_iarr2")));
    gm(mem, S_VM_InstArrayContains, 81, { 60, 80, 70 });
    chk(rd_i(mem, 81) == -1, "game: VM_InstArrayContains 没这个数组 = -1");
    // 0 = 全局那支：名字当全局变量名
    int sid_ga = intern_str("__selftest_game_global_arr");
    gset(mem, 82, mk_int(0)); gset(mem, 83, mk_str_sid(sid_ga)); gset(mem, 84, mk_int(4));
    gm(mem, S_VM_InstArrayAdd, 85, { 82, 83, 84 });
    chk(rd_i(mem, 85) == 1, "game: VM_InstArrayAdd 的 0 号目标 = 全局数组（会自动建）");
    gset(mem, 86, mk_int(0));
    gm(mem, S_VM_InstArrayItem, 87, { 82, 83, 86 });
    chk(rd_i(mem, 87) == 4, "game: 全局数组 Item(0) = 4");

    // ── 全局读写那 17 条 ──
    gset(mem, 90, mk_int(3));
    gm(mem, S_VM_SetFlame, -1, { 90 });
    gm(mem, S_VM_GetFlame, 91, { 90 });
    chk(rd_i(mem, 91) == 3, "game: VM_SetFlame / VM_GetFlame 往返");
    gset(mem, 92, mk_int(5));
    gm(mem, S_VM_SetWave, -1, { 92 });
    gm(mem, S_VM_GetWave, 93, { 92 });
    chk(rd_i(mem, 93) == 5, "game: VM_SetWave / VM_GetWave 往返");
    gm(mem, S_VM_SetShovelFlameRate, -1, {});          // 没给实参 → 写 1（老代码 g_set_g 的规矩）
    gset(mem, 94, mk_str_sid(S__VM_shovel_flame_rate));
    nat(mem, S_variable_global_get, 95, { 94 });
    chk(rd_i(mem, 95) == 1, "game: VM_SetShovelFlameRate 没给实参 → 写 1");
    gset(mem, 96, mk_int(1));
    gm(mem, S_VM_BanWeapon, -1, { 96 });
    gset(mem, 97, mk_str_sid(S__VM_ban_weapon));
    nat(mem, S_variable_global_get, 98, { 97 });
    chk(rd_i(mem, 98) == 1, "game: VM_BanWeapon 写的就是那张全局表");
    gm(mem, S_VM_GetCurCard, 99, {});
    chk(rd_t(mem, 99) == MEM_UNDEFINED, "game: VM_GetCurCard 没设过 → undefined");
    gm(mem, S_VM_GetLastCreatedEnemy, 100, {});
    chk(rd_t(mem, 100) == MEM_UNDEFINED, "game: VM_GetLastCreatedEnemy 没设过 → undefined");
    gset(mem, 101, mk_int(2));
    gm(mem, S_VM_SetCardLevelCap, -1, { 101 });
    gset(mem, 102, mk_str_sid(S__VM_card_level_cap));
    nat(mem, S_variable_global_get, 103, { 102 });
    chk(rd_i(mem, 103) == 2, "game: VM_SetCardLevelCap 写全局");

    // ── VM_RunStep：额外跑 N 轮 Step（_OBJECT_STEP + _OBJECT_STEP_END）──
    {
        vm_bin& v2 = new_vm();
        int o2 = intern_str("__selftest_runstep_obj");
        g_vm_of_object[o2] = &v2;
        std::vector<uint8_t> c;                        // 槽 200 += 1
        put_assign_int(c, 201, 1);
        c.push_back(OP_ADD); put32(c, 200); put32(c, 200); put32(c, 201);
        c.push_back(OP_HALT);
        Block b; b.name_sid = S__OBJECT_STEP; b.code = c;
        v2.blocks[S__OBJECT_STEP] = b;
        VM_INT idr = inst_new(v2, o2);

        gset(mem, 110, mk_int(idr)); gset(mem, 111, mk_int(3));
        gm(mem, S_VM_RunStep, 112, { 110, 111 });
        chk(rd_i(mem, 112) == 3, "game: VM_RunStep 跑了 3 轮");
        chk(rd_i(v2.mem, 200) == 3, "game: VM_RunStep 每轮都真的跑了 _OBJECT_STEP");
        gset(mem, 113, mk_int(0));                     // 轮数夹在 1..60
        gm(mem, S_VM_RunStep, 114, { 110, 113 });
        chk(rd_i(mem, 114) == 1 && rd_i(v2.mem, 200) == 4, "game: VM_RunStep(0) 夹到 1 轮");
        gm(mem, S_VM_RunStep, 115, { 11, 111 });       // 槽 11 那个实例已经销毁了
        chk(rd_i(mem, 115) == 0, "game: VM_RunStep(死实例) = 0");
        g_vm_of_object.erase(o2);
    }

    chk(g_vm_err == VM_OK, "game self: 全程没出错");
    g_vm_of_object.erase(obj);
    delete[] mem;
}

// ============================================================
// 12 宿主层（host/）：自己能干的那半要有真实现（文件 / 缓冲区），
//    看引擎的那半默认是"没接"（全假 / 空操作），不许崩
// ============================================================
static void test_host() {
    // 文件：写 → 读回 → 删
    const char* path = "__selftest_host.txt";
    chk(!host_file_exists(path), "host: 文件本来不存在");
    void* w = host_file_open(path, HOST_FILE_WRITE);
    chk(w != NULL, "host: file_open(write)");
    chk(host_file_writeln(w, "hello") && host_file_writeln(w, "world"), "host: file_writeln");
    host_file_close(w);
    chk(host_file_exists(path), "host: 写完文件在");

    void* r = host_file_open(path, HOST_FILE_READ_TEXT);
    chk(r != NULL, "host: file_open(read)");
    chk(host_file_readln(r) == "hello", "host: file_readln 第一行");
    chk(!host_file_eof(r), "host: 还有一行 → 不是 EOF");
    chk(host_file_readln(r) == "world", "host: file_readln 第二行");
    chk(host_file_eof(r), "host: 读完了 → EOF");
    host_file_close(r);
    chk(remove(path) == 0, "host: 收尾删掉");

    // 目录
    chk(!host_dir_exists("__selftest_dir_x"), "host: 目录本来不存在");
    chk(host_dir_create("__selftest_dir_x") && host_dir_exists("__selftest_dir_x"), "host: 建目录");
    std::error_code ec; fs::remove("__selftest_dir_x", ec);

    // 缓冲区：写进去 → peek 出来 → 游标 → 越界变长
    int b = host_buf_create(4, 0);
    chk(host_buf_alive(b) && host_buf_size(b) == 4, "host: buf_create(4)");
    uint8_t bytes[4] = { 1, 2, 3, 4 };
    chk(host_buf_write(b, bytes, 4) && host_buf_tell(b) == 4, "host: buf_write 游标走到 4");
    uint8_t out[4] = { 0 };
    chk(host_buf_peek(b, 0, out, 4) && out[2] == 3, "host: buf_peek");
    host_buf_seek(b, 0);
    chk(host_buf_read(b, out, 4) == 4 && out[0] == 1, "host: buf_read");
    uint8_t more[4] = { 9, 9, 9, 9 };
    host_buf_seek(b, 8);
    host_buf_write(b, more, 4);
    chk(host_buf_size(b) == 12, "host: 写越界自动变长");
    host_buf_delete(b);
    chk(!host_buf_alive(b), "host: buf_delete");

    // "没接引擎"那半：默认实现必须是安全的假 / 空操作
    chk(!g_host_in.key_pressed(65) && g_host_in.mouse_x() == 0.0, "host: 输入没接 → 全假 / 0");
    g_host_draw.set_colour(0xFFFFFF);                       // 空操作，只要不崩
    (void)g_host_draw.sprite_width(1);
    chk(g_host_draw.string_width("abc") == 0, "host: 绘制没接 → 量宽 0");
    host_draw_set_size(640, 480);
    chk(host_draw_width() == 640 && host_draw_height() == 480, "host: 画布尺寸能填进去");
    chk(!g_host_net.send_raw(0, "x", 1), "host: 网络没接 → 发送失败");
    chk(!host_date_current().empty(), "host: 时间能取到");
}

// ============================================================
// 13 inst_new 里会跑 Create（建和销对称）
//   手写一个 _OBJECT_CREATE 块：往槽 5 写 77 —— 除了 inst_new 没别人会跑它
// ============================================================
static void test_create_event() {
    vm_bin& vm = new_vm();
    int obj = intern_str("__selftest_create_obj");
    g_vm_of_object[obj] = &vm;

    std::vector<uint8_t> c;
    put_assign_int(c, 5, 77);
    c.push_back(OP_HALT);
    Block b; b.name_sid = S__OBJECT_CREATE; b.code = c;
    vm.blocks[S__OBJECT_CREATE] = b;

    chk(rd_i(vm.mem, 5) == 0, "create: 建之前槽 5 还是 0");
    VM_INT id = inst_new(vm, obj);
    chk(rd_i(vm.mem, 5) == 77, "create: inst_new 把 _OBJECT_CREATE 跑了一遍");
    chk(inst_alive(id) && inst_get(id)->obj == obj, "create: 实例也建出来了");
    chk(g_run_sp == 0, "create: 调用栈退干净了");

    vm.blocks.erase(S__OBJECT_CREATE);
    inst_kill(id);
    g_vm_of_object.erase(obj);
}

// ============================================================
// 14 转发**从 callbin 帧里**发出来时不许写到帧外
//   手写一个 callbin：函数体 = OP_NATIVE(_find_right_cell) ; HALT
//   （_find_right_cell 是转发类，帧里只有 slots 个槽；以前它会在 SCRATCH_BASE+32 拼参数）
//   验法：在公共内存那个位置埋个哨兵，跑完看它有没有被动过
// ============================================================
static std::vector<uint8_t> make_callbin_code(const char* name, int slots, int argc,
                                              const std::vector<uint8_t>& init,
                                              const std::vector<uint8_t>& body) {
    std::vector<uint8_t> c;
    put32(c, 0x4E424C43);                 // 'CLBN'
    put32(c, slots);
    put32(c, argc);
    put_header_str(c, name);
    put_header_str(c, "");
    std::vector<std::string> pool;
    pool.push_back("_VM_CONST_INIT");
    pool.push_back("_CALL_ENTRY");
    put32(c, (int)pool.size());
    for (size_t i = 0; i < pool.size(); i++) put_header_str(c, pool[i]);
    put32(c, (int)init.size()); put32(c, 0);
    c.insert(c.end(), init.begin(), init.end());
    put32(c, (int)body.size()); put32(c, 1);
    c.insert(c.end(), body.begin(), body.end());
    return c;
}

static void test_forward_in_frame() {
    std::vector<uint8_t> init; init.push_back(OP_HALT);
    std::vector<uint8_t> body;
    body.push_back(OP_NATIVE);
    int fid = (int)(S__find_right_cell - SID_NATIVE_BASE);      // 转发类里第 1 条
    body.push_back((uint8_t)(fid & 0xFF));
    body.push_back((uint8_t)((fid >> 8) & 0xFF));
    body.push_back(0);                                          // argc = 0
    put32(body, 0);                                             // dst = 槽 0
    body.push_back(OP_HALT);

    std::vector<uint8_t> d = make_callbin_code("__selftest_fwd_in_frame", 16, 0, init, body);
    vm_callbin cb;
    VM_CALLBIN_LOAD(d.data(), d.size(), cb);
    chk(cb.value == 16, "fwd-in-frame: callbin 装好了");

    if (g_call_mem.empty()) g_call_mem.assign(CALL_MEM_SLOTS, Value{0, 0});
    g_call_mem[(size_t)OUTER_TMP_SLOT] = mk_int(4660);          // 哨兵：0x1234
    Value r = callbin_call(intern_str("__selftest_fwd_in_frame"), NULL, 0);
    chk(g_call_mem[(size_t)OUTER_TMP_SLOT].v == 4660,
        "fwd-in-frame: 转发没写到帧外（临时槽哨兵没被踩）");
    chk(r.t == MEM_UNDEFINED || r.t == MEM_INT, "fwd-in-frame: 没崩、有返回值形态");
    chk(g_call_off == 0, "fwd-in-frame: 公共内存偏移归零");
    g_call_mem[(size_t)OUTER_TMP_SLOT] = mk_int(0);
}

// ============================================================
// 15 自己能算的那 27 条 native（文件 / 缓冲区 / 日期）—— 走表真跑一遍
//   签名照 GML：file_text_open_write(路径) → 句柄；buffer_write(buffer, type, 值) …
// ============================================================
static void test_host_natives() {
    Value* mem = new Value[MEM_SIZE]();
    const char* path = "__selftest_hn.txt";
    int sid_path = intern_str(path);

    // ── 文件：写两段 → 关 → 问存在 → 读回 ──
    gset(mem, 1, mk_str_sid(sid_path));
    nat(mem, S_file_text_open_write, 2, { 1 });
    VM_INT h = rd_i(mem, 2);
    chk(h >= 0, "hn: file_text_open_write 给句柄");
    int sid_hello = intern_str("hello");
    gset(mem, 3, mk_int(h)); gset(mem, 4, mk_str_sid(sid_hello));
    nat(mem, S_file_text_write_string, 5, { 3, 4 });          // 写 "hello"（不加换行）
    nat(mem, S_file_text_writeln, 7, { 3 });                  // 只写一个换行（GML 签名就是只有 fileid）
    nat(mem, S_file_text_close, 8, { 3 });
    chk(fs::file_size(path) == 6, "hn: 落盘 6 字节 = \"hello\\n\"");

    nat(mem, S_file_exists, 9, { 1 });
    chk(rd_i(mem, 9) == 1, "hn: file_exists = 1");

    nat(mem, S_file_text_open_read, 10, { 1 });
    VM_INT hr = rd_i(mem, 10);
    chk(hr >= 0, "hn: file_text_open_read 给句柄");
    gset(mem, 11, mk_int(hr));
    nat(mem, S_file_text_read_string, 12, { 11 });       // 读一个"词"（到空白为止）
    chk(rd_t(mem, 12) == MEM_STRING && str_at(rd_i(mem, 12)) == "hello",
        "hn: read_string 读回 \"hello\"");
    nat(mem, S_file_text_readln, 13, { 11 });            // GML 语义：跳过一行（无返回值）
    nat(mem, S_file_text_eof, 14, { 11 });
    chk(rd_i(mem, 14) == 1, "hn: readln 跳行后 → eof");
    nat(mem, S_file_text_close, 15, { 11 });

    // ── 缓冲区：建 → 写 u8 200 → 问大小 → 回零读回 ──
    gset(mem, 20, mk_int(4)); gset(mem, 21, mk_int(1));       // 长度 4，类型 u8(=1)
    nat(mem, S_buffer_create, 22, { 20, 21 });
    VM_INT b = rd_i(mem, 22);
    chk(b >= 0, "hn: buffer_create");
    gset(mem, 23, mk_int(b)); gset(mem, 24, mk_int(1)); gset(mem, 25, mk_int(200));
    nat(mem, S_buffer_write, 26, { 23, 24, 25 });             // buffer_write(buf, u8, 200)
    nat(mem, S_buffer_get_size, 27, { 23 });
    chk(rd_i(mem, 27) == 4, "hn: buffer_get_size = 4");
    gset(mem, 28, mk_int(0));
    nat(mem, S_buffer_seek, 29, { 23, 28, 28 });          // buffer_seek(buf, base=0, offset=0)
    nat(mem, S_buffer_read, 30, { 23, 24 });
    chk(rd_i(mem, 30) == 200, "hn: 写 u8 200 → seek(头,0) → 读回 200");
    nat(mem, S_buffer_delete, 31, { 23 });
    chk(rd_i(mem, 27) == 4, "hn: （删之前大小还是 4）");

    // ── 日期：GML 的 datetime（1899-12-30 起的天数）是浮点，2024 年 ≈ 45xxx ──
    nat(mem, S_date_current_datetime, 32, {});
    chk(rd_t(mem, 32) == MEM_FLOAT && rd_f(mem, 32) > (VM_FLOAT)40000
        && rd_f(mem, 32) < (VM_FLOAT)60000, "hn: date_current_datetime 是个合理天数");
    gset(mem, 33, mk_int(0));
    nat(mem, S_date_datetime_string, 34, { 32 });
    chk(rd_t(mem, 34) == MEM_STRING && str_at(rd_i(mem, 34)).size() >= 19,
        "hn: date_datetime_string 给出 \"YYYY-MM-DD hh:mm:ss\"");

    remove(path);
    delete[] mem;
}

// ============================================================
// 16 "名字 → 实现"的两张表：DLL 函数表优先，再 callbin 表
// ============================================================
static void dll_fn_boss(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, 999);                       // 覆盖同名 callbin 的结果（那边是 51）
}
static void dll_fn_myfunc(Value* mem, const uint8_t* args, int argc, int dst) {
    int a = (argc > 0) ? (int)arg_i(mem, args, 0) : 0;
    wr_i(mem, dst, 1234 + a);
}

static void test_dual_table() {
    chk(VM_FuncApiVersion() >= 1, "dll: 接口版本问得出来");
    chk(VM_RegisterFunc("__selftest_dll_fn", dll_fn_myfunc) == 1, "dll: 注册成功");
    chk(VM_RegisterFunc(0, dll_fn_myfunc) == 0 && VM_RegisterFunc("x", 0) == 0,
        "dll: 参数不对 → 0");

    Value* mem = new Value[MEM_SIZE]();
    gset(mem, 3, mk_int(41));
    uint8_t args[4] = { 3, 0, 0, 0 };

    // ① 走 native 表那条转发路：没注册 → callbin；注册 → DLL 表优先
    boss_random(mem, args, 1, 5);
    chk(mem[5].t == MEM_INT && mem[5].v == 51, "dll: 没注册时走 callbin（41 + 10 = 51）");
    chk(VM_RegisterFunc("boss_random", dll_fn_boss) == 1, "dll: 注册 boss_random");
    boss_random(mem, args, 1, 5);
    chk(mem[5].v == 999, "dll: 注册后 **DLL 表优先**（999，不再走 callbin）");
    VM_UnregisterFunc("boss_random");
    boss_random(mem, args, 1, 5);
    chk(mem[5].v == 51, "dll: 注销后回到 callbin");

    // ② 走 VM_CallFunc 那条路（名字取自第 0 个实参）
    gset(mem, 10, mk_str_sid(intern_str("__selftest_dll_fn")));
    gset(mem, 11, mk_int(7));
    gm(mem, S_VM_CallFunc, 12, { 10, 11 });
    chk(mem[12].v == 1241, "dll: VM_CallFunc 也走 DLL 表（1234 + 7）");
    gm(mem, S_VM_FuncExists, 13, { 10 });
    chk(mem[13].v == 1, "dll: VM_FuncExists 认 DLL 表");

    delete[] mem;
}

// ============================================================
// 17 出站通道：host_shm_*（host 层） + VM_HostPush（VM 侧唯一那条内置函数）
//
//   编码格式见 outer/host_push.cpp 的 ★【编码格式】★ 那一块。
//   ⚠ 期望字节**集中成下面这几个常量**：格式还在定，将来改格式只动这一处就行。
// ============================================================
namespace {

// 期望字节（**格式要改只动这一处**）：包里**没有类型字节**，就是纯 payload，小端
//   int  → s32
//   float→ f32
//   string → u32 长度（字节数）+ UTF-8
//   buf  → u32 长度（字节数）+ 裸字节（实参是 int 且是活着的宿主 buffer id）
//   undefined / 容器 → **0 字节**
const uint8_t EXP_INT[]   = { 0x02, 0x01, 0x00, 0x00 };                  // int 258（0x00000102 小端）
const uint8_t EXP_FLOAT[] = { 0x00, 0x00, 0xC0, 0x3F };                  // float 1.5f（IEEE 位 0x3FC00000）
const uint8_t EXP_STR[]   = { 0x02, 0x00, 0x00, 0x00, 'H', 'i' };        // string "Hi"（u32 长度 = 2）
const uint8_t EXP_BUF[]   = { 0x03, 0x00, 0x00, 0x00, 'X', 'Y', 'Z' };   // buf 3 字节（u32 长度 + 裸字节）
const int     EXP_UNDEF_N = 0;                                           // undefined → 一个字节都不写

const int EXP_TOTAL = (int)(sizeof(EXP_INT) + sizeof(EXP_FLOAT) + sizeof(EXP_STR));

// 逐字节比；不一样就把**第一个不同的位置**打出来（只报"错了"没法查）
bool bytes_eq(const uint8_t* got, const uint8_t* exp, int n, const char* what) {
    for (int i = 0; i < n; i++) {
        if (got[i] != exp[i]) {
            printf("      [diff] %s: 第 %d 字节 got=0x%02X exp=0x%02X\n", what, i,
                   (unsigned)got[i], (unsigned)exp[i]);
            return false;
        }
    }
    return true;
}

// 按名字调 —— 和 GML 里写 VM_HostPush(...) 编出来的那条路等价：
//   vm_call_by_name → 先查 DLL 函数表（VM_RegisterFunc 挂进去的）
void push_by_name(Value* mem, const char* name, std::vector<int> argslots, int dst) {
    std::vector<int32_t> tbl(argslots.begin(), argslots.end());
    vm_call_by_name(intern_str(name), mem, (const uint8_t*)tbl.data(), (int)tbl.size(), dst);
}

}  // namespace

static void test_host_push() {
    // ── 布局契约：版本导出 + 两处常量必须是同一份（生成物与代码对齐）──
    //   为什么值得测：这套接口是"不链接、只约定"，偏移错开**不会报错**只会读出垃圾；
    //   版本号是引擎唯一能问的暗号，所以它必须真的接得上、且和生成物一致。
    chk(host_layout_version() == HOST_LAYOUT_VERSION && host_layout_version() >= 1,
        "layout: host_layout_version() == HOST_LAYOUT_VERSION");
    chk(HOST_LAYOUT_VALUE_BYTES == (int)sizeof(Value),
        "layout: 契约的槽宽 == sizeof(Value)（槽就是 Value 数组的元素）");

    // ── host 层：复位 / 初始容量 / 首地址 / 追加 ──
    host_shm_reset();
    chk(host_shm_cursor() == 0, "push: host_shm_reset → cursor == 0");
    chk(host_shm_size() == 64 * 1024, "push: 初始容量 = 64KB");
    chk(host_shm_base() != NULL, "push: host_shm_base() 非空（引擎随时能取）");
    chk(host_shm_write("abc", 3), "push: host_shm_write(\"abc\", 3)");
    chk(host_shm_cursor() == 3, "push: 写 3 字节后 cursor == 3");
    chk(bytes_eq(host_shm_base(), (const uint8_t*)"abc", 3, "host_shm_write"),
        "push: host_shm_base()[0..2] == \"abc\"");

    // ── host 层"按类型追加"那三支：宿主/引擎侧**直接可用**（不必经过 VM）──
    //   编码规则在 host 那一层，这里验"三支拼出来和 VM 那条路是同一份格式"
    host_shm_reset();
    chk(host_shm_push_i32(258), "push: host_shm_push_i32(258)");
    chk(host_shm_push_f32(1.5f), "push: host_shm_push_f32(1.5f)");
    chk(host_shm_push_bytes("Hi", 2), "push: host_shm_push_bytes(\"Hi\", 2)");
    {
        uint8_t exp[64];
        int n = 0;
        memcpy(exp + n, EXP_INT,   sizeof(EXP_INT));   n += (int)sizeof(EXP_INT);
        memcpy(exp + n, EXP_FLOAT, sizeof(EXP_FLOAT)); n += (int)sizeof(EXP_FLOAT);
        memcpy(exp + n, EXP_STR,   sizeof(EXP_STR));   n += (int)sizeof(EXP_STR);
        chk(bytes_eq(host_shm_base(), exp, n, "host 三支"),
            "push: host 侧三支拼出来 == 同一份格式（和 VM 那条路一致）");
    }
    chk(host_shm_cursor() == EXP_TOTAL, "push: host 侧三支写完 cursor == 期望合计");
    chk(host_shm_push_bytes(NULL, 0), "push: host_shm_push_bytes(NULL, 0) 也认（空串 / 空 buf）");
    chk(host_shm_cursor() == EXP_TOTAL + 4, "push: 空 bytes = 4 字节（u32 长度 0）");
    host_shm_reset();

    // ── VM 侧：推 int / float / string 三个实参，逐字节校验 ──
    outer_init();                                   // 幂等；就算不调，vm_find_dll_fn 也会兜底登记
    Value* mem = new Value[MEM_SIZE]();
    host_shm_reset();

    gset(mem, 1, mk_int(258));
    { Value f; f.t = MEM_FLOAT; f.v = i_of((VM_FLOAT)1.5); gset(mem, 2, f); }
    gset(mem, 3, mk_str_sid(intern_str("Hi")));

    push_by_name(mem, "VM_HostPush", { 1, 2, 3 }, 4);
    chk(rd_t(mem, 4) == MEM_INT && rd_i(mem, 4) == EXP_TOTAL,
        "push: 返回值 = 写入的字节数（= 期望表合计）");
    chk(host_shm_cursor() == EXP_TOTAL, "push: 游标跟着推到同样的字节数");
    {
        uint8_t exp[64];
        int n = 0;
        memcpy(exp + n, EXP_INT,   sizeof(EXP_INT));   n += (int)sizeof(EXP_INT);
        memcpy(exp + n, EXP_FLOAT, sizeof(EXP_FLOAT)); n += (int)sizeof(EXP_FLOAT);
        memcpy(exp + n, EXP_STR,   sizeof(EXP_STR));   n += (int)sizeof(EXP_STR);
        chk(bytes_eq(host_shm_base(), exp, n, "VM_HostPush(258, 1.5, \"Hi\")"),
            "push: 逐字节 == 纯 payload（无类型字节）首尾相接");
    }

    // ── 再走一遍 GML 那条路：gm 按表调 VM_CallFunc("VM_HostPush", …) ──
    host_shm_reset();
    gset(mem, 10, mk_str_sid(intern_str("VM_HostPush")));
    gset(mem, 11, mk_int(258));
    gm(mem, S_VM_CallFunc, 12, { 10, 11 });
    chk(rd_i(mem, 12) == (int)sizeof(EXP_INT), "push: VM_CallFunc 那条路也通（返回值对）");
    chk(bytes_eq(host_shm_base(), EXP_INT, (int)sizeof(EXP_INT), "VM_CallFunc→VM_HostPush"),
        "push: VM_CallFunc 路逐字节 == EXP_INT");

    // ── 其它类型：undefined 和容器都**一个字节都不写**（包里没有类型信息）──
    host_shm_reset();
    gset(mem, 20, mk_undef());
    { Value arr; arr.t = MEM_ARRAY; arr.v = 0; gset(mem, 21, arr); }
    push_by_name(mem, "VM_HostPush", { 20, 21 }, 22);
    chk(rd_i(mem, 22) == EXP_UNDEF_N && host_shm_cursor() == EXP_UNDEF_N,
        "push: undefined / 容器 → 0 字节（返回值 0，游标不动）");

    // ── buf：**活着的宿主 buffer id** → 整块裸字节（u32 长度 + 内容）──
    //   判定规则见 outer/host_push.cpp 的 ★编码格式★（风险：活着的 buffer id 一律当 buf）
    host_shm_reset();
    int bid = host_buf_create(3, 1);                       // 3 字节的宿主 buffer
    chk(bid >= 0 && host_buf_write(bid, "XYZ", 3), "push: 建一个装了 \"XYZ\" 的宿主 buffer");
    gset(mem, 40, mk_int(bid));
    push_by_name(mem, "VM_HostPush", { 40 }, 41);
    chk(rd_i(mem, 41) == (int)sizeof(EXP_BUF), "push: buf 实参 → 返回值 = 写进去的字节数");
    chk(bytes_eq(host_shm_base(), EXP_BUF, (int)sizeof(EXP_BUF), "buffer id → buf"),
        "push: 活着的 buffer id → u32 长度 + 裸字节");
    chk(host_buf_tell(bid) == 3, "push: 搬 buf 用 peek（buffer 自己的读写位置没被推走）");

    // 不是活着的 buffer 的 int → 照旧 4 字节 s32（普通整数不会被误当 buf）
    host_shm_reset();
    gset(mem, 42, mk_int(258));
    push_by_name(mem, "VM_HostPush", { 42 }, 43);
    chk(bytes_eq(host_shm_base(), EXP_INT, (int)sizeof(EXP_INT), "普通 int"),
        "push: 不是 buffer 的 int 照旧 4 字节 s32");

    // 删掉之后同一个 id → 不再是 buf，回到 4 字节 s32（活/死就靠 host_buf_alive 判）
    host_buf_delete(bid);
    host_shm_reset();
    gset(mem, 44, mk_int(bid));
    push_by_name(mem, "VM_HostPush", { 44 }, 45);
    {
        uint8_t exp[4] = { (uint8_t)(bid & 0xFF), (uint8_t)((bid >> 8) & 0xFF),
                           (uint8_t)((bid >> 16) & 0xFF), (uint8_t)((bid >> 24) & 0xFF) };
        chk(rd_i(mem, 45) == 4 && bytes_eq(host_shm_base(), exp, 4, "死掉的 buffer id"),
            "push: 死掉的 buffer id → 当普通 int（4 字节 s32）");
    }

    // ── 复位后再写：从头开始 ──
    host_shm_reset();
    chk(host_shm_cursor() == 0, "push: 复位后 cursor 归零");
    gset(mem, 30, mk_int(258));
    push_by_name(mem, "VM_HostPush", { 30 }, 31);
    chk(rd_i(mem, 31) == (int)sizeof(EXP_INT) &&
        bytes_eq(host_shm_base(), EXP_INT, (int)sizeof(EXP_INT), "复位后再推"),
        "push: 复位后再推 → 仍从第 0 字节开始，内容同 EXP_INT");
    delete[] mem;

    // ── host 层容量：不够翻倍 + 16MB 封顶（**不静默截断**）──
    //   放最后：这一步会把容量顶到 16MB，后面没别的 push 了
    host_shm_reset();
    std::vector<uint8_t> big(100 * 1024, 0x5A);
    chk(host_shm_write(big.data(), (int)big.size()), "push: 一次写 100KB 成功");
    chk(host_shm_size() == 128 * 1024, "push: 不够就翻倍（64KB → 128KB）");
    chk(host_shm_cursor() == 100 * 1024, "push: cursor = 100KB");

    std::vector<uint8_t> chunk(1024 * 1024, 0x11);
    while (host_shm_write(chunk.data(), (int)chunk.size())) { }   // 一路写到"1MB 那笔放不下"
    chk(host_shm_size() == 16 * 1024 * 1024, "push: 上限 = 16MB（翻倍的终点）");

    int room = host_shm_size() - host_shm_cursor();               // 剩下的零头（1MB 的对齐差）
    chk(room > 0, "push: 到顶前还有零头空间");
    std::vector<uint8_t> rest((size_t)room, 0x22);
    chk(host_shm_write(rest.data(), room), "push: 零头也写得进去（正好填到 16MB）");
    chk(host_shm_cursor() == 16 * 1024 * 1024, "push: 填满后 cursor == 16MB");

    int at_top = host_shm_cursor();
    uint8_t one = 0;
    chk(!host_shm_write(&one, 1), "push: 到顶后再写 1 字节 → false");
    chk(host_shm_cursor() == at_top, "push: 失败时游标一个字节都不动（不截断）");

    // ── 满了之后走 VM_HostPush：要报**新错误码** VM_E_HOST_FULL + 回 -1 ──
    //   （这条专门验"没复用 VM_E_NOT_IMPL"：那个码的意思是"函数还没实现"，不是"满了"）
    g_vm_err = VM_OK;
    Value* m2 = new Value[MEM_SIZE]();
    gset(m2, 1, mk_int(258));
    push_by_name(m2, "VM_HostPush", { 1 }, 2);
    chk(rd_i(m2, 2) == -1, "push: 满了 → VM_HostPush 回 -1");
    chk(g_vm_err == VM_E_HOST_FULL, "push: 满了 → VM_E_HOST_FULL（不是 VM_E_NOT_IMPL）");
    chk(host_shm_cursor() == at_top, "push: 满时**第一条**都放不下 → 一个字节都没进流里");

    // ── 半途失败的边界（钉住行为）：每支 push 自己原子，但**整次调用不是** ──
    //   给 4 字节空位：第一个 int 进得去、第二个进不去 → 回 -1，
    //   但第一条 payload 是**完整的**留在流里（绝不会留半条）
    host_shm_reset();
    std::vector<uint8_t> fill((size_t)(16 * 1024 * 1024 - 4), 0x33);
    chk(host_shm_write(fill.data(), (int)fill.size()), "push: 先占满到只剩 4 字节");
    g_vm_err = VM_OK;
    gset(m2, 10, mk_int(7));
    gset(m2, 11, mk_int(8));
    push_by_name(m2, "VM_HostPush", { 10, 11 }, 12);
    chk(rd_i(m2, 12) == -1 && g_vm_err == VM_E_HOST_FULL,
        "push: 第二条放不下 → -1 + VM_E_HOST_FULL（a1 = 已写进去的字节数）");
    chk(host_shm_cursor() == 16 * 1024 * 1024,
        "push: 第一条是**完整**留在流里的（半条 payload 永远不会出现）");
    delete[] m2;
    host_shm_reset();
}

// ============================================================
// 18 入站通道：host_packet_*（host 层）+ VM_ApplyPacket（VM 侧解包）
//
//   包格式 / 坏包判定见 outer/packet.cpp 的文件头（唯一真源）—— 这里只做一件事：
//   按那份格式拼字节，看 VM 那边是不是照着规则做了、坏包是不是**响亮**。
//   ⚠ 拼包就用 std::vector<uint8_t>（1 字节对齐），和引擎给的那块内存一个待遇：
//     谁要是在 VM_ApplyPacket 里偷懒 (Value*)buf 解引用，这里就会炸出来。
// ============================================================
namespace {

// 拼包：每个 Value 原样 8 字节拼进去（就是 mem 那套 {t, v}；x86/Windows 小端）
void pk_push(std::vector<uint8_t>& p, Value v) {
    const uint8_t* b = (const uint8_t*)&v;
    p.insert(p.end(), b, b + sizeof(Value));
}

// 文本 → 包里的字符串值（**引擎造包就是这么造的**：先 intern 出池下标）
Value pk_str(const char* utf8) { return mk_str_sid(VM_InternText(utf8)); }

// DLL 注册函数：把收到的参数记在测试这边（返回值槽在 VM 内部，外面看不见）
int    g_pkt_fn_calls = 0;
VM_INT g_pkt_fn_arg0  = -1;
void pkt_fn(Value* mem, const uint8_t* args, int argc, int dst) {
    g_pkt_fn_calls++;
    g_pkt_fn_arg0 = (argc > 0) ? arg_i(mem, args, 0) : -1;
    wr_i(mem, dst, 4321);
}

}  // namespace

static void test_host_packet() {
    uint8_t dummy[16] = { 0 };

    // ── host 层：没注册 handler → 负数、不崩（引擎问"这条路接上了吗"的确定回答）──
    host_packet_set_handler(NULL);                       // 先撤销（前面的测试已经注册过）
    chk(host_packet_apply(dummy, 8) < 0, "pkt: 没注册 handler → 负数（不崩）");
    host_packet_set_handler(VM_ApplyPacket);             // 注册（VM 启动时就是这么调的）
    host_packet_set_handler(VM_ApplyPacket);             // 再注册一次：幂等，可重复调
    chk(host_packet_apply(NULL, 8) < 0, "pkt: 注册后 NULL 包 → 负数（不崩）");

    // ── ④ 文本 → 池下标：引擎造包的唯一入口 ──
    int s1 = VM_InternText("__selftest_pkt_global");
    int s2 = VM_InternText("__selftest_pkt_global");
    chk(s1 >= 0 && s1 == s2, "pkt: VM_InternText 同一文本两次 → 同一个 sid");
    chk(VM_SidOf("__selftest_pkt_global") == s1, "pkt: VM_SidOf 查得到刚 intern 的文本");
    chk(VM_SidOf("__selftest_pkt_never_text__") == -1, "pkt: VM_SidOf 只查不建 → -1");
    chk(VM_InternText(NULL) == -1 && VM_SidOf(NULL) == -1, "pkt: 两个导出对 NULL 回 -1");

    outer_init();                                // 幂等；保证 handler 就是挂上的那个

    // ── ① 设全局：[名字][值]（2 个值；第一个值既是分支依据又是变量名）──
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("__selftest_pkt_global"));
        pk_push(p, mk_int(4242));
        chk(host_packet_apply(p.data(), (int)p.size()) == 0, "pkt: ① 设全局 → 0");
        Value* gv = global_var_sid(s1);
        chk(gv && gv->t == MEM_INT && gv->v == 4242, "pkt: ① 全局变量 == 包里的值（4242）");
    }

    // ── ② 调**内置函数**（native 表）：挑一条副作用可观测的、参数简单的 ——
    //   variable_global_set(名字, 值) 把值写进全局表，外面用 global_var_sid 就看得见
    {
        int gn = VM_InternText("__selftest_pkt_native_global");
        chk(str_find("variable_global_set") >= SID_NATIVE_BASE &&
            str_find("variable_global_set") <  SID_GAME_BASE,
            "pkt: variable_global_set 在 native 段（这个名字确实走 kNativeFns）");
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("variable_global_set"));
        pk_push(p, mk_int(2));
        pk_push(p, mk_str_sid(gn));
        pk_push(p, mk_int(777));
        chk(host_packet_apply(p.data(), (int)p.size()) == 0, "pkt: ② 调 native → 0");
        Value* gv = global_var_sid(gn);
        chk(gv && gv->t == MEM_INT && gv->v == 777,
            "pkt: ② native 真的跑了（variable_global_set → 全局 777）");
    }

    // ── ② 调 **VM 函数**（game 表）：VM_SetFlame(x) 就是写全局 S_flame ──
    {
        chk(str_find("VM_SetFlame") >= SID_GAME_BASE && str_find("VM_SetFlame") < SID_FIELD_BASE,
            "pkt: VM_SetFlame 在 game 段（这个名字确实走 kGameFns）");
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("VM_SetFlame"));
        pk_push(p, mk_int(1));
        pk_push(p, mk_int(55));
        chk(host_packet_apply(p.data(), (int)p.size()) == 0, "pkt: ② 调 game 函数 → 0");
        Value* gv = global_var_sid(S_flame);
        chk(gv && gv->v == 55, "pkt: ② game 函数真的跑了（VM_SetFlame → 全局 flame = 55）");
    }

    // ── ② 调 **DLL 注册函数**：注册一个、造包调它、看它收到的参数 ──
    chk(VM_RegisterFunc("__selftest_pkt_fn", pkt_fn) == 1, "pkt: 注册 __selftest_pkt_fn");
    {
        g_pkt_fn_calls = 0; g_pkt_fn_arg0 = -1;
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("__selftest_pkt_fn"));
        pk_push(p, mk_int(1));
        pk_push(p, mk_int(343));
        chk(host_packet_apply(p.data(), (int)p.size()) == 0, "pkt: ② 调 DLL 注册函数 → 0");
        chk(g_pkt_fn_calls == 1 && g_pkt_fn_arg0 == 343,
            "pkt: ② DLL 注册函数收到了参数（343，走名字 → 实现那条路）");
    }

    // ── ③ 坏包：一律负数 + VM_E_BAD_PACKET，而且**没有副作用** ──
    const int   fn_calls0 = g_pkt_fn_calls;
    const Value gv0       = *global_var_sid(s1);         // 存个副本：下面比"有没有被改过"

    //   (a) size 不是 8 的倍数 / size <= 0
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("__selftest_pkt_global"));
        p.push_back(0);                                 // 9 字节：多出来的半截
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (a1) size 非 8 倍数 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (a1) → VM_E_BAD_PACKET");
    }
    g_vm_err = VM_OK;
    chk(host_packet_apply(dummy, 0) < 0 && g_vm_err == VM_E_BAD_PACKET,
        "pkt: 坏包 (a2) size <= 0 → 负数 + VM_E_BAD_PACKET");

    //   (b) 第一个值不是字符串
    {
        std::vector<uint8_t> p;
        pk_push(p, mk_int(1));
        pk_push(p, mk_int(2));
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (b) 第一个值不是字符串 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (b) → VM_E_BAD_PACKET");
    }

    //   (c) 值个数对不上：① 少给一个值；② argc 说 2 个、实际只给 1 个
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("__selftest_pkt_global"));     // 只有名字、没有值
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (c1) ① 只有一个值 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (c1) → VM_E_BAD_PACKET");
    }
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("__selftest_pkt_fn"));
        pk_push(p, mk_int(2));                           // 说 2 个参数
        pk_push(p, mk_int(343));                         // 只给 1 个
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (c2) ② argc 与实际不符 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (c2) → VM_E_BAD_PACKET");
    }

    //   (d) 函数名查不到（四张表都没有）
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("__selftest_pkt_no_such_fn"));
        pk_push(p, mk_int(0));
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (d) 函数名查不到 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (d) → VM_E_BAD_PACKET");
    }

    //   (e) argc 超上限（CALL_MAX_ARGS）
    {
        std::vector<uint8_t> p;
        pk_push(p, pk_str("CALL"));
        pk_push(p, pk_str("__selftest_pkt_fn"));
        pk_push(p, mk_int(CALL_MAX_ARGS + 1));
        g_vm_err = VM_OK;
        chk(host_packet_apply(p.data(), (int)p.size()) < 0, "pkt: 坏包 (e) argc 超上限 → 负数");
        chk(g_vm_err == VM_E_BAD_PACKET, "pkt: 坏包 (e) → VM_E_BAD_PACKET");
    }

    //   **没有副作用**：上面这几包一个都没落到全局表、也没调到 DLL 函数
    {
        Value* gv = global_var_sid(s1);
        chk(gv && gv->t == gv0.t && gv->v == gv0.v, "pkt: 坏包不改全局变量（① 那个还是原值）");
        chk(g_pkt_fn_calls == fn_calls0, "pkt: 坏包没调到 DLL 函数（副作用为 0）");
        chk(!global_exists_sid(str_find("__selftest_pkt_no_such_fn")),
            "pkt: 坏包没顺手建出个全局变量（函数名当全局名那条路没走）");
    }

    // ── 清场：小写 call 不算 CALL（严格比较）→ 走 ①，名字就是 "call" ──
    {
        int lc = VM_InternText("call");
        std::vector<uint8_t> p;
        pk_push(p, pk_str("call"));
        pk_push(p, mk_int(9));
        chk(host_packet_apply(p.data(), (int)p.size()) == 0, "pkt: 小写 call 不算 CALL → 走 ①");
        Value* gv = global_var_sid(lc);
        chk(gv && gv->v == 9, "pkt: 小写 call 当普通全局名（严格比较，不搞大小写宽容）");
    }
}

// ============================================================
// 19 B1 状态区：引擎**写**那块共享内存 → 6 条输入 native **读**
//     keyboard_check_pressed / mouse_check_button{,_pressed,_released} /
//     device_mouse_{x,y}_to_gui            （实现见 outer/host_state_in.cpp）
//
//   这里扮演**引擎那一侧**：按契约（host_layout.h）的槽号覆盖着写，然后调 native 看回值。
//   ⚠ 为什么值得单测：这条路**不经过任何函数指针、也不会链接报错**，槽号 / 位序搞错了
//     只会安静地回一个看着挺正常的 0/1 —— 所以映射关系必须被钉住。
// ============================================================
namespace {

// 引擎那侧怎么写 B1：按契约槽号写 {类型, 值}（就是覆盖式每帧写的一个槽）
void st_put_int(int slot, int v) {
    HostStateSlot* p = host_state_base();
    p[slot].t = MEM_INT;
    p[slot].v = v;
}
void st_put_float(int slot, float f) {
    HostStateSlot* p = host_state_base();
    p[slot].t = MEM_FLOAT;
    p[slot].v = i_of(f);
}
// 位图：置/清第 vk 位（契约取法：槽 = W0 + (vk>>5)，位 = vk & 31）
void st_put_key_bit(int bitmap_w0, int vk, bool on) {
    int slot = bitmap_w0 + HOST_LAYOUT_STATE_VK_WORD(vk);
    HostStateSlot* p = host_state_base();
    int32_t word = (p[slot].t == MEM_INT) ? p[slot].v : 0;
    int32_t bit  = (int32_t)(1u << HOST_LAYOUT_STATE_VK_BIT(vk));
    p[slot].t = MEM_INT;
    p[slot].v = on ? (word | bit) : (word & ~bit);
}

}  // namespace

static void test_input_state() {
    Value* mem = new Value[MEM_SIZE]();

    // 契约外的两个"翻译表"，各自的真源：
    //   mb_* 编号  → transformGML2txt/gm_builtin_consts.json（mb_left=1 / mb_middle=2 / mb_right=3）
    //   掩码位序   → host_layout.h 的 MOUSE_BUTTONS 注释（bit0 左 / bit1 右 / bit2 中）
    //   ⚠ 两套编号**不一样**（middle 在 GML 是 2、在掩码里是 bit2），所以下面的映射要逐条钉
    const int MB_LEFT = 1, MB_MIDDLE = 2, MB_RIGHT = 3;
    const int BIT_LEFT = 0, BIT_RIGHT = 1, BIT_MIDDLE = 2;
    // vk 键码 → gm_builtin_consts.json（vk_left=37 / vk_up=38 / vk_right=39 / 'A'=65）
    const int VK_LEFT = 37, VK_UP = 38, VK_RIGHT = 39, VK_A = 65;

    const int W_DOWN     = HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W0;
    const int W_PRESSED  = HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W0;
    const int W_RELEASED = HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W0;
    const int M_DOWN     = (int)HOST_LAYOUT_STATE_MOUSE_BUTTONS;
    const int M_PRESSED  = (int)HOST_LAYOUT_STATE_MOUSE_PRESSED;
    const int M_RELEASED = (int)HOST_LAYOUT_STATE_MOUSE_RELEASED;

    // ── host 层给的口：槽数 == 契约，首地址随时可取 ──
    chk(host_state_slots() == HOST_LAYOUT_STATE_SLOTS && host_state_base() != NULL,
        "st: host_state_slots() == 契约槽数，且 host_state_base() 非空（引擎随时能取）");

    host_state_clear();

    // ── ① gui 鼠标坐标：写什么读什么，且是**浮点**回值（不是 0/1）──
    st_put_float((int)HOST_LAYOUT_STATE_MOUSE_GUI_X, 123.5f);
    st_put_float((int)HOST_LAYOUT_STATE_MOUSE_GUI_Y, 456.25f);
    nat(mem, S_device_mouse_x_to_gui, 1, {});
    nat(mem, S_device_mouse_y_to_gui, 2, {});
    chk(rd_t(mem, 1) == MEM_FLOAT && rd_f(mem, 1) == 123.5f,
        "st: device_mouse_x_to_gui == B1 里的 123.5（浮点回值）");
    chk(rd_t(mem, 2) == MEM_FLOAT && rd_f(mem, 2) == 456.25f,
        "st: device_mouse_y_to_gui == B1 里的 456.25（浮点回值）");

    // ── ② 按住掩码：只置 left+right（bit0|bit1）→ 三条 mb_* 各回各的 ──
    st_put_int(M_DOWN, (1 << BIT_LEFT) | (1 << BIT_RIGHT));
    st_put_int(M_PRESSED, 0);
    st_put_int(M_RELEASED, 0);
    nat(mem, S_mouse_check_button, 3, {});
    gset(mem, 10, mk_int(MB_LEFT));       nat(mem, S_mouse_check_button, 4, { 10 });
    gset(mem, 10, mk_int(MB_RIGHT));      nat(mem, S_mouse_check_button, 5, { 10 });
    gset(mem, 10, mk_int(MB_MIDDLE));     nat(mem, S_mouse_check_button, 6, { 10 });
    chk(rd_t(mem, 3) == MEM_INT && rd_i(mem, 3) == 0,
        "st: mouse_check_button(一个实参都没给) == 0（不是 undefined / 不是崩）");
    chk(rd_i(mem, 4) == 1, "st: 按住 left+right → mouse_check_button(mb_left) == 1");
    chk(rd_i(mem, 5) == 1, "st: mb_right(3) 映射到契约 bit1 → == 1（mb 值**不是**位号）");
    chk(rd_i(mem, 6) == 0, "st: mb_middle(2) 映射到契约 bit2 → 没置位 == 0");

    // ── ③ pressed / down 是两个**独立**字段：只置"刚按下中键" ──
    st_put_int(M_PRESSED, 1 << BIT_MIDDLE);
    gset(mem, 10, mk_int(MB_MIDDLE));
    nat(mem, S_mouse_check_button_pressed, 7, { 10 });
    nat(mem, S_mouse_check_button,         8, { 10 });
    gset(mem, 10, mk_int(MB_LEFT));
    nat(mem, S_mouse_check_button_pressed, 9, { 10 });
    chk(rd_i(mem, 7) == 1, "st: 刚按下 middle → mouse_check_button_pressed(mb_middle) == 1");
    chk(rd_i(mem, 8) == 0, "st: 但**没按住** middle → mouse_check_button(mb_middle) == 0（两字段独立）");
    chk(rd_i(mem, 9) == 0, "st: 刚按下的不是 left → mouse_check_button_pressed(mb_left) == 0");

    // ── ④ released 同理：只置"刚松开右键" ──
    st_put_int(M_RELEASED, 1 << BIT_RIGHT);
    gset(mem, 10, mk_int(MB_RIGHT));   nat(mem, S_mouse_check_button_released, 11, { 10 });
    gset(mem, 10, mk_int(MB_LEFT));    nat(mem, S_mouse_check_button_released, 12, { 10 });
    gset(mem, 10, mk_int(MB_MIDDLE));  nat(mem, S_mouse_check_button_released, 13, { 10 });
    chk(rd_i(mem, 11) == 1, "st: 刚松开 right → mouse_check_button_released(mb_right) == 1");
    chk(rd_i(mem, 12) == 0 && rd_i(mem, 13) == 0, "st: left / middle 没松开 → 都 == 0");

    // ── ⑤ 契约外的 btn（mb_none / mb_any / 乱传）→ 一律 0，别拿 mb 值当位号撞上别的键 ──
    st_put_int(M_DOWN, (1 << BIT_LEFT) | (1 << BIT_RIGHT) | (1 << BIT_MIDDLE));
    gset(mem, 10, mk_int(0));   nat(mem, S_mouse_check_button, 14, { 10 });
    gset(mem, 10, mk_int(4));   nat(mem, S_mouse_check_button, 15, { 10 });
    gset(mem, 10, mk_int(-1));  nat(mem, S_mouse_check_button, 16, { 10 });
    chk(rd_i(mem, 14) == 0 && rd_i(mem, 15) == 0 && rd_i(mem, 16) == 0,
        "st: btn = 0 / 4 / -1 → 0（契约里只有 left/right/middle 三个位）");

    // ── ⑥ 掩码槽类型不对（引擎没照契约写 float）→ 当"没按"，不拿 float 的位模式当按键 ──
    st_put_float(M_DOWN, 3.0f);
    gset(mem, 10, mk_int(MB_LEFT));
    nat(mem, S_mouse_check_button, 17, { 10 });
    chk(rd_i(mem, 17) == 0, "st: 掩码槽写成 float → 当 0（位图/掩码不认 float）");

    // ── ⑦ 键盘位图：位号 = vk 键码（vk_left=37 → 槽 W0+1、位 5）──
    for (int i = 0; i < HOST_LAYOUT_STATE_KEY_WORDS; i++) {
        st_put_int(W_DOWN + i, 0);
        st_put_int(W_PRESSED + i, 0);
        st_put_int(W_RELEASED + i, 0);
    }
    st_put_key_bit(W_PRESSED, VK_LEFT, true);
    gset(mem, 20, mk_int(VK_LEFT));  nat(mem, S_keyboard_check_pressed, 21, { 20 });
    gset(mem, 20, mk_int(VK_UP));    nat(mem, S_keyboard_check_pressed, 22, { 20 });
    gset(mem, 20, mk_int(VK_RIGHT)); nat(mem, S_keyboard_check_pressed, 23, { 20 });
    gset(mem, 20, mk_int(VK_A));     nat(mem, S_keyboard_check_pressed, 24, { 20 });
    chk(rd_t(mem, 21) == MEM_INT && rd_i(mem, 21) == 1,
        "st: vk_left 那位置 1 → keyboard_check_pressed(vk_left) == 1（0/1 数字，不是布尔文本）");
    chk(rd_i(mem, 22) == 0 && rd_i(mem, 23) == 0,
        "st: 同一个字里的别的位（vk_up / vk_right）→ 0");
    chk(rd_i(mem, 24) == 0, "st: 另一个字里的键（'A'=65 → 槽 W0+2）→ 0");

    // ── ⑧ "刚按下"只读 pressed 那张：把 down 置上同一个键，结果不能变 ──
    st_put_key_bit(W_DOWN, VK_A, true);
    gset(mem, 20, mk_int(VK_A));
    nat(mem, S_keyboard_check_pressed, 25, { 20 });
    chk(rd_i(mem, 25) == 0, "st: 只按住（down 位图）不算刚按下 → keyboard_check_pressed == 0");

    // ── ⑨ 边界：vk 越界（负数 / >255）→ 回 0，**不许越界读内存** ──
    //   契约的 HOST_LAYOUT_STATE_VK_WORD(vk) 故意不裁：拿 -1 直接算就是槽 -1（数组前一个），
    //   拿 999 就是槽 31+（越过状态区）。这两条只要有一次不裁，这里就会读出别的内存 / 崩。
    gset(mem, 20, mk_int(-1));       nat(mem, S_keyboard_check_pressed, 26, { 20 });
    gset(mem, 20, mk_int(-100000));  nat(mem, S_keyboard_check_pressed, 27, { 20 });
    gset(mem, 20, mk_int(256));      nat(mem, S_keyboard_check_pressed, 28, { 20 });
    gset(mem, 20, mk_int(1 << 20));  nat(mem, S_keyboard_check_pressed, 29, { 20 });
    chk(rd_i(mem, 26) == 0 && rd_i(mem, 27) == 0 && rd_i(mem, 28) == 0 && rd_i(mem, 29) == 0,
        "st: vk = -1 / -100000 / 256 / 1<<20 → 全 0（先裁边界再算槽号，不越界读）");

    // ── ⑩ host_state_clear()：全部清成 undefined → 读出来一律是"没有" ──
    host_state_clear();
    nat(mem, S_device_mouse_x_to_gui, 30, {});
    gset(mem, 10, mk_int(MB_LEFT));
    nat(mem, S_mouse_check_button, 31, { 10 });
    gset(mem, 20, mk_int(VK_LEFT));
    nat(mem, S_keyboard_check_pressed, 32, { 20 });
    chk(rd_f(mem, 30) == 0.0f && rd_i(mem, 31) == 0 && rd_i(mem, 32) == 0,
        "st: host_state_clear() → undefined 读出来当 0（不是残留的上一次输入）");

    // ── ⑪ keyboard_get_string：B1 契约里**没有**"本帧输入的文本"这个字段
    //    （槽 40-63 是预留，JSON 注释提了"输入法文本"但没定义）→ 这条**故意没写实现**。
    //    这里钉住"它确实还是空壳"，免得以后有人以为它已经能用了。
    g_vm_err = VM_OK;
    nat(mem, S_keyboard_get_string, 33, {});
    chk(g_vm_err == VM_E_NOT_IMPL,
        "st: keyboard_get_string 仍是空壳（契约里没有文本字段，不自己编一个槽号）");

    delete[] mem;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);          // 崩溃时也能看到跑到哪一步了
    fs::path kernel = (argc > 1) ? fs::path(argv[1]) : fs::path("../../kernel");

    printf("== inner self-test ==\n");
    test_pool();
    test_run();
    test_str_concat(kernel);
    test_inst();
    test_arr();
    test_callbin();
    test_handles();
    test_bin_load();
    test_outer();
    test_tables_smoke();
    test_self_hosted();
    test_game_self();
    test_create_event();
    test_forward_in_frame();
    test_host();
    test_host_natives();
    test_dual_table();
    test_host_push();
    test_host_packet();
    test_input_state();
    if (fs::exists(kernel)) test_load(kernel);
    else printf("[skip] 没找到 kernel 目录：%s\n", kernel.string().c_str());

    printf("== %s (%d failed) ==\n", g_fail ? "FAILED" : "ALL OK", g_fail);
    return g_fail ? 1 : 0;
}
