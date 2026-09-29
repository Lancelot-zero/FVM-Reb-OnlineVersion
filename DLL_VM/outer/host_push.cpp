// ============================================================
// host_push.cpp — VM_HostPush：把一次调用的实参**追加**到出站共享内存
//
//   这是 VM → 宿主引擎那条通道上**唯一**一个给 VM 用的内置函数（host/host_shared.cpp）。
//   VM 侧只能"往后写"：复位 / 读游标 / 取首地址一律没暴露（那是宿主 / 引擎的事）——
//   所以这个函数**没有**对应的一堆兄弟，就它一个。
//
//   本文件**不定字节格式**，只做两层判断 + 搬运：
//     ① 这个实参该走哪一支？（int / float / string / buf / undefined）
//     ② 调 host 层那支 push（host_shm_push_i32 / _f32 / _bytes）
//   字节长什么样（几个字节、怎么排、长度字段多宽）**全在 host/host_shared.cpp 的
//   ★编码格式★ 那一块** —— 那里是唯一真源，改协议只改那儿。
//     为什么放 host：这是"宿主 ↔ 引擎"两端的约定；宿主侧也得能自己往里放数据，
//     所以能力本身（含编码）归 host，VM 只是"多一个说话的人"。
//
//   为什么不塞进 game_* 那几批 / 不动 508 条表：
//     native 表（358）和 game 表（150）的**顺序与条数是被编译期烘进旧 bin 的契约**，
//     加一条就得动号段。这里走的是"按名字调"那条路（transformGML2txt/gm2bin.py 的 CALL_BY_NAME
//     + 运行时的 VM_RegisterFunc），表里一个字都不用动。
//     路径：GML 里写 VM_HostPush(a, b, c)
//          → gm2bin.py 编成 OP_CALL VM_CallFunc("VM_HostPush", a, b, c)
//          → impl_VM_CallFunc → vm_call_by_name → 本文件的 VM_HostPush
//
//   ⚠ 跨 DLL 只走 POD：本函数只有 Value* / uint8_t* / int，没有 std::string 出现在签名里。
//   ⚠ 不 printf（外层一律不回控制台说话）：出错只 vm_set_err，查的人去日志里看。
// ============================================================
#include "outer.h"
#include "../host/host.h"
#include <vector>

namespace {

// ============================================================
// 【唯一的类型判定】哪些实参走 buf 那一支
//   "buf" = 宿主层那块 buffer（host_buffer.cpp，GML 的 buffer_create / buffer_load 出来的），
//   带上它是为了**把一段裸二进制原样递出去**（音频 / 贴图 / 存档块那种），
//   不用先变成 string（string 是 UTF-8 文本语义，装二进制会坏）。
//
//   ⚠⚠ 判定规则（有风险，调用方必须知道）⚠⚠
//     实参是 **int 类型、且这个值是一个"活着的"宿主 buffer id**（host_buf_alive）→ 走 buf；
//     **否则**（含"已经删掉的 buffer id"）按 int 走。
//     风险：**任何活着的 buffer id 都会被当 buf**，调用方别拿这种 id 当普通整数传
//     （传了就变成"整块内容被搬走"，而不是那 4 字节整数）。要传这种整数，就先把 buffer 删掉、
//     或者换个不撞 buffer id 的变量传。
//     这是格式的临时约定（用户在定），改判定只动这一个函数。
// ============================================================
bool arg_is_buf(const Value& v) {
    return v.t == MEM_INT && host_buf_alive((int)v.v);
}

// 写不下（16MB 到顶 / 理论上单条超 u32）→ 统一报错：置 VM_E_HOST_FULL + 回 -1
//   a1 = 本次**已经**写进去的字节数（前面几个实参已经是完整的，见 VM_HostPush 的说明）
//   a2 = 容量（上限）
void push_fail(Value* mem, int dst, long long written) {
    vm_set_err(VM_E_HOST_FULL, (int)written, host_shm_size());
    wr_i(mem, dst, -1);
}

}  // namespace

// ============================================================
// VM_HostPush(实参…) → 写入的字节数（int）
//   签名和表里 native/game **完全同形**（这样注册进 DLL 表后是直通，零搬运）
//   逐条实参挑一支 host_shm_push_* 调：
//     buf（活着的宿主 buffer id）→ 取那份内容 → host_shm_push_bytes
//     int                        → host_shm_push_i32
//     float                      → host_shm_push_f32
//     string                     → str_at(v.v) 的字节 → host_shm_push_bytes
//     undefined / 容器            → 哪支都不调（0 字节）
//   ⚠ 包里**没有类型字节**（见 host_shared.cpp 的 ★格式★）：两端靠事先约定的布局，
//     传错顺序 / 类型不会被发现 —— 这是设计前提，不是能"顺手校验"的地方
//   ⚠ 失败时：**已经写进去的那几条完整实参留在流里**（每支 push 自己原子，整次调用不是）——
//     满了本来就说明引擎没及时消费，流这时候已经不可信，引擎该 reset 之后重来。
//     所以这里只保证"不会留下**半条** payload"，不保证"整次调用一笔头"。
//   成功但全是 undefined → 回 0（**0 是合法返回值**，不是错误）
// ============================================================
void VM_HostPush(Value* mem, const uint8_t* args, int argc, int dst) {
    int n = (argc > 0) ? argc : 0;
    long long total = 0;                                // 本次已经写进去的字节数

    for (int k = 0; k < n; k++) {
        Value v = arg_v(mem, args, k);
        bool wrote = true;

        if (arg_is_buf(v)) {
            // buf：把整块内容取出来交出去。
            //   为什么先拷进临时缓冲：host_buffer 那边的池子只肯"按偏移 peek 一段"，
            //   不给内部指针（给了就等于把它的生命周期暴露出去）；这也保证了
            //   "内容取全了才 push" —— push 失败时不会留半条。
            int bn = host_buf_size((int)v.v);
            if (bn < 0) bn = 0;
            if (bn == 0) {
                wrote = host_shm_push_bytes(NULL, 0);    // 空 buf：只有 u32 长度 0
            } else {
                std::vector<uint8_t> tmp((size_t)bn);
                // 按偏移偷看，**不动**这块 buffer 自己的读写位置（那是调用方的游标）
                if (!host_buf_peek((int)v.v, 0, &tmp[0], bn)) wrote = false;
                else wrote = host_shm_push_bytes(&tmp[0], (uint32_t)bn);
            }
            if (wrote) total += 4 + (long long)bn;       // u32 长度 + 内容
        } else {
            switch (v.t) {
            case MEM_INT:
                wrote = host_shm_push_i32((int32_t)v.v);
                if (wrote) total += 4;
                break;
            case MEM_FLOAT:
                wrote = host_shm_push_f32(f_of(v.v));    // 槽的 v 段就是 f32 的位
                if (wrote) total += 4;
                break;
            case MEM_STRING: {
                const std::string& s = str_at(v.v);      // 池下标 → 文本（越界 → 空串）
                // 长度字段是 u32：64 位下真出现超 4GB 的串 → 表示不了，算失败（不截断、不谎报）
                if (s.size() > (size_t)0xFFFFFFFFu) { wrote = false; break; }
                wrote = host_shm_push_bytes(s.empty() ? NULL : s.data(), (uint32_t)s.size());
                if (wrote) total += 4 + (long long)s.size();
                break;
            }
            default:
                // undefined / 数组 / map / grid / list … → **一个字节都不写**
                //   （包里没有类型信息，这里"不写"就是"这段值不存在"的表示法）
                break;
            }
        }

        if (!wrote) { push_fail(mem, dst, total); return; }
    }

    wr_i(mem, dst, (int)total);                         // 返回值 = 这次写进去的字节数
}

// ============================================================
// 注册到"名字 → 实现"那张表（fn_dll.cpp 的 DLL 函数表）
//   ⚠ 为什么**不用**静态构造函数注册（fn_table.cpp 里 PatchGameFnsAtStart 那个套路）：
//     静态初始化期，跨编译单元的全局对象构造顺序**未定义**，而 VM_RegisterFunc 要 intern 名字
//     → 要碰 pool.cpp 的字符串池（std::vector / unordered_map）。顺序倒了就是踩没构造好的对象，
//     而且这种崩法只在换链接顺序时才现形，最难查。
//     所以这里只摆一个**零初始化的 POD 标志**，等"第一次按名字查找"时再登记 ——
//     那时静态初始化早就结束了（vm_find_dll_fn 开头会调它，outer_init() 也调一次）。
//   ⚠ 幂等：标志置位后就是一次 bool 判断，热路径上可以忽略。
// ============================================================
static bool g_host_push_registered = false;

void host_push_register_once() {
    if (g_host_push_registered) return;
    g_host_push_registered = true;
    VM_RegisterFunc("VM_HostPush", VM_HostPush);        // 名字在这儿 intern 一次（装载期唯一的文本入口）
}
