// ============================================================
// call.cpp — 内层：函数调用机制（callbin）
//
//   callbin 没有自己的内存：每次调用在**公共内存**（g_call_mem）上借一帧，
//   帧布局（编译器保证）：槽 0 = 返回值，槽 1..argc = 实参，字面量 / 局部从 argc+1 起
//
//       frame = g_call_mem + g_call_off          （槽为单位）
//       g_call_off += 这一帧要的槽数              ← 进函数
//       ... 跑 _VM_CONST_INIT，再跑 _CALL_ENTRY ...
//       g_call_off -= 这一帧要的槽数              ← 出函数（栈式回收）
//
//   所以**每次调用都有自己的空间**（递归也各有一帧），公共内存就一片固定的：
//   不够 / 层数太深 → 置错误码 + 竖 g_call_abort（整条调用链立刻收手），返回 undefined。
// ============================================================
#include "vm_def.h"

static int g_call_depth = 0;      // 现在嵌了几层

Value callbin_call(VM_INT fn_sid, const Value* argv, int argn) {
    std::map<VM_INT, vm_callbin>::iterator it = g_call_dict.find(fn_sid);
    if (it == g_call_dict.end()) {
        vm_set_err(VM_E_FN_NOT_FOUND, fn_sid, 0);
        return mk_undef();
    }
    vm_callbin& cb = it->second;
    if (argn < 0 || argn > CALL_MAX_ARGS) {
        vm_set_err(VM_E_CALL_MEM, argn, CALL_MAX_ARGS);
        return mk_undef();
    }
    if (cb.value <= 0) { vm_set_err(VM_E_CALL_MEM, cb.value, 0); return mk_undef(); }

    if (g_call_mem.empty()) g_call_mem.assign(CALL_MEM_SLOTS, Value{0, 0});

    // 层数太深：C++ 栈会先炸，所以先闸住（跟"公共内存不够"一样：报错 + 整条链收手）
    if (g_call_depth >= CALL_MAX_DEPTH) {
        vm_set_err(VM_E_CALL_DEPTH, g_call_depth, 0);
        g_call_abort = true;
        return mk_undef();
    }
    if (g_call_off + (size_t)cb.value > CALL_MEM_SLOTS) {
        vm_set_err(VM_E_CALL_MEM, cb.value, (int)g_call_off);
        g_call_abort = true;
        return mk_undef();
    }

    Value* frame = g_call_mem.data() + g_call_off;
    memset(frame, 0, sizeof(Value) * (size_t)cb.value);   // 每帧从 0 开始，不继承上一次的残留
    g_call_off += (size_t)cb.value;                       // 进函数：偏移往前走
    g_call_depth++;

    // 槽 1..argc = 实参（写进去会 retain，所以是"帧存了一份"）
    for (int i = 0; i < argn; i++) slot_write(frame, 1 + i, argv[i]);

    // 压一层：实例沿用调用方的（callbin 的"当前实例"就是调用方那个），但**没有 vm_bin**
    //   ⚠ 所以 callbin 里调 run_block 是跑调用方那个 VM —— 它自己那块要等它有独立内存才行
    int sp0 = g_run_sp;
    run_push_frame(run_inst());

    if (!cb.init.code.empty()) VM_RUN(cb.init, frame);    // 字面量填进这一帧（帧复用，每帧都得填）
    if (!g_call_abort) VM_RUN(cb.block, frame);           // 函数体
    while (g_run_sp > sp0) run_pop();

    bool aborted = g_call_abort;
    Value r = aborted ? mk_undef() : frame[0];            // 槽 0 = 返回值
    if (!aborted) retain(r);                              // 返回值**转交**给调用方的那一份引用
                                                          //   （先拿到引用，再放帧）

    // 整帧的容器引用全放掉：实参、函数体里建的 array/map/grid/list、以及槽 0 那一份
    //   （不这么做：调用方每次调用都会漏掉这一帧里所有容器的引用）
    for (int s = 0; s < cb.value; s++) release(frame[s]);

    g_call_depth--;
    g_call_off -= (size_t)cb.value;                       // 出函数：偏移退回来
    if (g_call_depth == 0 && aborted) g_call_abort = false;   // 最外层收尾，把旗子放掉

    return aborted ? mk_undef() : r;                      // 这一份引用归调用方（写完槽要 release）
}
