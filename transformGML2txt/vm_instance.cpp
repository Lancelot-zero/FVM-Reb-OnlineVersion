// ============================================================
// vm_instance.cpp — 引擎级全局表
//   1) 实例表 g_instances     下标 = 实例 id，0 不用（0 = "全局/当前实例"的约定值）
//      池化管理，跟四个容器池一个套路：
//        · 销毁 → 下标进 g_inst_free 队列，**内容先留着**（obj / vars 等复用时再清）
//        · 新建 → 先看队列里有没有回收的下标（先来先出），没有再往表尾新增
//      每个实例记一个 owner VM（Instance::vm），VM 里用 instances 维护自己名下的实例 id；
//      VM 析构时把名下的实例一起收掉，免得 owner 指针悬空。
//   2) 对象名 → VM          g_vm_of_object（跨对象派发找 VM 用）
//   3) 全局变量             g_globals（GML 的 global.x，进程级，VM 挂掉也不清）
//   4) 全局字符串池          g_strings / g_str_map（所有 Vm 共用一张，见 vm.h 那一段）
// ============================================================

#include "vm.h"
#include <deque>

std::vector<Instance> g_instances;

// 活着的实例：id（只增不减） → 槽位（可复用）
std::unordered_map<int, int> g_inst_alive;
static int g_next_id = 0;                 // 下一个实例 id（只增不减）
// 正在跑 Destroy 事件的实例（重入保护：Destroy 里再销毁自己 → 直接返回，不然无限递归）
static std::unordered_set<int> g_inst_dying;                 // 下一个实例 id（只增不减）

// ── 全局字符串池 ──
//   文本 → 全局下标，intern 去重；Value.t == MEM_STRING 的 v 就是这里的下标。
//   装载 bin / callbin 时靠 remap_block_strings 把字节码里内联的局部下标换成全局下标。
std::vector<std::string>             g_strings;
std::unordered_map<std::string, int> g_str_map;

static const std::string kEmptyString;

// 预置段灌过没有。必须挡在**任何** intern 之前：函数名要占住固定的下标区间，
//   晚一步进来（比如先合并了某个 bin 的池）下标就整体错位、SID_* 全废。
static bool g_pool_seeded = false;

void pool_seed() {
    if (g_pool_seeded) return;
    g_pool_seeded = true;                                  // 先置位：下面 intern_str 会看到，不再递归回来
    seed_names();                                          // names.cpp：把 S_xxx 那 843 个按顺序灌进来

    // 自检：随便挑几个，确认"真灌进去的下标"和"枚举值"一致。
    //   枚举值和灌池顺序都由 names.h/names.cpp 保证，这里是从另一条路（名字 → 池）验一遍。
    //   错了就是"悄悄读写到别的字符串/属性上"，不报错不崩溃，所以必须开机验。
    struct { const char* n; int sid; } probe[] = {
        { str_at(S__find_right_cell).c_str(), S__find_right_cell },   // native 段头
        { "VM_BanCard",       S_VM_BanCard       },   // game 段头
        { "image_blend",      S_image_blend      },   // 字段段尾
    };
    for (size_t i = 0; i < sizeof(probe) / sizeof(probe[0]); i++) {
        int got = intern_str(probe[i].n);
        if (got != probe[i].sid)
            printf("[vm] 字符串池预置段对不上：%s 在 %d，枚举是 %d\n",
                   probe[i].n, got, probe[i].sid);
    }
}

bool str_ok(int i) { return i >= 0 && i < (int)g_strings.size(); }

const std::string& str_at(int i) {
    return str_ok(i) ? g_strings[(size_t)i] : kEmptyString;
}

// 「名字 → 池下标」的唯一入口（只查不建）：没有 → -1
//   全项目所有"池里有没有这个名字"的判断都走它，别的地方不再直接摸 g_str_map。
int str_find(const std::string& s) {
    std::unordered_map<std::string, int>::iterator it = g_str_map.find(s);
    return (it == g_str_map.end()) ? -1 : it->second;
}

// 「名字 → 池下标」，没有就建（池只增不减）
int intern_str(const std::string& s) {
    if (!g_pool_seeded) pool_seed();                       // 兜底：谁先碰池谁负责把预置段灌上
    int id = str_find(s);
    if (id >= 0) return id;                                // 已有 → 原下标
    g_strings.push_back(s);
    id = (int)g_strings.size() - 1;
    g_str_map[s] = id;
    return id;
}

// 对象名 → 拥有它的 VM（全局一张表，见 vm.h 的说明）
std::unordered_map<std::string, Vm*> g_vm_of_object;

// 全局变量表：名字的池下标 → 值（GML 的 global.x，进程级，VM 挂掉也不清）
std::unordered_map<int, Value> g_globals;

Value* global_var_sid(int sid) {
    std::unordered_map<int, Value>::iterator it = g_globals.find(sid);
    return (it == g_globals.end()) ? NULL : &it->second;
}

void global_var_set_sid(int sid, Value v) { g_globals[sid] = v; }

bool global_exists_sid(int sid) { return g_globals.find(sid) != g_globals.end(); }

// 名字版：一次哈希换池下标；名字没进过池 → 不可能存在
Value* global_var(const std::string& name) {
    int sid = str_find(name);
    return (sid < 0) ? NULL : global_var_sid(sid);
}

void global_var_set(const std::string& name, Value v) { global_var_set_sid(intern_str(name), v); }

bool global_exists(const std::string& name) {
    int sid = str_find(name);
    return (sid >= 0) && global_exists_sid(sid);
}

Vm* vm_of_object(const std::string& obj) {
    std::unordered_map<std::string, Vm*>::iterator it = g_vm_of_object.find(obj);
    return (it == g_vm_of_object.end()) ? NULL : it->second;
}

// mod VM 注册表：mod id → VM（见 vm.h 的说明）
std::unordered_map<std::string, Vm*> g_mod_vms;

Vm* mod_vm(const std::string& mod_id) {
    std::unordered_map<std::string, Vm*>::iterator it = g_mod_vms.find(mod_id);
    return (it == g_mod_vms.end()) ? NULL : it->second;
}

void mod_vm_register(const std::string& mod_id, Vm* vm) {
    if (mod_id.empty()) return;
    std::unordered_map<std::string, Vm*>::iterator it = g_mod_vms.find(mod_id);
    if (it != g_mod_vms.end() && it->second != vm)
        printf("[vm] mod id '%s' already belongs to another VM, taking it over\n", mod_id.c_str());
    g_mod_vms[mod_id] = vm;
    if (vm) vm->mod_id = mod_id;
}

// 被销毁的实例下标，等着复用（先来先出，跟容器池一致）
static std::deque<int> g_inst_free;

// ============================================================
// 19 个内置变量（名单在 vm.h 的 INNER_FIELD_LIST）
//   序号 = Instance::_inner 的下标；名字预置进全局字符串池（SID_FIELD_BASE 起 19 个）。
//   规矩（定死了，别再扩）：
//     · **只有这 19 个走 _inner** —— 它们在任何实例上都存在，读 `inst.x` 不会出错
//     · 其它属性一律走名字 → vars，**不要**给自定义属性编 id/固定槽：
//       `inst.atk` 这种没设过的时候必须返回 undefined（mod 脚本靠这个兜底，
//       平台侧每帧有几十次 VM_IsUndefined），编成固定槽会把这个语义弄没
//     · VM_GetProp / VM_SetProp：属性名的池下标落在字段段就偏移直取，否则查 vars；
//       写不存在的属性就地在 vars 里建；读不存在的返回 undefined（不是报错）
// ============================================================

// 池下标 → 内置字段（不是内置名 → NULL）—— 一次区间判断，没有字符串比较
Value* inst_field_sid(Instance& in, int sid) {
    return sid_is_field(sid) ? &in._inner[field_of_sid(sid)] : NULL;
}

// 名字 → 内置字段：一次哈希拿池下标，再判区间（名字不在池里 = 自定义属性）
Value* inst_field(Instance& in, const std::string& name) {
    int sid = str_find(name);
    return (sid < 0) ? NULL : inst_field_sid(in, sid);
}

// 默认值照 GML：位置/动画帧/角度/速度类 = 0，缩放和透明度 = 1，
// sprite_index = -1（没有精灵），visible = 1，image_blend = c_white(16777215)
void inst_init(Instance& in) {
    Value zero;  zero.t  = MEM_INT;   zero.v  = 0;
    Value one;   one.t   = MEM_FLOAT; { float f = 1.0f; memcpy(&one.v, &f, 4); }
    for (int i = 0; i < FIELD_COUNT; i++) in._inner[i] = zero;
    inst_f(in, F_image_xscale) = one;
    inst_f(in, F_image_yscale) = one;
    inst_f(in, F_image_alpha)  = one;
    inst_f(in, F_image_speed)  = one;
    inst_f(in, F_sprite_index) = Value{MEM_INT, -1};
    inst_f(in, F_visible)      = Value{MEM_INT, 1};
    inst_f(in, F_image_blend)  = Value{MEM_INT, 16777215};
}



// ── InstBox：数组 / map 双模式（见 vm.h 的说明）──
void InstBox::to_map() {
    for (size_t i = 0; i < arr.size(); i++) {
        std::unordered_map<int, int>::iterator it = g_inst_alive.find(arr[i]);
        mp[arr[i]] = (it == g_inst_alive.end()) ? -1 : it->second;   // 槽位从全局表拿
    }
    arr.clear();
    use_map = true;
    cur = mp.begin();
    cur_rank = 0;
}

void InstBox::add(int id, int slot) {
    if (!use_map) {
        arr.push_back(id);                       // id 单调 → 天然升序
        if ((int)arr.size() > 16) to_map();       // 跨过 16 → 转 map（不回）
        return;
    }
    int pos = (int)std::distance(mp.begin(), mp.lower_bound(id));
    mp[id] = slot;
    if (pos <= cur_rank) cur_rank++;              // 插在游标前面 → 排名 +1（游标节点没变）
}

void InstBox::erase(int id) {
    if (!use_map) {
        for (size_t i = 0; i < arr.size(); i++)
            if (arr[i] == id) { arr.erase(arr.begin() + i); return; }   // 位移保序
        return;
    }
    std::map<int, int>::iterator it = mp.find(id);
    if (it == mp.end()) return;
    int pos = (int)std::distance(mp.begin(), it);
    if (pos == cur_rank) {                        // 删的就是游标那个
        int n = (int)mp.size();
        if (cur_rank + 1 < n) { std::map<int, int>::iterator nx = it; ++nx; mp.erase(it); cur = nx; }
        else if (cur_rank > 0) { std::map<int, int>::iterator pv = it; --pv; mp.erase(it); cur = pv; cur_rank--; }
        else { mp.erase(it); cur = mp.end(); cur_rank = 0; }
        return;
    }
    if (pos < cur_rank) cur_rank--;               // 删的在游标前面 → 排名 -1（游标不动）
    mp.erase(it);
}

int InstBox::at(int k) {
    if (k < 0) return -1;
    if (!use_map) return (k < (int)arr.size()) ? arr[(size_t)k] : -1;
    int n = (int)mp.size();
    if (k >= n) return -1;
    if (cur_rank < 0 || cur_rank >= n) { cur = mp.begin(); cur_rank = 0; }   // 游标失位（防御）
    int d_cur = k - cur_rank, d_beg = k, d_end = n - 1 - k;
    int ad = (d_cur < 0) ? -d_cur : d_cur;
    if (ad <= d_beg && ad <= d_end) {             // 游标最近 → 加加减减
        if (d_cur > 0) { for (int i = 0; i < d_cur; i++) { ++cur; ++cur_rank; } }
        else           { for (int i = 0; i < -d_cur; i++) { --cur; --cur_rank; } }
    } else if (d_beg <= d_end) {                  // 从头走
        cur = mp.begin(); cur_rank = 0;
        for (int i = 0; i < k; i++) { ++cur; ++cur_rank; }
    } else {                                      // 从尾走
        cur = mp.end(); --cur; cur_rank = n - 1;
        for (int i = n - 1; i > k; i--) { --cur; --cur_rank; }
    }
    return cur->first;
}

void InstBox::ids(std::vector<int>& out) const {
    out.clear();
    if (!use_map) { out = arr; return; }
    for (std::map<int, int>::const_iterator it = mp.begin(); it != mp.end(); ++it) out.push_back(it->first);
}

int inst_new(Vm* vm, const std::string& obj) {
    int slot;
    if (!g_inst_free.empty()) {
        slot = g_inst_free.front();               // 复用回收的**槽位**
        g_inst_free.pop_front();
        Instance& in = g_instances[(size_t)slot];
        in.vars.clear();                          // 上一任的变量不许留给新实例
        in.obj   = obj;
        in.vm    = vm;
        inst_init(in);                            // 内置字段回默认值
    } else {
        if (g_instances.empty()) g_instances.push_back(Instance());   // 0 号留位：全局/当前实例的哨兵
        Instance in;
        in.obj   = obj;
        in.vm    = vm;
        inst_init(in);
        g_instances.push_back(in);
        slot = (int)g_instances.size() - 1;
    }
    int id = ++g_next_id;                         // **id 只增不减**，和槽位无关
    g_instances[(size_t)slot].id = id;
    g_inst_alive[id] = slot;                      // 登记：id → 槽位
    if (vm) vm->box.add(id, slot);                // owner 名下的容器（数组/map 自动）
    return id;
}

Instance* inst_get(int id) {
    std::unordered_map<int, int>::iterator it = g_inst_alive.find(id);
    if (it == g_inst_alive.end()) return NULL;    // 没有 / 已死
    int slot = it->second;
    if (slot <= 0 || slot >= (int)g_instances.size()) return NULL;
    return &g_instances[(size_t)slot];            // map 里有 = 活着（alive 标志已删）
}

void inst_kill(int id) {
    std::unordered_map<int, int>::iterator it = g_inst_alive.find(id);
    if (it == g_inst_alive.end()) return;         // 已经死了，别重复回收
    int slot = it->second;
    g_inst_alive.erase(it);                       // 从"活着的"里移除
    if (slot <= 0 || slot >= (int)g_instances.size()) return;
    Instance& in = g_instances[(size_t)slot];
    if (in.vm) in.vm->box.erase(id);              // 从 owner 名下摘掉（容器内部负责保序/游标）
    in.vm = NULL;
    g_inst_free.push_back(slot);                  // **槽位**进复用队列（内容留着，复用时清）
}

// VM 挂掉：把自己名下的实例全收掉
void Vm::destroy_owned_instances() {
    std::vector<int> ids;
    box.ids(ids);                                 // inst_kill 会改 box，先拷一份
    for (size_t i = 0; i < ids.size(); i++) inst_kill(ids[i]);
    box.clear();
}

// VM 挂掉：把"对象名 → VM"表里指向自己的条目摘掉（不留悬空指针）
void Vm::unregister_objects() {
    std::unordered_map<std::string, Vm*>::iterator it = g_vm_of_object.begin();
    while (it != g_vm_of_object.end()) {
        if (it->second == this) it = g_vm_of_object.erase(it);
        else ++it;
    }
    // mod id → VM 那张表也要摘（同一个理由）
    std::unordered_map<std::string, Vm*>::iterator mit = g_mod_vms.begin();
    while (mit != g_mod_vms.end()) {
        if (mit->second == this) mit = g_mod_vms.erase(mit);
        else ++mit;
    }
}

int inst_count(const std::string& obj) {
    Vm* v = vm_of_object(obj);                    // 一个 VM = 一个对象 → 直接问它的容器
    return v ? v->box.count() : 0;                // O(1)
}

int inst_at(const std::string& obj, int k) {
    Vm* v = vm_of_object(obj);
    return v ? v->box.at(k) : -1;                 // 数组 O(1) / map 走游标
}

Value* inst_var_sid(int id, int sid) {
    Instance* in = inst_get(id);
    if (!in) return NULL;
    Value* f = inst_field_sid(*in, sid);               // 内置字段优先（一次区间判断）
    if (f) return f;
    std::unordered_map<int, Value>::iterator it = in->vars.find(sid);
    return (it == in->vars.end()) ? NULL : &it->second;
}

void inst_var_set_sid(int id, int sid, Value v) {
    Instance* in = inst_get(id);
    if (!in) return;
    Value* f = inst_field_sid(*in, sid);
    if (f) { *f = v; return; }                         // 内置字段直接写
    in->vars[sid] = v;
}

Value* inst_var(int id, const std::string& name) {
    // 名字先换成池下标（vars 的 key 就是它）：没进过池的名字两边都不可能命中
    int sid = str_find(name);
    return (sid < 0) ? NULL : inst_var_sid(id, sid);
}

void inst_var_set(int id, const std::string& name, Value v) {
    inst_var_set_sid(id, intern_str(name), v);         // 没有就进池（写新属性必然要建 key）
}

// ============================================================
// VM_GetProp / VM_SetProp（属性读写，字节码里那 4 万个调用点走这里）
//   属性名传进来就是**池下标**，所以：
//     · 落在字段段（SID_FIELD_BASE..+19） → `_inner[下标 - SID_FIELD_BASE]`，一次偏移
//     · inst == 0                        → 全局变量（GML 的 global.x）
//     · 其它                             → vars；读不到返回 undefined，写到就地建
// ⚠ 写容器句柄（array/map/grid/list）时不在这里做引用计数 —— 和 inst_var_set 一个口径，
//   容器句柄的 retain/release 由调用方（hot 的 set_var / 容器池）负责。
// ============================================================
static Value undef() { Value u; u.t = MEM_UNDEFINED; u.v = 0; return u; }

static Instance* prop_target(Vm& vm, const uint8_t* args, int* out_global) {
    *out_global = 0;
    int slot = argslot(args, 0);
    if (rd_t(vm, slot) != MEM_INT) return NULL;
    int id = rd_i(vm, slot);
    if (id == 0) { *out_global = 1; return NULL; }     // 0 = 全局
    return inst_get(id);
}

void impl_VM_GetProp(Vm& vm, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(vm, dst, undef()); return; }
    int nslot = argslot(args, 1);
    if (rd_t(vm, nslot) != MEM_STRING) { wr_v(vm, dst, undef()); return; }
    int sid = rd_i(vm, nslot);
    if (!str_ok(sid)) { wr_v(vm, dst, undef()); return; }

    int is_global = 0;
    Instance* in = prop_target(vm, args, &is_global);
    if (is_global) {                                   // 全局：读不到 → undefined
        Value* p = global_var(str_at(sid));
        wr_v(vm, dst, p ? *p : undef());
        return;
    }
    if (!in) { wr_v(vm, dst, undef()); return; }

    Value* f = inst_field_sid(*in, sid);               // 内置字段：偏移直取
    if (f) { wr_v(vm, dst, *f); return; }

    std::unordered_map<int, Value>::iterator it = in->vars.find(sid);
    wr_v(vm, dst, (it == in->vars.end()) ? undef() : it->second);
}

void impl_VM_SetProp(Vm& vm, const uint8_t* args, int argc, int dst) {
    (void)dst;
    if (argc < 3) return;
    int nslot = argslot(args, 1);
    if (rd_t(vm, nslot) != MEM_STRING) return;
    int sid = rd_i(vm, nslot);
    if (!str_ok(sid)) return;
    Value v = rd_v(vm, argslot(args, 2));

    int is_global = 0;
    Instance* in = prop_target(vm, args, &is_global);
    if (is_global) { global_var_set(str_at(sid), v); return; }
    if (!in) return;

    Value* f = inst_field_sid(*in, sid);
    if (f) { *f = v; return; }                         // 内置字段：偏移直写
    in->vars[sid] = v;                                 // 没有就就地建
}

// ============================================================
// 实例相关内置函数
//   和 GML 对齐的几条（照抄引擎语义，别"顺手优化"）：
//     · instance_exists(实例) / instance_exists(对象名) 都收
//     · instance_create_depth 会**跑一次 Create 事件**（GML 就是这么定义的）
//     · instance_destroy 会**跑一次 Destroy 事件**再回收下标
//     · 事件派发走 run_block_on：它会先看硬编码热块，没有才解释字节码
//   槽里的"实例"就是 id（int），对象名是字符串 —— 两种入口都要认。
// ============================================================

// 从参数槽读一个"实例或对象名"：实例 id（>0）返回 1、对象名返回 2、别的返回 0
static int inst_arg(Vm& vm, const uint8_t* args, int k, int* out_id, std::string* out_obj) {
    int slot = argslot(args, k);
    int ty   = rd_t(vm, slot);
    if (ty == MEM_STRING) {
        int si = rd_i(vm, slot);
        if (str_ok(si)) { *out_obj = str_at(si); return 2; }
        return 0;
    }
    if (ty == MEM_INT) { *out_id = rd_i(vm, slot); return 1; }
    return 0;
}

void instance_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    int k = inst_arg(vm, args, 0, &id, &obj);
    if (k == 2) { wr_i(vm, dst, inst_count(obj) > 0 ? 1 : 0); return; }
    wr_i(vm, dst, inst_get(id) ? 1 : 0);              // k == 0 时 id 是 -1 → 0
}

void instance_number(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    int k = inst_arg(vm, args, 0, &id, &obj);
    if (k == 2) { wr_i(vm, dst, inst_count(obj)); return; }
    wr_i(vm, dst, inst_get(id) ? 1 : 0);
}

// instance_create_depth(x, y, depth, obj)：建实例、摆好位置、跑 Create 事件，返回 id
void instance_create_depth(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    if (inst_arg(vm, args, 3, &id, &obj) != 2) {      // 第 4 个参数必须是对象名
        printf("[vm] instance_create_depth: argument 4 must be an object name\n");
        wr_i(vm, dst, -4);                            // noone
        return;
    }
    int nid = inst_new(&vm, obj);
    inst_var_set_sid(nid, S_x,     rd_v(vm, argslot(args, 0)));
    inst_var_set_sid(nid, S_y,     rd_v(vm, argslot(args, 1)));
    inst_var_set_sid(nid, S_depth, rd_v(vm, argslot(args, 2)));
    run_block_on(vm, obj, nid, str_at(S__OBJECT_CREATE));     // GML：创建时跑 Create
    wr_i(vm, dst, nid);
}

// instance_destroy([inst])：不给参数就是当前实例（GML 的 instance_destroy()）
void instance_destroy(Vm& vm, const uint8_t* args, int argc, int dst) {
    int target = vm.cur_instance;
    if (argc >= 1) {
        int id = -1; std::string obj;
        int k = inst_arg(vm, args, 0, &id, &obj);
        if (k == 2) { target = inst_at(obj, 0); if (target < 0) { wr_i(vm, dst, 0); return; } }
        else if (k == 1) target = id;
        else { wr_i(vm, dst, 0); return; }
    }
    Instance* in = inst_get(target);
    if (!in) { wr_i(vm, dst, 0); return; }
    std::string obj = in->obj;                        // ⚠ 先把名字拷出来：inst_kill 之后指针还指着，但别依赖它
    inst_destroy(target);                             // 唯一入口：跑 Destroy 事件 + 回收
    wr_i(vm, dst, 0);
}


// ============================================================
// 销毁一个实例（**唯一入口**）—— 四步，顺序不能换：
//   1) 跑 _OBJECT_DESTROY 事件      ← 必须最先：事件里的 self 变量是靠 id 查
//                                     g_inst_alive 拿到的（inst_var_sid → inst_get）
//   2) 从 g_inst_alive 移除         ← 从此这个 id 查不到（= 不存在了）
//   3) 释放槽位（回 g_inst_free）    ← 内容留着，等复用时清
//   4) 从 owner VM 的 instances 摘掉
//   上面 2~4 都在 inst_kill 里；事件由这里负责跑。
// ============================================================
void inst_destroy(int id) {
    Instance* in = inst_get(id);
    if (!in) return;                       // 幂等：已经死了，或从来不存在
    if (g_inst_dying.count(id)) return;    // 正在销毁它自己的 Destroy 事件里 → 不再进来一次
    g_inst_dying.insert(id);
    Vm* vm = in->vm;
    std::string obj = in->obj;             // 先把名字拷出来（kill 之后别依赖指针）
    if (vm) run_block_on(*vm, obj, id, str_at(S__OBJECT_DESTROY));
    g_inst_dying.erase(id);                // 事件跑完才允许它被真正回收
    inst_kill(id);                         // 移除 + 释放槽位 + 摘 owner
}
// variable_instance_exists(inst, name)：内置字段恒存在；自定义变量查 vars
//   注意这里**不看**当前实例——名字是"没有就是没有"，跟读属性返回 undefined 是两回事
void variable_instance_exists(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    int k = inst_arg(vm, args, 0, &id, &obj);
    if (k == 2) { id = inst_at(obj, 0); }
    Instance* in = inst_get(id);
    if (!in) { wr_i(vm, dst, 0); return; }
    int slot = argslot(args, 1);
    if (rd_t(vm, slot) != MEM_STRING) { printf("[vm] variable_instance_exists: argument 2 must be a string\n"); wr_i(vm, dst, 0); return; }
    int si = rd_i(vm, slot);
    if (!str_ok(si)) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, inst_var(id, str_at(si)) ? 1 : 0);
}

// variable_instance_get(inst, name)：没有 → undefined（不是 0）
void variable_instance_get(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    int k = inst_arg(vm, args, 0, &id, &obj);
    if (k == 2) id = inst_at(obj, 0);
    int slot = argslot(args, 1);
    if (rd_t(vm, slot) != MEM_STRING || !inst_get(id)) {
        Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u);
        return;
    }
    Value* p = inst_var(id, str_at(rd_i(vm, slot)));
    if (!p) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); return; }
    wr_v(vm, dst, *p);
}

// variable_instance_set(inst, name, value)：没有就建（GML 也这样）
void variable_instance_set(Vm& vm, const uint8_t* args, int argc, int dst) {
    int id = -1; std::string obj;
    int k = inst_arg(vm, args, 0, &id, &obj);
    if (k == 2) id = inst_at(obj, 0);
    if (!inst_get(id)) { wr_i(vm, dst, 0); return; }
    int slot = argslot(args, 1);
    if (rd_t(vm, slot) != MEM_STRING) { printf("[vm] variable_instance_set: argument 2 must be a string\n"); wr_i(vm, dst, 0); return; }
    std::string name = str_at(rd_i(vm, slot));      // 拷贝一份：全局池可能被 intern 撑大重分配
    Value* p  = inst_var(id, name);
    Value  nv = rd_v(vm, argslot(args, 2));
    retain(nv);                                       // 存进实例变量 = 多一个引用
    if (p && (p->t != nv.t || p->v != nv.v)) release(*p);
    inst_var_set(id, name, nv);
    wr_i(vm, dst, 0);
}

