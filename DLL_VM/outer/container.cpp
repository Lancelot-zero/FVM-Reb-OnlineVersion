// ============================================================
// container.cpp — 容器类（44 条）[自举]
//
//   数组 / ds_list / ds_map / ds_grid / struct
//   池和引用计数都在内层（core/pool.cpp）：这里只写"取参 → 动池 → 回值"。
//
//   语义照老代码（transformGML2txt/vm_container.cpp）：
//     · 元素进容器 retain、出来 / 删掉 release
//     · 新容器建出来引用计数是 1，交出去走 give_handle
//     · 进容器前问 allow_store(child, parent)：容器只收"比自己新"的 → 环在结构上形成不了
//     · 越界 / 类型不对 → 回值和老代码一致（undefined / 0 / -1 / 空操作，不是崩）
//   池一律直接写（g_arr_pool 等是内层放出来的 extern），不另开一层包装。
// ============================================================
#include "outer.h"
#include <cstdlib>      // rand()：array_shuffle 用

// ── "清空"一个容器：里面的句柄要一个个放掉（一层，不递归）──
//   vector::clear() 是不够的：那样只是把 8 字节丢掉，池里的引用计数不会减。
//   不递归是因为递归会跟着引用图一路走到底，环会卡死在里面；
//   残留的引用由内层复用这个下标时再放（arr_new 那套）。
static void arr_drop(int i) {
    std::vector<Value>& a = g_arr_pool[i];
    for (size_t k = 0; k < a.size(); k++) release(a[k]);
    a.clear();
}
static void map_drop(int i) {
    std::map<Value, Value, ValueLess>& m = g_map_pool[i];
    for (std::map<Value, Value, ValueLess>::iterator it = m.begin(); it != m.end(); ++it) {
        release(it->first);
        release(it->second);
    }
    m.clear();
}
static void list_drop(int i) {
    std::vector<Value>& l = g_list_pool[i];
    for (size_t k = 0; k < l.size(); k++) release(l[k]);
    l.clear();
}

// ── 数组 ──
// array_literal(...)：GML 的 [a, b, c]：建数组 → 元素逐个进池
void array_literal(Value* mem, const uint8_t* args, int argc, int dst) {
    int a = arr_new();
    Value self{MEM_ARRAY, a};
    for (int k = 0; k < argc; k++) {
        Value e = rd_v(mem, argslot(args, k));
        if (!allow_store(e, self)) continue;     // 会成环就不收这个元素
        retain(e);                               // 元素进容器 = 多一个引用
        g_arr_pool[a].push_back(e);
    }
    give_handle(mem, dst, self);
}

// array_set(arr, index, value)：越界就先补 0 到 index（GML 的数组是稀疏的）
void array_set(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, -1); return; }
    VM_INT i = rd_i(mem, argslot(args, 1));
    if (i < 0) { wr_i(mem, dst, -1); return; }
    Value nv = rd_v(mem, argslot(args, 2));
    if (!allow_store(nv, Value{MEM_ARRAY, a})) { wr_i(mem, dst, -1); return; }
    std::vector<Value>& arr = g_arr_pool[a];
    if (i >= (int)arr.size()) arr.resize((size_t)i + 1);   // 变长的那部分是 Value{0,0} = int 0
    Value ov = arr[(size_t)i];
    retain(nv);                                  // 先拿新的
    if (ov.t != nv.t || ov.v != nv.v) release(ov);   // 再放旧的
    arr[(size_t)i] = nv;
    wr_i(mem, dst, 0);
}

// array_get(arr, index)：越界什么都不写（老代码就是这个行为）
void array_get(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) return;
    VM_INT i = rd_i(mem, argslot(args, 1));
    if (i < 0 || i >= (int)g_arr_pool[a].size()) return;
    wr_v(mem, dst, g_arr_pool[a][(size_t)i]);    // wr_v 会 retain，槽接一份引用
}

// array_push(arr, value) → 新下标
void array_push(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, -1); return; }
    Value nv = rd_v(mem, argslot(args, 1));
    if (!allow_store(nv, Value{MEM_ARRAY, a})) { wr_i(mem, dst, -1); return; }
    retain(nv);
    g_arr_pool[a].push_back(nv);
    wr_i(mem, dst, (VM_INT)g_arr_pool[a].size() - 1);
}

// array_pop(arr)：弹出最后一个；容器手上那份引用转给槽（没人接就放掉）
void array_pop(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) return;
    std::vector<Value>& arr = g_arr_pool[a];
    if (arr.empty()) return;
    Value v = arr.back();
    arr.pop_back();
    give_handle(mem, dst, v);
}

// array_clear(arr)：放掉元素（一层），数组本身还在
void array_clear(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    arr_drop(a);
    wr_i(mem, dst, 0);
}

// array_contains(arr, value)：元素比较一律按 [类型 + 值]（1 和 "1" 不是一回事）
void array_contains(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    Value k = rd_v(mem, argslot(args, 1));
    const std::vector<Value>& arr = g_arr_pool[a];
    for (size_t i = 0; i < arr.size(); i++)
        if (arr[i].t == k.t && arr[i].v == k.v) { wr_i(mem, dst, 1); return; }
    wr_i(mem, dst, 0);
}

// ── 数组：长度 / 查下标 / 删 / 改长 / 建 / 打乱 / 抄 ──
//   都是"就地改这个数组"，句柄不变（跟 GML 的数组是一个引用语义）。
void array_length(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, (VM_INT)g_arr_pool[a].size());
}

void array_get_index(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, -1); return; }
    Value k = rd_v(mem, argslot(args, 1));
    const std::vector<Value>& arr = g_arr_pool[a];
    for (size_t i = 0; i < arr.size(); i++)
        if (arr[i].t == k.t && arr[i].v == k.v) { wr_i(mem, dst, (VM_INT)i); return; }
    wr_i(mem, dst, -1);                          // 没有 → -1（GML 也是 -1）
}

// array_delete(arr, index, number)：从 index 起删 number 个，后面的往前挪
void array_delete(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    VM_INT i = rd_i(mem, argslot(args, 1));
    VM_INT n = rd_i(mem, argslot(args, 2));
    std::vector<Value>& arr = g_arr_pool[a];
    if (i < 0 || i >= (int)arr.size() || n <= 0) { wr_i(mem, dst, 0); return; }
    if (i + n > (int)arr.size()) n = (VM_INT)arr.size() - i;   // GML 里超出就删到末尾
    for (VM_INT k = 0; k < n; k++) release(arr[(size_t)i + (size_t)k]);   // 删掉的元素放掉引用
    arr.erase(arr.begin() + i, arr.begin() + i + n);
    wr_i(mem, dst, 0);
}

// array_resize(arr, size)：变长填空 0（GML 填的就是 0），变短放掉被砍掉的
void array_resize(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    VM_INT n = rd_i(mem, argslot(args, 1));
    if (n < 0) n = 0;
    std::vector<Value>& arr = g_arr_pool[a];
    if ((size_t)n < arr.size())
        for (size_t k = (size_t)n; k < arr.size(); k++) release(arr[k]);
    arr.resize((size_t)n);                       // 变长的那部分是真 0（不是容器，不用 retain）
    wr_i(mem, dst, 0);
}

// array_create(size [, value])：建一个新数组，元素都填 value（默认 0）
void array_create(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT n = rd_i(mem, argslot(args, 0));
    if (n < 0) n = 0;
    Value fill{MEM_INT, 0};
    if (argc >= 2) fill = rd_v(mem, argslot(args, 1));
    int a = arr_new();
    Value self{MEM_ARRAY, a};
    // 填的那个值本身是容器、又比新数组老 → 整个数组就只建成空的（老代码的行为）
    if (!allow_store(fill, self)) { give_handle(mem, dst, self); return; }
    for (int k = 0; k < n; k++) { retain(fill); g_arr_pool[a].push_back(fill); }
    give_handle(mem, dst, self);
}

// array_shuffle(arr)：标准 Fisher-Yates（老代码用的 rand()）
void array_shuffle(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int a = handle_arg(mem, args, 0, MEM_ARRAY);
    if (a < 0) { wr_i(mem, dst, 0); return; }
    std::vector<Value>& arr = g_arr_pool[a];
    for (int i = (int)arr.size() - 1; i > 0; i--) {
        int j = (int)(rand() % (i + 1));
        Value t = arr[(size_t)i]; arr[(size_t)i] = arr[(size_t)j]; arr[(size_t)j] = t;
    }
    wr_i(mem, dst, 0);
}

// array_copy(dest, dest_index, src, src_index, length)：把 src 的一段抄进 dest
//   老代码里没有这条，按 GML 手册补：
//     · 两个数组可能是同一个（自抄）→ 先整段读进临时表再写，不然写到一半就把源覆盖了
//     · dest 不够长就变长（新格子填 0）
//     · 负索引从末尾数（-1 = 最后一个），负 length 表示反向抄
//     · 源不够长：缺的那些写 0（GML 这里不报错，我们也不报）
void array_copy(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int da = handle_arg(mem, args, 0, MEM_ARRAY);
    int sa = handle_arg(mem, args, 2, MEM_ARRAY);
    if (da < 0 || sa < 0) { wr_i(mem, dst, 0); return; }
    std::vector<Value>& dv = g_arr_pool[da];
    std::vector<Value>& sv = g_arr_pool[sa];
    VM_INT di = rd_i(mem, argslot(args, 1));
    VM_INT si = rd_i(mem, argslot(args, 3));
    VM_INT n  = (argc >= 5) ? rd_i(mem, argslot(args, 4)) : 0;
    if (n == 0) { wr_i(mem, dst, 0); return; }
    int step = (n < 0) ? -1 : 1;
    if (n < 0) n = -n;
    if (di < 0) di = (VM_INT)dv.size() + di;     // 负偏移：从末尾数
    if (si < 0) si = (VM_INT)sv.size() + si;
    if (di < 0 || si < 0) { wr_i(mem, dst, 0); return; }   // 越界：什么都不动
    if ((size_t)di + (size_t)n > dv.size()) dv.resize((size_t)di + (size_t)n);
    std::vector<Value> tmp;
    tmp.reserve((size_t)n);
    for (int k = 0; k < n; k++) {
        VM_INT s = si + k * step;
        if (s < 0 || s >= (VM_INT)sv.size()) { tmp.push_back(Value{MEM_INT, 0}); continue; }
        retain(sv[(size_t)s]);
        tmp.push_back(sv[(size_t)s]);
    }
    for (int k = 0; k < n; k++) {
        Value nv = tmp[(size_t)k];
        Value ov = dv[(size_t)di + (size_t)k];
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        dv[(size_t)di + (size_t)k] = nv;         // tmp 里那份引用直接转给 dest
    }
    wr_i(mem, dst, 0);
}

// ── ds_list ──
void ds_list_create(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    give_handle(mem, dst, Value{MEM_LIST, list_new()});
}

void ds_list_destroy(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, 0); return; }
    release(Value{MEM_LIST, l});                  // 调用方交还自己那份引用
    wr_i(mem, dst, 0);
}

void ds_list_clear(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, 0); return; }
    list_drop(l);
    wr_i(mem, dst, 0);
}

// ds_list_add(list, value…)：可以一次加多个
void ds_list_add(Value* mem, const uint8_t* args, int argc, int dst) {
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, -1); return; }
    for (int k = 1; k < argc; k++) {
        Value nv = rd_v(mem, argslot(args, k));
        if (!allow_store(nv, Value{MEM_LIST, l})) { wr_i(mem, dst, -1); return; }
        retain(nv);
        g_list_pool[l].push_back(nv);
    }
    wr_i(mem, dst, 0);
}

void ds_list_delete(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, 0); return; }
    VM_INT i = rd_i(mem, argslot(args, 1));
    std::vector<Value>& lst = g_list_pool[l];
    if (i >= 0 && i < (int)lst.size()) {
        release(lst[(size_t)i]);                  // 删掉的那个元素不再被列表引用
        lst.erase(lst.begin() + i);
    }
    wr_i(mem, dst, 0);
}

void ds_list_size(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, (VM_INT)g_list_pool[l].size());
}

void ds_list_find_value(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) return;
    VM_INT i = rd_i(mem, argslot(args, 1));
    if (i < 0 || i >= (int)g_list_pool[l].size()) return;
    wr_v(mem, dst, g_list_pool[l][(size_t)i]);
}

void ds_list_find_index(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int l = handle_arg(mem, args, 0, MEM_LIST);
    if (l < 0) { wr_i(mem, dst, -1); return; }
    Value v = rd_v(mem, argslot(args, 1));
    const std::vector<Value>& lst = g_list_pool[l];
    for (size_t i = 0; i < lst.size(); i++)
        if (lst[i].t == v.t && lst[i].v == v.v) { wr_i(mem, dst, (VM_INT)i); return; }
    wr_i(mem, dst, -1);
}

// ── ds_map / struct（结构体就是键是字符串的字典，同一个池）──
//   struct_get / struct_set / struct_exists 直接转到 ds_map_* 的实现上
//   （老代码里报错打的是 ds_map_* 的名字，现在不打印了，转过去更省事）。
void ds_map_create(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    give_handle(mem, dst, Value{MEM_MAP, map_new()});
}

void ds_map_set(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    Value key = rd_v(mem, argslot(args, 1));
    Value nv  = rd_v(mem, argslot(args, 2));
    Value self{MEM_MAP, m};
    if (!allow_store(key, self)) { wr_i(mem, dst, 0); return; }   // 键也可能是容器
    if (!allow_store(nv, self))  { wr_i(mem, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(key);
    if (it != mp.end()) {
        Value ov = it->second;
        retain(nv);
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        it->second = nv;
    } else {
        retain(key);                              // 新键，map 里存了一份
        retain(nv);
        mp[key] = nv;
    }
    wr_i(mem, dst, 0);
}

// ds_map_add(map, key, value)：键不存在才加（GM 的 ds_map_add 不覆盖老值）
void ds_map_add(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    Value key = rd_v(mem, argslot(args, 1));
    Value nv  = rd_v(mem, argslot(args, 2));
    Value self{MEM_MAP, m};
    if (!allow_store(key, self)) { wr_i(mem, dst, 0); return; }
    if (!allow_store(nv, self))  { wr_i(mem, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    if (mp.find(key) != mp.end()) { wr_i(mem, dst, 0); return; }  // 键在 → 不动老的
    retain(key);
    retain(nv);
    mp[key] = nv;
    wr_i(mem, dst, 0);
}

void ds_map_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, g_map_pool[m].count(rd_v(mem, argslot(args, 1))) ? 1 : 0);
}

void ds_map_find_value(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) return;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(rd_v(mem, argslot(args, 1)));
    if (it != mp.end()) wr_v(mem, dst, it->second);   // wr_v 里 retain
}

void ds_map_delete(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(rd_v(mem, argslot(args, 1)));
    if (it != mp.end()) {
        release(it->first);                       // 键和值都不再被这个字典引用
        release(it->second);
        mp.erase(it);
    }
    wr_i(mem, dst, 0);
}

void ds_map_size(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, (VM_INT)g_map_pool[m].size());
}

void ds_map_clear(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    map_drop(m);
    wr_i(mem, dst, 0);
}

// ds_map_keys_to_array(map)：键拿出来做成新数组
void ds_map_keys_to_array(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) return;
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::vector<Value> keys;
    for (std::map<Value, Value, ValueLess>::iterator it = mp.begin(); it != mp.end(); ++it) {
        retain(it->first);                        // 键又被新数组存了一份
        keys.push_back(it->first);
    }
    int a = arr_new();
    Value self{MEM_ARRAY, a};
    // 这里**故意不问 allow_store**：map 里的键一定比这个新数组老，问了必然拒收，
    // 而"键搬进数组"本来就不是"容器装容器"那种引用语义（老代码在这里也只是空调一次）。
    g_arr_pool[a] = keys;                         // keys 里那份引用转给新数组
    give_handle(mem, dst, self);
}

void ds_map_destroy(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    release(Value{MEM_MAP, m});                   // 调用方交还自己那份引用
    wr_i(mem, dst, 0);
}

void struct_get(Value* mem, const uint8_t* args, int argc, int dst) {
    ds_map_find_value(mem, args, argc, dst);
}

void struct_set(Value* mem, const uint8_t* args, int argc, int dst) {
    ds_map_set(mem, args, argc, dst);
}

void struct_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    ds_map_exists(mem, args, argc, dst);
}

// variable_struct_remove(struct, key)：删结构体的一个键（照 ds_map_delete）
void variable_struct_remove(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int m = handle_arg(mem, args, 0, MEM_MAP);
    if (m < 0) { wr_i(mem, dst, 0); return; }
    std::map<Value, Value, ValueLess>& mp = g_map_pool[m];
    std::map<Value, Value, ValueLess>::iterator it = mp.find(rd_v(mem, argslot(args, 1)));
    if (it != mp.end()) {
        release(it->first);
        release(it->second);
        mp.erase(it);
    }
    wr_i(mem, dst, 0);
}

// ── ds_grid：一维连续内存 + 宽高，下标 = r * w + c ──
void ds_grid_create(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT w = rd_i(mem, argslot(args, 0));
    VM_INT h = rd_i(mem, argslot(args, 1));
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    int gi = grid_new();
    Grid& g = g_grid_pool[gi];
    g.w = w;
    g.h = h;
    g.cells.assign((size_t)w * (size_t)h, Value{MEM_INT, 0});   // 0 不是容器，不用 retain
    give_handle(mem, dst, Value{MEM_GRID, gi});
}

void ds_grid_get(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) return;
    VM_INT c = rd_i(mem, argslot(args, 1));
    VM_INT r = rd_i(mem, argslot(args, 2));
    Grid& g = g_grid_pool[gi];
    if (c < 0 || c >= g.w || r < 0 || r >= g.h) return;
    wr_v(mem, dst, g.cells[(size_t)r * (size_t)g.w + (size_t)c]);
}

void ds_grid_set(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) return;
    VM_INT c = rd_i(mem, argslot(args, 1));
    VM_INT r = rd_i(mem, argslot(args, 2));
    Grid& g = g_grid_pool[gi];
    if (c < 0 || c >= g.w || r < 0 || r >= g.h) return;
    size_t k = (size_t)r * (size_t)g.w + (size_t)c;
    Value nv = rd_v(mem, argslot(args, 3));
    if (!allow_store(nv, Value{MEM_GRID, gi})) return;
    Value ov = g.cells[k];
    retain(nv);
    if (ov.t != nv.t || ov.v != nv.v) release(ov);
    g.cells[k] = nv;
    wr_i(mem, dst, 0);
}

void ds_grid_width(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, g_grid_pool[gi].w);
}

void ds_grid_height(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, g_grid_pool[gi].h);
}

// ds_grid_clear(grid, value)：整张表填上同一个值（尺寸不变）
void ds_grid_clear(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) return;
    Grid& g = g_grid_pool[gi];
    Value nv = rd_v(mem, argslot(args, 1));
    if (!allow_store(nv, Value{MEM_GRID, gi})) return;
    for (size_t k = 0; k < g.cells.size(); k++) {
        Value ov = g.cells[k];
        retain(nv);                               // 每个格子存一份新的
        if (ov.t != nv.t || ov.v != nv.v) release(ov);
        g.cells[k] = nv;
    }
    wr_i(mem, dst, 0);
}

void ds_grid_destroy(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    int gi = handle_arg(mem, args, 0, MEM_GRID);
    if (gi < 0) { wr_i(mem, dst, 0); return; }
    release(Value{MEM_GRID, gi});
    wr_i(mem, dst, 0);
}

// ── ds_exists(句柄, 类型)：句柄是不是这个类型的、且还活着 ──
//   我们只实现 list / map / grid；queue / stack / priority 那些类型号 → 0
void ds_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    VM_INT want = rd_i(mem, argslot(args, 1));
    Value v  = rd_v(mem, argslot(args, 0));
    VM_INT ty;
    switch (want) {
    case 0:  ty = MEM_LIST;  break;               // ds_type_list
    case 1:  ty = MEM_MAP;   break;               // ds_type_map
    case 3:  ty = MEM_GRID;  break;               // ds_type_grid
    default: wr_i(mem, dst, 0); return;           // queue / stack / priority：我们没实现
    }
    wr_i(mem, dst, (v.t == ty && container_alive(v)) ? 1 : 0);
}
