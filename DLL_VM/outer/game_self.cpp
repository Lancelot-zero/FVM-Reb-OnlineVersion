// ============================================================
// game_self.cpp — 名字带 VM_、其实**不用宿主**的那批（自举，46 条）
//
//   OP_CALL 那条路上的 VM_* 大多数要游戏引擎 / 平台（刷怪 / 卡片 / 绘制 / 音效…），
//   但这 46 条从头到尾只动 VM 自己的数据，所以先收编：
//     · 命名数组 9   VM_Array*          —— 一个 VM 一份（同对象的所有实例共用）
//     · 实例数组 8   VM_InstArray*      —— 落在实例身上；第一实参 0 = 全局那支
//     · 实例/属性 8  GetProp/SetProp/…  —— 实例表 + 属性表 + 命名数组
//     · 杂项 4       Random/Floor/Ceil/RunStep
//     · 全局读写 17  flame / wave / 卡片上限 / Ban* / 上一次事件…
//   在 patch_game_fns() 里把空壳换成 impl_*（见 fn_table.cpp），不碰宿主。
//
//   ⚠ 为什么看不见它们在 game_api.cpp 里：
//     game_api.cpp 只生成 150 条**空壳**（那个文件是名单驱动的，一条条手写不现实）；
//     已实现的都写成 impl_XXX 放在这儿（或 callbin_fn.cpp），
//     再由 fn_table.cpp 的 patch_game_fns() 在**启动时**把 kGameFns[] 里对应的表项换过来。
//     → 加新实现：写 impl_XXX 到本文件，再到 patch_game_fns() 挂一行即可。
//
//   语义真源：
//     · transformGML2txt/vm_api.cpp + vm_instance.cpp（老 C++，逐条照抄）
//     · 老 C++ 没实现的（VM_InstArray* / VM_RunStep）照
//       scripts/scr_command_VM/scr_command_VM.gml 的原版抄，只把落点从"GML 侧的真数组"
//       换成新内层的 Instance::arrays（inst_arr_*）和全局变量 + 容器池（inst = 0 那支）。
//
//   约定（和老代码一致，别改）：
//     · 实参就是**槽号**：arg_v / arg_i / arg_num 取值，不是"先读地址再取值"
//     · 字符串一律池下标：只判 str_ok，不取文本、不拼字符串、不比较文本
//     · 参数不对照老代码回 undefined / 0 / -1 / 空操作，**不置错误码**
//       （VM_E_NOT_IMPL 只留给真没实现的；这里乱置会盖掉真正的 g_vm_err）
//     · 往长驻位置写值（实例变量 / 全局 / 数组）的引用计数账由内层结；
//       只有自己往 std::vector 里塞 / 挪元素时才手动 retain / release
// ============================================================
#include "outer.h"
#include <cmath>
#include <cstdlib>      // rand
#include <set>
#include <vector>

namespace {

// 数字结果写槽：整数存 INT、否则 FLOAT（和 builtin.cpp 的 mk_num_v 一个规矩）
Value mk_num_v(VM_FLOAT x) {
    if (std::floor(x) == x && x >= (VM_FLOAT)-2147483647.0 && x <= (VM_FLOAT)2147483647.0)
        return mk_int((VM_INT)x);
    Value v; v.t = MEM_FLOAT; v.v = i_of(x);
    return v;
}
void wr_num(Value* mem, int dst, VM_FLOAT x) { wr_v(mem, dst, mk_num_v(x)); }

// 取第 k 个实参当"名字"→ 池下标；不是字符串 / 不在池里 → -1
VM_INT name_arg(Value* mem, const uint8_t* args, int k) {
    Value v = arg_v(mem, args, k);
    if (v.t != MEM_STRING || !str_ok(v.v)) return -1;
    return v.v;
}

// 第 k 个实参当整数（浮点截断；老代码 gnum_at 就是这个口径）
VM_INT num_arg(Value* mem, const uint8_t* args, int k) {
    return (VM_INT)arg_num(mem, args, k);
}

// ── 命名数组：当前 VM 那份（没有 → NULL；create=true 时没有就建）──
//   老代码存在 Vm::named_arrays 上，新内层挪到 g_named_arrays（key = vm_bin*），
//   所以"当前是哪个 VM"只能问调用栈（cur_vm）—— callbin 借内存的帧里没有 → 当没有
std::vector<Value>* narr(VM_INT sid, bool create) {
    vm_bin* vm = cur_vm();
    if (!vm || sid < 0) return NULL;
    return create ? vm_array_make(vm, sid) : vm_array_find(vm, sid);
}

// 元素比较：照老代码的 g_val_eq（int / float 混用也算相等）
//   ⚠ 和实例数组那边的"类型 + 值"严格比较**故意不一样**：两边各自照各自的老代码
bool val_eq(const Value& a, const Value& b) {
    if (a.t != b.t) {
        bool an = (a.t == MEM_INT || a.t == MEM_FLOAT), bn = (b.t == MEM_INT || b.t == MEM_FLOAT);
        if (!(an && bn)) return false;
        if (a.t == MEM_FLOAT) return (double)f_of(a.v) == (double)b.v;
        return (double)a.v == (double)f_of(b.v);
    }
    return a.v == b.v;
}

// ── VM_InstArray* 的第一个实参：整数 0 = 全局那支（名字当全局变量名，GML 的约定）；
//    其余整数 = 实例 id；字符串 = 对象名（取名下第 0 个，和 inst_fn.cpp 一个口径）──
Instance* ia_inst(Value* mem, const uint8_t* args, int* is_global) {
    *is_global = 0;
    Value v = arg_v(mem, args, 0);
    if (v.t == MEM_INT && v.v == 0) { *is_global = 1; return NULL; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    return inst_get(inst_pick(id, obj, kind));
}

// 全局那支的数组：global.<name> 里必须是个**活着的数组句柄**；
//   create=true 时没有 / 不是数组 → 新建一个空数组存进去（GML 就是 `if (!is_array(_g)) _g = []`）
std::vector<Value>* ia_global(VM_INT sid, bool create) {
    Value* g = global_var_sid(sid);
    if (g && g->t == MEM_ARRAY && container_alive(*g)) return &g_arr_pool[(size_t)g->v];
    if (!create) return NULL;
    Value self{MEM_ARRAY, arr_new()};
    global_var_set_sid(sid, self);       // 全局表接手一份（它自己 retain）
    release(self);                       // 创造者那份交出去
    return &g_arr_pool[(size_t)self.v];
}

}  // namespace

// ============================================================
// 命名数组（9 条）—— 一个 VM 一份：同一个对象的所有实例看到的是同一份
// ============================================================
// VM_ArrayGet(name, i)：越界 / 没有这个数组 → undefined
void impl_VM_ArrayGet(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(mem, dst, mk_undef()); return; }
    std::vector<Value>* p = narr(name_arg(mem, args, 0), false);
    VM_INT i = num_arg(mem, args, 1);
    if (!p || i < 0 || i >= (VM_INT)p->size()) { wr_v(mem, dst, mk_undef()); return; }
    wr_v(mem, dst, (*p)[(size_t)i]);         // 写槽时 wr_v 会 retain
}

// VM_ArraySet(name, i, v)：没有就建；越界先补 0 到 i（老代码就这么写）
void impl_VM_ArraySet(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    std::vector<Value>* p = narr(name_arg(mem, args, 0), true);
    if (!p) return;
    VM_INT i = num_arg(mem, args, 1);
    if (i < 0) return;
    Value nv = arg_v(mem, args, 2);
    if (i >= (VM_INT)p->size()) p->resize((size_t)i + 1, mk_int(0));   // 0 不是容器，不用 retain
    Value ov = (*p)[(size_t)i];
    retain(nv);                              // 先拿新的
    if (ov.t != nv.t || ov.v != nv.v) release(ov);   // 再放旧的
    (*p)[(size_t)i] = nv;
}

// VM_ArrayDel(name, i)：删掉第 i 个（元素后面往前挪），越界空操作
void impl_VM_ArrayDel(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    std::vector<Value>* p = narr(name_arg(mem, args, 0), false);
    if (!p) return;
    VM_INT i = num_arg(mem, args, 1);
    if (i < 0 || i >= (VM_INT)p->size()) return;
    release((*p)[(size_t)i]);                // 这个元素不再被数组引用
    p->erase(p->begin() + i);
}

// VM_ArrayADD(name, v)：尾部追一个；没有这个数组就建
void impl_VM_ArrayADD(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 2) return;
    std::vector<Value>* p = narr(name_arg(mem, args, 0), true);
    if (!p) return;
    Value nv = arg_v(mem, args, 1);
    retain(nv);
    p->push_back(nv);
}

void impl_VM_ArraySize(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    std::vector<Value>* p = narr(name_arg(mem, args, 0), false);
    wr_i(mem, dst, p ? (VM_INT)p->size() : 0);
}

// VM_ArrayClear(name)：数组还在、清成长度 0（和 ArrayClearAll 不是一回事）
void impl_VM_ArrayClear(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    std::vector<Value>* p = narr(name_arg(mem, args, 0), false);
    if (!p) return;
    for (size_t i = 0; i < p->size(); i++) release((*p)[i]);
    p->clear();
}

// VM_ArrayClearAll()：把**这个 VM** 的命名数组全删掉（收尾用）
//   老代码是 vm.named_arrays.clear()（那时候没引用计数），
//   新内层有 vm_array_clear_all：它会先放掉每个元素，不会漏引用
void impl_VM_ArrayClearAll(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)mem; (void)args; (void)argc; (void)dst;   // 老代码也不写 dst（没人接返回值）
    vm_bin* vm = cur_vm();
    if (vm) vm_array_clear_all(vm);
}

void impl_VM_ArrayExists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, narr(name_arg(mem, args, 0), false) ? 1 : 0);
}

// VM_ArrayContains(name, v)：没有这个数组 → 0；比较口径见 val_eq
void impl_VM_ArrayContains(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, 0); return; }
    std::vector<Value>* p = narr(name_arg(mem, args, 0), false);
    if (!p) { wr_i(mem, dst, 0); return; }
    Value key = arg_v(mem, args, 1);
    for (size_t i = 0; i < p->size(); i++)
        if (val_eq((*p)[i], key)) { wr_i(mem, dst, 1); return; }
    wr_i(mem, dst, 0);
}

// ============================================================
// 实例数组（8 条）—— 落在 Instance::arrays（inst_arr_*）
//   第一实参 0 = 全局那支：名字当全局变量名，数组存在 global.<name> 里（容器池的 MEM_ARRAY）
//   返回值照原版 GML：Exists 0/1，Size/Item/Set/Add/Del/Contains 失败 -1、成功 1
// ============================================================

// VM_InstArrayExists(inst, name)：有且**是数组** → 1
void impl_VM_InstArrayExists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, 0); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, 0); return; }
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        Value* g = global_var_sid(sid);
        wr_i(mem, dst, (g && g->t == MEM_ARRAY && container_alive(*g)) ? 1 : 0);
        return;
    }
    wr_i(mem, dst, (in && inst_arr_exists(*in, sid)) ? 1 : 0);
}

// VM_InstArraySize(inst, name)：实例/变量不存在或不是数组 → -1
void impl_VM_InstArraySize(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, false);
        wr_i(mem, dst, p ? (VM_INT)p->size() : -1);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, inst_arr_size(*in, sid));
}

// VM_InstArrayItem(inst, name, k)：越界 / 不是数组 → -1
//   ⚠ 先查长度再取值：元素本身可能就是 undefined，拿 undefined 当"越界"会认错
void impl_VM_InstArrayItem(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    VM_INT k = num_arg(mem, args, 2);
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, false);
        if (!p || k < 0 || k >= (VM_INT)p->size()) { wr_i(mem, dst, -1); return; }
        wr_v(mem, dst, (*p)[(size_t)k]);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    VM_INT n = inst_arr_size(*in, sid);
    if (k < 0 || k >= n) { wr_i(mem, dst, -1); return; }
    wr_v(mem, dst, inst_arr_item(*in, sid, k));
}

// VM_InstArraySet(inst, name, k, v)：
//   全局那支：没有 / 不是数组就新建，越界变长（GML 的 array_resize）
//   实例那支：**不自动建、不自动扩容**，变量不存在或不是数组或 k 越界 → -1
void impl_VM_InstArraySet(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 4) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    VM_INT k = num_arg(mem, args, 2);
    if (k < 0) { wr_i(mem, dst, -1); return; }
    Value nv = arg_v(mem, args, 3);
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, true);
        if (k >= (VM_INT)p->size()) p->resize((size_t)k + 1, mk_int(0));
        Value ov = (*p)[(size_t)k];
        retain(nv);
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        (*p)[(size_t)k] = nv;
        wr_i(mem, dst, 1);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    if (k >= inst_arr_size(*in, sid)) { wr_i(mem, dst, -1); return; }
    inst_arr_set(*in, sid, k, nv);
    wr_i(mem, dst, 1);
}

// VM_InstArrayAdd(inst, name, v)：
//   全局那支：没有就建，然后追加；实例那支：不自动建数组（-1）
void impl_VM_InstArrayAdd(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    Value nv = arg_v(mem, args, 2);
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, true);
        retain(nv);
        p->push_back(nv);
        wr_i(mem, dst, 1);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    inst_arr_add(*in, sid, nv);
    wr_i(mem, dst, 1);
}

// VM_InstArrayDel(inst, name, k)：越界 / 不是数组 → -1
void impl_VM_InstArrayDel(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    VM_INT k = num_arg(mem, args, 2);
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, false);
        if (!p || k < 0 || k >= (VM_INT)p->size()) { wr_i(mem, dst, -1); return; }
        release((*p)[(size_t)k]);
        p->erase(p->begin() + k);
        wr_i(mem, dst, 1);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    if (k < 0 || k >= inst_arr_size(*in, sid)) { wr_i(mem, dst, -1); return; }
    inst_arr_delete(*in, sid, k, 1);
    wr_i(mem, dst, 1);
}

// VM_InstArrayClear(inst, name)：清成 []
//   ⚠ 和原版**故意不一样**：原版是"变量不存在 / 不是数组 → -1"，
//     但编译器把 `x = []` 编成"先 Clear 再逐个 Add"，所以我们按"不存在就当建一个空数组"来写
//     （老代码头的约定，见 transformGML2txt/vm_api.cpp 顶部那段）
void impl_VM_InstArrayClear(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, true);
        for (size_t i = 0; i < p->size(); i++) release((*p)[i]);
        p->clear();
        wr_i(mem, dst, 1);
        return;
    }
    if (!in) { wr_i(mem, dst, -1); return; }
    // ⚠ 这里必须用 inst_arr_create（不存在就建一个空的）：inst_arr_clear 对不存在的名字
    //   是"直接返回"，建不出数组来，而 `x = []` 后面还要靠 Exists / Add 接着走
    inst_arr_create(*in, sid, 0, mk_int(0));
    wr_i(mem, dst, 1);
}

// VM_InstArrayContains(inst, name, v)：有 1 / 没有 0 / 异常 -1
//   比较按"类型 + 值"（和 inst_arr_index 一致；1 和 "1" 不是一回事）
void impl_VM_InstArrayContains(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    Value key = arg_v(mem, args, 2);
    int is_global = 0;
    Instance* in = ia_inst(mem, args, &is_global);
    if (is_global) {
        std::vector<Value>* p = ia_global(sid, false);
        if (!p) { wr_i(mem, dst, -1); return; }
        for (size_t i = 0; i < p->size(); i++)
            if ((*p)[i].t == key.t && (*p)[i].v == key.v) { wr_i(mem, dst, 1); return; }
        wr_i(mem, dst, 0);
        return;
    }
    if (!in || !inst_arr_exists(*in, sid)) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, (inst_arr_index(*in, sid, key) >= 0) ? 1 : 0);
}

// ============================================================
// 实例 / 属性（8 条）
// ============================================================
// 目标解析（照老代码 prop_target）：第 0 个实参**必须是整数**
//   · 0 = 全局（0 永远不是真实例 id，所以拿它当"读 global.<name>"的哨兵）
//   · 其余 = 实例 id（负数 / 已死 → 查不到 → undefined / 空操作）
//   老 C++ 只认整数这一条路，这里保持一致（对象名请走 instance_* 那批 native）
void impl_VM_GetProp(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(mem, dst, mk_undef()); return; }
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) { wr_v(mem, dst, mk_undef()); return; }
    Value iv = arg_v(mem, args, 0);
    if (iv.t != MEM_INT) { wr_v(mem, dst, mk_undef()); return; }
    if (iv.v == 0) {
        Value* p = global_var_sid(sid);          // 读不到 → undefined
        wr_v(mem, dst, p ? *p : mk_undef());
        return;
    }
    Instance* in = inst_get(iv.v);
    if (!in) { wr_v(mem, dst, mk_undef()); return; }
    Value* p = inst_prop(*in, sid);              // 内置字段 / vars；没设过 → NULL
    wr_v(mem, dst, p ? *p : mk_undef());
}

// VM_SetProp(inst, name, v)：没有就建；实例不存在 → 静默（老代码一致，不报错）
void impl_VM_SetProp(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    VM_INT sid = name_arg(mem, args, 1);
    if (sid < 0) return;
    Value iv = arg_v(mem, args, 0);
    if (iv.t != MEM_INT) return;
    Value nv = arg_v(mem, args, 2);
    if (iv.v == 0) { global_var_set_sid(sid, nv); return; }
    Instance* in = inst_get(iv.v);
    if (!in) return;
    inst_prop_set(*in, sid, nv);       // 账由内层结（retain 新的 / release 旧的）
}

// VM_IsUndefined(v)：1 = 值是 undefined，0 = 不是（没给实参 = 拿一个 undefined 来问）
void impl_VM_IsUndefined(Value* mem, const uint8_t* args, int argc, int dst) {
    Value v = (argc > 0) ? arg_v(mem, args, 0) : mk_undef();
    wr_i(mem, dst, (v.t == MEM_UNDEFINED) ? 1 : 0);
}

// VM_IsDestroyed(id)：查得到 = 0，查不到 = 1
//   （老代码对"不是整数"的实参也照读 .v，新代码 likewise：id 拿不到就是 0 → 查不到 → 1）
void impl_VM_IsDestroyed(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT id = (argc > 0) ? num_arg(mem, args, 0) : 0;
    wr_i(mem, dst, inst_get(id) ? 0 : 1);
}

// VM_GetInstanceCount("obj_x")：这个对象还活着几个
void impl_VM_GetInstanceCount(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, obj_count(sid));
}

// VM_GetInstanceAt("obj_x", k)：第 k 个活实例 id（没有 / 越界 → -1）
void impl_VM_GetInstanceAt(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, -1); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, -1); return; }
    wr_i(mem, dst, inst_at(sid, num_arg(mem, args, 1)));
}

// VM_CreateInstance("obj_x" [, x, y, depth])：建实例 → 返回 id；对象名不是字符串 → noone
//   ⚠ 挂在**对象自己的 VM** 上（不是调用方 VM）：内层 inst_kill 是按 g_vm_of_object[obj]
//     把实例摘掉的，挂错地方个数算不准、槽位永远摘不掉（见 inst_fn.cpp 同一处的说明）；
//     对象还没有 VM 才退回调用方 VM。老代码一律挂调用方，那是它当时唯一的选择。
//   ⚠ 老 C++ 不跑 _OBJECT_CREATE（GML 版是经 instance_create_depth 走的、会跑）。
//     这里照老 C++，**没跑** Create —— 语义差异记在文件尾。
void impl_VM_CreateInstance(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, NOONE); return; }
    VM_INT sid = name_arg(mem, args, 0);
    if (sid < 0) { wr_i(mem, dst, NOONE); return; }
    vm_bin* owner = vm_of_object_sid(sid);
    if (!owner) owner = cur_vm();
    if (!owner) { wr_i(mem, dst, NOONE); return; }

    VM_INT nid = inst_new(*owner, sid);
    if (argc > 1) inst_var_set_sid(nid, S_x,     arg_v(mem, args, 1));
    if (argc > 2) inst_var_set_sid(nid, S_y,     arg_v(mem, args, 2));
    if (argc > 3) inst_var_set_sid(nid, S_depth, arg_v(mem, args, 3));
    wr_i(mem, dst, nid);
}

// VM_DestroyInstance(inst)：走销毁唯一入口（含 Destroy 事件）；id < 0 → 空操作
void impl_VM_DestroyInstance(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 1) return;
    VM_INT id = num_arg(mem, args, 0);
    if (id >= 0) inst_destroy(id);
}

// ============================================================
// 杂项（4 条）
// ============================================================
void impl_VM_Random(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_num(mem, dst, ((VM_FLOAT)rand() / (VM_FLOAT)RAND_MAX) * arg_num(mem, args, 0));
}

void impl_VM_Floor(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_num(mem, dst, std::floor(arg_num(mem, args, 0)));
}

void impl_VM_Ceil(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    wr_num(mem, dst, std::ceil(arg_num(mem, args, 0)));
}

// 正在被"额外跑 Step"的实例：同一个实例重入 → 直接回 0（GML 版的 global._VM_step_running）
static std::set<VM_INT> g_step_running;

// VM_RunStep(inst [, times])：让这个实例额外跑 n 轮 Step（做加速），回实际跑了几轮
//   一轮 = _OBJECT_STEP + _OBJECT_STEP_END。
//   ⚠ GML 版写的是 Begin Step + Step + End Step，但**这份 VM 的块名映射里没有 Begin Step**：
//     翻译器把 Step_0.gml 映成 _OBJECT_STEP、Step_2.gml 映成 _OBJECT_STEP_END，
//     常量清单里也没有 _OBJECT_BEGIN_STEP —— 不自己 intern 一个没人编出来的块名。
//   ⚠ 对象没装载 / 那个事件没写：GML 的 event_perform 也算一轮，这里同样计数（只是没块可跑）。
//   ⚠ 防递归：重跑期间实例再调自己 → 0（脚本写 VM_RunStep(self,3) 的净效果 = 本帧跑 4 轮）
void impl_VM_RunStep(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    VM_INT target = inst_pick(id, obj, kind);
    Instance* in = inst_get(target);
    if (!in) { wr_i(mem, dst, 0); return; }
    if (g_step_running.find(target) != g_step_running.end()) { wr_i(mem, dst, 0); return; }

    VM_INT times = (argc > 1) ? num_arg(mem, args, 1) : 1;
    if (times < 1)  times = 1;                 // 没给 / 给 0 / 给负数 → 1 轮
    if (times > 60) times = 60;                // 上限 60（别让一帧里炸掉）

    // ⚠ 先把对象名抄下来：Step 里把实例销毁了，槽位会被下一个实例复用时 in 已经指向别人
    VM_INT obj_sid = in->obj;
    vm_bin* owner = vm_of_object_sid(obj_sid);
    if (!owner) { wr_i(mem, dst, 0); return; }

    g_step_running.insert(target);             // 跑完一定要擦掉（下面没有 return 路径）
    VM_INT ran = 0;
    for (VM_INT k = 0; k < times; k++) {
        if (!inst_alive(target)) break;
        run_block_on(*owner, obj_sid, target, S__OBJECT_STEP);
        if (!inst_alive(target)) break;
        run_block_on(*owner, obj_sid, target, S__OBJECT_STEP_END);
        ran++;
    }
    g_step_running.erase(target);
    wr_i(mem, dst, ran);
}

// ============================================================
// 全局读写（17 条）—— 名字都在常量清单里，key 就是池下标（和 variable_global_* 同一张表）
// ============================================================
// 读全局：没有 → undefined（老代码 g_get_g）
static void g_get(Value* mem, int dst, VM_INT sid) {
    Value* p = global_var_sid(sid);
    wr_v(mem, dst, p ? *p : mk_undef());
}
// 写全局：**没给实参就写 1**（老代码 g_set_g 的规矩，Set* 那几条当"开关"用时就是这么调的）
static void g_set(Value* mem, const uint8_t* args, int argc, VM_INT sid) {
    if (argc < 1) { global_var_set_sid(sid, mk_int(1)); return; }
    global_var_set_sid(sid, arg_v(mem, args, 0));
}

void impl_VM_GetFlame(Value* mem, const uint8_t* a, int c, int d)             { (void)a; (void)c; g_get(mem, d, S_flame); }
void impl_VM_GetWave(Value* mem, const uint8_t* a, int c, int d)              { (void)a; (void)c; g_get(mem, d, S__VM_wave); }
void impl_VM_SetFlame(Value* mem, const uint8_t* a, int c, int d)             { (void)d; g_set(mem, a, c, S_flame); }
void impl_VM_SetWave(Value* mem, const uint8_t* a, int c, int d)              { (void)d; g_set(mem, a, c, S__VM_wave); }
void impl_VM_SetShovelFlameRate(Value* mem, const uint8_t* a, int c, int d)   { (void)d; g_set(mem, a, c, S__VM_shovel_flame_rate); }
void impl_VM_SetCardLevelCap(Value* mem, const uint8_t* a, int c, int d)      { (void)d; g_set(mem, a, c, S__VM_card_level_cap); }
void impl_VM_SetCardShapeCap(Value* mem, const uint8_t* a, int c, int d)      { (void)d; g_set(mem, a, c, S__VM_card_shape_cap); }
void impl_VM_SetCardSkillCap(Value* mem, const uint8_t* a, int c, int d)      { (void)d; g_set(mem, a, c, S__VM_card_skill_cap); }
void impl_VM_BanWeapon(Value* mem, const uint8_t* a, int c, int d)            { (void)d; g_set(mem, a, c, S__VM_ban_weapon); }
void impl_VM_BanSuperWeapon(Value* mem, const uint8_t* a, int c, int d)       { (void)d; g_set(mem, a, c, S__VM_ban_super_weapon); }
void impl_VM_BanShield(Value* mem, const uint8_t* a, int c, int d)            { (void)d; g_set(mem, a, c, S__VM_ban_shield); }
void impl_VM_GetCurCard(Value* mem, const uint8_t* a, int c, int d)           { (void)a; (void)c; g_get(mem, d, S__VM_cur_card); }
void impl_VM_GetLastCreatedCard(Value* mem, const uint8_t* a, int c, int d)   { (void)a; (void)c; g_get(mem, d, S__VM_last_created_card); }
void impl_VM_GetLastDestroyedCard(Value* mem, const uint8_t* a, int c, int d) { (void)a; (void)c; g_get(mem, d, S__VM_last_destroyed_card); }
void impl_VM_GetLastCreatedEnemy(Value* mem, const uint8_t* a, int c, int d)  { (void)a; (void)c; g_get(mem, d, S__VM_last_created_enemy); }
void impl_VM_GetLastKilledEnemy(Value* mem, const uint8_t* a, int c, int d)   { (void)a; (void)c; g_get(mem, d, S__VM_last_killed_enemy); }
void impl_VM_GetLastClickedButton(Value* mem, const uint8_t* a, int c, int d) { (void)a; (void)c; g_get(mem, d, S__VM_last_clicked_button); }
