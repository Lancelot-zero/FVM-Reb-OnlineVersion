// ============================================================
// vm.cpp — 内层：解释循环 + 装载
//
//   VM_RUN        跑一个块（bin 的块 / callbin 的块都是它）
//   VM_BIN_Load   读 .bin 的字节流 → vm_bin
//   VM_CALLBIN_LOAD 读 .callbin 的字节流 → vm_callbin（顺带登记进函数字典）
//
//   bin 的块      → 没有返回值（跑完就行）
//   callbin 的块  → 返回函数返回值（槽 0）
//   OP_HALT 时返回**槽 0**里的那个 Value
//
//   内层零文本：出错只记错误码（g_vm_err），文本在外层的日志里格式化
// ============================================================
#include "vm_def.h"
#include <array>

// ============================================================
// 错误码（只有码和参数）
// ============================================================
int g_vm_err = VM_OK;
int g_vm_err_a1 = 0;
int g_vm_err_a2 = 0;

void vm_set_err(int code, int a1, int a2) {
    if (g_vm_err != VM_OK) return;      // 只留**第一个**错误（后面都是连锁反应）
    g_vm_err = code;
    g_vm_err_a1 = a1;
    g_vm_err_a2 = a2;
}

// ============================================================
// 二元运算（17 个：算术 / 比较 / 位运算，布局都是 dst a b）
//   · 任何一个操作数是浮点 → 按浮点算（GML 的 real）
//   · mod 只接整数（和 GML 一致）：碰浮点就当错误，停掉这个块
//   · 比较也是浮点比（保留老实现的精度行为）
//   · 位运算一律在**值**段上按整数做，结果一律 INT
//   · OP_ADD 特殊：**任一侧是字符串**就真拼接（见下面的 str_concat），不走数字分支
// ============================================================
static inline VM_INT   vint (Value* mem, int s) { return mem[s].v; }
static inline bool     vflt (Value* mem, int s) { return mem[s].t == MEM_FLOAT; }
static inline VM_FLOAT vnum (Value* mem, int s) { return vflt(mem, s) ? f_of(mem[s].v) : (VM_FLOAT)mem[s].v; }

// ============================================================
// 字符串拼接：OP_ADD 的字符串路径
//   语义按 GM：**任一侧**是字符串就按文本拼（不是"两边都是字符串"才拼）
//     string + string          → 直接接起来
//     string + int / float     → 那一侧走 value_to_text（和 string() 同一份）
//     int    + string          → 同上，顺序按 a、b 原样（"7" + "x" ≠ "x" + "7"）
//     string + undefined/容器  → 也走 value_to_text（= 平台的 string() 口径：
//                                "undefined" / "[array]" / "[struct]" / "[grid]" / "[list]"）
//   结果 intern_str 回池 → 同文本自然去重、共享同一个 sid
//   ⚠ 池字符串不可变、不回收（没有引用计数）→ 结果不用 retain/release，直接 wr_v
//   ⚠ 上限 STR_CONCAT_MAX：池只增不减，一次拼接就是一次长驻分配；超了说明脚本
//     在拿拼接当缓冲区使（比如循环累加），响亮报错比把池撑爆强。1 MB 远超游戏里
//     任何一条正常文本（老实现的池下标在文件格式里还是 u16）
// ============================================================
static const size_t STR_CONCAT_MAX = 1024 * 1024;      // 1 MB

static bool str_concat(Value* mem, int d, int a, int b) {
    // 先把两边的文本**复制**出来：下面 intern_str 会让 g_strings 扩容，
    //   抱着 str_at 的引用接着往后拼就是悬空引用（builtin.cpp arg_str 那条注释同一个坑）
    std::string out, rhs;
    if (mem[a].t == MEM_STRING) out = str_at(mem[a].v); else out = value_to_text(mem[a]);
    if (mem[b].t == MEM_STRING) rhs = str_at(mem[b].v); else rhs = value_to_text(mem[b]);

    size_t n = out.size() + rhs.size();
    if (n > STR_CONCAT_MAX) {                      // 先比长度再拼：拼完再判就已经把那几 MB 分配出来了
        vm_set_err(VM_E_STR_TOO_LONG, (int)n, (int)STR_CONCAT_MAX);
        return false;                              // 调用方停掉这个块（和 mod 碰浮点一个规矩）
    }
    out += rhs;
    wr_v(mem, d, mk_str_sid(intern_str(out)));
    return true;
}

// 二元运算。返回 false = 出错（错误码已置），调用方要**停掉这个块**
static bool bin_op(int op, Value* mem, int d, int a, int b) {
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        // ⚠ 字符串检查必须在 vflt() **之前**：`1.5 + "a"` 里字符串在右边，
        //   先判浮点就会把池下标当数字加 —— 那正是原来那个**静默**错
        if (op == OP_ADD && (mem[a].t == MEM_STRING || mem[b].t == MEM_STRING))
            return str_concat(mem, d, a, b);
        if (vflt(mem, a) || vflt(mem, b)) {
            VM_FLOAT x = vnum(mem, a), y = vnum(mem, b), r;
            if (op == OP_ADD)      r = x + y;
            else if (op == OP_SUB) r = x - y;
            else if (op == OP_MUL) r = x * y;
            else                   r = (y == (VM_FLOAT)0) ? (VM_FLOAT)0 : (x / y);
            wr_f(mem, d, r);
        } else {
            VM_INT x = vint(mem, a), y = vint(mem, b), r;
            if (op == OP_ADD)      r = x + y;
            else if (op == OP_SUB) r = x - y;
            else if (op == OP_MUL) r = x * y;
            else                   r = (y != 0) ? (x / y) : 0;   // 整型除法：向零截断
            wr_i(mem, d, r);
        }
        return true;

    case OP_MOD:
        if (vflt(mem, a) || vflt(mem, b)) return true;          // 调用方已经报错，不会再走到这
        { VM_INT y = vint(mem, b); wr_i(mem, d, (y != 0) ? (vint(mem, a) % y) : 0); }
        return true;

    case OP_EQ: case OP_NEQ: case OP_GT: case OP_GTE: case OP_LT: case OP_LTE: {
        VM_FLOAT x = vnum(mem, a), y = vnum(mem, b);
        bool r;
        if (op == OP_EQ)       r = (x == y);
        else if (op == OP_NEQ) r = (x != y);
        else if (op == OP_GT)  r = (x >  y);
        else if (op == OP_GTE) r = (x >= y);
        else if (op == OP_LT)  r = (x <  y);
        else                   r = (x <= y);
        wr_i(mem, d, r ? 1 : 0);
        return true;
    }

    case OP_BAND: wr_i(mem, d, vint(mem, a) &  vint(mem, b)); return true;
    case OP_BOR:  wr_i(mem, d, vint(mem, a) |  vint(mem, b)); return true;
    case OP_BXOR: wr_i(mem, d, vint(mem, a) ^  vint(mem, b)); return true;
    // 移位量先按 32 位取模：槽是 int32，移 32 位以上在 C++ 里是未定义行为
    case OP_SHL:  wr_i(mem, d, vint(mem, a) << (vint(mem, b) & 31)); return true;
    case OP_SHR:  wr_i(mem, d, vint(mem, a) >> (vint(mem, b) & 31)); return true;
    }
    return true;
}

// ============================================================
// 执行一个块
//   "当前是哪个实例"从调用栈顶拿（run_inst()）—— 不再挂在 VM 上
//   ⚠ 压栈 / 弹栈是调用方的事（vm_run_event / 业务层派发 / callbin 调用）
// ============================================================
Value VM_RUN(Block& block, Value* mem) {
    const uint8_t* base = block.code.data();
    const uint8_t* p    = base;
    const uint8_t* end  = base + block.code.size();

    while (p < end) {
        if (g_call_abort) return mk_undef();          // callbin 触上限：立刻收手，一层层退回去
        int op = *p++;
        switch (op) {

        case OP_ASSIGN: {
            int     dst = ld32(p); p += 4;
            uint8_t ty  = *p++;
            Value v; v.t = ty; v.v = 0;
            if (ty == MEM_STRING)      { v.v = (VM_INT)ld16(p); p += 2; }
            else if (ty == MEM_FLOAT)  { v.v = i_of((VM_FLOAT)ldf(p)); p += 4; }   // 文件里是 f32 → 放宽到 VM_FLOAT
            else                       { v.v = (VM_INT)ld32(p); p += 4; }
            wr_v(mem, dst, v);                        // 走 wr_v 才会把 dst 里原来那个容器引用放掉
            break;
        }

        case OP_COPY: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            wr_v(mem, dst, mem[src]);                 // 整 8 字节搬家 + 引用计数
            break;
        }

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_EQ:  case OP_NEQ: case OP_GT:  case OP_GTE: case OP_LT: case OP_LTE:
        case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            if (op == OP_MOD && (vflt(mem, a) || vflt(mem, b))) {
                vm_set_err(VM_E_NONINT_OPERAND, block.name_sid, 0);   // mod 只接整数
                return mk_undef();
            }
            if (!bin_op(op, mem, d, a, b)) return mk_undef();         // 拼接超长：错误码已置，块停在这
            break;
        }

        case OP_BNOT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            wr_i(mem, d, ~vint(mem, a));
            break;
        }

        case OP_CALL: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < kGameFnCount && kGameFns[fid]) kGameFns[fid](mem, args, argc, dst);
            else vm_set_err(VM_E_UNKNOWN_GAME_FN, fid, 0);
            break;
        }

        case OP_NATIVE: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < kNativeFnCount && kNativeFns[fid]) kNativeFns[fid](mem, args, argc, dst);
            else vm_set_err(VM_E_UNKNOWN_NATIVE, fid, 0);
            break;
        }

        case OP_IF: {
            int cond = ld32(p); p += 4;
            int tip  = ld32(p); p += 4;
            int fip  = ld32(p); p += 4;
            VM_INT ty = mem[cond].t;
            bool t;
            // GML 的真值：<= 0.5 为假、> 0.5 为真（手册 Boolean 一节）
            if (ty == MEM_INT)        t = (mem[cond].v > 0);
            else if (ty == MEM_FLOAT) t = (f_of(mem[cond].v) > (VM_FLOAT)0.5);
            else                      t = false;      // 非数字：GML 的 is_real 为假 → 当 0
            int tgt = t ? tip : fip;
            if (tgt != -1) p = base + tgt;            // -1 = 不跳转，往下走
            break;
        }

        case OP_JMP:
            p = base + ld32(p);
            break;

        case OP_HALT:
            return mem[0];                            // 返回值就在槽 0

        // 取地址处的值：src 槽里存的是**内存里的字节偏移**，把那个地址的一个槽读到 dst
        case OP_LOAD_ELEM: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            VM_INT a = mem[src].v;
            if (a >= 0 && a + (VM_INT)sizeof(Value) <= (VM_INT)(MEM_SIZE * sizeof(Value)))
                memcpy(&mem[dst], (const char*)mem + a, sizeof(Value));
            break;
        }

        default:
            vm_set_err(VM_E_UNKNOWN_OPCODE, op, block.name_sid);
            return mk_undef();
        }
    }
    return mk_undef();
}

// ============================================================
// 跑一个事件块（把"当前实例"压栈 → VM_RUN → 弹栈）
//   ⚠ 只跑字节码块 —— "先查有没有 C++ 热块"是外层的派发（run_block_on）
// ============================================================
void vm_run_event(vm_bin& vm, VM_INT event_sid, VM_INT inst_id) {
    std::unordered_map<VM_INT, Block>::iterator it = vm.blocks.find(event_sid);
    if (it == vm.blocks.end()) return;                 // 这个对象没有这个事件
    int sp0 = g_run_sp;
    run_push(vm, inst_id);
    VM_RUN(it->second, vm.mem);
    while (g_run_sp > sp0) run_pop();                  // 里面异常路径多压了也一起收干净
}

// ============================================================
// 操作码 → 操作数字节数（-1 = 变长 / 未知）
//   一次初始化，之后纯数组下标，无分支
// ============================================================
static const std::array<signed char, 256> kOpLen = [] {
    std::array<signed char, 256> a;  a.fill(-1);
    a[OP_COPY] = 8;
    a[OP_ADD] = a[OP_SUB] = a[OP_MUL] = a[OP_DIV] = a[OP_MOD] = 12;
    a[OP_EQ] = a[OP_NEQ] = a[OP_GT] = a[OP_GTE] = a[OP_LT] = a[OP_LTE] = 12;
    a[OP_IF] = 12;  a[OP_JMP] = 4;  a[OP_HALT] = 0;
    a[OP_LOAD_ELEM] = a[OP_BNOT] = 8;
    for (int i = OP_BAND; i <= OP_SHR; i++) a[(size_t)i] = 12;
    return a;
}();

// ============================================================
// 装载期：把 bin 自带的字符串池并进全局池，并把字节码里内联的字符串常量
//   改写成全局下标。
//   只有 OP_ASSIGN 会内联字符串下标（[dst i32][ty u8][idx u16]），别的指令只带槽号。
//   走一遍指令流：没正好走到块尾就说明指令布局和这份表对不上 → 装载失败。
//   宁可装载失败，也不能让错位的下标进池 —— 那是静默改语义。
// ============================================================
static bool remap_block_strings(std::vector<Block>& blocks,
                                const std::vector<std::string>& local_pool) {
    std::vector<int> remap(local_pool.size());
    for (size_t i = 0; i < local_pool.size(); i++) {
        remap[i] = intern_str(local_pool[i]);
        if (remap[i] > 0xFFFF) {                       // OP_ASSIGN 的 u16 装不下
            vm_set_err(VM_E_BIN_TOO_MANY_STRS, 0xFFFF, remap[i]);
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
                if (p + 5 > n) { vm_set_err(VM_E_BIN_BLOCK, (int)bi, (int)p); return false; }
                p += 4;                                // dst
                uint8_t ty = c[p++];
                if (ty == MEM_STRING) {
                    if (p + 2 > n) { vm_set_err(VM_E_BIN_BLOCK, (int)bi, (int)p); return false; }
                    int idx = (int)ld16(&c[p]);
                    if (idx < 0 || idx >= (int)remap.size()) {
                        vm_set_err(VM_E_BIN_BLOCK, (int)bi, idx);
                        return false;
                    }
                    uint16_t g = (uint16_t)remap[(size_t)idx];
                    c[p]     = (uint8_t)(g & 0xFF);
                    c[p + 1] = (uint8_t)(g >> 8);
                    p += 2;
                } else {
                    p += 4;                            // 浮点 / 整数
                }
            } else if (op == OP_CALL || op == OP_NATIVE) {                if (p + 3 > n) { vm_set_err(VM_E_BIN_BLOCK, (int)bi, (int)p); return false; }
                p += 2;                                // fid u16
                int argc = (int)c[p++];
                if (p + 4 + (size_t)argc * 4 > n) { vm_set_err(VM_E_BIN_BLOCK, (int)bi, (int)p); return false; }
                p += 4 + (size_t)argc * 4;             // dst + args
            } else {
                int k = kOpLen[op];
                if (k < 0 || p + (size_t)k > n) { vm_set_err(VM_E_UNKNOWN_OPCODE, (int)op, (int)bi); return false; }
                p += (size_t)k;
            }
        }
    }
    return true;
}

// ============================================================
// .bin / .callbin 共用的那半截：从 d[o] 开始读"字符串池 + 块表"
//   [i32 字符串个数] 重复: [i32 字节长度][UTF-8]
//   然后重复: [i32 字节码长度][i32 块名在**本文件字符串池**里的下标][字节码]
//   块名在这里就 intern 成全局下标（装载期是唯一允许"文本 → 池下标"的地方）
// ============================================================
static bool parse_bin_body(const uint8_t* d, size_t size, size_t o,
                           std::vector<std::string>& strings, std::vector<Block>& blocks) {
    if (o + 4 > size) { vm_set_err(VM_E_BIN_TRUNC, (int)(o + 4), (int)size); return false; }
    int32_t sc = ld32(d + o); o += 4;
    if (sc < 0) { vm_set_err(VM_E_BIN_HEAD, (int)o, sc); return false; }
    for (int i = 0; i < sc; i++) {
        if (o + 4 > size) { vm_set_err(VM_E_BIN_TRUNC, (int)(o + 4), (int)size); return false; }
        int32_t len = ld32(d + o); o += 4;
        if (len < 0 || o + (size_t)len > size) { vm_set_err(VM_E_BIN_TRUNC, (int)(o + len), (int)size); return false; }
        strings.push_back(std::string((const char*)d + o, (size_t)len));
        o += (size_t)len;
    }
    while (o + 8 <= size) {
        int32_t blen = ld32(d + o); o += 4;
        int32_t nidx = ld32(d + o); o += 4;
        if (blen < 0 || o + (size_t)blen > size) {
            vm_set_err(VM_E_BIN_TRUNC, (int)(o + blen), (int)size);
            return false;
        }
        Block b;
        b.name_sid = intern_str((nidx >= 0 && nidx < (int)strings.size())
                                    ? strings[(size_t)nidx] : std::string());
        b.code.assign(d + o, d + o + (size_t)blen);
        o += (size_t)blen;
        blocks.push_back(b);
    }
    return true;
}

// ============================================================
// 装载 .bin（字节流）
//   文件头就是字符串池，没有 magic、没有对象名 ——
//   对象名由业务层在别处登记（一个对象一个 VM）
//
//   _VM_CONST_INIT 不是运行逻辑，是**字面量表**（所有 bin / callbin 都有）：
//   它只有 OP_ASSIGN，把这个 bin 用到的字符串 / 浮点 / 整数填进字面量槽。
//   bin 的 mem 是长驻的 → 装载期跑一次就填好了，块本身**不留**（它不是一个事件）
// ============================================================
Value VM_BIN_Load(const uint8_t* data, size_t size, vm_bin& out) {
    g_vm_err = VM_OK;
    if (!data || size < 4) { vm_set_err(VM_E_BIN_TRUNC, 4, (int)size); return mk_undef(); }

    std::vector<std::string> strings;
    std::vector<Block>       blocks;
    if (!parse_bin_body(data, size, 0, strings, blocks)) return mk_undef();
    if (!remap_block_strings(blocks, strings)) return mk_undef();
    if (blocks.empty()) { vm_set_err(VM_E_BIN_HEAD, 0, 0); return mk_undef(); }

    // 复位（别留下上一个 bin 的东西）；游标要指回**自己的** mp.end()
    out.blocks.clear();
    out.inst_count = 0;
    out.arr.clear();
    out.mp.clear();
    out.cur = out.mp.end();
    out.cur_rank = 0;
    memset(out.mem, 0, sizeof(out.mem));

    for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks[i].name_sid == S__VM_CONST_INIT) {
            VM_RUN(blocks[i], out.mem);              // 字面量表：读入即执行（一次）
            continue;                                // 不留进 blocks
        }
        out.blocks[blocks[i].name_sid] = blocks[i];
    }
    return mk_int(1);
}

// ============================================================
// 装载 .callbin
//   头：[i32 magic 'CLBN'] [i32 槽位总数] [i32 参数个数] [i32 name_len][name] [i32 desc_len][desc]
//   后面和普通 bin 一模一样（字符串池 + 块表），函数体块名 = _CALL_ENTRY
//   → 登记进函数字典：函数名池下标 → vm_callbin（函数名 → 下标只在装载期做）
// ============================================================
Value VM_CALLBIN_LOAD(const uint8_t* data, size_t size, vm_callbin& out) {
    g_vm_err = VM_OK;
    static const int32_t CALLBIN_MAGIC = 0x4E424C43;      // 'C''L''B''N'（小端读）
    if (!data || size < 16) { vm_set_err(VM_E_BIN_TRUNC, 16, (int)size); return mk_undef(); }

    size_t o = 0;
    int32_t magic = ld32(data + o); o += 4;
    if (magic != CALLBIN_MAGIC) { vm_set_err(VM_E_BIN_HEAD, 16, magic); return mk_undef(); }
    int32_t slots = ld32(data + o); o += 4;
    int32_t argc  = ld32(data + o); o += 4;
    (void)argc;                                           // 参数个数是编译器的事，内层不看
    if (slots <= 0) { vm_set_err(VM_E_BIN_HEAD, (int)o, slots); return mk_undef(); }

    std::string name;
    for (int k = 0; k < 2; k++) {                         // name / desc（desc 不要，跳过）
        if (o + 4 > size) { vm_set_err(VM_E_BIN_TRUNC, (int)(o + 4), (int)size); return mk_undef(); }
        int32_t n = ld32(data + o); o += 4;
        if (n < 0 || o + (size_t)n > size) { vm_set_err(VM_E_BIN_TRUNC, (int)(o + n), (int)size); return mk_undef(); }
        if (k == 0) name.assign((const char*)data + o, (size_t)n);
        o += (size_t)n;
    }
    if (name.empty()) { vm_set_err(VM_E_BIN_HEAD, (int)o, 0); return mk_undef(); }

    std::vector<std::string> strings;
    std::vector<Block>       blocks;
    if (!parse_bin_body(data, size, o, strings, blocks) || blocks.empty()) return mk_undef();
    if (!remap_block_strings(blocks, strings)) return mk_undef();

    // 现有 callbin 都是两块：_VM_CONST_INIT（填字面量）+ _CALL_ENTRY（函数体）
    out.init  = Block();
    out.block = Block();
    for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks[i].name_sid == S__VM_CONST_INIT)      out.init  = blocks[i];
        else if (blocks[i].name_sid == CALLBIN_ENTRY)    out.block = blocks[i];
        else if (out.block.code.empty())                 out.block = blocks[i];   // 兜底：没名字就用第一个
    }
    if (out.block.code.empty()) { vm_set_err(VM_E_BIN_BLOCK, (int)o, 0); return mk_undef(); }

    out.value = slots;
    g_call_dict[intern_str(name)] = out;                  // 函数名 → callbin
    return mk_int(1);
}
