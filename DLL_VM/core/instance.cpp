// ============================================================
// instance.cpp — 内层：实例的一切操作
//
//   槽位 = 在 g_instances 里的位置，**可复用**（死了进 g_inst_free）
//   id   = 实例的身份，**只增不减**（永不复用）→ 旧引用不会撞上新实例
//   g_inst_alive（id → 槽位）是"活没活着"的**唯一权威**（没有 alive 字段）
//
//   每个 VM 一份名下实例的容器（存在 vm_bin 里）：
//     <16 用 arr（存槽位）
//     ≥16 用 mp（存槽位）+ 游标 cur / cur_rank，**转过去就不回来**
//     ⚠ 集合空 = 还没转过（转过之后 arr 就不用了）
//   两种模式都保序；取第 k 个：|k-cur_rank| / k / (n-1-k) 三条路走最短的
// ============================================================
#include "vm_def.h"
#include <cstdlib>          // rand（打乱用）

// ============================================================
// 内置字段的默认值（照 GML）：
//   位置 / 动画帧 / 角度 / 速度类 = 0，缩放和透明度 = 1，
//   sprite_index = -1（没有精灵），visible = 1，image_blend = c_white(16777215)
// ============================================================
void inst_attr_init(Instance& in) {
    Value zero = mk_int(0);
    for (int i = 0; i < FIELD_COUNT; i++) in.attr[i] = zero;
    VM_FLOAT one = (VM_FLOAT)1;             // 槽里的默认值（用 VM_FLOAT，别写死 float）
    Value f1; f1.t = MEM_FLOAT; f1.v = i_of(one);
    in.attr[F_image_xscale] = f1;
    in.attr[F_image_yscale] = f1;
    in.attr[F_image_alpha]  = f1;
    in.attr[F_image_speed]  = f1;
    in.attr[F_sprite_index] = mk_int(-1);
    in.attr[F_visible]      = mk_int(1);
    in.attr[F_image_blend]  = mk_int(16777215);
}

// ============================================================
// VM 名下的实例容器（对外只暴露 inst_new / inst_kill / inst_kth）
// ============================================================
void vm_inst_add(vm_bin& vm, int slot) {
    vm.inst_count++;
    if (vm.mp.empty()) {                       // 数组模式（还没转过）
        vm.arr.push_back(slot);
        if ((int)vm.arr.size() > 16) {         // 跨过 16 → 转集合 + 游标（不回）
            for (size_t i = 0; i < vm.arr.size(); i++) vm.mp.insert(vm.arr[i]);
            vm.arr.clear();
            vm.cur = vm.mp.begin();
            vm.cur_rank = 0;
        }
        return;
    }
    int pos = (int)std::distance(vm.mp.begin(), vm.mp.lower_bound(slot));
    vm.mp.insert(slot);
    if (pos <= vm.cur_rank) vm.cur_rank++;     // 插在游标前面 → 排名 +1（游标节点没变）
}

void vm_inst_erase(vm_bin& vm, int slot) {
    if (vm.inst_count > 0) vm.inst_count--;
    if (vm.mp.empty()) {                       // 数组模式：位移保序
        for (size_t i = 0; i < vm.arr.size(); i++)
            if (vm.arr[i] == slot) { vm.arr.erase(vm.arr.begin() + i); return; }
        return;
    }
    std::set<int>::iterator it = vm.mp.find(slot);
    if (it == vm.mp.end()) return;
    int pos = (int)std::distance(vm.mp.begin(), it);
    if (pos == vm.cur_rank) {                  // 删的就是游标那个
        int n = (int)vm.mp.size();
        if (vm.cur_rank + 1 < n) { std::set<int>::iterator nx = it; ++nx; vm.mp.erase(it); vm.cur = nx; }
        else if (vm.cur_rank > 0) { std::set<int>::iterator pv = it; --pv; vm.mp.erase(it); vm.cur = pv; vm.cur_rank--; }
        else { vm.mp.erase(it); vm.cur = vm.mp.end(); vm.cur_rank = 0; }
    } else {
        if (pos < vm.cur_rank) vm.cur_rank--;  // 删的在游标前面 → 排名 -1（游标不动）
        vm.mp.erase(it);
    }
    if (vm.mp.empty()) { vm.cur = vm.mp.end(); vm.cur_rank = 0; }   // 空了 → 游标归位
}

// 第 k 个实例 id（越界 → -1）
VM_INT inst_kth(vm_bin& vm, VM_INT k) {
    if (k < 0) return -1;
    if (vm.mp.empty()) {
        if (k >= (VM_INT)vm.arr.size()) return -1;
        int slot = vm.arr[(size_t)k];
        return (slot > 0 && slot < (int)g_instances.size()) ? g_instances[(size_t)slot].id : -1;
    }
    int n = (int)vm.mp.size();
    if (k >= n) return -1;
    if (vm.cur_rank < 0 || vm.cur_rank >= n) { vm.cur = vm.mp.begin(); vm.cur_rank = 0; }   // 游标失位（防御）
    int kk = (int)k;
    int d_cur = kk - vm.cur_rank, d_beg = kk, d_end = n - 1 - kk;
    int ad = (d_cur < 0) ? -d_cur : d_cur;
    if (ad <= d_beg && ad <= d_end) {          // 游标最近 → 加加减减
        if (d_cur > 0) { for (int i = 0; i < d_cur; i++)  { ++vm.cur; ++vm.cur_rank; } }
        else           { for (int i = 0; i < -d_cur; i++) { --vm.cur; --vm.cur_rank; } }
    } else if (d_beg <= d_end) {               // 从头走
        vm.cur = vm.mp.begin(); vm.cur_rank = 0;
        for (int i = 0; i < kk; i++) { ++vm.cur; ++vm.cur_rank; }
    } else {                                   // 从尾走
        vm.cur = vm.mp.end(); --vm.cur; vm.cur_rank = n - 1;
        for (int i = n - 1; i > kk; i--) { --vm.cur; --vm.cur_rank; }
    }
    int slot = *vm.cur;
    return (slot > 0 && slot < (int)g_instances.size()) ? g_instances[(size_t)slot].id : -1;
}

// ============================================================
// 建 / 取 / 销
// ============================================================
VM_INT inst_new(vm_bin& vm, VM_INT obj_sid) {
    int slot;
    if (!g_inst_free.empty()) {
        slot = g_inst_free.back();              // 复用回收的**槽位**
        g_inst_free.pop_back();
        Instance& in = g_instances[(size_t)slot];
        in.vars.clear();                        // 上一任的不许留给新实例
        in.arrays.clear();
        in.obj = obj_sid;
        inst_attr_init(in);                     // 内置字段回默认值
    } else {
        if (g_instances.empty()) g_instances.push_back(Instance());   // 0 号留位：全局 / 当前实例的哨兵
        Instance in;
        in.vars.clear();
        in.arrays.clear();
        in.obj = obj_sid;
        inst_attr_init(in);
        g_instances.push_back(in);
        slot = (int)g_instances.size() - 1;
    }
    VM_INT id = ++g_next_id;                    // **id 只增不减**，和槽位无关
    g_instances[(size_t)slot].id = id;
    g_inst_alive[id] = slot;                    // 登记：id → 槽位
    vm_inst_add(vm, slot);                      // 记进 owner 名下的容器
    // 建和销对称：销毁在 inst_destroy 里跑 Destroy，创建就在这儿跑 Create
    //   （实例已经登记好了才跑 → Create 里能用自己、能 instance_destroy 自己）
    vm_run_event(vm, S__OBJECT_CREATE, id);
    return id;
}

Instance* inst_get(VM_INT id) {
    std::unordered_map<VM_INT, int>::iterator it = g_inst_alive.find(id);
    if (it == g_inst_alive.end()) return NULL;  // 没有 / 已死
    int slot = it->second;
    if (slot <= 0 || slot >= (int)g_instances.size()) return NULL;
    return &g_instances[(size_t)slot];          // 表里有 = 活着
}

bool inst_alive(VM_INT id) { return g_inst_alive.find(id) != g_inst_alive.end(); }

// 只回收（不跑事件）：从 VM 的容器摘掉 → 释放槽位
void inst_kill(VM_INT id) {
    std::unordered_map<VM_INT, int>::iterator it = g_inst_alive.find(id);
    if (it == g_inst_alive.end()) return;       // 已经死了，别重复回收
    int slot = it->second;
    g_inst_alive.erase(it);
    if (slot <= 0 || slot >= (int)g_instances.size()) return;
    Instance& in = g_instances[(size_t)slot];
    vm_bin* v = vm_of_object_sid(in.obj);       // 一个对象一个 VM → 名下的容器就在它身上
    if (v) vm_inst_erase(*v, slot);
    in.obj = 0;
    g_inst_free.push_back(slot);                // **槽位**进复用队列（内容留着，复用时清）
}

// 正在跑 _OBJECT_DESTROY 的实例：Destroy 里再销毁自己 → 直接返回，不然无限递归
static std::set<VM_INT> g_inst_dying;

// 销毁**唯一入口**：跑 Destroy 事件 → 回收
void inst_destroy(VM_INT id) {
    if (g_inst_dying.find(id) != g_inst_dying.end()) return;
    Instance* in = inst_get(id);
    if (!in) return;
    vm_bin* v = vm_of_object_sid(in->obj);
    g_inst_dying.insert(id);
    if (v) vm_run_event(*v, S__OBJECT_DESTROY, id);
    g_inst_dying.erase(id);
    inst_kill(id);
}

// ============================================================
// 按对象名问（一个对象一个 VM，所以直接问那个 VM 的容器）
// ============================================================
VM_INT obj_count(VM_INT obj_sid) {
    vm_bin* v = vm_of_object_sid(obj_sid);
    return v ? v->inst_count : 0;
}

VM_INT inst_at(VM_INT obj_sid, VM_INT k) {
    vm_bin* v = vm_of_object_sid(obj_sid);
    return v ? inst_kth(*v, k) : -1;
}

// ============================================================
// 属性读写（实例用引用：手上已经有实例本体时用它）
//   内置字段优先（一次区间判断）；其余一律走 vars
//   读不存在的属性 → NULL（调用方给 undefined，不是报错）
// ============================================================
Value* inst_prop(Instance& in, VM_INT sid) {
    Value* f = inst_attr(in, sid);
    if (f) return f;
    std::unordered_map<VM_INT, Value>::iterator it = in.vars.find(sid);
    return (it == in.vars.end()) ? NULL : &it->second;
}

//   写进实例变量 = 存进一个**长驻位置** → 引用计数的账在这里结
//   （和 slot_write / inst_arr_set 一个规矩：先 retain 新的，再 release 旧的）
void inst_prop_set(Instance& in, VM_INT sid, Value v) {
    Value* f = inst_attr(in, sid);
    if (f) {
        if (f->t != v.t || f->v != v.v) { retain(v); release(*f); }
        *f = v;
        return;
    }
    std::unordered_map<VM_INT, Value>::iterator it = in.vars.find(sid);
    if (it == in.vars.end()) { retain(v); in.vars[sid] = v; return; }
    if (it->second.t == v.t && it->second.v == v.v) return;
    retain(v);
    release(it->second);
    it->second = v;
}

// 手上只有 id：套一次 inst_get
Value* inst_var_sid(VM_INT id, VM_INT sid) {
    Instance* in = inst_get(id);
    return in ? inst_prop(*in, sid) : NULL;
}

void inst_var_set_sid(VM_INT id, VM_INT sid, Value v) {
    Instance* in = inst_get(id);
    if (in) inst_prop_set(*in, sid, v);
}

// ============================================================
// 实例的数组（Instance::arrays）
//   语义照 GML：数组就挂在"这个实例的这个名字"上，就地改
//   元素可能是池里的容器句柄 → 存进去 retain、拿出来 / 删掉 release
//   （实例数组本身不是句柄、也没有引用计数，所以不参与"逆序拒收"那套）
// ============================================================
std::vector<Value>* inst_arr(Instance& in, VM_INT sid) {
    std::unordered_map<VM_INT, std::vector<Value>>::iterator it = in.arrays.find(sid);
    return (it == in.arrays.end()) ? NULL : &it->second;
}

bool inst_arr_exists(Instance& in, VM_INT sid) { return inst_arr(in, sid) != NULL; }

// 要一个能写的数组：有就用，没有就建空的
static std::vector<Value>& arr_ref(Instance& in, VM_INT sid) { return in.arrays[sid]; }

void inst_arr_create(Instance& in, VM_INT sid, VM_INT n, Value fill) {
    std::vector<Value>& a = arr_ref(in, sid);
    for (size_t k = 0; k < a.size(); k++) release(a[k]);
    a.clear();
    if (n < 0) n = 0;
    a.reserve((size_t)n);
    for (VM_INT k = 0; k < n; k++) { retain(fill); a.push_back(fill); }   // 每个位置各一份引用
}

VM_INT inst_arr_size(Instance& in, VM_INT sid) {
    std::vector<Value>* a = inst_arr(in, sid);
    return a ? (VM_INT)a->size() : 0;
}

Value inst_arr_item(Instance& in, VM_INT sid, VM_INT k) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a || k < 0 || k >= (VM_INT)a->size()) return mk_undef();
    return (*a)[(size_t)k];                    // 不加引用：写槽走 wr_v（它会 retain）
}

void inst_arr_set(Instance& in, VM_INT sid, VM_INT k, Value v) {
    if (k < 0) return;
    std::vector<Value>& a = arr_ref(in, sid);
    if (k >= (VM_INT)a.size()) {
        a.resize((size_t)k + 1, mk_int(0));    // 补 0（GML 也一样；0 不是容器，不用 retain）
    }
    Value ov = a[(size_t)k];
    retain(v);                                 // 先拿新的
    if (ov.t != v.t || ov.v != v.v) release(ov);   // 再放旧的
    a[(size_t)k] = v;
}

void inst_arr_add(Instance& in, VM_INT sid, Value v) {
    retain(v);
    arr_ref(in, sid).push_back(v);
}

Value inst_arr_pop(Instance& in, VM_INT sid) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a || a->empty()) return mk_undef();
    Value v = a->back();
    a->pop_back();
    return v;                                  // 那一份引用转交给调用方（用完要 release）
}

VM_INT inst_arr_index(Instance& in, VM_INT sid, Value v) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a) return -1;
    for (size_t i = 0; i < a->size(); i++)
        if ((*a)[i].t == v.t && (*a)[i].v == v.v) return (VM_INT)i;
    return -1;                                 // 没有 → -1（GML 也是 -1）
}

void inst_arr_delete(Instance& in, VM_INT sid, VM_INT k, VM_INT n) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a || k < 0 || k >= (VM_INT)a->size() || n <= 0) return;
    if (k + n > (VM_INT)a->size()) n = (VM_INT)a->size() - k;   // 超出就删到末尾
    for (VM_INT i = 0; i < n; i++) release((*a)[(size_t)(k + i)]);
    a->erase(a->begin() + k, a->begin() + k + n);
}

void inst_arr_resize(Instance& in, VM_INT sid, VM_INT n) {
    if (n < 0) n = 0;
    std::vector<Value>& a = arr_ref(in, sid);
    for (size_t i = (size_t)n; i < a.size(); i++) release(a[i]);   // 砍掉的放引用
    a.resize((size_t)n, mk_int(0));                               // 变长填 0
}

void inst_arr_clear(Instance& in, VM_INT sid) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a) return;
    for (size_t i = 0; i < a->size(); i++) release((*a)[i]);
    a->clear();
}

void inst_arr_shuffle(Instance& in, VM_INT sid) {
    std::vector<Value>* a = inst_arr(in, sid);
    if (!a) return;
    for (int i = (int)a->size() - 1; i > 0; i--) {   // 标准 Fisher-Yates
        int j = (int)(rand() % (i + 1));
        Value t = (*a)[(size_t)i];
        (*a)[(size_t)i] = (*a)[(size_t)j];
        (*a)[(size_t)j] = t;
    }
}
