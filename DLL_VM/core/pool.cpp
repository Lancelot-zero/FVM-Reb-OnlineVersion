// ============================================================
// pool.cpp — 内层：全局池（都在这个文件里）
//
//   这些**不属于任何 VM**，所有 VM 共用一份：
//     字符串池       g_strings / g_str_map
//     全局变量表     g_globals              （GML 的 global.x）
//     实例表         g_instances / g_inst_free / g_inst_alive
//     对象名 → VM    g_vm_of_object
//     mod id → VM    g_mod_vms
//     callbin        g_call_dict / g_call_mem
//     容器池         g_arr_pool / g_map_pool / g_grid_pool / g_list_pool
//
//   实例的**操作**在 instance.cpp；这里只放表本身。
//   内层零文本：这里没有 printf，出问题只把状态改坏成"安全"的样子。
// ============================================================
#include "vm_def.h"
#include <cstdio>       // snprintf：往**缓冲区**里格式化数字（不是往控制台打；内层照样零文本输出）

// ============================================================
// 字符串池：文本只在这儿；Value.t == MEM_STRING 时 v 就是这里的下标
//   池**只增不减**（卸载一个 VM 不回收它带来的字符串，可能还有别人在用）
//   ⚠ 单线程：只在装载期和 intern 时写，平时只读
// ============================================================
std::vector<std::string>                 g_strings;
std::unordered_map<std::string, VM_INT>  g_str_map;

static const std::string kEmptyString;

// 预置段灌过没有。必须挡在**任何** intern 之前：
//   函数名要占住固定下标区间，晚一步（先合并了某个 bin 的池）下标就整体错位、S_xxx 全废。
static bool g_pool_seeded = false;

void pool_seed() {
    if (g_pool_seeded) return;
    g_pool_seeded = true;          // 先置位：下面 intern_str 会看到，不再递归回来
    seed_names();                  // const_str.cpp：把 S_xxx 那批按顺序灌进来
}

bool str_ok(VM_INT i) { return i >= 0 && i < (VM_INT)g_strings.size(); }

const std::string& str_at(VM_INT i) {
    return str_ok(i) ? g_strings[(size_t)i] : kEmptyString;
}

// 「名字 → 池下标」的唯一入口（只查不建）：没有 → -1
VM_INT str_find(const std::string& s) {
    std::unordered_map<std::string, VM_INT>::iterator it = g_str_map.find(s);
    return (it == g_str_map.end()) ? -1 : it->second;
}

VM_INT intern_str(const std::string& s) {
    if (!g_pool_seeded) pool_seed();               // 兜底：谁先碰池谁负责把预置段灌上
    VM_INT id = str_find(s);
    if (id >= 0) return id;
    g_strings.push_back(s);
    id = (VM_INT)g_strings.size() - 1;
    g_str_map[s] = id;
    return id;
}

// ============================================================
// 值 → GML 文本：`string(x)` 的**唯一**实现
//   格式不是这里发明的，照的是老实现 transformGML2txt/vm_builtin.cpp 的 string()
//   （本仓库 outer/builtin.cpp 原来那份和它逐行一样，现在两边都并到这里）：
//     整数      → 十进制（"%lld"，槽是 int32，等价于老的 "%d"）
//     浮点      → **两位小数**（"%.2f"：1.5 → "1.50"、2.0 → "2.00"）
//     字符串    → 池里的原文本（不是池下标）
//     undefined → 名字段里的文本（S_undefined = "undefined"）
//     容器      → 占位名 "[array]" / "[struct]" / "[grid]" / "[list]"
//   ⚠ 为什么必须只有一份：内层 OP_ADD 拼串和外层 string() 要是各写一套，
//     `"a" + x` 和 `"a" + string(x)` 迟早对不上（这次就是来堵这个口子的）。
//   ⚠ 返回的是**副本**（std::string 按值）。调用方拿到副本再 intern 是安全的：
//     intern_str 会让 g_strings 扩容，池里元素的**引用**会悬空，副本不会。
// ============================================================
std::string value_to_text(const Value& v) {
    if (v.t == MEM_STRING) return str_ok(v.v) ? str_at(v.v) : std::string();
    char buf[64];
    if (v.t == MEM_FLOAT)          snprintf(buf, sizeof(buf), "%.2f", (double)f_of(v.v));
    else if (v.t == MEM_UNDEFINED) snprintf(buf, sizeof(buf), "%s", str_at(S_undefined).c_str());
    else if (v.t == MEM_ARRAY)     snprintf(buf, sizeof(buf), "[array]");
    else if (v.t == MEM_MAP)       snprintf(buf, sizeof(buf), "[struct]");
    else if (v.t == MEM_GRID)      snprintf(buf, sizeof(buf), "[grid]");
    else if (v.t == MEM_LIST)      snprintf(buf, sizeof(buf), "[list]");
    else                           snprintf(buf, sizeof(buf), "%lld", (long long)v.v);
    return std::string(buf);
}

// ============================================================
// 全局变量表（GML 的 global.x：进程级，VM 挂掉也不清）
// ============================================================
std::unordered_map<VM_INT, Value> g_globals;

Value* global_var_sid(VM_INT sid) {
    std::unordered_map<VM_INT, Value>::iterator it = g_globals.find(sid);
    return (it == g_globals.end()) ? NULL : &it->second;
}

// 写全局 = 存进一个**长驻位置**（进程级、没人来收）→ 引用计数的账在这里结
//   （和 slot_write / inst_prop_set 一个规矩：先 retain 新的，再 release 旧的）
void global_var_set_sid(VM_INT sid, Value v) {
    std::unordered_map<VM_INT, Value>::iterator it = g_globals.find(sid);
    if (it == g_globals.end()) { retain(v); g_globals[sid] = v; return; }
    if (it->second.t == v.t && it->second.v == v.v) return;
    retain(v);
    release(it->second);
    it->second = v;
}

bool global_exists_sid(VM_INT sid) { return g_globals.find(sid) != g_globals.end(); }

// ============================================================
// 实例表
//   槽位（在 g_instances 里的位置）可复用；实例 id 只增不减 —— 两个东西
//   0 号槽留位：0 = "全局 / 当前实例"的约定值，不作实例
// ============================================================
std::vector<Instance>          g_instances;
std::unordered_map<VM_INT, int> g_inst_alive;   // id → 槽位（唯一权威）
std::vector<int>               g_inst_free;     // 回收的槽位，等着复用
VM_INT                         g_next_id = 0;   // 下一个实例 id（只增不减）

// ============================================================
// 对象名 → VM / mod id → VM
//   一个对象的块只装在它自己的 VM 里；跨对象派发就靠这张表找到"那个对象归谁管"
// ============================================================
std::unordered_map<VM_INT, vm_bin*> g_vm_of_object;   // 对象名池下标 → VM
std::unordered_map<VM_INT, vm_bin*> g_mod_vms;        // mod id 池下标 → VM

vm_bin* vm_of_object_sid(VM_INT sid) {
    if (sid < 0 || !str_ok(sid)) return NULL;
    std::unordered_map<VM_INT, vm_bin*>::iterator it = g_vm_of_object.find(sid);
    return (it == g_vm_of_object.end()) ? NULL : it->second;      // 零哈希
}

vm_bin* mod_vm_sid(VM_INT sid) {
    if (sid < 0 || !str_ok(sid)) return NULL;
    std::unordered_map<VM_INT, vm_bin*>::iterator it = g_mod_vms.find(sid);
    return (it == g_mod_vms.end()) ? NULL : it->second;           // 零哈希
}

// ============================================================
// callbin：函数名池下标 → callbin；公共内存一片，函数调函数就在上面加偏移
// ============================================================
std::map<VM_INT, vm_callbin> g_call_dict;
std::vector<Value>           g_call_mem;      // 首次调用时才分配（1 MB / 8）
size_t                       g_call_off = 0;  // 当前偏移（槽）
bool                         g_call_abort = false;

// ============================================================
// 命名数组（VM_Array* 用）：每个 VM 一份（= 同类型对象的所有实例共用）
//   元素是槽里的值 → 删掉 / 清空时要 release（和实例数组一个规矩）
// ============================================================
std::unordered_map<vm_bin*, std::unordered_map<VM_INT, std::vector<Value>>> g_named_arrays;

std::vector<Value>* vm_array_find(vm_bin* vm, VM_INT name_sid) {
    if (!vm) return NULL;
    std::unordered_map<vm_bin*, std::unordered_map<VM_INT, std::vector<Value>>>::iterator it
        = g_named_arrays.find(vm);
    if (it == g_named_arrays.end()) return NULL;
    std::unordered_map<VM_INT, std::vector<Value>>::iterator a = it->second.find(name_sid);
    return (a == it->second.end()) ? NULL : &a->second;
}

std::vector<Value>* vm_array_make(vm_bin* vm, VM_INT name_sid) {
    if (!vm) return NULL;
    return &g_named_arrays[vm][name_sid];
}

void vm_array_drop(vm_bin* vm, VM_INT name_sid) {
    if (!vm) return;
    std::unordered_map<vm_bin*, std::unordered_map<VM_INT, std::vector<Value>>>::iterator it
        = g_named_arrays.find(vm);
    if (it == g_named_arrays.end()) return;
    std::unordered_map<VM_INT, std::vector<Value>>::iterator a = it->second.find(name_sid);
    if (a == it->second.end()) return;
    for (size_t i = 0; i < a->second.size(); i++) release(a->second[i]);
    it->second.erase(a);
    if (it->second.empty()) g_named_arrays.erase(it);
}

void vm_array_clear_all(vm_bin* vm) {
    if (!vm) return;
    std::unordered_map<vm_bin*, std::unordered_map<VM_INT, std::vector<Value>>>::iterator it
        = g_named_arrays.find(vm);
    if (it == g_named_arrays.end()) return;
    for (std::unordered_map<VM_INT, std::vector<Value>>::iterator a = it->second.begin();
         a != it->second.end(); ++a)
        for (size_t i = 0; i < a->second.size(); i++) release(a->second[i]);
    g_named_arrays.erase(it);
}

// ============================================================
// 调用栈（数组模拟）：每层 VM_RUN 压一个"当前实例 id + 这块码归谁"
// ============================================================
VM_INT  g_run_stack[RUN_STACK_MAX];
vm_bin* g_run_vm[RUN_STACK_MAX];
int     g_run_sp = 0;

void run_push(vm_bin& vm, VM_INT inst_id) {
    if (g_run_sp >= RUN_STACK_MAX) { vm_set_err(VM_E_CALL_DEPTH, g_run_sp, 0); return; }
    g_run_stack[g_run_sp] = inst_id;
    g_run_vm[g_run_sp] = &vm;
    g_run_sp++;
}

void run_push_frame(VM_INT inst_id) {              // callbin：借公共内存，没有 vm_bin
    if (g_run_sp >= RUN_STACK_MAX) { vm_set_err(VM_E_CALL_DEPTH, g_run_sp, 0); return; }
    g_run_stack[g_run_sp] = inst_id;
    g_run_vm[g_run_sp] = NULL;
    g_run_sp++;
}

void run_pop() { if (g_run_sp > 0) g_run_sp--; }

// ============================================================
// 容器池（全局：ds_map / ds_grid / ds_list 本来就是引擎级 id）
//   句柄 = Value{类型, 池下标}；一个"存储位置"存着句柄 = 一个引用
//   release 减到 0 → 下标进复用队列，**不递归释放**（残留引用等复用时再放）
// ============================================================
std::vector<std::vector<Value>>                g_arr_pool;
std::vector<std::map<Value, Value, ValueLess>> g_map_pool;
std::vector<Grid>                              g_grid_pool;
std::vector<std::vector<Value>>                g_list_pool;

std::vector<VM_INT> g_arr_refs,  g_arr_stamp;
std::vector<VM_INT> g_map_refs,  g_map_stamp;
std::vector<VM_INT> g_grid_refs, g_grid_stamp;
std::vector<VM_INT> g_list_refs, g_list_stamp;
std::vector<int>    g_arr_used,  g_map_used, g_grid_used, g_list_used;

static VM_INT g_stamp_ctr = 0;             // 创建时间戳（新建 / 复用都取新号）

// 放掉一个槽里残留的引用（一层，不递归）——只在复用它之前调
static void arr_drop_children(int i) {
    std::vector<Value>& a = g_arr_pool[(size_t)i];
    for (size_t k = 0; k < a.size(); k++) release(a[k]);
    a.clear();
}
static void map_drop_children(int i) {
    std::map<Value, Value, ValueLess>& m = g_map_pool[(size_t)i];
    for (std::map<Value, Value, ValueLess>::iterator it = m.begin(); it != m.end(); ++it) {
        release(it->first);
        release(it->second);
    }
    m.clear();
}
static void grid_drop_children(int i) {
    Grid& g = g_grid_pool[(size_t)i];
    for (size_t k = 0; k < g.cells.size(); k++) release(g.cells[k]);
    g.cells.clear();
    g.w = 0;
    g.h = 0;
}
static void list_drop_children(int i) {
    std::vector<Value>& l = g_list_pool[(size_t)i];
    for (size_t k = 0; k < l.size(); k++) release(l[k]);
    l.clear();
}

// 归还：引用计数置 0、下标进复用队列；内容留着（残留引用等复用时放）
void arr_free(int i) {
    if (i < 0 || i >= (int)g_arr_refs.size()) return;
    g_arr_refs[(size_t)i] = 0;
    g_arr_used.push_back(i);
}
void map_free(int i) {
    if (i < 0 || i >= (int)g_map_refs.size()) return;
    g_map_refs[(size_t)i] = 0;
    g_map_used.push_back(i);
}
void grid_free(int i) {
    if (i < 0 || i >= (int)g_grid_refs.size()) return;
    g_grid_refs[(size_t)i] = 0;
    g_grid_used.push_back(i);
}
void list_free(int i) {
    if (i < 0 || i >= (int)g_list_refs.size()) return;
    g_list_refs[(size_t)i] = 0;
    g_list_used.push_back(i);
}

// 取槽：队列里有回收的先拿（拿之前放掉残留引用），没有就往后新增；新容器造出来就有主（refs = 1）
int arr_new() {
    int i;
    if (!g_arr_used.empty()) {
        i = g_arr_used.back();
        g_arr_used.pop_back();
        arr_drop_children(i);
    } else {
        g_arr_pool.push_back(std::vector<Value>());
        g_arr_refs.push_back(0);
        g_arr_stamp.push_back(0);
        i = (int)g_arr_pool.size() - 1;
    }
    g_arr_refs[(size_t)i]  = 1;
    g_arr_stamp[(size_t)i] = ++g_stamp_ctr;
    return i;
}
int map_new() {
    int i;
    if (!g_map_used.empty()) {
        i = g_map_used.back();
        g_map_used.pop_back();
        map_drop_children(i);
    } else {
        g_map_pool.push_back(std::map<Value, Value, ValueLess>());
        g_map_refs.push_back(0);
        g_map_stamp.push_back(0);
        i = (int)g_map_pool.size() - 1;
    }
    g_map_refs[(size_t)i]  = 1;
    g_map_stamp[(size_t)i] = ++g_stamp_ctr;
    return i;
}
int grid_new() {
    int i;
    if (!g_grid_used.empty()) {
        i = g_grid_used.back();
        g_grid_used.pop_back();
        grid_drop_children(i);
    } else {
        g_grid_pool.push_back(Grid());
        g_grid_refs.push_back(0);
        g_grid_stamp.push_back(0);
        i = (int)g_grid_pool.size() - 1;
    }
    g_grid_refs[(size_t)i]  = 1;
    g_grid_stamp[(size_t)i] = ++g_stamp_ctr;
    return i;
}
int list_new() {
    int i;
    if (!g_list_used.empty()) {
        i = g_list_used.back();
        g_list_used.pop_back();
        list_drop_children(i);
    } else {
        g_list_pool.push_back(std::vector<Value>());
        g_list_refs.push_back(0);
        g_list_stamp.push_back(0);
        i = (int)g_list_pool.size() - 1;
    }
    g_list_refs[(size_t)i]  = 1;
    g_list_stamp[(size_t)i] = ++g_stamp_ctr;
    return i;
}

// 句柄还活着吗（下标有效且引用计数 > 0）
bool container_alive(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: return v.v >= 0 && v.v < (int)g_arr_refs.size()  && g_arr_refs[(size_t)v.v]  > 0;
    case MEM_MAP:   return v.v >= 0 && v.v < (int)g_map_refs.size()  && g_map_refs[(size_t)v.v]  > 0;
    case MEM_GRID:  return v.v >= 0 && v.v < (int)g_grid_refs.size() && g_grid_refs[(size_t)v.v] > 0;
    case MEM_LIST:  return v.v >= 0 && v.v < (int)g_list_refs.size() && g_list_refs[(size_t)v.v] > 0;
    }
    return false;
}

// 按类型归还（不合法就什么都不做）
void container_free(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: arr_free(v.v);  return;
    case MEM_MAP:   map_free(v.v);  return;
    case MEM_GRID:  grid_free(v.v); return;
    case MEM_LIST:  list_free(v.v); return;
    }
}

void retain(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: if (container_alive(v)) g_arr_refs[(size_t)v.v]++;  return;
    case MEM_MAP:   if (container_alive(v)) g_map_refs[(size_t)v.v]++;  return;
    case MEM_GRID:  if (container_alive(v)) g_grid_refs[(size_t)v.v]++; return;
    case MEM_LIST:  if (container_alive(v)) g_list_refs[(size_t)v.v]++; return;
    }
    // 不是容器：int / float / string / undefined 都不用管
}

void release(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY:
        if (!container_alive(v)) return;
        if (--g_arr_refs[(size_t)v.v] == 0) arr_free(v.v);
        return;
    case MEM_MAP:
        if (!container_alive(v)) return;
        if (--g_map_refs[(size_t)v.v] == 0) map_free(v.v);
        return;
    case MEM_GRID:
        if (!container_alive(v)) return;
        if (--g_grid_refs[(size_t)v.v] == 0) grid_free(v.v);
        return;
    case MEM_LIST:
        if (!container_alive(v)) return;
        if (--g_list_refs[(size_t)v.v] == 0) list_free(v.v);
        return;
    }
    // 不是容器：什么都不做
}

// 写一个槽：先把新值 retain，再把槽里原来那个 release
void slot_write(Value* mem, int slot, Value v) {
    if (slot < 0) return;
    Value old = mem[slot];
    retain(v);
    if (old.t != v.t || old.v != v.v) release(old);
    mem[slot] = v;
}

// 取第 k 个实参当容器句柄：类型不对 / 已经死了 → -1
//   （内层不打印；调用方看到 -1 就直接返回，错误码由调用方自己按需置）
int handle_arg(Value* mem, const uint8_t* args, int k, VM_INT type) {
    Value v = mem[argslot(args, k)];
    if (v.t != type || !container_alive(v)) return -1;
    return v.v;
}

// 把一个**刚建出来**的句柄交给 dst（创建时引用计数是 1，先算创造者自己拿着）
//   dst < 0（语句位置，没人接）→ 放掉创造者那份
//   有 dst → 槽接手（wr_v 里 retain 一份），再放掉创造者那份，净剩 1
void give_handle(Value* mem, int dst, const Value& v) {
    if (dst < 0) release(v);
    else { wr_v(mem, dst, v); release(v); }
}

// 容器的时间戳（不是容器 / 越界 → 当"最新"，不拦）
VM_INT container_stamp(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: if (v.v >= 0 && v.v < (int)g_arr_stamp.size())  return g_arr_stamp[(size_t)v.v];  break;
    case MEM_MAP:   if (v.v >= 0 && v.v < (int)g_map_stamp.size())  return g_map_stamp[(size_t)v.v];  break;
    case MEM_GRID:  if (v.v >= 0 && v.v < (int)g_grid_stamp.size()) return g_grid_stamp[(size_t)v.v]; break;
    case MEM_LIST:  if (v.v >= 0 && v.v < (int)g_list_stamp.size()) return g_list_stamp[(size_t)v.v]; break;
    }
    return 0x7fffffff;
}

// 把 child 放进 parent 之前调：容器只收"比自己新"的（正序），逆序 / 自己塞自己 → 不收。
//   强引用图于是永远是全正序的，环在结构上就形成不了（不需要可达性搜索，热路径一次整数比较）。
bool allow_store(const Value& child, const Value& parent) {
    if (child.t != MEM_ARRAY && child.t != MEM_MAP && child.t != MEM_GRID && child.t != MEM_LIST)
        return true;
    if (child.t == parent.t && child.v == parent.v) return false;
    return container_stamp(child) > container_stamp(parent);
}
