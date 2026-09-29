// ============================================================
// vm_def.h — 内层：类型 / 结构体 / 全部内层声明
//
//   内层不许有：printf、文件 IO、argv、自检、字符串字面量
//   （文本只在 const_str.h 那一份清单里，运行时一律池下标）
//
//   文件划分：
//     const_str.h/.cpp  字符常量（唯一真源）+ 灌池
//     pool.cpp          全局池：字符串 / 全局变量 / 实例表 / 对象→VM / callbin / 容器
//     instance.cpp      实例：建 / 删 / 存活 / 第 k 个 / 变量读写
//     vm.cpp            VM_RUN（解释循环 + 调用栈）+ VM_BIN_Load + VM_CALLBIN_LOAD
// ============================================================
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "const_str.h"          // 名字清单（顺序 = 池下标 = 函数 / 字段编号）

// ============================================================
// 槽里用的整数 / 浮点 —— **不许直接写 int / float / double**
//   现在跑 32 位；以后要上 64 位，只改下面这几行（外加 bin 里字面量的宽度）
//     VM_INT   int32 → int64
//     VM_UINT  uint32 → uint64
//     VM_FLOAT float → double
//   ⚠ VM_INT 和 VM_FLOAT **必须同宽**：浮点值就是塞在槽的 value 段里的（f_of / i_of），
//     不同宽 memcpy 就错位了
//   ⚠ 字节码（bin 文件）里的宽度是**编译器**定的，跟这里无关：
//     里面的 s32 / u16 / u8 走 ld32 / ld16 那套，**不要**拿 VM_INT 去替代它们，
//     不然文件格式就跟着变了、老 bin 全部读不了
//   ⚠ 槽号（slot）/ 偏移（offset）仍然是**宿主数组的下标** → 用 int / size_t，
//     它们不是"槽里的值"
// ============================================================
#define VM_INT    int32_t
#define VM_UINT   uint32_t
#define VM_FLOAT  float

// ============================================================
// 槽里的值：8 字节 [type][value]
//   type == MEM_STRING 时 v 是**全局字符串池的下标**（不是文本、不是指针）
//   type == 容器时 v 是那个容器池的下标
// ============================================================
struct Value { VM_INT t; VM_INT v; };

// 槽类型（和编译器 / 老 bin 一致，不能改号）
static const VM_INT MEM_INT       = 0;
static const VM_INT MEM_FLOAT     = 1;
static const VM_INT MEM_STRING    = 2;
static const VM_INT MEM_ARRAY     = 3;    // vector<Value>
static const VM_INT MEM_MAP       = 4;    // map<Value,Value>
static const VM_INT MEM_GRID      = 5;    // 二维表
static const VM_INT MEM_UNDEFINED = 6;    // GML 的 undefined
static const VM_INT MEM_LIST      = 7;    // ds_list

// ============================================================
// 操作码（和编译器 / 老 bin 一致，不能改号）
// ============================================================
enum Opcode {
    OP_ASSIGN = 1,
    OP_COPY   = 2,
    OP_ADD    = 3,
    OP_SUB    = 4,
    OP_MUL    = 5,
    OP_DIV    = 6,
    OP_MOD    = 7,
    OP_EQ     = 8,
    OP_NEQ    = 9,
    OP_GT     = 10,
    OP_GTE    = 11,
    OP_LT     = 12,
    OP_LTE    = 13,
    OP_CALL   = 14,      // 游戏 API：fid 是 kGameFns 的下标
    OP_IF     = 15,
    OP_JMP    = 16,
    OP_HALT   = 17,
    OP_NATIVE = 18,      // 内部函数：fid 是 kNativeFns 的下标
    OP_LOAD_ELEM = 19,   // dst = 池[src]里的元素
    OP_BAND   = 20,
    OP_BOR    = 21,
    OP_BXOR   = 22,
    OP_BNOT   = 23,
    OP_SHL    = 24,
    OP_SHR    = 25,
};

// 指令布局（字节）
//   OP_ASSIGN    dst(s32) type(u8) value(MEM_STRING:u16 / MEM_FLOAT:f32 / 其它:s32)
//   OP_COPY      dst(s32) src(s32)
//   OP_ADD~MOD   dst(s32) a(s32) b(s32)
//   OP_EQ~LTE    dst(s32) a(s32) b(s32)
//   OP_CALL      fid(u16) argc(u8) dst(s32) arg(s32)*argc      dst = -1 无返回值
//   OP_NATIVE    同上
//   OP_IF        cond(s32) true_ip(s32) false_ip(s32)
//   OP_JMP       ip(s32)
//   OP_HALT
//   OP_LOAD_ELEM dst(s32) src(s32)
//   OP_BAND~SHR  dst(s32) a(s32) b(s32)
//   OP_BNOT      dst(s32) a(s32)

// ============================================================
// 上限
// ============================================================
// 一个 bin 一块定长内存：
//   槽 0            = 返回值
//   1 .. 4095       = 字面量池
//   4096 ..         = 各块的局部段
//   末尾 SCRATCH_SLOTS 个给 C++ 硬编码块当临时槽
static const int SCRATCH_BASE = 32768;
static const int SCRATCH_SLOTS = 64;
static const int MEM_SIZE = SCRATCH_BASE + SCRATCH_SLOTS;

// ============================================================
// 实例
//   实例的"变量"分三块：attr（C++ 预定义的字段）/ vars（脚本动态的）/ arrays（数组）
//   ⚠ 变量和容器的生命周期归 GML；内层只在明确拥有所有权时 release
// ============================================================
#define INNER_FIELD_MAKE_ENUM(NAME) F_##NAME,
enum {
    INNER_FIELD_LIST(INNER_FIELD_MAKE_ENUM)
    FIELD_COUNT
};
#undef INNER_FIELD_MAKE_ENUM

// 字段名在池里的区间：names.h 的顺序保证"内置变量名"紧跟在 game 名字后面，
//   所以头一个字段名（x）的池下标就是基址 —— 不用去数 native / game 各有多少个。
static const VM_INT SID_FIELD_BASE = (VM_INT)S_x;

// ── 各段的池下标基址 / 表长（都用枚举头直接取，不去数条数）──
//   池顺序 = empty | native(NATIVE_FN_LIST + NATIVE_IMPL_LIST) | game | 字段名 | 其余
//   ⚠ native / game 的**池顺序就是表顺序**（编译器烘 fid 用的就是表下标）：
//        fn_of_sid(sid) = kNativeFns[sid - SID_NATIVE_BASE]
static const VM_INT SID_NATIVE_BASE = (VM_INT)S__find_right_cell;   // native 段头
static const VM_INT SID_IMPL_BASE   = (VM_INT)S_run_block;          // native 表尾（后加的那批）
static const VM_INT SID_GAME_BASE   = (VM_INT)S_VM_BanCard;         // game 段头

static const int NATIVE_FN_COUNT = (int)(SID_GAME_BASE - SID_NATIVE_BASE);   // 358
static const int GAME_FN_COUNT   = (int)(SID_FIELD_BASE - SID_GAME_BASE);    // 150

static inline VM_INT field_sid(VM_INT f) { return SID_FIELD_BASE + f; }
static inline bool sid_is_field(VM_INT sid) {
    return sid >= SID_FIELD_BASE && sid < SID_FIELD_BASE + FIELD_COUNT;
}
static inline VM_INT field_of_sid(VM_INT sid) { return sid - SID_FIELD_BASE; }

struct Instance {
    VM_INT id;                    // 身份：**只增不减**，和槽位（在 g_instances 里的位置）是两回事
    VM_INT obj;                   // 对象名的**池下标**

    Value attr[FIELD_COUNT];      // 定长属性：C++ 里预定义的那堆字段（x / y / image_index …）
    std::unordered_map<VM_INT, Value> vars;                 // 其余属性：名字池下标 → 值
    std::unordered_map<VM_INT, std::vector<Value>> arrays;  // 数组：数组名池下标 → 数组
};

// 内置字段指针（不是内置名 → NULL；调用方再去看 vars）
static inline Value* inst_attr(Instance& in, VM_INT sid) {
    return sid_is_field(sid) ? &in.attr[field_of_sid(sid)] : NULL;
}

// ============================================================
// 块 / VM / callbin
// ============================================================
struct Block {
    VM_INT               name_sid;   // 块名的**池下标**
    std::vector<uint8_t> code;
};

// 一个 bin = 一个 VM = 一个对象
//   ⚠ `_VM_CONST_INIT`（字面量表，全都是 OP_ASSIGN）**不在 blocks 里**：
//     装载期跑一次把字面量填进 mem（mem 是长驻的），它不是一个事件
struct vm_bin {
    Value                            mem[MEM_SIZE];   // 定长，直接是数组（不用指针）

    std::unordered_map<VM_INT, Block> blocks;         // 事件名池下标 → 块

    // ── 名下的实例（设计见 docs/vm_design.md §2）──
    //   arr 存**槽位**（<16 用它），mp 存**槽位**（≥16 用它，转了不回），
    //   cur / cur_rank = map 模式下的游标（上次访问到哪个、第几位）
    VM_INT                  inst_count;     // 实例个数
    std::vector<int>        arr;            // 槽位 = 宿主数组下标 → int
    std::set<int>           mp;             // 同上
    std::set<int>::iterator cur;
    int                     cur_rank;

    vm_bin() : inst_count(0), cur_rank(0) {
        memset(mem, 0, sizeof(mem));               // 全 0 = MEM_INT + 0，和 GML 的槽初值一致
        cur = mp.end();
    }
};

// 一个 callbin = 一个外部函数：**没有自己的内存**，跑在公共内存上
//   ⚠ 现有导出的 callbin **都是两个块**（_VM_CONST_INIT + _CALL_ENTRY），bin 也是：
//     _VM_CONST_INIT 是**字面量表**（只有 OP_ASSIGN，不是运行逻辑）
//     bin 的 mem 长驻 → 装载期跑一次就完事，块不留（见 VM_BIN_Load）
//     callbin 的帧是每次借的 → 每帧都得重填，所以 init 得留着
struct vm_callbin {
    Block init;       // _VM_CONST_INIT：每次调用先跑它，把字面量填进这一帧
    Block block;      // _CALL_ENTRY：函数体
    int   value;      // 它需要的内存大小（槽位）；函数调函数就在公共内存上再加偏移
};

// ============================================================
// 接口（三个就够）
// ============================================================
// 跑一个块：跑 bin 的块 → 没有返回值；跑 callbin 的块 → 返回函数的返回值
//   "当前是哪个实例"由内层的调用栈管（VM_RUN 自己压 / 弹）
Value VM_RUN(Block& block, Value* mem);

// 装载：字节流 + 大小 + 出参；成功 = 1，失败 = undefined
Value VM_BIN_Load    (const uint8_t* data, size_t size, vm_bin&     out);
Value VM_CALLBIN_LOAD(const uint8_t* data, size_t size, vm_callbin& out);

// 跑一个事件块（把"当前实例"压栈 → VM_RUN → 弹栈）；没有这个块就什么都不做
//   ⚠ 只跑字节码块 —— "先查有没有 C++ 热块"是外层的派发（run_block_on）
void vm_run_event(vm_bin& vm, VM_INT event_sid, VM_INT inst_id);

// 浮点值就在槽的 v 段里（和整数同一个位置、同一个宽度），f_of / i_of 就是同一块位的两种读法
//   （先定义：下面的 rd_f / wr_f 都要用）
static inline VM_FLOAT f_of(VM_INT v)     { VM_FLOAT f; memcpy(&f, &v, sizeof(VM_FLOAT)); return f; }
static inline VM_INT   i_of(VM_FLOAT f)   { VM_INT   i; memcpy(&i, &f, sizeof(VM_FLOAT)); return i; }

// ============================================================
// 槽读写（外层实现函数直接用它，参数就是内存地址）
//   ⚠ slot（槽号）是**宿主数组的下标**，不是槽里的值 → 用 int
// ============================================================
static inline Value    rd_v(Value* mem, int slot) { return mem[slot]; }
static inline VM_INT   rd_t(Value* mem, int slot) { return mem[slot].t; }
static inline VM_INT   rd_i(Value* mem, int slot) { return mem[slot].v; }
static inline VM_FLOAT rd_f(Value* mem, int slot) { return f_of(mem[slot].v); }

void  retain(const Value& v);                       // 多一个地方存着这个句柄
void  release(const Value& v);                      // 少一个；减到 0 回收
void  slot_write(Value* mem, int slot, Value v);    // 先 retain 新的，再 release 旧的

static inline void wr_v(Value* mem, int slot, Value v) { if (slot >= 0) slot_write(mem, slot, v); }
static inline void wr_i(Value* mem, int slot, VM_INT x)   { wr_v(mem, slot, Value{MEM_INT, x}); }
static inline void wr_f(Value* mem, int slot, VM_FLOAT f) {
    Value v; v.t = MEM_FLOAT; v.v = i_of(f); wr_v(mem, slot, v);
}

static inline Value mk_int(VM_INT x)        { return Value{MEM_INT, x}; }
static inline Value mk_str_sid(VM_INT sid)  { return Value{MEM_STRING, sid}; }
static inline Value mk_undef()              { return Value{MEM_UNDEFINED, 0}; }

// 参数表：入口指向**第一个实参槽号**，每个 4 字节
static inline int argslot(const uint8_t* a, int k) {
    int32_t s; memcpy(&s, a + k * 4, 4); return (int)s;
}

// ── 取参 / 回值（外层写实现函数时用的就是这几条）──
static inline bool container_t(const Value& v) {          // 是不是容器句柄（看类型）
    return v.t == MEM_ARRAY || v.t == MEM_MAP || v.t == MEM_GRID || v.t == MEM_LIST;
}
static inline Value arg_v  (Value* mem, const uint8_t* args, int k) { return mem[argslot(args, k)]; }
static inline VM_INT arg_i (Value* mem, const uint8_t* args, int k) { return mem[argslot(args, k)].v; }
static inline VM_FLOAT arg_num(Value* mem, const uint8_t* args, int k) {   // 数字参数：整数也当数
    Value v = mem[argslot(args, k)];
    return (v.t == MEM_FLOAT) ? f_of(v.v) : (VM_FLOAT)v.v;
}
// 取第 k 个实参当容器句柄：类型不对 / 已经死了 → -1（调用方直接返回，别再往下走）
int handle_arg(Value* mem, const uint8_t* args, int k, VM_INT type);

// 把一个**刚建出来**的句柄交给 dst：
//   dst < 0（语句位置，没人接）→ 放掉创造者那份；有 dst → 槽接手（wr_v 会 retain），再放掉创造者那份
void give_handle(Value* mem, int dst, const Value& v);

// ============================================================
// 取指用的小端读取 —— 这是**字节码文件格式**的宽度（编译器定的），不是槽的宽度
//   bin 里的 s32 / u16 / u8 永远是这个大小，换 64 位也不动它
// ============================================================
static inline int32_t  ld32(const uint8_t* p) { int32_t  v; memcpy(&v, p, 4); return v; }
static inline uint16_t ld16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline float    ldf (const uint8_t* p) { float    v; memcpy(&v, p, 4); return v; }

// ============================================================
// 函数（外层实现；内层只按表下标调，不认识具体是谁）
//   kNativeFns = OP_NATIVE 那张表，kGameFns = OP_CALL 那张表
//   名字与顺序是**契约**（已编译的 mod 依赖这套 id）：只加实现，不动顺序
// ============================================================
typedef void (*Fn)(Value* mem, const uint8_t* args, int argc, int dst);
extern Fn kNativeFns[];
extern Fn kGameFns[];
extern const int kNativeFnCount;   // 表自己报出长度（内层不用去数）
extern const int kGameFnCount;

// ============================================================
// 错误：内层**只记码和参数**，文本在外层的日志文件里格式化
// ============================================================
enum VmErrCode {
    VM_OK = 0,
    VM_E_BLOCK_NOT_FOUND,     // a1 = 块名池下标
    VM_E_UNKNOWN_OPCODE,      // a1 = opcode，a2 = 块名池下标
    VM_E_UNKNOWN_NATIVE,      // a1 = native id
    VM_E_UNKNOWN_GAME_FN,     // a1 = game fn id
    VM_E_NONINT_OPERAND,      // a1 = 块名池下标
    VM_E_BIN_TOO_MANY_STRS,   // a1 = 上限
    VM_E_BIN_HEAD,            // a1 = 出事的位置
    VM_E_BIN_BLOCK,           // a1 = 块序号，a2 = 偏移
    VM_E_BIN_TRUNC,           // a1 = 需要，a2 = 实际
    VM_E_FN_NOT_FOUND,        // a1 = 函数名池下标
    VM_E_CALL_DEPTH,          // a1 = 当前层数
    VM_E_CALL_MEM,            // a1 = 这一帧要多少槽，a2 = 已经用了多少
    VM_E_NULL_MEM,            // a1 = 槽号（内存地址是空的）
    VM_E_NOT_IMPL,            // a1 = 函数名池下标（**要宿主**、还没实现的那种）
    // 出站共享内存写不下（VM_HostPush）：a1 = 本次**已经**写进去的字节数，a2 = 容量（上限）
    //   ⚠ **不复用 VM_E_NOT_IMPL**：那个的意思是"这条函数还没实现"，拿它表示"内存满了"
    //     会把"宿主没接"和"接满了"两件事混成一件，排查时看不出区别。
    //   ⚠ 语义：不会留下**半条** payload（每支 host_shm_push_* 自己原子），
    //     但整次调用不是一笔头 —— 已经写完的那几个实参留在流里。
    VM_E_HOST_FULL,
    // 入站包不合法（引擎 → VM 那条通道，见 outer/packet.cpp）：
    //   a1 = 具体原因（outer/packet.cpp 的 PacketBadReason 清单，**不复用别的码**：
    //        "包长错了" / "函数名没有" / "内存满了" / "这条还没实现" 得能分开看）
    //   a2 = 出事处的原始数值（size / 值个数 / argc / 池下标 / 类型号，按 a1 解释）
    //   ⚠ 这个码是**响亮报错**用的，坏包一律不许静默跳过（用户明确要求）
    VM_E_BAD_PACKET,
    // 字符串拼接太长（OP_ADD 的字符串路径）：a1 = 拼出来会是多长，a2 = 上限。
    //   为什么不截断：池字符串**不可变、不回收**，截断就是静默改语义（用户明确要求响亮报错）
    VM_E_STR_TOO_LONG
};
extern int g_vm_err;      // 最近一次错误码（VM_OK = 没出错）
extern int g_vm_err_a1;
extern int g_vm_err_a2;
void vm_set_err(int code, int a1, int a2);

// ============================================================
// 字符串池（全局一张；文本只在池里，别处一律池下标）
// ============================================================
extern std::vector<std::string>             g_strings;
extern std::unordered_map<std::string, VM_INT> g_str_map;

bool str_ok(VM_INT i);                       // 下标合法？
const std::string& str_at(VM_INT i);         // 取文本（越界 → 空串）
VM_INT str_find(const std::string& s);       // 名字 → 下标（**只查不建**，没有 → -1）
VM_INT intern_str(const std::string& s);     // 名字 → 下标（没有就建）
void   pool_seed();                          // 灌预置段（幂等；intern_str 会自己兜一下）

// 值 → GML 文本：`string(x)` 的**唯一**实现（pool.cpp）。
//   ⚠ 内层 OP_ADD 的字符串拼接和外层 string() 都走这一份 —— 只有一份，
//     `"a" + x` 才不可能和 `"a" + string(x)` 出来两种格式。
std::string value_to_text(const Value& v);

// ============================================================
// 全局变量表（GML 的 global.x：进程级，VM 挂了也不清）
// ============================================================
extern std::unordered_map<VM_INT, Value> g_globals;
Value* global_var_sid(VM_INT sid);                 // 没有 → NULL
void   global_var_set_sid(VM_INT sid, Value v);    // 没有就建；**引用计数的账在这儿结**
bool   global_exists_sid(VM_INT sid);

// ============================================================
// 实例表（槽位表 + 复用队列 + id→槽位）
//   ⚠ 槽位 = 宿主数组下标 → int；实例 id / 对象名下标 = 会被 GML 看到的值 → VM_INT
// ============================================================
extern std::vector<Instance>              g_instances;   // 下标 = **槽位**（0 号留位：全局 / 当前实例的哨兵）
extern std::unordered_map<VM_INT, int>    g_inst_alive;  // 实例 id（只增不减）→ 槽位（可复用）——唯一权威
extern std::vector<int>                   g_inst_free;   // 回收的槽位，等着复用
extern VM_INT                             g_next_id;     // 下一个实例 id（只增不减）

// 建一个实例 → 返回实例 id
//   ⚠ Create 事件在这儿跑（**建和销对称**：inst_destroy 里跑 Destroy）——
//     所以调用方**不要**再自己跑一遍 Create
//   ⚠ 这里只跑字节码块；热块优先那条路（run_block_on）等热块写起来再说
VM_INT    inst_new(vm_bin& vm, VM_INT obj_sid);
Instance* inst_get(VM_INT id);                     // 没有 / 已死 → NULL
bool      inst_alive(VM_INT id);                   // 查 g_inst_alive
void      inst_kill(VM_INT id);                    // 只回收（不跑事件）：从 VM 的 arr/mp 摘掉 + 槽位进复用队列
void      inst_destroy(VM_INT id);                 // 销毁**唯一入口**：跑 _OBJECT_DESTROY → inst_kill
void      inst_attr_init(Instance& in);            // 内置字段回默认值（inst_new 里调）

VM_INT    inst_kth(vm_bin& vm, VM_INT k);          // 这个 VM 名下第 k 个实例 id（<16 数组直取 / ≥16 走游标）
VM_INT    inst_at(VM_INT obj_sid, VM_INT k);       // 按对象名问（= 找那个对象的 VM，再 inst_kth）
VM_INT    obj_count(VM_INT obj_sid);               // 这个对象还活着几个（= 那个 VM 的 inst_count）

// 属性读写：实例用**引用**（手上已经有实例本体时用它，省一次 id → 槽位的查找）
//   内置字段优先（一次区间判断），其余一律走 vars；没设过 → NULL（调用方给 undefined）
//   ⚠ 写进去 = 存进**长驻位置** → 引用计数的账由内层结（先 retain 新的、再 release 旧的），
//     外层算完直接写，**不要**再手动 retain/release（会重复计数）
Value*    inst_prop(Instance& in, VM_INT sid);
void      inst_prop_set(Instance& in, VM_INT sid, Value v);

// 实例变量读写（手上只有 id 时用）：就是 inst_prop 前面套一次 inst_get
Value*    inst_var_sid(VM_INT id, VM_INT sid);
void      inst_var_set_sid(VM_INT id, VM_INT sid, Value v);

// ── 实例的数组（Instance::arrays：数组名池下标 → 数组）──
//   语义照 GML：句柄就是"这个实例的这个名字"，就地改，没有引用计数那套
//   （元素本身可能是池里的容器句柄 → 存进去 retain、拿出来/删掉 release）
//   元素比较一律按 [类型 + 值]：1 和 "1" 不是一回事
bool  inst_arr_exists (Instance& in, VM_INT sid);
std::vector<Value>* inst_arr(Instance& in, VM_INT sid);                       // 没有 → NULL
void  inst_arr_create (Instance& in, VM_INT sid, VM_INT n, Value fill);       // 建成 n 个（都填 fill）
VM_INT inst_arr_size  (Instance& in, VM_INT sid);                            // 没有 → 0
Value inst_arr_item   (Instance& in, VM_INT sid, VM_INT k);                   // 越界 → undefined（**不加引用**，写槽走 wr_v）
void  inst_arr_set    (Instance& in, VM_INT sid, VM_INT k, Value v);          // 越界就先补 0 到 k
void  inst_arr_add    (Instance& in, VM_INT sid, Value v);                    // 尾部追一个
Value inst_arr_pop    (Instance& in, VM_INT sid);                            // 弹出最后一个（那份引用归**调用方**）
VM_INT inst_arr_index (Instance& in, VM_INT sid, Value v);                    // 没有 → -1
void  inst_arr_delete (Instance& in, VM_INT sid, VM_INT k, VM_INT n);
void  inst_arr_resize (Instance& in, VM_INT sid, VM_INT n);                   // 变长填 0，变短放掉被砍的
void  inst_arr_clear  (Instance& in, VM_INT sid);
void  inst_arr_shuffle(Instance& in, VM_INT sid);

// VM 名下实例容器的内部操作（对外只用上面那几个；槽位是宿主下标 → int）
void vm_inst_add(vm_bin& vm, int slot);
void vm_inst_erase(vm_bin& vm, int slot);

// ============================================================
// 对象名 → VM / mod id → VM（全局各一张；跨对象派发靠它找"那个对象归谁管"）
// ============================================================
extern std::unordered_map<VM_INT, vm_bin*> g_vm_of_object;   // 对象名池下标 → VM
extern std::unordered_map<VM_INT, vm_bin*> g_mod_vms;        // mod id 池下标 → VM
vm_bin* vm_of_object_sid(VM_INT sid);
vm_bin* mod_vm_sid(VM_INT sid);

// ============================================================
// callbin 字典 + 公共内存
// ============================================================
extern std::map<VM_INT, vm_callbin> g_call_dict;   // 函数名池下标 → callbin
extern std::vector<Value>           g_call_mem;    // 公共内存（固定大小，首次调用时分配）
extern size_t                       g_call_off;    // 当前偏移（槽）：函数调函数就在上面加
extern bool                         g_call_abort;  // 触上限 → 整条调用链收手

static const size_t CALL_MEM_SLOTS = 131072;    // 公共内存 1 MB / 8

// 一个函数最多收几个实参（超过就报错；静默截断更糟）
static const int CALL_MAX_ARGS  = 32;
// 嵌套层数上限：每层调用都是**真的在 C++ 栈上递归**（run → OP_CALL → callbin_call → run），
//   不闸住就先炸栈（0xC00000FD），而"炸栈"既没报错也没有 undefined
static const int CALL_MAX_DEPTH = 512;

// 调一个 callbin：取参 / 切上下文（公共内存 + 偏移）/ 派发 / 回值 / 报错
//   fn_sid = 函数名的池下标；argv/argn = 实参
//   返回函数的返回值；找不到 / 层数太深 / 公共内存不够 → undefined（并置错误码）
Value callbin_call(VM_INT fn_sid, const Value* argv, int argn);

// callbin 的函数体块名（池下标）
static const VM_INT CALLBIN_ENTRY = S__CALL_ENTRY;

// ============================================================
// 命名数组（game API 的 VM_Array* 用）
//   **每个 VM 一份** = 同一个对象的所有实例共用一份（语义照旧，老代码放 Vm::named_arrays）
//   vm_bin 不加字段：表放池里，key = VM 本体
//   名字用**池下标**；数组本身不是句柄、没有引用计数那套，但元素的引用要结账
// ============================================================
extern std::unordered_map<vm_bin*, std::unordered_map<VM_INT, std::vector<Value>>> g_named_arrays;

std::vector<Value>* vm_array_find(vm_bin* vm, VM_INT name_sid);              // 没有 → NULL
std::vector<Value>* vm_array_make(vm_bin* vm, VM_INT name_sid);              // 没有就建
void                vm_array_drop(vm_bin* vm, VM_INT name_sid);              // 删一个（放掉元素引用）
void                vm_array_clear_all(vm_bin* vm);                          // 全删（VM 收尾用）

// ============================================================
// 调用栈（数组模拟）：每层 VM_RUN 压一个"当前实例 id" + "这块码归谁"
//   · 实例 id → 每层都知道自己这层是哪个实例
//   · vm_bin* → 外层实现函数（run_block / 派发）要知道"我手上这个 mem 是哪个 VM 的"
//   （callbin 那种借公共内存跑的帧没有 vm_bin → 压 NULL，见 call.cpp）
// ============================================================
static const int RUN_STACK_MAX = 512;
extern VM_INT   g_run_stack[RUN_STACK_MAX];   // 实例 id（0 = 全局 / 当前实例的哨兵）
extern vm_bin*  g_run_vm[RUN_STACK_MAX];      // 这块码的属主（没有 → NULL）
extern int      g_run_sp;                     // 栈顶（下一个空位；宿主下标）

static inline VM_INT   run_inst() { return (g_run_sp > 0) ? g_run_stack[g_run_sp - 1] : 0; }
static inline vm_bin*  cur_vm()   { return (g_run_sp > 0) ? g_run_vm[g_run_sp - 1] : NULL; }

void run_push(vm_bin& vm, VM_INT inst_id);    // 压一层（栈满 → 不压，返回时也别弹）
void run_push_frame(VM_INT inst_id);          // 压一层"没有 vm_bin"的（callbin 借内存的那种）
void run_pop();

// ============================================================
// 容器池（全局；四种容器 + 引用计数 + 时间戳 + 复用队列）
// ============================================================
struct Grid { VM_INT w; VM_INT h; std::vector<Value> cells; };
struct ValueLess {
    bool operator()(const Value& a, const Value& b) const {
        if (a.t != b.t) return a.t < b.t;
        return a.v < b.v;
    }
};

extern std::vector<std::vector<Value>>                g_arr_pool;    // 数组
extern std::vector<std::map<Value, Value, ValueLess>> g_map_pool;    // ds_map / struct
extern std::vector<Grid>                              g_grid_pool;   // ds_grid
extern std::vector<std::vector<Value>>                g_list_pool;   // ds_list

// 引用计数 / 时间戳 / 复用队列（下标 = 池下标，存的是宿主下标 → int）
extern std::vector<VM_INT> g_arr_refs,  g_arr_stamp;
extern std::vector<VM_INT> g_map_refs,  g_map_stamp;
extern std::vector<VM_INT> g_grid_refs, g_grid_stamp;
extern std::vector<VM_INT> g_list_refs, g_list_stamp;
extern std::vector<int>    g_arr_used,  g_map_used, g_grid_used, g_list_used;

bool    container_alive(const Value& v);   // 句柄还在池里活着？
void    container_free (const Value& v);   // 下标回复用队列（内容留着，复用时再清）
VM_INT  container_stamp(const Value& v);   // 创建时间戳（新建 / 复用都取新号）

// 把 child 放进 parent 之前调：容器只收"比自己新"的（正序）→ 环在结构上形成不了
bool allow_store(const Value& child, const Value& parent);

int arr_new();   void arr_free(int i);
int map_new();   void map_free(int i);
int grid_new();  void grid_free(int i);
int list_new();  void list_free(int i);
