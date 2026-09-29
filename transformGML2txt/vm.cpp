// ============================================================
// vm.cpp — 执行循环 + 装载 + 入口
//   声明都在 vm.h，游戏 API 在 vm_api.cpp，内部函数在 vm_native.cpp，
//   容器/二维表在 vm_container.cpp
// ============================================================

#include "vm.h"
#include <array>

// ============================================================
// 块表
// ============================================================
const Block* Vm::find_block_sid(int sid) const {
    std::unordered_map<int, Block>::const_iterator it = blocks.find(sid);
    return (it == blocks.end()) ? NULL : &it->second;
}

// 手上只有名字：先换池下标（不在池里 = 这个 VM 不可能有这个名字的块）


// 同 run_named，但池下标已经在手上（字节码 run_block 那条路）
bool Vm::run_named_sid(int sid) {
    int obj_sid = obj;                     // obj 本身就是池下标
    if (obj_sid >= 0 && run_hot_block_sid(*this, obj_sid, sid)) return true;
    const Block* b = find_block_sid(sid);
    if (!b) {
        set_error(VM_E_BLOCK_NOT_FOUND, sid);
        return false;
    }
    return run(*b);
}

// ============================================================
// 执行一个块
// ============================================================
bool Vm::run(const Block& blk) {
    const uint8_t* base = blk.code.data();
    const uint8_t* p    = base;
    const uint8_t* end  = base + blk.code.size();
    int* m = (int*)mem;

    while (p < end) {
        // 强制结束旗子（callbin 触上限时置上）：立刻收手，一层层 false 传回去
        if (g_call_abort) return false;
        switch (*p++) {

        case OP_ASSIGN: {
            int     dst = ld32(p); p += 4;
            uint8_t ty  = *p++;
            Value v; v.t = ty; v.v = 0;
            if (ty == MEM_STRING) {
                v.v = (int)ld16(p); p += 2;
            } else if (ty == MEM_FLOAT) {
                float f = ldf(p); p += 4;
                memcpy(&v.v, &f, 4);
            } else {
                v.v = ld32(p); p += 4;
            }
            wr_v(*this, dst, v);      // 走 wr_v 才会把 dst 里原来那个容器引用放掉
            break;
        }

        case OP_COPY: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            wr_v(*this, dst, rd_v(*this, src));   // 整 8 字节搬家 + 引用计数
            break;
        }

        case OP_ADD: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x + y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] + m[b * 2 + 1];
            }
            break;
        }

        case OP_SUB: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x - y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] - m[b * 2 + 1];
            }
            break;
        }

        case OP_MUL: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = x * y;
            } else {
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = m[a * 2 + 1] * m[b * 2 + 1];
            }
            break;
        }

        case OP_DIV: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            int ta = m[a * 2], tb = m[b * 2];
            if (ta == MEM_FLOAT || tb == MEM_FLOAT) {
                float x = (ta == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
                float y = (tb == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
                m[d * 2] = MEM_FLOAT;
                *(float*)(m + d * 2 + 1) = (y == 0.0f) ? 0.0f : (x / y);
            } else {
                int x = m[a * 2 + 1], y = m[b * 2 + 1];
                m[d * 2]     = MEM_INT;
                m[d * 2 + 1] = (y != 0) ? (x / y) : 0;   // 整型除法：向零截断
            }
            break;
        }

        case OP_MOD: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            // mod 只接整数（和 GML 一致）：任何一个操作数是浮点就当错误，停掉这个块
            if (m[a * 2] == MEM_FLOAT || m[b * 2] == MEM_FLOAT) {
                set_error(VM_E_NONINT_OPERAND, blk.name_sid);
                return false;
            }
            int x = m[a * 2 + 1], y = m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (y != 0) ? (x % y) : 0;
            break;
        }

        // 比的是槽里的值：数字比数值，字符串比字符串池下标
        case OP_EQ: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x == y) ? 1 : 0;
            break;
        }

        case OP_NEQ: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x != y) ? 1 : 0;
            break;
        }

        case OP_GT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x > y) ? 1 : 0;
            break;
        }

        case OP_GTE: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x >= y) ? 1 : 0;
            break;
        }

        case OP_LT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x < y) ? 1 : 0;
            break;
        }

        case OP_LTE: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            float x = (m[a * 2] == MEM_FLOAT) ? *(float*)(m + a * 2 + 1) : (float)m[a * 2 + 1];
            float y = (m[b * 2] == MEM_FLOAT) ? *(float*)(m + b * 2 + 1) : (float)m[b * 2 + 1];
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = (x <= y) ? 1 : 0;
            break;
        }

        case OP_CALL: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < GAME_FN_COUNT) kGameFns[fid](*this, args, argc, dst);
            else set_error(VM_E_UNKNOWN_GAME_FN, fid);
            break;
        }

        case OP_NATIVE: {
            int     fid  = (int)ld16(p); p += 2;
            int     argc = (int)*p++;
            int     dst  = ld32(p); p += 4;
            const uint8_t* args = p;
            p += argc * 4;
            if (fid >= 0 && fid < NATIVE_FN_COUNT) kNativeFns[fid](*this, args, argc, dst);
            else set_error(VM_E_UNKNOWN_NATIVE, fid);
            break;
        }

        case OP_IF: {
            int cond = ld32(p); p += 4;
            int tip  = ld32(p); p += 4;
            int fip  = ld32(p); p += 4;
            int ty = m[cond * 2];
            bool t;
            // GML 的真值：<= 0.5 为假、> 0.5 为真（手册 Boolean 一节）
            if (ty == MEM_INT)        t = (m[cond * 2 + 1] > 0);
            else if (ty == MEM_FLOAT) t = (*(float*)(m + cond * 2 + 1) > 0.5f);
            else                      t = false;    // 非数字：GML 的 is_real 为假 → 当 0
            int tgt = t ? tip : fip;
            if (tgt != -1) p = base + tgt;          // -1 = 不跳转，往下走
            break;
        }

        case OP_JMP:
            p = base + ld32(p);
            break;

        case OP_HALT:
            return true;

        // 取地址处的值：src 槽里存的是内存地址，把那个地址的 8 字节读到 dst
        case OP_LOAD_ELEM: {
            int dst = ld32(p); p += 4;
            int src = ld32(p); p += 4;
            int a   = m[src * 2 + 1];
            if (a >= 0 && a + 8 <= MEM_SIZE * 8) memcpy(m + dst * 2, mem + a, 8);
            break;
        }

        // ── 位运算：都在槽的「值」段上做，结果一律是 INT ──
        case OP_BAND: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] & m[b * 2 + 1];
            break;
        }

        case OP_BOR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] | m[b * 2 + 1];
            break;
        }

        case OP_BXOR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] ^ m[b * 2 + 1];
            break;
        }

        case OP_BNOT: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = ~m[a * 2 + 1];
            break;
        }

        // 移位量先按 32 位取模：槽是 int32，移 32 位以上在 C++ 里是未定义行为
        case OP_SHL: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] << (m[b * 2 + 1] & 31);
            break;
        }

        case OP_SHR: {
            int d = ld32(p); p += 4;
            int a = ld32(p); p += 4;
            int b = ld32(p); p += 4;
            m[d * 2]     = MEM_INT;
            m[d * 2 + 1] = m[a * 2 + 1] >> (m[b * 2 + 1] & 31);
            break;
        }
        default:
            set_error(VM_E_UNKNOWN_OPCODE, (int)*(p - 1), blk.name_sid);
            return false;
        }
    }
    return true;
}

// ============================================================
// 装载期：把 bin 自带的字符串池并进全局池，并把字节码里内联的字符串常量
//   改写成全局下标。
//   只有 OP_ASSIGN 会内联字符串下标（[dst i32][ty u8][idx u16]），别的指令只带槽号。
//   这里走一遍指令流：没正好走到块尾就说明指令布局和这份表对不上 → 报错返回 false。
//   宁可装载失败，也不能让错位的下标进池 —— 那是静默改语义。
// ============================================================
// opcode -> 操作数字节数（-1 = 变长/未知）—— 一次初始化，之后纯数组下标，无分支
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

int op_fixed_len(uint8_t op) { return kOpLen[op]; }

// ── VM 的生命周期（原来写在实例那个文件里 ✗ → 并回 VM 自己 ✓）──

void Vm::destroy_owned_instances() {
    std::vector<int> ids;
    box.ids(ids);                                 // inst_kill 会改 box，先拷一份
    for (size_t i = 0; i < ids.size(); i++) inst_kill(ids[i]);
    box.clear();
}

void Vm::unregister_objects() {
    std::unordered_map<int, Vm*>::iterator it = g_vm_of_object.begin();
    while (it != g_vm_of_object.end()) {
        if (it->second == this) it = g_vm_of_object.erase(it);
        else ++it;
    }
    // mod id → VM 那张表也要摘（同一个理由）
    std::unordered_map<int, Vm*>::iterator mit = g_mod_vms.begin();
    while (mit != g_mod_vms.end()) {
        if (mit->second == this) mit = g_mod_vms.erase(mit);
        else ++mit;
    }
}
