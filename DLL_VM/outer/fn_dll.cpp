// ============================================================
// fn_dll.cpp — "名字 → 实现" 的两张表 + 统一查找
//
//   ① DLL 函数表  g_dll_fns[名字池下标] = C++ 函数入口   ← 我们/插件 DLL 注册进来的
//   ② callbin 表  g_call_dict[名字池下标] = vm_callbin    ← 扫描 .callbin 得来的（内层管）
//
//   查找顺序：**先 DLL 表，再 callbin 表**，都没有 → 响亮失败（VM_E_FN_NOT_FOUND）。
//
//   为什么要这张表：native 里"没有 C++ 实现"的那些（转发 79 条 + 项目业务 72 条）
//   不用往 508 条表里塞实现 —— 表项的函数体只是"转一下"，真正的实现在这儿按**名字**挂。
//   于是给哪条写实现 = 注册一个同名函数，**表的顺序一个字不用动**（已编译的 bin 照旧）。
//
//   ⚠ 调用约定：注册进来的函数和表里的 native/game **完全同形**
//       void fn(Value* mem, const uint8_t* args, int argc, int dst)
//     实参就是槽号（args 是 int32 槽号表）、结果写 dst 槽 —— 所以命中时是**直通**：
//     同一份 mem、同一张 args 表，一次间接调用，零搬运。
//   ⚠ 跨 DLL 只走 POD（Value / int / uint8_t* / const char*）。别把 std::string / std::vector
//     放进注册签名里 —— CRT 分配器不是同一份就会踩坏堆。
// ============================================================
#include "outer.h"

// 接口版本：跨 DLL 的契约（Value 里 VM_INT/VM_FLOAT 的宽度、Fn 的形状）一改就得 +1，
//   否则老插件 DLL 进来会按错的布局解释槽 → 直接拒绝注册更安全。
static const int VM_FUNC_API_VERSION = 1;

static std::unordered_map<VM_INT, Fn> g_dll_fns;

Fn vm_find_dll_fn(VM_INT name_sid) {
    host_push_register_once();          // 内置的 VM_HostPush 在这儿兜底登记（POD 标志，幂等；理由见 host_push.cpp）
    std::unordered_map<VM_INT, Fn>::iterator it = g_dll_fns.find(name_sid);
    return (it == g_dll_fns.end()) ? NULL : it->second;
}

int VM_FuncApiVersion() { return VM_FUNC_API_VERSION; }

// 注册一个：名字 → 函数入口。返回 1 = 成功，0 = 参数不对
//   ⚠ 名字在这里 intern 一次（装载/注册期是唯一允许"文本 → 池下标"的地方），运行时零文本
int VM_RegisterFunc(const char* name, Fn fn) {
    if (!name || !*name || !fn) return 0;
    g_dll_fns[intern_str(name)] = fn;
    return 1;
}

int VM_RegisterFuncs(const VMFuncDesc* list, int count) {
    if (!list || count <= 0) return 0;
    int ok = 0;
    for (int i = 0; i < count; i++) ok += VM_RegisterFunc(list[i].name, list[i].fn);
    return ok;
}

// 注销：插件 DLL 卸载前必须调到（不然留下悬空的函数指针）
void VM_UnregisterFunc(const char* name) {
    if (!name || !*name) return;
    std::unordered_map<VM_INT, Fn>::iterator it = g_dll_fns.find(str_find(name));   // 只查不建
    if (it != g_dll_fns.end()) g_dll_fns.erase(it);
}

// ── 统一查找 + 调用 ──
//   DLL 表命中 → **直接调**（同一份 mem / args / dst，零搬运）
//   否则 → callbin 表（借公共内存的帧跑字节码；结果写 dst）
void vm_call_by_name(VM_INT name_sid, Value* mem, const uint8_t* args, int argc, int dst) {
    Fn f = vm_find_dll_fn(name_sid);
    if (f) { f(mem, args, argc, dst); return; }

    int n = (argc > CALL_MAX_ARGS) ? CALL_MAX_ARGS : argc;
    if (n < 0) n = 0;
    Value av[CALL_MAX_ARGS];
    for (int i = 0; i < n; i++) av[i] = arg_v(mem, args, i);

    Value r = callbin_call(name_sid, av, n);      // 没有就置 VM_E_FN_NOT_FOUND（响亮）
    wr_v(mem, dst, r);                            // 写进调用方的槽（wr_v 会 retain）
    release(r);                                   // 放掉转交过来的那一份
}
