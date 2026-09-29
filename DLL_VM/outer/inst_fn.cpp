// ============================================================
// inst_fn.cpp — 实例类（10 条）[自举]
//
//   instance_exists / instance_number / instance_create_depth / instance_destroy /
//   instance_find / instance_log_enable / instance_log_disable /
//   variable_instance_exists / variable_instance_get / variable_instance_set
//
//   语义真源 = transformGML2txt/vm_instance.cpp（那 471 行），只把 `Vm& vm` 换成 mem：
//   槽读写走 rd_* / wr_* / arg_*，实例本体的操作全交给内层（core/instance.cpp）。
//
//   参数里"实例 id"和"对象名"是两回事，但两条路都收：
//     · MEM_STRING 的实参 = 对象名（v **就是**池下标，不取文本、不做字符串比较）
//     · MEM_INT    的实参 = 实例 id
//   一个对象一个 VM（g_vm_of_object）→ "第 k 个""还活着几个"都去问那个 VM 名下的容器。
//
//   ⚠ 返回值照 GML / 老代码，不"顺手优化"：
//     · instance_create_depth 第 4 个参数不是对象名 → noone(-4)（老代码也是 -4）
//     · 读不存在的实例变量 → undefined（**不是** 0）
//     · instance_find 越界 → noone(-4)：mod 里就是 `if (menu != noone)` 这么判的
//
//   ⚠ 内层/外层都不 printf：参数不对时按上面那几条回值，不置错误码
//     （老代码在这几处只打印；错误表里也没有"参数不对"这一档，
//      乱置一个会盖掉真正的 g_vm_err，宿主可能当成致命错）
// ============================================================
#include "outer.h"

// GML 的 noone。**不能**拿内层 inst_at 的 -1 顶替：mod 脚本用 `!= noone`（-4）判断，
//   回 -1 会让 `instance_find(obj,0)` 的"没有"通过判断，接着去读一个死实例的属性。
//   ⚠ 写成 extern：const 在命名空间作用域默认是**内部链接**，不加就跨不了文件（声明在 outer.h）
extern const VM_INT NOONE = -4;

// ============================================================
// 第 k 个实参当"实例或对象名"看（老代码的 inst_arg 就是这三种结果）：
//   2 = 对象名（*out_obj 给池下标）、1 = 实例 id、0 = 既不是字符串也不是整数
//   out 参数由调用方先置 -1：取不到时照老代码的默认值继续走（而不是崩在解引用上）
//   （不是 static：game_self.cpp 那批 VM_* 也用同一个口径，声明在 outer.h）
// ============================================================
int inst_arg(Value* mem, const uint8_t* args, int k, VM_INT* out_id, VM_INT* out_obj) {
    Value v = arg_v(mem, args, k);
    if (v.t == MEM_STRING) {
        if (!str_ok(v.v)) return 0;         // 池下标都不合法 → 当"不是对象名"，别拿它去查 VM
        *out_obj = v.v;
        return 2;
    }
    if (v.t == MEM_INT) { *out_id = v.v; return 1; }
    return 0;
}

// "实例 id / 对象名" → 一个具体实例 id：对象名取它名下第 0 个（= GML `with(obj)` 取的那个）。
//   没有 → -1（内层 inst_at 的越界约定；调用方自己把它当"没找到"）
VM_INT inst_pick(VM_INT id, VM_INT obj, int kind) {
    if (kind == 2) return inst_at(obj, 0);
    return (kind == 1) ? id : -1;
}

// ============================================================
// instance_exists(实例 / 对象名)
//   问对象 = "名下还有活着的吗"（一次计数，O(1)）；问实例 = 直接查活的表
// ============================================================
void instance_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    if (kind == 2) { wr_i(mem, dst, obj_count(obj) > 0 ? 1 : 0); return; }
    wr_i(mem, dst, inst_get(id) ? 1 : 0);      // kind == 0 时 id 还是 -1 → 0
}

// ============================================================
// instance_number(实例 / 对象名)
//   问实例不是 GML 的标准用法（GML 只收对象），老代码回 1，这里照抄
// ============================================================
void instance_number(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)argc;
    if (argc < 1) { wr_i(mem, dst, 0); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    if (kind == 2) { wr_i(mem, dst, obj_count(obj)); return; }
    wr_i(mem, dst, inst_get(id) ? 1 : 0);
}

// ============================================================
// instance_create_depth(x, y, depth, obj)
//   建实例 → 摆好 x/y/depth → 回 id
//   ⚠ Create 事件在**内层的 inst_new 里**跑（建和销对称），这儿不许再跑一遍
//   第 4 个参数必须是对象名：不是 → noone（老代码只打印，返回值同样是 -4）
// ============================================================
void instance_create_depth(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT id = -1, obj = -1;
    if (argc < 4 || inst_arg(mem, args, 3, &id, &obj) != 2) { wr_i(mem, dst, NOONE); return; }

    // 登记在**对象自己的 VM** 上：内层的 inst_kill 是按 g_vm_of_object[obj] 把实例摘掉的，
    //   挂到调用方 VM 上会和"对象 → VM"那条线错开（个数算不准、槽位永远摘不掉）。
    //   对象没有 VM（纯热块 / 还没装载）才退回调用方 VM —— 老代码一律挂调用方，那是它唯一的选择。
    vm_bin* owner = vm_of_object_sid(obj);
    if (!owner) owner = cur_vm();
    if (!owner) { wr_i(mem, dst, NOONE); return; }   // callbin 帧里连调用方 VM 都没有

    VM_INT nid = inst_new(*owner, obj);
    inst_var_set_sid(nid, S_x,     arg_v(mem, args, 0));
    inst_var_set_sid(nid, S_y,     arg_v(mem, args, 1));
    inst_var_set_sid(nid, S_depth, arg_v(mem, args, 2));
    wr_i(mem, dst, nid);
}

// ============================================================
// instance_destroy([inst])
//   不给参数 = 销毁当前实例（GML 的 instance_destroy()）
//   给对象名 = 销毁它名下第 0 个；那一条链在 GML 里也是"先取一个再毁"
// ============================================================
void instance_destroy(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT target = run_inst();                // 老代码的 vm.cur_instance → 新内层就是调用栈顶
    if (argc >= 1) {
        VM_INT id = -1, obj = -1;
        int kind = inst_arg(mem, args, 0, &id, &obj);
        if (kind == 0) { wr_i(mem, dst, 0); return; }
        target = inst_pick(id, obj, kind);
        if (target < 0) { wr_i(mem, dst, 0); return; }   // 对象一个实例都没有
    }
    if (!inst_get(target)) { wr_i(mem, dst, 0); return; }   // 幂等：已经死了就当没这回事
    inst_destroy(target);                      // **唯一入口**：跑 Destroy 事件 → 回收
    wr_i(mem, dst, 0);
}

// ============================================================
// variable_instance_exists(inst / 对象名, name)
//   内置字段恒存在（第一次区间判断就命中），自定义变量查 vars
//   名字是"没有就是没有"，**不看**它的值是不是 undefined（那是 variable_instance_get 的事）
// ============================================================
void variable_instance_exists(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, 0); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    Instance* in = inst_get(inst_pick(id, obj, kind));
    if (!in) { wr_i(mem, dst, 0); return; }
    Value n = arg_v(mem, args, 1);
    if (n.t != MEM_STRING || !str_ok(n.v)) { wr_i(mem, dst, 0); return; }
    wr_i(mem, dst, inst_prop(*in, n.v) ? 1 : 0);    // 手上已经有实例本体 → 用引用版，省一次查表
}

// ============================================================
// variable_instance_get(inst / 对象名, name)
//   没有 → undefined（GML 的约定，mod 脚本靠 is_undefined 兜底，所以不能回 0）
//   名字池下标不在池里也走"没有"这条路（查 vars 自然查不到）
// ============================================================
void variable_instance_get(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_v(mem, dst, mk_undef()); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    Instance* in = inst_get(inst_pick(id, obj, kind));
    Value n = arg_v(mem, args, 1);
    if (!in || n.t != MEM_STRING) { wr_v(mem, dst, mk_undef()); return; }
    Value* p = inst_prop(*in, n.v);
    wr_v(mem, dst, p ? *p : mk_undef());
}

// ============================================================
// variable_instance_set(inst / 对象名, name, value)
//   没有就建（GML 也这样）；实例不存在 → 静默回 0（老代码一致，不报错）
// ============================================================
void variable_instance_set(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 3) { wr_i(mem, dst, 0); return; }
    VM_INT id = -1, obj = -1;
    int kind = inst_arg(mem, args, 0, &id, &obj);
    Instance* in = inst_get(inst_pick(id, obj, kind));
    Value n = arg_v(mem, args, 1);
    if (!in || n.t != MEM_STRING) { wr_i(mem, dst, 0); return; }

    Value  nv = arg_v(mem, args, 2);
    inst_prop_set(*in, n.v, nv);       // 引用计数的账由内层结（retain 新的 / release 旧的）
    wr_i(mem, dst, 0);
}

// ============================================================
// instance_find(obj, k) —— 对象名下第 k 个活实例
//   没有 / 越界 / 参数不对 → noone(-4)：这是 GML 的返回值，也是 mod 判断的写法
//   （`var menu = instance_find(obj_pause_menu, 0); if (menu != noone) ...`）
//   顺序由内层容器给（arr 模式 = 登记顺序；≥16 转集合模式后 = 槽位顺序），这里不重排
// ============================================================
void instance_find(Value* mem, const uint8_t* args, int argc, int dst) {
    if (argc < 2) { wr_i(mem, dst, NOONE); return; }
    Value o = arg_v(mem, args, 0);
    if (o.t != MEM_STRING || !str_ok(o.v)) { wr_i(mem, dst, NOONE); return; }
    Value kv = arg_v(mem, args, 1);
    VM_INT k;
    if      (kv.t == MEM_INT)   k = kv.v;
    else if (kv.t == MEM_FLOAT) k = (VM_INT)f_of(kv.v);   // 索引是小数就截断成整数
    else { wr_i(mem, dst, NOONE); return; }
    VM_INT got = inst_at(o.v, k);                 // k < 0 / 越界 → -1
    wr_i(mem, dst, (got < 0) ? NOONE : got);
}

// ============================================================
// instance_log_enable / instance_log_disable —— **语义待定，先明确的空操作**
//
//   这两条在 mod 里是**脚本函数**（scripts/src_hook_function、kernel/function 各一份），
//   干的是 GML 侧的记账：
//     enable  = global._evt_log_enabled = true  + 清空 global._evt_created / _evt_destroyed
//     disable = global._evt_log_enabled = false + 清空那两个数组
//   （obj_event_manager 的帧尾就是遍历 _evt_created / _evt_destroyed 记日志的）
//
//   为什么不在 C++ 里照抄：那三个全局的名字不在常量清单里，实现它要运行时 intern 文本、
//   再自己 arr_new 造两个数组塞进全局表；而且脚本侧同名函数跟 native 谁先命中还没定论
//   （定了才谈得上"谁是真源"）。**不瞎编**：这里什么都不做、回 0。
//   ⚠ 不置 VM_E_NOT_IMPL：它在 Step 里每帧都会被调，置了会把真正的 g_vm_err 冲掉。
// ============================================================
void instance_log_enable(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, 0);
}

void instance_log_disable(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_i(mem, dst, 0);
}
