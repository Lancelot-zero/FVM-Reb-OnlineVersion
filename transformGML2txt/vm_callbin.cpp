// ============================================================
// vm_callbin.cpp — 外部函数（callbin）
//
//   一个 .callbin = 一个函数。装进字典（函数名 → CallBin），别的块用 VM_CallFunc 调它。
//
//   它有自己的 Vm（字符串池、块表都是它自己的），但**没有自己的 mem**：
//   每次被调用，mem 指向共享内存里属于自己的那一帧：
//
//       frame = g_call_mem + g_call_off * 8
//       g_call_off += slots        // 进函数
//       ... 跑函数体 ...
//       g_call_off -= slots        // 出函数（栈式回收）
//
//   所以**每次调用都有自己的内存空间**（递归也各有一帧），共享内存就一片固定的：
//   超出 → 打印报错、强制结束，顶层 VM_CallFunc 拿到 undefined。
//
//   帧布局（编译器保证）：槽 0 = 返回值，槽 1..argc = 实参，字面量/局部从 argc+1 起。
//
//   文件格式：头 + 跟普通 bin 一模一样的"字符串池 + 块"
//     [i32 magic 'CLBN'] [i32 槽位总数] [i32 参数个数]
//     [i32 name_len][name] [i32 desc_len][desc]
// ============================================================

#include "vm.h"
#include <cstring>

std::vector<char> g_call_mem;                       // 共享内存（首次调用时才分配）
size_t            g_call_off = 0;                   // 当前偏移（槽）
std::map<int, CallBin> g_call_dict;                 // 函数名的池下标 → callbin
bool              g_call_abort = false;             // 触上限 → 强制结束整条调用链

static Value undef_v() { Value u; u.t = MEM_UNDEFINED; u.v = 0; return u; }

// 一个函数最多收几个实参（超过就报错；调用方按 argc 校验，静默截断更糟）
static const int CALL_MAX_ARGS = 32;

// 嵌套层数上限：每层调用都是**真的在 C++ 栈上递归**（run → OP_CALL → impl_VM_CallFunc → run）。
//   共享内存能装 16384 层（8 槽/层），但 C++ 栈装不下那么多 —— 不闸住就先炸栈（0xC00000FD），
//   而"炸栈"既没报错也没有 undefined，不符合"超了要打印报错 + 顶层返回 undefined"。
static const int CALL_MAX_DEPTH = 512;
static int g_call_depth = 0;

void callbin_clear() {
    std::map<int, CallBin>::iterator it = g_call_dict.begin();
    for (; it != g_call_dict.end(); ++it) delete it->second.vm;
    g_call_dict.clear();
    g_call_off = 0;
}

static bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) { printf("[vm] cannot open %s\n", path.c_str()); return false; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize((size_t)(sz > 0 ? sz : 0));
    bool ok = (sz <= 0) || (fread(out.data(), 1, (size_t)sz, fp) == (size_t)sz);
    fclose(fp);
    if (!ok) printf("[vm] read failed %s\n", path.c_str());
    return ok;
}

bool callbin_load(const std::string& path) {
    std::vector<uint8_t> d;
    if (!read_file(path, d)) return false;
    if (d.size() < 16) { printf("[vm] %s 太短，不是 callbin\n", path.c_str()); return false; }

    size_t o = 0;
    int32_t magic = ld32(d.data() + o); o += 4;
    if (magic != CALLBIN_MAGIC) {
        printf("[vm] %s 不是 callbin（magic 不对：0x%08X）\n", path.c_str(), (unsigned)magic);
        return false;
    }
    int32_t slots = ld32(d.data() + o); o += 4;
    int32_t argc  = ld32(d.data() + o); o += 4;
    if (slots <= 0 || argc < 0 || argc > CALL_MAX_ARGS) {
        printf("[vm] %s 头不合法：slots=%d argc=%d\n", path.c_str(), slots, argc);
        return false;
    }
    std::string name, desc;
    for (int k = 0; k < 2; k++) {
        if (o + 4 > d.size()) { printf("[vm] %s 头截断\n", path.c_str()); return false; }
        int32_t n = ld32(d.data() + o); o += 4;
        if (n < 0 || o + (size_t)n > d.size()) { printf("[vm] %s 头截断\n", path.c_str()); return false; }
        (k == 0 ? name : desc).assign((const char*)d.data() + o, (size_t)n);
        o += (size_t)n;
    }
    if (name.empty()) { printf("[vm] %s 函数名为空\n", path.c_str()); return false; }

    std::vector<std::string> strings;
    std::vector<Block>       blocks;
    if (!parse_bin_body(d.data(), d.size(), o, strings, blocks) || blocks.empty()) {
        printf("[vm] %s 的块表读不出来\n", path.c_str());
        return false;
    }
    // 本 callbin 的池并进全局池 + 字节码里的字符串常量换成全局下标
    if (!remap_block_strings(blocks, strings)) return false;

    // 字典的 key = 函数名在全局池里的下标（池全局化了，同一个名字永远是同一下标）
    int sid = intern_str(name);

    // 同名重载：把旧的换掉（热重载复用这个名字）
    std::map<int, CallBin>::iterator old = g_call_dict.find(sid);
    if (old != g_call_dict.end()) { delete old->second.vm; g_call_dict.erase(old); }

    Vm* f = new Vm(true);                     // true = 不分配 mem，mem 每次调用时借共享内存
    // 注意：**不登记进 g_vm_of_object** —— 它是函数，不是对象；
    //   登记了会让 vm_of_object("函数名") 命中，把函数当成对象。
    f->obj = intern_str(name);    // 字段是池下标
    for (size_t i = 0; i < blocks.size(); i++)
        f->blocks[str_at(blocks[i].name_sid)] = blocks[i];   // key = 块名的池下标

    CallBin cb;
    cb.name_sid = intern_str(name);
    cb.desc  = desc;
    cb.argc  = argc;
    cb.slots = slots;
    cb.vm    = f;
    g_call_dict[sid] = cb;
    return true;
}

// ── 真正干活的：调一个 callbin（sid = 函数名的池下标）──
bool callbin_invoke(Vm& caller, int sid, const Value* argv, int argn, Value* ret) {
    std::map<int, CallBin>::iterator it = g_call_dict.find(sid);
    if (it == g_call_dict.end()) {
        printf("[vm] VM_CallFunc: 字典里没有 \"%s\"\n", str_at(sid).c_str());
        return false;
    }
    CallBin& cb = it->second;
    if (argn != cb.argc) {
        printf("[vm] VM_CallFunc: \"%s\" 要 %d 个参数，给了 %d 个\n", str_at(cb.name_sid).c_str(), cb.argc, argn);
        return false;
    }
    if (g_call_mem.empty()) g_call_mem.assign(CALL_MEM_SLOTS * 8, 0);

    // 嵌套太深：C++ 栈会先炸，所以先闸住（跟"共享内存不够"一样：报错 + 强制结束整条链）
    if (g_call_depth >= CALL_MAX_DEPTH) {
        printf("[vm] VM_CallFunc: 调用层数太深（%d 层）—— 强制结束，顶层返回 undefined\n",
               g_call_depth);
        g_call_abort = true;
        return false;
    }

    // 共享内存不够：打印报错 + 强制结束整条链，顶层拿到 undefined
    if (g_call_off + (size_t)cb.slots > CALL_MEM_SLOTS) {
        printf("[vm] VM_CallFunc: 共享内存不够了 —— \"%s\" 要 %d 槽，已用 %d/%d，强制结束\n",
               str_at(cb.name_sid).c_str(), cb.slots, (int)g_call_off, (int)CALL_MEM_SLOTS);
        g_call_abort = true;
        return false;
    }

    Vm* f = cb.vm;
    char* frame = g_call_mem.data() + g_call_off * 8;
    char* prev_frame = f->mem;                   // 递归时外层那一帧，跑完要还回去
    f->mem = frame;                              // 借这一帧（owns_mem 一直是 false）
    memset(frame, 0, (size_t)cb.slots * 8);      // 每帧从 0 开始，不继承上一次的残留
    g_call_off += (size_t)cb.slots;              // 进函数：偏移往前走
    g_call_depth++;

    f->run_named_sid(S__VM_CONST_INIT);              // 字面量填进这一帧（每帧都得填，帧是复用的）
    for (int i = 0; i < argn; i++) wr_v(*f, 1 + i, argv[i]);   // 槽 1..argc = 实参
    f->cur_instance = caller.cur_instance;       // 函数体里的"当前实例"就是调用方那个
    f->run_named_sid(CALLBIN_ENTRY);                 // 跑函数体

    bool aborted = g_call_abort;                 // 触上限了：这一层也算失败，别算返回值
    Value r = aborted ? undef_v() : rd_v(*f, 0); // 槽 0 = 返回值
    if (!aborted) retain(r);                     // 返回值**转交**给调用方的那一份引用
                                                 //   （先把引用拿到手，再放帧 —— 反了的话句柄可能已被回收）
    // 整帧的容器引用全放掉：实参、函数体里建的 array/map/grid/list、以及槽 0 那一份。
    //   不这么做：调用方每次调用都会漏掉这一帧里所有容器的引用（池里的身子永远不回收）。
    //   非容器（数字/字符串/undefined）release 是空操作，所以整段扫过去就行。
    for (int s = 0; s < cb.slots; s++) release(rd_v(*f, s));

    g_call_depth--;
    g_call_off -= (size_t)cb.slots;              // 出函数：偏移退回来
    f->mem = prev_frame;                         // 还回外层那一帧（置 NULL 会把递归的上层弄坏）
    if (g_call_depth == 0 && aborted) g_call_abort = false;   // 最外层收尾，把旗子放掉
    if (aborted) return false;
    if (ret) *ret = r;                           // 这一份引用归调用方，调用方写完 dst 要 release
    return true;
}

// ── 游戏 API：VM_CallFunc(函数名, 实参…) ──
void impl_VM_CallFunc(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 1) {
        printf("[vm] VM_CallFunc: 至少要给一个函数名\n");
        if (dst >= 0) wr_v(vm, dst, undef_v());
        return;
    }
    int s0 = argslot(args, 0);
    if (rd_t(vm, s0) != MEM_STRING) {
        printf("[vm] VM_CallFunc: 第一个参数必须是函数名（字符串）\n");
        if (dst >= 0) wr_v(vm, dst, undef_v());
        return;
    }
    int si = rd_i(vm, s0);
    if (!str_ok(si)) {
        printf("[vm] VM_CallFunc: 函数名下标越界 %d\n", si);
        if (dst >= 0) wr_v(vm, dst, undef_v());
        return;
    }
    // si 就是函数名的池下标，直接当字典 key —— 不用取文本、不用构造 std::string
    int n = argc - 1;
    if (n > CALL_MAX_ARGS) {
        printf("[vm] VM_CallFunc: \"%s\" 实参太多（%d > %d）\n", str_at(si).c_str(), n, CALL_MAX_ARGS);
        if (dst >= 0) wr_v(vm, dst, undef_v());
        return;
    }
    Value av[CALL_MAX_ARGS];
    for (int i = 0; i < n; i++) av[i] = rd_v(vm, argslot(args, i + 1));

    Value ret = undef_v();
    if (!callbin_invoke(vm, si, av, n, &ret)) ret = undef_v();
    if (dst >= 0) wr_v(vm, dst, ret);   // 写进调用方的槽（wr_v 会自己 retain 一份）
    release(ret);                       // 放掉 callbin_invoke 转交过来的那一份（非容器空操作）
}

// ── 游戏 API：VM_FuncExists(函数名) → 1/0 ──
void impl_VM_FuncExists(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(vm, dst, 0); return; }
    int s0 = argslot(args, 0);
    if (rd_t(vm, s0) != MEM_STRING) { wr_i(vm, dst, 0); return; }
    int si = rd_i(vm, s0);
    if (!str_ok(si)) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, g_call_dict.count(si) ? 1 : 0);
}

// ── 游戏 API：VM_FuncDesc(函数名) → 说明字符串（没有 → ""） ──
void impl_VM_FuncDesc(Vm& vm, const uint8_t* args, int argc, int dst) {
    std::string desc;
    if (argc >= 1) {
        int s0 = argslot(args, 0);
        if (rd_t(vm, s0) == MEM_STRING) {
            int si = rd_i(vm, s0);
            if (str_ok(si)) {
                std::map<int, CallBin>::iterator it = g_call_dict.find(si);
                if (it != g_call_dict.end()) desc = it->second.desc;
            }
        }
    }
    Value v; v.t = MEM_STRING; v.v = intern_str(desc);
    wr_v(vm, dst, v);
}
