// ============================================================
// packet.cpp — 入站包：**引擎 → VM** 那条通道的 VM 侧（解包 + 执行）
//
//   链路（三个名字都是引擎认的，见 host/host.h 第 7 节）：
//     引擎：host_packet_apply(buf, size)
//       → host/host_packet.cpp 把这一下**转给注册进来的 handler**
//         → 本文件 VM_ApplyPacket（outer_init() 里注册的）
//           → 按包里的规则做一件事：设一个全局变量 / 按名字调一次函数
//
//   和出站（VM → 引擎）是**两套东西**，别混：
//     出站：outer/host_push.cpp 的 VM_HostPush 往 host_shm 追加**纯 payload**（没有类型字节，
//           靠两端事先约定的布局）；引擎拿 base + cursor 自己读
//     入站：包是**自描述的**（每个值带自己的类型），因为这是"指令"不是"数据流"：
//           指令得能被引擎单独拼出来，不能要求引擎也维护一套"约定布局"的默契
//
//   ★【包格式】★ —— 整个入站通道的字节规则**只有这一块**，格式要改就改这里
//
//     包 = 一连串 Value，**每个 8 字节**（就是 mem 那套 {VM_INT t; VM_INT v;}），小端。
//     槽里的字符串值带的是**池下标**，不是文本 —— 引擎拼包前先用 VM_InternText()
//     把文本换成池下标（只认文本、不建新的用 VM_SidOf()）。
//
//     值的个数（= size / 8）必须是**整数**个；第一个值必须是字符串，按它的**文本**分支：
//
//       ① 文本 != "CALL"（**严格**比较：小写 call 不算，就是普通的全局变量名）
//            [名字(字符串)][值]                      ← 正好 2 个值，多一个少一个都是坏包
//          → global_var_set_sid(head.v, 那个值)
//            ⚠ 第一个值**既是分支依据、又是变量名**：外层要写 global.my_x = 1，
//              就拼 [VM_InternText("my_x"), mk_int(1)] 这一包（两个值）。
//
//       ② 文本 == "CALL"
//            ["CALL"][函数名(字符串)][argc(int)][参数1]…[参数argc]   ← 正好 3 + argc 个
//          → 按名字调一次函数：
//              内置（native 表 kNativeFns）和 VM 函数（game 表 kGameFns）都支持，
//              两张表都不在的名字再走 vm_call_by_name（DLL 注册表 / callbin 表）。
//            ⚠ "走哪张表"完全按**名字的池下标落在哪一段**决定（池顺序 = native | game | 字段 | 其余，
//              见 core/vm_def.h）：这就是 outer 里现成的解析方式，别另造一套名字表。
//
//   ★【怎么把参数交给被调函数】★
//     被调函数（Fn）只拿到**一个** mem 指针：实参槽号表指进去的槽、返回值槽，
//     都得在**同一块内存**里。包本身**不能**当 mem 传（memcpy 出来的临时拷贝、
//     而且包里每个值只 8 字节、没有"返回值槽"可言）。
//     所以这里常驻一块**自己的 scratch 内存**（g_pkt_mem，形状和 vm_bin::mem 一模一样），
//     把 argc 个参数槽写进 SCRATCH_BASE 往后那一段，用这批**槽号**当 args 表，
//     返回值写这批槽后面的另一个槽 —— 和 GML 里一次普通调用的形状完全一致。
//       +0 .. +31  → 实参（CALL_MAX_ARGS 个）
//       +32        → 返回值（没人来接：包是单向的，返回 0 / 负数就完了）
//     ⚠ SCRATCH_BASE+32 起、+63 止 是 outer 的临时槽区（OUTER_TMP_SLOT / game_a / game_b）。
//       本文件的返回值槽和 OUTER_TMP_SLOT 重合，**这里是有意的**：这个槽的内容
//       没有任何观察者（返回值不往外传，只用来放掉最后那份引用），就算被临时槽用户踩了，
//       也只是"少放掉一个没人要的句柄"，不会乱。实参那 32 个槽在低半区，现在没人用。
//     ⚠ 为什么不借"当前 VM"的 mem（cur_vm()）：引擎是在**任意时刻**调进来的，
//       VM 可能正跑到一半；往它的 scratch 里塞参数就是踩它正在用的临时槽。
//       自己一块 ⇒ 谁调、什么时候调，都不会影响正在跑的那一帧。
//       代价：像 run_block 那种要求 `mem == cur_vm()->mem` 的条目在这里会**响亮报错**
//       （VM_E_BLOCK_NOT_FOUND），而不是带着错误的前提跑下去 —— 这正是想要的。
//
//   ★【坏包一律响亮】★（用户明确要求：不许静默）
//     buf == NULL / size <= 0 / size 不是 8 的倍数 / 第一个值不是字符串（或池下标坏了）
//     / 值的个数和这一条规则对不上 / argc 与实际参数个数不符 / argc < 0 / argc > CALL_MAX_ARGS
//     / 函数名不是字符串 / 函数名在四张表里都查不到
//       → 一律 vm_set_err(VM_E_BAD_PACKET, 原因, 原始数值) + 返回负数。
//     ⚠ 一条都不能"跳过继续"：包是引擎按约定拼的，拼错了说明两端口径不一致，
//       静默跳过 = 让引擎以为生效了（这种错最难查）。
//
//   返回值约定：**成功 → 0**（一个包只做一件事，没有"条数"概念）；**失败 → -1**。
//     被调函数**自己**报的错（g_vm_err）不改这个返回值：包是合法的、名字也存在，
//     调用本身的成败由那一层的语义决定（它已经响亮置过码了）。
//
//   ⚠ 跨 DLL 只走 POD：VM_ApplyPacket / VM_InternText / VM_SidOf 的签名里没有
//     std::string / std::vector（host_packet.h 那边也是 void* + int）。
//   ⚠ 不 printf：出错只 vm_set_err，查的人去日志里看（和外面其它文件一个规矩）。
// ============================================================
#include "outer.h"
#include <cstring>
#include <string>

namespace {

// 包格式的宽度就是 Value 的宽度（两端口径的唯一真源）。
//   写成 static_assert 而不是散落的 8：以后 VM_INT 真上 64 位，这里会**响亮地**编译不过
//   （那等于改协议，引擎那边得一起改，不该悄悄跟着变）。
static_assert(sizeof(Value) == 8, "入站包的每个值必须正好 8 字节（改了就是改协议，引擎要一起改）");
const int PACKET_VALUE_BYTES = (int)sizeof(Value);

// 坏包的原因（填进 VM_E_BAD_PACKET 的 a1）——**只在这里定义**，日志格式化那边按这份清单解释。
//   a2 放"出事处的原始数值"，按 a1 解释。
enum PacketBadReason {
    PK_BUF_NULL = 0,     // buf == NULL                       a2 = 0
    PK_SIZE,             // size <= 0 或 size % 8 != 0         a2 = size
    PK_HEAD_TYPE,        // 第一个值不是字符串 / 池下标越界       a2 = 类型号或池下标
    PK_COUNT,            // 值的个数和这一条规则对不上            a2 = 实际值个数
    PK_FN_TYPE,          // ② 函数名不是字符串 / 池下标越界       a2 = 类型号或池下标
    PK_ARGC_TYPE,        // ② argc 不是 int                    a2 = 类型号
    PK_ARGC_RANGE,       // ② argc < 0 或 > CALL_MAX_ARGS      a2 = argc
    PK_FN_NOT_FOUND      // ② 函数名四张表都查不到               a2 = 函数名池下标
};

// 实参槽基址 / 返回值槽（都是 g_pkt_mem 里的槽号）
const int PKT_ARG_BASE = SCRATCH_BASE;
const int PKT_RET_SLOT = SCRATCH_BASE + CALL_MAX_ARGS;

// 本文件自己的 scratch 内存（形状和 vm_bin::mem 一样）：
//   ⚠ static 而不是栈上：MEM_SIZE 是 3 万多槽（256KB），放栈上就是一次爆栈；
//     放静态区还保证槽号和地址**永远是同一份**（被调函数按槽号读，不认地址）。
//   ⚠ 零初始化：没写过的地方就是 MEM_INT + 0，和 VM 的槽初值一致。
Value g_pkt_mem[MEM_SIZE];

// 报坏包 + 回负数（**唯一**的失败出口：原因和原始数值都进错误码，别的地方别自己构造）
int bad_packet(int reason, int raw) {
    vm_set_err(VM_E_BAD_PACKET, reason, raw);
    return -1;
}

// 读包里第 k 个值。
//   ⚠ 引擎给的内存**不保证 8 字节对齐**（可能是个 char 缓冲区中间）→ 一律 memcpy 进局部
//     Value 再读，绝不 (Value*)buf 解引用（未对齐访问在 x86 上"看着能跑"，换平台/换优化就炸，
//     而且这种崩法最难查）。memcpy 会被编译器编成一条 8 字节 load，没有性能代价。
Value packet_value(const void* buf, int k) {
    Value v;
    memcpy(&v, (const uint8_t*)buf + (size_t)k * (size_t)PACKET_VALUE_BYTES, sizeof(Value));
    return v;
}

}  // namespace

// ============================================================
// VM_ApplyPacket(buf, size) —— 引擎唯一要调的那个（host_packet_set_handler 注册的就是它）
//   成功 → 0；坏包 / 失败 → -1（并 vm_set_err(VM_E_BAD_PACKET, …)）
//   ⚠ 签名必须和 HostPacketFn 一模一样（int (*)(void*, int)），否则注册口对不上
// ============================================================
int VM_ApplyPacket(void* buf, int size) {
    if (!buf) return bad_packet(PK_BUF_NULL, 0);
    if (size <= 0) return bad_packet(PK_SIZE, size);
    if (size % PACKET_VALUE_BYTES != 0) return bad_packet(PK_SIZE, size);   // 半个值 = 坏包

    const int nv = size / PACKET_VALUE_BYTES;      // 包里值的个数

    // ── 第一个值：必须是字符串（它是**分支依据**，也是 ① 的变量名）──
    const Value head = packet_value(buf, 0);
    if (head.t != MEM_STRING || !str_ok(head.v)) return bad_packet(PK_HEAD_TYPE, head.v);

    // ── 规则 ①：文本 != "CALL" → 设一个全局变量 ──
    //   严格比较（std::string 的 != 是逐字节的）：小写 call / "CALL " / "CALLx" 都走这条，
    //   当普通全局变量名 —— 不搞"大小写不敏感"那种隐式宽容（宽容 = 拼错了还以为生效了）。
    if (str_at(head.v) != "CALL") {
        if (nv != 2) return bad_packet(PK_COUNT, nv);          // 多一个 / 少一个都是坏包
        const Value val = packet_value(buf, 1);
        // 变量名就是第一个值的池下标（head.v）；引用计数的账由内层结（先 retain 新的、再 release 旧的）
        global_var_set_sid(head.v, val);
        return 0;
    }

    // ── 规则 ②：["CALL"][函数名][argc][参数…] → 按名字调一次函数 ──
    if (nv < 3) return bad_packet(PK_COUNT, nv);               // 连头都拼不全

    const Value fn = packet_value(buf, 1);
    if (fn.t != MEM_STRING || !str_ok(fn.v)) return bad_packet(PK_FN_TYPE, fn.v);

    const Value argc_v = packet_value(buf, 2);
    if (argc_v.t != MEM_INT) return bad_packet(PK_ARGC_TYPE, argc_v.t);
    const int argc = argc_v.v;
    if (argc < 0 || argc > CALL_MAX_ARGS) return bad_packet(PK_ARGC_RANGE, argc);
    if (nv != 3 + argc) return bad_packet(PK_COUNT, nv);       // argc 和实际参数个数不符

    // ── 名字 → 表项：**按池下标所在段**决定走哪张表（现成的解析方式，不另造名字表）──
    //   fn_of_sid  = native 段（SID_NATIVE_BASE .. SID_GAME_BASE）
    //   game 段    = kGameFns[sid - SID_GAME_BASE]（和 patch_game_fns 同一个下标算法）
    //   都不在     = 不是 508 条表里的名字 → 交给 vm_call_by_name（DLL 注册表 → callbin 表）
    Fn f = fn_of_sid(fn.v);
    if (!f && fn.v >= SID_GAME_BASE && fn.v < SID_FIELD_BASE) f = kGameFns[fn.v - SID_GAME_BASE];

    bool by_name = false;
    if (!f) {
        // 名字查不到就**别调**：vm_call_by_name 里那条 callbin 路会置 VM_E_FN_NOT_FOUND，
        //   但那时错误码已经不是"坏包"了（看起来像"函数内部出错"）。查得到才放行，
        //   这样"引擎拼了个没有的函数名"能明确报成 PK_FN_NOT_FOUND。
        //   判法照抄 impl_VM_FuncExists：DLL 函数表 + callbin 表。
        if (!vm_find_dll_fn(fn.v) && g_call_dict.find(fn.v) == g_call_dict.end())
            return bad_packet(PK_FN_NOT_FOUND, fn.v);
        by_name = true;
    }

    // ── 参数搬进 scratch（**同一块 mem 里**：实参槽 + 返回值槽）──
    //   slot_write（不是直接赋值）：包里的容器句柄也要走引用计数那套规矩
    //   （先 retain 新的、再 release 旧的）；int / float / string 那边是空操作。
    int32_t tbl[CALL_MAX_ARGS];
    for (int i = 0; i < argc; i++) {
        const Value a = packet_value(buf, 3 + i);
        slot_write(g_pkt_mem, PKT_ARG_BASE + i, a);
        tbl[i] = PKT_ARG_BASE + i;                    // args 表里放的是**槽号**，不是值
    }
    slot_write(g_pkt_mem, PKT_RET_SLOT, mk_int(0));    // 返回槽先归零，别接上一次的残留

    if (by_name) vm_call_by_name(fn.v, g_pkt_mem, (const uint8_t*)tbl, argc, PKT_RET_SLOT);
    else         f(g_pkt_mem, (const uint8_t*)tbl, argc, PKT_RET_SLOT);

    // ── 收尾：把 scratch 里的引用放掉 ──
    //   实参是**包**转交过来的（引擎不持有引用），返回值没人接 → 这两份都得这里放。
    //   不放的话：每调一次就漏掉一批容器句柄的引用（池永远回收不了）。
    for (int i = 0; i < argc; i++) slot_write(g_pkt_mem, PKT_ARG_BASE + i, mk_int(0));
    slot_write(g_pkt_mem, PKT_RET_SLOT, mk_int(0));
    return 0;
}

// ============================================================
// 引擎造包用的两个口（**必须先有它俩**，否则包里那个"字符串值"没法造）
//
//   包里的字符串 Value 带的是**池下标**，不是文本 —— 池是 VM 的私有物，
//   引擎没有别的办法把一段文本变成合法下标。所以：
//     VM_InternText(utf8)  文本 → 池下标（池里没有就建一个）
//     VM_SidOf(utf8)       文本 → 池下标（**只查不建**，没有回 -1）
//   ⚠ 引擎造包的顺序是：sid = VM_InternText("global_name") / VM_InternText("func_name")，
//     然后拼 [mk_str_sid(sid)][值] 或 ["CALL"][mk_str_sid(sid)][argc][参数…]。
//   ⚠ 跨 DLL 只过 POD：const char* 进、int 出，没有 std::string。
//   ⚠ 失败一律 -1：NULL / 空指针进不来。intern 本身不会失败（池只增），
//     所以 -1 只有一个意思："调用方给的参数不对"，不是一个"池满了"的状态。
// ============================================================
int VM_InternText(const char* utf8) {
    if (!utf8) return -1;
    return (int)intern_str(utf8);
}

int VM_SidOf(const char* utf8) {
    if (!utf8) return -1;
    return (int)str_find(utf8);      // 只查不建 —— 引擎想"问一下有没有"时用它，别把池撑大
}
