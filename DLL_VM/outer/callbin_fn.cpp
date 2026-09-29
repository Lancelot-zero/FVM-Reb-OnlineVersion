// ============================================================
// callbin_fn.cpp — callbin 的三个入口（游戏 API 里的那三个）
//
//   VM_CallFunc(函数名, 实参…) → 返回值   ← 走"名字 → 实现"的统一查找（fn_dll.cpp）：
//                                            先 DLL 函数表（命中直通），再 callbin 表
//   VM_FuncExists(函数名)      → 1/0
//   VM_FuncDesc(函数名)        → 说明文字（新结构体里没这个字段了 → 空串）
// ============================================================
#include "outer.h"

void impl_VM_CallFunc(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_v(mem, dst, mk_undef()); return; }
    Value f = arg_v(mem, args, 0);
    if (f.t != MEM_STRING || !str_ok(f.v)) { wr_v(mem, dst, mk_undef()); return; }

    // 实参表按槽号**平移一位**（跳过第 0 个"函数名"）——表在 C++ 栈上、指向的还是调用方那批槽，
    //   所以 DLL 函数命中时依然是直通（不搬值、不占临时槽）
    int n = argc - 1;
    if (n > CALL_MAX_ARGS) n = CALL_MAX_ARGS;
    int32_t tbl[CALL_MAX_ARGS];
    for (int i = 0; i < n; i++) tbl[i] = argslot(args, i + 1);

    vm_call_by_name(f.v, mem, (const uint8_t*)tbl, n, dst);
}

void impl_VM_FuncExists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    Value f = arg_v(mem, args, 0);
    if (f.t != MEM_STRING || !str_ok(f.v)) { wr_i(mem, dst, 0); return; }
    // 两张表任一有就算"存在"
    if (vm_find_dll_fn(f.v)) { wr_i(mem, dst, 1); return; }
    wr_i(mem, dst, g_call_dict.find(f.v) != g_call_dict.end() ? 1 : 0);
}

// 新结构体（vm_callbin）里没有 desc 字段 —— 说明文字目前没人存，先给空串
//   这个空串是**返回值**，不是"名字" → 不进 const_str.h 的名单；
//   池里也没有现成的空串条目（名单第 0 项 X(empty) 的文本是 "empty"，不是 ""），
//   所以本文件 intern 一次、缓存下标（intern_str 按文本去重，和每次现调等价）。
static VM_INT desc_empty_sid() {
    static VM_INT sid = -1;
    if (sid < 0) sid = intern_str("");
    return sid;
}

void impl_VM_FuncDesc(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_v(mem, dst, mk_str_sid(desc_empty_sid()));
}
