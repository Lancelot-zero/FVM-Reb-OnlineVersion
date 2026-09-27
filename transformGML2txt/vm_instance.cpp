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
// ============================================================

#include "vm.h"
#include <deque>

std::vector<Instance> g_instances;

// 对象名 → 拥有它的 VM（全局一张表，见 vm.h 的说明）
std::unordered_map<std::string, Vm*> g_vm_of_object;

// 全局变量表：名字 → 值（见 vm.h 的说明）
std::unordered_map<std::string, Value> g_globals;

Value* global_var(const std::string& name) {
    std::unordered_map<std::string, Value>::iterator it = g_globals.find(name);
    return (it == g_globals.end()) ? NULL : &it->second;
}

void global_var_set(const std::string& name, Value v) {
    g_globals[name] = v;
}

bool global_exists(const std::string& name) {
    return g_globals.find(name) != g_globals.end();
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
// 内置字段表：名字 → 成员指针
//   既是"名字派发"的查表，也是"内置变量清单"（要遍历全部变量时用它 + vars）
//
//   快路径的规矩（定死了，别再扩）：
//     · **只有内置变量走这张表** —— 它们在任何实例上都存在，读 `inst.x` 不会出错
//     · 其它属性一律走名字 → vars，**不要**给自定义属性编 id/固定槽：
//       `inst.atk` 这种没设过的时候必须返回 undefined（mod 脚本靠这个兜底，
//       平台侧每帧有几十次 VM_IsUndefined），编成固定槽会把这个语义弄没
//     · 以后实现 VM_GetProp / VM_SetProp 时：先 inst_field()，不命中再查 vars；
//       写不存在的属性就地在 vars 里建；读不存在的返回 undefined（不是报错）
//   名字仍是字符串（全局字符串池 + Value 当 key 那套偷懒没做）
// ============================================================
struct FieldDef { const char* name; Value Instance::* ptr; };

static const FieldDef kFields[] = {
    { "x",                 &Instance::x                 },
    { "y",                 &Instance::y                 },
    { "sprite_index",      &Instance::sprite_index      },
    { "image_index",       &Instance::image_index       },
    { "image_speed",       &Instance::image_speed       },
    { "image_xscale",      &Instance::image_xscale      },
    { "image_yscale",      &Instance::image_yscale      },
    { "image_alpha",       &Instance::image_alpha       },
    { "image_angle",       &Instance::image_angle       },
    { "image_blend",       &Instance::image_blend       },
    { "depth",             &Instance::depth             },
    { "speed",             &Instance::speed             },
    { "direction",         &Instance::direction         },
    { "hspeed",            &Instance::hspeed            },
    { "vspeed",            &Instance::vspeed            },
    { "gravity",           &Instance::gravity           },
    { "gravity_direction", &Instance::gravity_direction },
    { "friction",          &Instance::friction          },
    { "visible",           &Instance::visible           },
};
static const size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

Value* inst_field(Instance& in, const std::string& name) {
    for (size_t i = 0; i < kFieldCount; i++)
        if (name == kFields[i].name) return &(in.*(kFields[i].ptr));
    return NULL;
}

// 默认值照 GML：位置/动画帧/角度/速度类 = 0，缩放和透明度 = 1，
// sprite_index = -1（没有精灵），visible = 1，image_blend = c_white(16777215)
void inst_init(Instance& in) {
    Value zero;  zero.t  = MEM_INT;   zero.v  = 0;
    Value one;   one.t   = MEM_FLOAT; { float f = 1.0f; memcpy(&one.v, &f, 4); }
    for (size_t i = 0; i < kFieldCount; i++) in.*(kFields[i].ptr) = zero;
    in.image_xscale = one;
    in.image_yscale = one;
    in.image_alpha  = one;
    in.image_speed  = one;
    in.sprite_index = Value{MEM_INT, -1};
    in.visible      = Value{MEM_INT, 1};
    in.image_blend  = Value{MEM_INT, 16777215};
}

int inst_new(Vm* vm, const std::string& obj) {
    int id;
    if (!g_inst_free.empty()) {
        id = g_inst_free.front();                 // 复用回收的下标
        g_inst_free.pop_front();
        Instance& in = g_instances[(size_t)id];
        in.vars.clear();                          // 上一任的变量不许留给新实例
        in.obj   = obj;
        in.alive = true;
        in.vm    = vm;
        inst_init(in);                            // 内置字段回默认值
    } else {
        if (g_instances.empty()) g_instances.push_back(Instance());   // 0 号留位：全局/当前实例的哨兵
        Instance in;
        in.obj   = obj;
        in.alive = true;
        in.vm    = vm;
        inst_init(in);
        g_instances.push_back(in);
        id = (int)g_instances.size() - 1;
    }
    if (vm) vm->instances.push_back(id);
    return id;
}

Instance* inst_get(int id) {
    if (id <= 0 || id >= (int)g_instances.size()) return NULL;
    return g_instances[(size_t)id].alive ? &g_instances[(size_t)id] : NULL;
}

void inst_kill(int id) {
    if (id <= 0 || id >= (int)g_instances.size()) return;
    Instance& in = g_instances[(size_t)id];
    if (!in.alive) return;                        // 已经死了，别重复回收
    in.alive = false;
    if (in.vm) {                                  // 从 owner 名下摘掉
        std::vector<int>& v = in.vm->instances;
        for (size_t i = 0; i < v.size(); i++)
            if (v[i] == id) { v[i] = v.back(); v.erase(v.end() - 1); break; }   // 和末尾交换再砍尾，不做整段挪动
    }
    in.vm = NULL;
    g_inst_free.push_back(id);                    // 下标进复用队列（内容留着，复用时清）
}

// VM 挂掉：把自己名下的实例全收掉
void Vm::destroy_owned_instances() {
    std::vector<int> ids(instances);              // inst_kill 会改 instances，先拷一份
    for (size_t i = 0; i < ids.size(); i++) inst_kill(ids[i]);
    instances.clear();
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
    int n = 0;
    for (size_t i = 1; i < g_instances.size(); i++)
        if (g_instances[i].alive && g_instances[i].obj == obj) n++;
    return n;
}

int inst_at(const std::string& obj, int k) {
    if (k < 0) return -1;
    for (size_t i = 1; i < g_instances.size(); i++) {
        if (g_instances[i].alive && g_instances[i].obj == obj) {
            if (k == 0) return (int)i;
            k--;
        }
    }
    return -1;
}

Value* inst_var(int id, const std::string& name) {
    Instance* in = inst_get(id);
    if (!in) return NULL;
    Value* f = inst_field(*in, name);
    if (f) return f;                                   // 内置字段优先
    std::map<std::string, Value>::iterator it = in->vars.find(name);
    return (it == in->vars.end()) ? NULL : &it->second;
}

void inst_var_set(int id, const std::string& name, Value v) {
    Instance* in = inst_get(id);
    if (!in) return;
    Value* f = inst_field(*in, name);
    if (f) { *f = v; return; }                         // 内置字段直接写
    in->vars[name] = v;
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
        if (si >= 0 && si < (int)vm.strings.size()) { *out_obj = vm.strings[(size_t)si]; return 2; }
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
    inst_var_set(nid, "x",     rd_v(vm, argslot(args, 0)));
    inst_var_set(nid, "y",     rd_v(vm, argslot(args, 1)));
    inst_var_set(nid, "depth", rd_v(vm, argslot(args, 2)));
    run_block_on(vm, obj, nid, "_OBJECT_CREATE");     // GML：创建时跑 Create
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
    run_block_on(vm, obj, target, "_OBJECT_DESTROY"); // GML：销毁前跑 Destroy
    inst_kill(target);
    wr_i(vm, dst, 0);
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
    if (si < 0 || si >= (int)vm.strings.size()) { wr_i(vm, dst, 0); return; }
    wr_i(vm, dst, inst_var(id, vm.strings[(size_t)si]) ? 1 : 0);
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
    Value* p = inst_var(id, vm.strings[(size_t)rd_i(vm, slot)]);
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
    const std::string& name = vm.strings[(size_t)rd_i(vm, slot)];
    Value* p  = inst_var(id, name);
    Value  nv = rd_v(vm, argslot(args, 2));
    retain(nv);                                       // 存进实例变量 = 多一个引用
    if (p && (p->t != nv.t || p->v != nv.v)) release(*p);
    inst_var_set(id, name, nv);
    wr_i(vm, dst, 0);
}

