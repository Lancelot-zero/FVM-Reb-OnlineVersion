// ============================================================
// vm_container.cpp — 容器操作
//   四个池都是全局（见下面）：数组 g_arr_pool，字典/结构体 g_map_pool，
//   二维表 g_grid_pool，列表 g_list_pool
//   槽里存的都是池索引（和字符串存池索引一个套路）
//   句柄是带类型的：建容器时写的是 Value{MEM_ARRAY/MEM_MAP/MEM_GRID/MEM_LIST, 池下标}。
//   入口一律走 handle_arg()：类型不对 / 已经销毁 就打印错误并返回 -1，调用方直接 return。
//
//   引用计数（本文件的 retain / release）：
//     · 一个"存储位置"（内存槽、容器里的元素、以后的实例变量）存着句柄 = 一个引用
//     · release 减到 0 → 下标进该池的删除队列，**不递归释放**，里面遗留的引用留着
//     · 等这个下标被 pool_take 复用的时候，才把残留的引用放掉（一层，不递归）
//       之所以敢延迟：引用计数保证"残留引用指向的子容器一定还活着"
//     · 循环引用（a[0] = a）不会报错也不会崩，就是永远回收不掉（跟 GM 一样靠 leak collector 兜）
//
//   以后新写任何"产出句柄"的函数（array_create / array_deep_copy / inst_array_create …）
//   要照这个写：pool_take() 拿下标 → 元素 retain → dst < 0 就 release，否则 wr_v
//   以后新写任何"把句柄存进容器"的函数，存之前必须先 allow_store()（逆序拒收）：
//   它返回 false 的时候，这个元素不许真的存进去。
//   元素 = Value{int t; int v;}，就是一个内存槽的 8 字节
//   槽读写 helper（argslot / rd_* / wr_*）在 vm.h 里
// ============================================================

#include "vm.h"
#include <cstdlib>
#include <deque>

// ============================================================
// 容器池（全局，不挂在 Vm 上）
//   GML 那边的 ds_map / ds_grid / ds_list 本来就是引擎级的全局 id
//   （ds_map_create() 返回全局唯一 id，跟哪个 VM、哪个实例都无关），
//   所以池放外面。槽里存的还是池下标。
//   数组也一样放全局：实例数组、全局数组都进这个池（句柄 = 池下标）。
//   （局部数组还没定，先也走这个池）
// ============================================================
static std::vector<std::vector<Value>>                g_arr_pool;    // 数组（实例数组 / 全局数组都放这儿）
static std::vector<std::map<Value, Value, ValueLess>> g_map_pool;    // ds_map / struct
static std::vector<Grid>                              g_grid_pool;   // ds_grid
static std::vector<std::vector<Value>>                g_list_pool;   // ds_list

// 引用计数，和池同下标（平行的数组，省得把池改成结构体、动一堆取值写法）
static std::vector<int> g_arr_refs;
static std::vector<int> g_map_refs;
static std::vector<int> g_grid_refs;
static std::vector<int> g_list_refs;

// ============================================================
// 逆序检测用的平行数组（和池同下标）
//   stamp —— 创建时间戳：新建/复用时取全局自增号（复用等于新容器）
// ============================================================
static int g_stamp_ctr = 0;

static std::vector<int>  g_arr_stamp,  g_map_stamp,  g_grid_stamp,  g_list_stamp;

// ============================================================
// 每个池配一个删除队列 + 一个计数器
//   队列   g_xxx_used  —— 引用计数归 0 的下标（进队列时内容不清，残留引用等复用时再放），
//                         下次建容器优先从这儿取，先来先出
//   计数器 g_xxx_count —— 当前有几个容器（活着的，即 refs > 0）
//   池自己的大小 g_xxx_pool.size() = 活着的 + 队列里等复用的
// ============================================================
static std::deque<int> g_arr_used;     static int g_arr_count  = 0;   // 数组
static std::deque<int> g_map_used;     static int g_map_count  = 0;   // 字典 / 结构体
static std::deque<int> g_grid_used;    static int g_grid_count = 0;   // 二维表
static std::deque<int> g_list_used;    static int g_list_count = 0;   // 列表

// ============================================================
// 放掉一个槽里残留的引用（一层，不递归）
//   只在 pool_take 复用这个下标之前调用
// ============================================================
static void arr_drop_children(int i) {
    std::vector<Value>& a = g_arr_pool[i];
    for (size_t k = 0; k < a.size(); k++) release(a[k]);
    a.clear();
}
static void map_drop_children(int i) {
    std::map<Value, Value, ValueLess>& m = g_map_pool[i];
    for (std::map<Value, Value, ValueLess>::iterator it = m.begin(); it != m.end(); ++it) {
        release(it->first);
        release(it->second);
    }
    m.clear();
}
static void grid_drop_children(int i) {
    Grid& g = g_grid_pool[i];
    for (size_t k = 0; k < g.cells.size(); k++) release(g.cells[k]);
    g.cells.clear();
    g.w = 0;
    g.h = 0;
}
static void list_drop_children(int i) {
    std::vector<Value>& l = g_list_pool[i];
    for (size_t k = 0; k < l.size(); k++) release(l[k]);
    l.clear();
}

// ============================================================
// 取槽 / 归还槽
//   归还：引用计数置 0、下标进删除队列；池里内容留着（残留引用等复用时放）
//   取槽：队列里有回收的先拿（先来先出），拿之前先放掉残留引用、清空
// ============================================================
static void arr_free(int i) {
    if (i < 0 || i >= (int)g_arr_pool.size()) return;
    g_arr_refs[i] = 0;
    g_arr_used.push_back(i);
    g_arr_count--;
}
static void map_free(int i) {
    if (i < 0 || i >= (int)g_map_pool.size()) return;
    g_map_refs[i] = 0;
    g_map_used.push_back(i);
    g_map_count--;
}
static void grid_free(int i) {
    if (i < 0 || i >= (int)g_grid_pool.size()) return;
    g_grid_refs[i] = 0;
    g_grid_used.push_back(i);
    g_grid_count--;
}
static void list_free(int i) {
    if (i < 0 || i >= (int)g_list_pool.size()) return;
    g_list_refs[i] = 0;
    g_list_used.push_back(i);
    g_list_count--;
}

static int arr_take() {
    int i;
    if (!g_arr_used.empty()) {
        i = g_arr_used.front();
        g_arr_used.pop_front();
        arr_drop_children(i);            // 复用了这个下标 → 现在才放掉里面的残留引用
    } else {
        g_arr_pool.push_back(std::vector<Value>());
        g_arr_refs.push_back(0);
        g_arr_stamp.push_back(0);
        i = (int)g_arr_pool.size() - 1;
    }
    g_arr_refs[i]  = 1;                  // 新容器造出来就有主
    g_arr_stamp[i] = ++g_stamp_ctr;      // 新容器的时间戳（复用也重新取号）
    g_arr_count++;
    return i;
}
static int map_take() {
    int i;
    if (!g_map_used.empty()) {
        i = g_map_used.front();
        g_map_used.pop_front();
        map_drop_children(i);
    } else {
        g_map_pool.push_back(std::map<Value, Value, ValueLess>());
        g_map_refs.push_back(0);
        g_map_stamp.push_back(0);
        i = (int)g_map_pool.size() - 1;
    }
    g_map_refs[i]  = 1;
    g_map_stamp[i] = ++g_stamp_ctr;
    g_map_count++;
    return i;
}
static int grid_take() {
    int i;
    if (!g_grid_used.empty()) {
        i = g_grid_used.front();
        g_grid_used.pop_front();
        grid_drop_children(i);
    } else {
        g_grid_pool.push_back(Grid());
        g_grid_refs.push_back(0);
        g_grid_stamp.push_back(0);
        i = (int)g_grid_pool.size() - 1;
    }
    g_grid_refs[i]  = 1;
    g_grid_stamp[i] = ++g_stamp_ctr;
    g_grid_count++;
    return i;
}
static int list_take() {
    int i;
    if (!g_list_used.empty()) {
        i = g_list_used.front();
        g_list_used.pop_front();
        list_drop_children(i);
    } else {
        g_list_pool.push_back(std::vector<Value>());
        g_list_refs.push_back(0);
        g_list_stamp.push_back(0);
        i = (int)g_list_pool.size() - 1;
    }
    g_list_refs[i]  = 1;
    g_list_stamp[i] = ++g_stamp_ctr;
    g_list_count++;
    return i;
}

// ============================================================
// 引用计数本体
// ============================================================
// 句柄还活着吗（池下标有效且引用计数 > 0）
static bool is_live(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: return v.v >= 0 && v.v < (int)g_arr_refs.size()  && g_arr_refs[v.v]  > 0;
    case MEM_MAP:   return v.v >= 0 && v.v < (int)g_map_refs.size()  && g_map_refs[v.v]  > 0;
    case MEM_GRID:  return v.v >= 0 && v.v < (int)g_grid_refs.size() && g_grid_refs[v.v] > 0;
    case MEM_LIST:  return v.v >= 0 && v.v < (int)g_list_refs.size() && g_list_refs[v.v] > 0;
    }
    return false;
}

void retain(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY:
        if (v.v < 0 || v.v >= (int)g_arr_refs.size()) { printf("[vm] retain: bad array index %d\n", v.v); return; }
        if (g_arr_refs[v.v] <= 0) { printf("[vm] retain: array %d is already dead\n", v.v); return; }
        g_arr_refs[v.v]++;
        return;
    case MEM_MAP:
        if (v.v < 0 || v.v >= (int)g_map_refs.size()) { printf("[vm] retain: bad map index %d\n", v.v); return; }
        if (g_map_refs[v.v] <= 0) { printf("[vm] retain: map %d is already dead\n", v.v); return; }
        g_map_refs[v.v]++;
        return;
    case MEM_GRID:
        if (v.v < 0 || v.v >= (int)g_grid_refs.size()) { printf("[vm] retain: bad grid index %d\n", v.v); return; }
        if (g_grid_refs[v.v] <= 0) { printf("[vm] retain: grid %d is already dead\n", v.v); return; }
        g_grid_refs[v.v]++;
        return;
    case MEM_LIST:
        if (v.v < 0 || v.v >= (int)g_list_refs.size()) { printf("[vm] retain: bad list index %d\n", v.v); return; }
        if (g_list_refs[v.v] <= 0) { printf("[vm] retain: list %d is already dead\n", v.v); return; }
        g_list_refs[v.v]++;
        return;
    }
    // 不是容器：int / float / string / undefined 都不用管
}

void release(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY:
        if (v.v < 0 || v.v >= (int)g_arr_refs.size()) { printf("[vm] release: bad array index %d\n", v.v); return; }
        if (g_arr_refs[v.v] <= 0) { printf("[vm] release: array %d refcount underflow\n", v.v); return; }
        if (--g_arr_refs[v.v] == 0) arr_free(v.v);
        return;
    case MEM_MAP:
        if (v.v < 0 || v.v >= (int)g_map_refs.size()) { printf("[vm] release: bad map index %d\n", v.v); return; }
        if (g_map_refs[v.v] <= 0) { printf("[vm] release: map %d refcount underflow\n", v.v); return; }
        if (--g_map_refs[v.v] == 0) map_free(v.v);
        return;
    case MEM_GRID:
        if (v.v < 0 || v.v >= (int)g_grid_refs.size()) { printf("[vm] release: bad grid index %d\n", v.v); return; }
        if (g_grid_refs[v.v] <= 0) { printf("[vm] release: grid %d refcount underflow\n", v.v); return; }
        if (--g_grid_refs[v.v] == 0) grid_free(v.v);
        return;
    case MEM_LIST:
        if (v.v < 0 || v.v >= (int)g_list_refs.size()) { printf("[vm] release: bad list index %d\n", v.v); return; }
        if (g_list_refs[v.v] <= 0) { printf("[vm] release: list %d refcount underflow\n", v.v); return; }
        if (--g_list_refs[v.v] == 0) list_free(v.v);
        return;
    }
    // 不是容器：什么都不做
}

// ============================================================
// 逆序拒收（运行期）
//   规定：容器只收"比自己新"的容器（老容器里放新建的容器）。
//   每个容器带一个创建时间戳（新建/复用都取新号），往容器里塞容器时比一下：
//     · 塞进去的比目标新（正序）→ 放行。**强引用图永远是"全正序"的，所以不可能成环**
//     · 塞进去的比目标老（逆序）→ 拒收 + 打印（元素不存）。包括自己塞自己
//   这样环在结构上就无法形成：环里时间戳不可能一直递增，逆序那一脚会被挡在门外，
//   不需要再跑可达性搜索（BFS），热路径只有一次整数比较。
// ============================================================
static int stamp_of(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: if (v.v >= 0 && v.v < (int)g_arr_stamp.size())  return g_arr_stamp[v.v];  break;
    case MEM_MAP:   if (v.v >= 0 && v.v < (int)g_map_stamp.size())  return g_map_stamp[v.v];  break;
    case MEM_GRID:  if (v.v >= 0 && v.v < (int)g_grid_stamp.size()) return g_grid_stamp[v.v]; break;
    case MEM_LIST:  if (v.v >= 0 && v.v < (int)g_list_stamp.size()) return g_list_stamp[v.v]; break;
    }
    return 0x7fffffff;                   // 不是容器 / 越界：当"最新"，不拦
}

// 把 child 放进 parent 之前调用：能放返回 true；逆序（或自己塞自己）就打印 + 返回 false
static bool allow_store(const Value& child, const Value& parent, int fsid) {
    if (!is_container(child)) return true;
    bool self = (child.t == parent.t && child.v == parent.v);
    if (!self && stamp_of(child) > stamp_of(parent)) return true;   // 正序：放行
    printf("[vm] %s: backwards reference rejected (%s), element not stored\n",
           str_at(fsid).c_str(), self ? "a container cannot hold itself"
                     : "an older container cannot be stored into a newer one");
    return false;
}

// ============================================================
// 句柄入口校验
//   参数必须是"带类型的容器句柄"，而且必须还活着（引用计数 > 0）。
//   不满足就打印错误、返回 -1，调用方立刻 return——
//   绝不拿一个来路不明的下标去池里乱串。
// ============================================================
// 类型名 → **池下标**（拼报错信息时用 str_at 取文本；名字全在 names.h 里）
static int type_name(int ty) {
    switch (ty) {
    case MEM_INT:       return S_int;
    case MEM_FLOAT:     return S_float;
    case MEM_STRING:    return S_string;
    case MEM_ARRAY:     return S_array;
    case MEM_MAP:       return S_map_struct;
    case MEM_GRID:      return S_grid;
    case MEM_LIST:      return S_list;
    case MEM_UNDEFINED: return S_undefined;
    }
    return S_unknown;
}

// k = 第几个参数（0 起），want = 期望类型，who = 函数名（报错用）
static int handle_arg(Vm& vm, const uint8_t* args, int k, int want, int fsid) {
    int slot = argslot(args, k);
    int ty   = rd_t(vm, slot);
    if (ty != want) {
        printf("[vm] %s: argument %d must be %s (got %s)\n",
               str_at(fsid).c_str(), k + 1, str_at(type_name(want)).c_str(), str_at(type_name(ty)).c_str());
        return -1;
    }
    Value v = rd_v(vm, slot);
    if (!is_live(v)) {
        printf("[vm] %s: argument %d (%s %d) was already destroyed\n",
               str_at(fsid).c_str(), k + 1, str_at(type_name(want)).c_str(), v.v);
        return -1;
    }
    return v.v;
}

// 把"刚建出来"的句柄交给 dst（创建时引用计数是 1，先算作创造者自己拿着）
//   dst < 0（语句位置，没人接）→ 放掉创造者那份
//   有 dst → 槽接手（wr_v 里 retain 一份），再放掉创造者那份，净剩 1
static void give_handle(Vm& vm, int dst, const Value& v) {
    if (dst < 0) release(v);
    else { wr_v(vm, dst, v); release(v); }
}

// ── 数组 ──
void array_literal(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = arr_take();
    Value self{MEM_ARRAY, a};
    for (int k = 0; k < argc; k++) {
        Value e = rd_v(vm, argslot(args, k));
        if (!allow_store(e, self, S_array_literal)) continue;   // 会成环就不收这个元素
        retain(e);                        // 元素进容器 = 多一个引用
        g_arr_pool[a].push_back(e);
    }
    give_handle(vm, dst, self);
}
void array_set(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_set);
    if (a < 0) { wr_i(vm, dst, -1); return; }
    int i = rd_i(vm, argslot(args, 1));
    if (i < 0) { wr_i(vm, dst, -1); return; }
    Value nv = rd_v(vm, argslot(args, 2));
    if (!allow_store(nv, Value{MEM_ARRAY, a}, S_array_set)) { wr_i(vm, dst, -1); return; }
    if (i >= (int)g_arr_pool[a].size()) g_arr_pool[a].resize((size_t)i + 1);
    Value ov = g_arr_pool[a][i];
    retain(nv);                           // 先拿新的
    if (ov.t != nv.t || ov.v != nv.v) release(ov);   // 再放旧的
    g_arr_pool[a][i] = nv;
    wr_i(vm, dst, 0);
}
void array_get(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_get);
    if (a < 0) return;
    int i = rd_i(vm, argslot(args, 1));
    if (i < 0 || i >= (int)g_arr_pool[a].size()) return;
    wr_v(vm, dst, g_arr_pool[a][i]);      // wr_v 会 retain，槽接一份引用
}
void array_push(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_push);
    if (a < 0) { wr_i(vm, dst, -1); return; }
    Value nv = rd_v(vm, argslot(args, 1));
    if (!allow_store(nv, Value{MEM_ARRAY, a}, S_array_push)) { wr_i(vm, dst, -1); return; }
    retain(nv);
    g_arr_pool[a].push_back(nv);
    wr_i(vm, dst, (int)g_arr_pool[a].size() - 1);
}
void array_pop(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_pop);
    if (a < 0) return;
    if (g_arr_pool[a].empty()) return;
    Value v = g_arr_pool[a].back();
    g_arr_pool[a].pop_back();
    if (dst < 0) release(v);              // 没人接 → 容器这份引用放掉
    else { wr_v(vm, dst, v); release(v); } // 槽接手（wr_v 里 retain），再把容器这份放掉
}
void array_clear(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_clear);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    arr_drop_children(a);                 // 放掉元素（一层，不递归），然后清空
    wr_i(vm, dst, 0);
}
void array_contains(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_contains);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    Value k = rd_v(vm, argslot(args, 1));
    for (size_t i = 0; i < g_arr_pool[a].size(); i++)
        if (g_arr_pool[a][i].t == k.t && g_arr_pool[a][i].v == k.v) { wr_i(vm, dst, 1); return; }
    wr_i(vm, dst, 0);
}

// ── 数组：长度 / 查下标 / 删 / 改长 / 建 / 打乱 ──
//   都是"就地改这个数组"，句柄不变（跟 GML 的数组是一个引用语义）。
//   元素比较一律按 [类型 + 值] 比 —— 1 和 "1" 不是一回事（GML 的 == 也这么判）。
void array_length(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_length);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, (int)g_arr_pool[a].size());
}
void array_get_index(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_get_index);
    if (a < 0) { wr_i(vm, dst, -1); return; }
    Value k = rd_v(vm, argslot(args, 1));
    for (size_t i = 0; i < g_arr_pool[a].size(); i++)
        if (g_arr_pool[a][i].t == k.t && g_arr_pool[a][i].v == k.v) { wr_i(vm, dst, (int)i); return; }
    wr_i(vm, dst, -1);                    // 没有 → -1（GML 也是 -1）
}
// array_delete(arr, index, number)：从 index 起删 number 个，后面的往前挪
void array_delete(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_delete);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    int i = rd_i(vm, argslot(args, 1));
    int n = rd_i(vm, argslot(args, 2));
    std::vector<Value>& arr = g_arr_pool[a];
    if (i < 0 || i >= (int)arr.size() || n <= 0) { wr_i(vm, dst, 0); return; }
    if (i + n > (int)arr.size()) n = (int)arr.size() - i;   // GML 里超出就删到末尾
    for (int k = 0; k < n; k++) release(arr[(size_t)i + (size_t)k]);   // 删掉的元素放掉引用
    arr.erase(arr.begin() + i, arr.begin() + i + n);
    wr_i(vm, dst, 0);
}
// array_resize(arr, size)：变长填空 0（GML 填的就是 0），变短放掉被砍掉的
void array_resize(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_resize);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    int n = rd_i(vm, argslot(args, 1));
    if (n < 0) n = 0;
    std::vector<Value>& arr = g_arr_pool[a];
    if ((size_t)n < arr.size()) {
        for (size_t k = (size_t)n; k < arr.size(); k++) release(arr[k]);
    }
    arr.resize((size_t)n);                // 变长的那部分是真 0（int 0，不是容器，不用 retain）
    wr_i(vm, dst, 0);
}
// array_create(size [, value])：建一个新数组，元素都填 value（默认 0）
void array_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    int n = rd_i(vm, argslot(args, 0));
    if (n < 0) n = 0;
    Value fill; fill.t = MEM_INT; fill.v = 0;
    if (argc >= 2) fill = rd_v(vm, argslot(args, 1));
    int a = arr_take();
    Value self{MEM_ARRAY, a};
    if (!allow_store(fill, self, S_array_create)) { give_handle(vm, dst, self); return; }
    for (int k = 0; k < n; k++) { retain(fill); g_arr_pool[a].push_back(fill); }
    give_handle(vm, dst, self);
}
void array_shuffle(Vm& vm, const uint8_t* args, int argc, int dst) {
    int a = handle_arg(vm, args, 0, MEM_ARRAY, S_array_shuffle);
    if (a < 0) { wr_i(vm, dst, 0); return; }
    std::vector<Value>& arr = g_arr_pool[a];
    for (int i = (int)arr.size() - 1; i > 0; i--) {          // 标准 Fisher-Yates
        int j = (int)(rand() % (i + 1));
        Value t = arr[(size_t)i]; arr[(size_t)i] = arr[(size_t)j]; arr[(size_t)j] = t;
    }
    wr_i(vm, dst, 0);
}

// ds_exists(句柄, 类型)：句柄是不是这个类型的、且还活着
//   GML 里拿到一个 ds_* 句柄后判断"是不是还在"，我们这里就是查引用计数 > 0
void ds_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    int want = rd_i(vm, argslot(args, 1));
    Value v  = rd_v(vm, argslot(args, 0));
    int ty;
    switch (want) {
    case 0:  ty = MEM_LIST;  break;       // ds_type_list
    case 1:  ty = MEM_MAP;   break;       // ds_type_map
    case 3:  ty = MEM_GRID;  break;       // ds_type_grid
    default: wr_i(vm, dst, 0); return;    // queue / stack / priority：我们没实现
    }
    wr_i(vm, dst, (v.t == ty && is_live(v)) ? 1 : 0);
}

// ── 字典（结构体同一种，键是字符串）──
//   注意 struct_get / struct_set / struct_exists 是转发到这里的，
//   所以报错信息里打的是 ds_map_* 的函数名。
void ds_map_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = map_take();
    give_handle(vm, dst, Value{MEM_MAP, m});
}
void ds_map_set(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_set);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    Value key = rd_v(vm, argslot(args, 1));
    Value nv  = rd_v(vm, argslot(args, 2));
    Value self{MEM_MAP, m};
    if (!allow_store(key, self, S_ds_map_set)) { wr_i(vm, dst, 0); return; }   // 键也可能是容器
    if (!allow_store(nv, self, S_ds_map_set)) { wr_i(vm, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
    if (it != mp.end()) {
        Value ov = it->second;
        retain(nv);
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        it->second = nv;
    } else {
        retain(key);                      // 新键，map 里存了一份
        retain(nv);
        mp[key] = nv;
    }
    wr_i(vm, dst, 0);
}
void ds_map_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_exists);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, g_map_pool[m].count(rd_v(vm, argslot(args, 1))) ? 1 : 0);
}
void ds_map_find_value(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_find_value);
    if (m < 0) return;
    std::map<Value, Value, ValueLess>::iterator it = g_map_pool[m].find(rd_v(vm, argslot(args, 1)));
    if (it != g_map_pool[m].end()) wr_v(vm, dst, it->second);   // wr_v 里 retain
}
void ds_map_delete(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_delete);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(rd_v(vm, argslot(args, 1)));
    if (it != mp.end()) {
        release(it->first);               // 键和值都不再被这个字典引用
        release(it->second);
        mp.erase(it);
    }
    wr_i(vm, dst, 0);
}
// variable_struct_remove(struct, key)：删结构体的一个键（照 ds_map_delete 写）
void variable_struct_remove(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(vm, args, 0, MEM_MAP, S_variable_struct_remove);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(rd_v(vm, argslot(args, 1)));
    if (it != mp.end()) {
        release(it->first);
        release(it->second);
        mp.erase(it);
    }
    wr_i(vm, dst, 0);
}


// ============================================================
// 批次 4 第一档：格子/地形（只读）
//   语义参考 scr_command_VM.gml 里的同名 GML 实现（不照抄，按 VM 的结构写）
//   arg 约定：和平台一致 —— 实参是 VM 槽地址，值用 rd_v(argslot) 取
// ============================================================
static Value v_str(int sid) { Value v; v.t = MEM_STRING; v.v = sid; return v; }
static bool v_num_of(Vm& vm, const Value& v, double* out) {
    if (v.t == MEM_INT)   { *out = (double)v.v; return true; }
    if (v.t == MEM_FLOAT) { float f; memcpy(&f, &v.v, 4); *out = (double)f; return true; }
    return false;
}
static double g_num_arg(Vm& vm, const uint8_t* args, int k) {
    Value v = rd_v(vm, argslot(args, k));
    double d = 0; v_num_of(vm, v, &d);
    return d;
}
// 取二维数组 global.grid_terrains[row][col]（元素是 struct）→ 失败返回 false
static bool terrain_cell(Vm& vm, int row, int col, Value* out) {
    Value* g = global_var_sid(S_grid_terrains);
    if (!g || g->t != MEM_ARRAY) return false;
    int ra = g->v;
    if (ra < 0 || ra >= (int)g_arr_pool.size()) return false;
    if (row < 0 || row >= (int)g_arr_pool[ra].size()) return false;
    Value rv = g_arr_pool[ra][row];
    if (rv.t != MEM_ARRAY) return false;
    int ca = rv.v;
    if (ca < 0 || ca >= (int)g_arr_pool.size()) return false;
    if (col < 0 || col >= (int)g_arr_pool[ca].size()) return false;
    *out = g_arr_pool[ca][col];
    return true;
}

// VM_GetTerrain(col,row)：越界 → -1；"water"→1、"obstacle"→2、其它→0
void impl_VM_GetTerrain(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    double cols = 0, rows = 0;
    Value* gc = global_var_sid(S_grid_cols); if (gc) v_num_of(vm, *gc, &cols);
    Value* gr = global_var_sid(S_grid_rows); if (gr) v_num_of(vm, *gr, &rows);
    if (row < 0 || row >= (int)rows || col < 0 || col >= (int)cols) { wr_i(vm, dst, -1); return; }
    Value cell;
    if (!terrain_cell(vm, row, col, &cell) || cell.t != MEM_MAP) { wr_i(vm, dst, 0); return; }
    Value t;
    if (!map_find(cell, v_str(S_type), &t) || t.t != MEM_STRING) { wr_i(vm, dst, 0); return; }
    if (t.v == S_water)    { wr_i(vm, dst, 1); return; }
    if (t.v == S_obstacle) { wr_i(vm, dst, 2); return; }
    wr_i(vm, dst, 0);
}

// VM_CellCount(name,i,j)：名为 name 的全局表（二维压成一维，行距 = grid_cols+2）
//   任何异常 → -1；正常 → 该格数组长度
void impl_VM_CellCount(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value nv = rd_v(vm, argslot(args, 0));
    if (nv.t != MEM_STRING || !str_ok(nv.v)) { wr_i(vm, dst, -1); return; }
    Value* tbl = global_var_sid(nv.v);
    if (!tbl || tbl->t != MEM_ARRAY) { wr_i(vm, dst, -1); return; }
    double cols = 0, rows = 0;
    Value* gc = global_var_sid(S_grid_cols); if (gc) v_num_of(vm, *gc, &cols);
    Value* gr = global_var_sid(S_grid_rows); if (gr) v_num_of(vm, *gr, &rows);
    int i = (int)g_num_arg(vm, args, 1), j = (int)g_num_arg(vm, args, 2);
    if (i < 0 || i >= (int)cols || j < 0 || j >= (int)rows) { wr_i(vm, dst, -1); return; }
    int idx = j * ((int)cols + 2) + i;
    int ta = tbl->v;
    if (ta < 0 || ta >= (int)g_arr_pool.size()) { wr_i(vm, dst, -1); return; }
    if (idx < 0 || idx >= (int)g_arr_pool[ta].size()) { wr_i(vm, dst, -1); return; }
    Value cell = g_arr_pool[ta][idx];
    if (cell.t != MEM_ARRAY) { wr_i(vm, dst, -1); return; }
    int ca = cell.v;
    if (ca < 0 || ca >= (int)g_arr_pool.size()) { wr_i(vm, dst, -1); return; }
    wr_i(vm, dst, (int)g_arr_pool[ca].size());
}

// VM_CellItem(name,i,j,k)：该格数组的第 k 个；异常 → undefined
void impl_VM_CellItem(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value nv = rd_v(vm, argslot(args, 0));
    if (nv.t != MEM_STRING || !str_ok(nv.v)) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return; }
    Value* tbl = global_var_sid(nv.v);
    if (!tbl || tbl->t != MEM_ARRAY) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return; }
    double cols = 0;
    Value* gc = global_var_sid(S_grid_cols); if (gc) v_num_of(vm, *gc, &cols);
    int i = (int)g_num_arg(vm, args, 1), j = (int)g_num_arg(vm, args, 2), k = (int)g_num_arg(vm, args, 3);
    int idx = j * ((int)cols + 2) + i;
    int ta = tbl->v;
    if (ta < 0 || ta >= (int)g_arr_pool.size() || idx < 0 || idx >= (int)g_arr_pool[ta].size()) {
        Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return;
    }
    Value cell = g_arr_pool[ta][idx];
    if (cell.t != MEM_ARRAY) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return; }
    int ca = cell.v;
    if (ca < 0 || ca >= (int)g_arr_pool.size() || k < 0 || k >= (int)g_arr_pool[ca].size()) {
        Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return;
    }
    wr_v(vm, dst, g_arr_pool[ca][k]);
}

// VM_CellContains(name,i,j,value)：该格里有没有这个值（数字/字符串各比各的）
void impl_VM_CellContains(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    Value nv = rd_v(vm, argslot(args, 0));
    if (nv.t != MEM_STRING || !str_ok(nv.v)) { wr_i(vm, dst, 0); return; }
    Value* tbl = global_var_sid(nv.v);
    if (!tbl || tbl->t != MEM_ARRAY) { wr_i(vm, dst, 0); return; }
    double cols = 0;
    Value* gc = global_var_sid(S_grid_cols); if (gc) v_num_of(vm, *gc, &cols);
    int i = (int)g_num_arg(vm, args, 1), j = (int)g_num_arg(vm, args, 2);
    int idx = j * ((int)cols + 2) + i;
    int ta = tbl->v;
    if (ta < 0 || ta >= (int)g_arr_pool.size() || idx < 0 || idx >= (int)g_arr_pool[ta].size()) { wr_i(vm, dst, 0); return; }
    Value cell = g_arr_pool[ta][idx];
    if (cell.t != MEM_ARRAY) { wr_i(vm, dst, 0); return; }
    int ca = cell.v;
    if (ca < 0 || ca >= (int)g_arr_pool.size()) { wr_i(vm, dst, 0); return; }
    Value key = rd_v(vm, argslot(args, 3));
    for (size_t z = 0; z < g_arr_pool[ca].size(); z++) {
        const Value& e = g_arr_pool[ca][z];
        if (e.t == key.t && e.v == key.v) { wr_i(vm, dst, 1); return; }
        if (e.t == MEM_INT && key.t == MEM_FLOAT) { double d; v_num_of(vm, key, &d); if ((double)e.v == d) { wr_i(vm, dst, 1); return; } }
        if (e.t == MEM_FLOAT && key.t == MEM_INT) { double d; v_num_of(vm, e, &d); if (d == (double)key.v) { wr_i(vm, dst, 1); return; } }
    }
    wr_i(vm, dst, 0);
}


// VM_SetTerrain(col,row,type)：把 grid_terrains[row][col].type 写成给定值
//   （越界什么都不做；写 map 用 ds_map_set 那套 allow_store）
void impl_VM_SetTerrain(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    double cols = 0, rows = 0;
    Value* gc = global_var_sid(S_grid_cols); if (gc) v_num_of(vm, *gc, &cols);
    Value* gr = global_var_sid(S_grid_rows); if (gr) v_num_of(vm, *gr, &rows);
    if (row < 0 || row >= (int)rows || col < 0 || col >= (int)cols) return;
    Value cell;
    if (!terrain_cell(vm, row, col, &cell) || cell.t != MEM_MAP) return;
    Value nv = rd_v(vm, argslot(args, 2));
    if (nv.t != MEM_STRING) return;                       // type 只接受字符串
    int m = cell.v;
    if (m < 0 || m >= (int)g_map_pool.size()) return;
    Value key = v_str(S_type);
    if (!allow_store(key, cell, S_VM_SetTerrain)) return;
    if (!allow_store(nv, cell, S_VM_SetTerrain)) return;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
    if (it != mp.end()) { release(it->second); it->second = nv; }
    else mp[key] = nv;
}


// ── 批次 4A：格子/植物（数据读写；语义参考 scr_command_VM.gml，按 VM 结构重写）──
// 取 global.grid_plants 某格的实例列表（拿不到 → NULL）
static std::vector<Value>* plant_cell(Vm& vm, int col, int row) {
    Value* g = global_var_sid(S_grid_plants);
    if (!g || g->t != MEM_GRID) return NULL;
    int gi = g->v;
    if (gi < 0 || gi >= (int)g_grid_pool.size()) return NULL;
    Grid& gr = g_grid_pool[gi];
    if (col < 0 || col >= gr.w || row < 0 || row >= gr.h) return NULL;
    Value cell = gr.cells[(size_t)row * gr.w + col];
    if (cell.t != MEM_LIST) return NULL;
    int li = cell.v;
    if (li < 0 || li >= (int)g_list_pool.size()) return NULL;
    return &g_list_pool[li];
}
// 某实例的 plant_id 是不是 "player"
static bool is_player(Vm& vm, int id) {
    Value* p = inst_var_sid(id, S_plant_id);
    return p && p->t == MEM_STRING && p->v == S_player;
}
static int grid_dim(Vm& vm, int sid) {
    Value* p = global_var_sid(sid);
    double d = 0;
    if (p) v_num_of(vm, *p, &d);
    return (int)d;
}

// VM_ClearPlants(col,row)：col/row == -1 表示整列/整行；player 不清
void impl_VM_ClearPlants(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    int cols = grid_dim(vm, S_grid_cols), rows = grid_dim(vm, S_grid_rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            std::vector<Value>* lst = plant_cell(vm, c, r);
            if (!lst) continue;
            for (int i = (int)lst->size() - 1; i >= 0; i--) {
                Value e = (*lst)[i];
                int id = (e.t == MEM_INT) ? e.v : -1;
                if (id < 0 || !inst_get(id)) continue;
                if (is_player(vm, id)) continue;          // 玩家本体不清
                inst_destroy(id);                         // 唯一入口（含 Destroy 事件）
            }
        }
    }
}

// VM_WakePlants(col,row)：睡着的非玩家植物 awake_buff_timer = 1
void impl_VM_WakePlants(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    int cols = grid_dim(vm, S_grid_cols), rows = grid_dim(vm, S_grid_rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            std::vector<Value>* lst = plant_cell(vm, c, r);
            if (!lst) continue;
            for (size_t i = 0; i < lst->size(); i++) {
                Value e = (*lst)[i];
                int id = (e.t == MEM_INT) ? e.v : -1;
                if (id < 0 || !inst_get(id)) continue;
                if (is_player(vm, id)) continue;
                Value* st = inst_var_sid(id, S_state);
                int s = (st && st->t == MEM_INT) ? st->v : -1;
                if (s != 4) continue;                     // CARD_STATE.SLEEP == 4
                Value one; one.t = MEM_INT; one.v = 1;
                inst_var_set_sid(id, S_awake_buff_timer, one);
            }
        }
    }
}

// VM_GetPlantAt(col,row,layer)：第一个匹配的植物实例 id；没有 → -1
//   （VM_ClientWrapId 目前是转发占位 → 先直接返回实例 id，语义待宿主映射）
void impl_VM_GetPlantAt(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    Value lv = rd_v(vm, argslot(args, 2));
    bool all = (lv.t == MEM_STRING && lv.v == S_all);
    int cols = grid_dim(vm, S_grid_cols), rows = grid_dim(vm, S_grid_rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            std::vector<Value>* lst = plant_cell(vm, c, r);
            if (!lst) continue;
            for (size_t i = 0; i < lst->size(); i++) {
                Value e = (*lst)[i];
                int id = (e.t == MEM_INT) ? e.v : -1;
                if (id < 0 || !inst_get(id)) continue;
                if (!all) {
                    Value* t = inst_var_sid(id, S_plant_type);
                    if (!(t && t->t == MEM_STRING && lv.t == MEM_STRING && t->v == lv.v)) continue;
                }
                wr_i(vm, dst, id);
                return;
            }
        }
    }
    wr_i(vm, dst, -1);
}

// VM_GetPlantCountAt(col,row,type)：数匹配的植物
void impl_VM_GetPlantCountAt(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int col = (int)g_num_arg(vm, args, 0), row = (int)g_num_arg(vm, args, 1);
    Value tv = rd_v(vm, argslot(args, 2));
    bool all = (tv.t == MEM_STRING && tv.v == S_all);
    int cols = grid_dim(vm, S_grid_cols), rows = grid_dim(vm, S_grid_rows);
    int c1 = (col == -1) ? 0 : col, c2 = (col == -1) ? cols - 1 : col;
    int r1 = (row == -1) ? 0 : row, r2 = (row == -1) ? rows - 1 : row;
    int n = 0;
    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            std::vector<Value>* lst = plant_cell(vm, c, r);
            if (!lst) continue;
            for (size_t i = 0; i < lst->size(); i++) {
                Value e = (*lst)[i];
                int id = (e.t == MEM_INT) ? e.v : -1;
                if (id < 0 || !inst_get(id)) continue;
                if (!all) {
                    Value* pid = inst_var_sid(id, S_plant_id);
                    if (!(pid && pid->t == MEM_STRING && tv.t == MEM_STRING && pid->v == tv.v)) continue;
                }
                n++;
            }
        }
    }
    wr_i(vm, dst, n);
}

// VM_ClearPlantsByType(name)：name == -1 → 清所有非玩家植物
//   按卡片精确清需要 deck_get_card_data（还没实现）→ 那种情况只提示一次
void impl_VM_ClearPlantsByType(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)argc; (void)dst;
    Value nv = rd_v(vm, argslot(args, 0));
    bool all = (nv.t == MEM_INT && nv.v == -1);
    if (!all) {
        static bool warned = false;
        if (!warned) { warned = true; printf("[vm] VM_ClearPlantsByType: 按卡片清需要 deck_get_card_data（未实现），本次跳过\n"); }
        return;
    }
    int cols = grid_dim(vm, S_grid_cols), rows = grid_dim(vm, S_grid_rows);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            std::vector<Value>* lst = plant_cell(vm, c, r);
            if (!lst) continue;
            for (int i = (int)lst->size() - 1; i >= 0; i--) {
                Value e = (*lst)[i];
                int id = (e.t == MEM_INT) ? e.v : -1;
                if (id < 0 || !inst_get(id)) continue;
                if (is_player(vm, id)) continue;
                inst_destroy(id);
            }
        }
    }
}

// VM_GetPlantCount()：数活着的 obj_card_parent
void impl_VM_GetPlantCount(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(vm, dst, inst_count(std::string("obj_card_parent")));
}

void ds_map_size(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_size);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, (int)g_map_pool[m].size());
}
void ds_map_clear(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_clear);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    map_drop_children(m);
    wr_i(vm, dst, 0);
}
void ds_map_keys_to_array(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_keys_to_array);
    if (m < 0) return;
    std::vector<Value> keys;
    for (std::map<Value, Value, ValueLess>::iterator it = g_map_pool[m].begin(); it != g_map_pool[m].end(); ++it) {
        retain(it->first);                // 键又被新数组存了一份
        keys.push_back(it->first);
    }
    int a = arr_take();
    Value self{MEM_ARRAY, a};
    for (size_t k = 0; k < keys.size(); k++) allow_store(keys[k], self, S_ds_map_keys_to_array);
    g_arr_pool[a] = keys;
    give_handle(vm, dst, self);
}
void ds_map_destroy(Vm& vm, const uint8_t* args, int argc, int dst) {
    int m = handle_arg(vm, args, 0, MEM_MAP, S_ds_map_destroy);
    if (m < 0) { wr_i(vm, dst, 0); return; }
    release(Value{MEM_MAP, m});           // 调用方交还自己那份引用
    wr_i(vm, dst, 0);
}

// ── 结构体：就是键是字符串的字典 ──
void struct_get(Vm& vm, const uint8_t* args, int argc, int dst) { ds_map_find_value(vm, args, argc, dst); }
void struct_set(Vm& vm, const uint8_t* args, int argc, int dst) { ds_map_set(vm, args, argc, dst); }
void struct_exists(Vm& vm, const uint8_t* args, int argc, int dst) { ds_map_exists(vm, args, argc, dst); }

// ── 二维表：一维连续内存 + 宽高，下标 = r * w + c ──
void ds_grid_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    int w = rd_i(vm, argslot(args, 0)), h = rd_i(vm, argslot(args, 1));
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    int gi = grid_take();
    Grid& g = g_grid_pool[gi];
    g.w = w;
    g.h = h;
    Value z; z.t = MEM_INT; z.v = 0;
    g.cells.assign((size_t)w * (size_t)h, z);   // 0 不是容器，不用 retain
    give_handle(vm, dst, Value{MEM_GRID, gi});
}
void ds_grid_get(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_get);
    if (gi < 0) return;
    int c = rd_i(vm, argslot(args, 1)), r = rd_i(vm, argslot(args, 2));
    Grid& g = g_grid_pool[gi];
    if (c < 0 || c >= g.w || r < 0 || r >= g.h) return;
    wr_v(vm, dst, g.cells[(size_t)r * g.w + c]);
}
void ds_grid_set(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_set);
    if (gi < 0) return;
    int c = rd_i(vm, argslot(args, 1)), r = rd_i(vm, argslot(args, 2));
    Grid& g = g_grid_pool[gi];
    if (c < 0 || c >= g.w || r < 0 || r >= g.h) return;
    size_t k = (size_t)r * g.w + c;
    Value nv = rd_v(vm, argslot(args, 3));
    if (!allow_store(nv, Value{MEM_GRID, gi}, S_ds_grid_set)) return;
    Value ov = g.cells[k];
    retain(nv);
    if (ov.t != nv.t || ov.v != nv.v) release(ov);
    g.cells[k] = nv;
    wr_i(vm, dst, 0);
}
void ds_grid_width(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_width);
    if (gi < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, g_grid_pool[gi].w);
}
void ds_grid_height(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_height);
    if (gi < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, g_grid_pool[gi].h);
}
void ds_grid_clear(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_clear);
    if (gi < 0) return;
    Grid& g = g_grid_pool[gi];
    Value nv = rd_v(vm, argslot(args, 1));
    if (!allow_store(nv, Value{MEM_GRID, gi}, S_ds_grid_clear)) return;
    for (size_t k = 0; k < g.cells.size(); k++) {         // 旧格子里的引用全放掉
        Value ov = g.cells[k];
        retain(nv);                                      // 每个格子存一份新的
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        g.cells[k] = nv;
    }
    wr_i(vm, dst, 0);
}
void ds_grid_destroy(Vm& vm, const uint8_t* args, int argc, int dst) {
    int gi = handle_arg(vm, args, 0, MEM_GRID, S_ds_grid_destroy);
    if (gi < 0) { wr_i(vm, dst, 0); return; }
    release(Value{MEM_GRID, gi});
    wr_i(vm, dst, 0);
}

// ── 列表（GML ds_list：引擎级全局 id，池在 g_list_pool）──
void ds_list_create(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = list_take();
    give_handle(vm, dst, Value{MEM_LIST, l});
}
void ds_list_destroy(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_destroy);
    if (l < 0) { wr_i(vm, dst, 0); return; }
    release(Value{MEM_LIST, l});
    wr_i(vm, dst, 0);
}
void ds_list_clear(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_clear);
    if (l < 0) { wr_i(vm, dst, 0); return; }
    list_drop_children(l);
    wr_i(vm, dst, 0);
}
void ds_list_add(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_add);
    if (l < 0) { wr_i(vm, dst, -1); return; }
    for (int k = 1; k < argc; k++) {                      // ds_list_add 可以一次加多个
        Value nv = rd_v(vm, argslot(args, k));
        if (!allow_store(nv, Value{MEM_LIST, l}, S_ds_list_add)) { wr_i(vm, dst, -1); return; }
        retain(nv);
        g_list_pool[l].push_back(nv);
    }
    wr_i(vm, dst, 0);
}
void ds_list_delete(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_delete);
    if (l < 0) { wr_i(vm, dst, 0); return; }
    int i = rd_i(vm, argslot(args, 1));
    if (i >= 0 && i < (int)g_list_pool[l].size()) {
        release(g_list_pool[l][i]);       // 删掉的那个元素不再被列表引用
        g_list_pool[l].erase(g_list_pool[l].begin() + i);
    }
    wr_i(vm, dst, 0);
}
void ds_list_size(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_size);
    if (l < 0) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, (int)g_list_pool[l].size());
}
void ds_list_find_value(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_find_value);
    if (l < 0) return;
    int i = rd_i(vm, argslot(args, 1));
    if (i < 0 || i >= (int)g_list_pool[l].size()) return;
    wr_v(vm, dst, g_list_pool[l][i]);
}
void ds_list_find_index(Vm& vm, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(vm, args, 0, MEM_LIST, S_ds_list_find_index);
    if (l < 0) { wr_i(vm, dst, -1); return; }
    Value v = rd_v(vm, argslot(args, 1));
    for (size_t i = 0; i < g_list_pool[l].size(); i++)
        if (g_list_pool[l][i].t == v.t && g_list_pool[l][i].v == v.v) { wr_i(vm, dst, (int)i); return; }
    wr_i(vm, dst, -1);
}

// ── 只读访问（给 vm_builtin.cpp 的 json_stringify 这类"要遍历容器"的函数用）──
//   池是 static 的，外面拿不到，所以从这儿开三个小口子；只读，不改引用计数
int container_size(const Value& v) {
    switch (v.t) {
    case MEM_ARRAY: if (v.v >= 0 && v.v < (int)g_arr_pool.size()  && g_arr_refs[v.v]  > 0) return (int)g_arr_pool[v.v].size();  break;
    case MEM_LIST:  if (v.v >= 0 && v.v < (int)g_list_pool.size() && g_list_refs[v.v] > 0) return (int)g_list_pool[v.v].size(); break;
    case MEM_MAP:   if (v.v >= 0 && v.v < (int)g_map_pool.size()  && g_map_refs[v.v]  > 0) return (int)g_map_pool[v.v].size();  break;
    }
    return 0;
}
Value container_at(const Value& v, int i) {          // 数组 / 列表（map 走 map_pairs）
    if (v.t == MEM_ARRAY && v.v >= 0 && v.v < (int)g_arr_pool.size() && g_arr_refs[v.v] > 0
            && i >= 0 && i < (int)g_arr_pool[v.v].size()) return g_arr_pool[v.v][(size_t)i];
    if (v.t == MEM_LIST && v.v >= 0 && v.v < (int)g_list_pool.size() && g_list_refs[v.v] > 0
            && i >= 0 && i < (int)g_list_pool[v.v].size()) return g_list_pool[v.v][(size_t)i];
    Value u; u.t = MEM_UNDEFINED; u.v = 0;
    return u;
}
std::vector<std::pair<Value, Value> > map_pairs(const Value& v) {
    std::vector<std::pair<Value, Value> > out;
    if (v.t != MEM_MAP || v.v < 0 || v.v >= (int)g_map_pool.size() || g_map_refs[v.v] <= 0) return out;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[v.v];
    for (std::map<Value, Value, ValueLess>::iterator it = mp.begin(); it != mp.end(); ++it)
        out.push_back(std::make_pair(it->first, it->second));
    return out;
}

// 只读查一个 key：**不动引用计数、不碰 scratch 槽**，找到返回 true。
//   给 hot/ 里 m_get 用 —— 那边的句柄常常是"借来的"（引用只在 scratch 槽里），
//   再走一次 ds_map_find_value 的话，call_ret 会先把那份引用放掉，查的就是个死 map
bool map_find(const Value& v, const Value& key, Value* out) {
    if (v.t != MEM_MAP || v.v < 0 || v.v >= (int)g_map_pool.size() || g_map_refs[v.v] <= 0) return false;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[v.v];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
    if (it == mp.end()) return false;
    if (out) *out = it->second;
    return true;
}

// merge_color(col1, col2, amount)：GM 的颜色是 BGR 三个字节，按分量线性插值
//   （原名 merge_colour 是英式拼法，两个名字一个实现）
void merge_colour(Vm& vm, const uint8_t* args, int argc, int dst) {
    int c1 = rd_i(vm, argslot(args, 0));
    int c2 = rd_i(vm, argslot(args, 1));
    double amount = 0;
    {
        int slot = argslot(args, 2);
        if (rd_t(vm, slot) == MEM_FLOAT) { float f; int iv = rd_i(vm, slot); memcpy(&f, &iv, 4); amount = (double)f; }
        else amount = (double)rd_i(vm, slot);
    }
    if (amount < 0) amount = 0;
    if (amount > 1) amount = 1;
    int out = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        int a = (c1 >> shift) & 255;
        int b = (c2 >> shift) & 255;
        int v = (int)((double)a + ((double)b - (double)a) * amount + 0.5);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        out |= v << shift;
    }
    wr_i(vm, dst, out);
}

// ── 还没写的 ──
void VM_Execute(Vm& vm, const uint8_t* args, int argc, int dst) { (void)vm; (void)args; (void)argc; (void)dst; }
