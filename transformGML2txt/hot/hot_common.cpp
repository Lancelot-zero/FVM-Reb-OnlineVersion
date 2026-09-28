// 本文件由 vm_hot.cpp 拆出来（热函数：每个对象一个文件）。改完记得跑 build.bat

#include "hot_common.h"

// ============================================================
// vm_hot.cpp — 热点块的 C++ 硬编码实现
//
//   有些块特别热（父对象的 Create / Step 之类，每个实例每帧都要跑），
//   与其解释字节码，不如直接用 C++ 写死放在这儿。
//   注册进 kHotBlocks 的 (对象名, 块名) 会被 Vm::run_named 优先用上。
//
//   为什么需要它：obj_card_parent / obj_enemy_parent / obj_bullet_parent
//   在白名单里是"跳过不转"的，VM 里根本没有它们的块 —— 这些块只能硬编码。
//
//   写新条目的规矩：
//     · 只放"热"的；跑一两次的别往这儿堆，解释器跑就行（别抢解释器的活）
//     · 每个函数返回 true = "我处理了"；返回 false 表示没这套硬编码（一般直接 return true）
//     · 当前实例 = vm.cur_instance（宿在派发事件前设好），读写它的变量走 inst_var/inst_var_set
//     · 跨实例（别的对象/别人）用 inst_at("对象名", k) 找 id，再 inst_var(_set)
//     · 调内部函数/游戏 API 用 call_fn(vm, "名字", 值数组, 个数, dst)：
//       它把值写进 SCRATCH 槽再按名字派发；dst = -1 表示不要返回值
//     · 语义必须跟 GML 那边一模一样（这是"换个实现"，不是"改逻辑"）
// ============================================================
// ── 小工具 ──
int iv_of(const Value* v, int dflt) { return v ? v->v : dflt; }

//   数值一律按 double 算，存回去时"整数就存 int、否则存 float"（GML 那套）
double num_of(const Value* v) {
    if (!v) return 0;
    if (v->t == MEM_FLOAT) { float f; memcpy(&f, &v->v, 4); return (double)f; }
    return (double)v->v;
}


Value mk_num(double x) {
    Value v;
    if (x == (double)(int)x) { v.t = MEM_INT;   v.v = (int)x; }
    else                     { v.t = MEM_FLOAT; float f = (float)x; memcpy(&v.v, &f, 4); }
    return v;
}


Value mk_str(Vm& vm, const char* s) {
    Value v; v.t = MEM_STRING; v.v = intern_str(s);
    return v;
}


// Initialize / Create 里那种"一次写几十个字段"的地方，现在直接写：
//   内置字段（19 个）→ in->xxx；其余 → in->vars[intern_str(name)]。这里几个 helper 留给**每帧**那些
//   按名字读写的地方用（它们名字可能是运行时来的，走这条不出错）。
void set_num(int inst, const char* name, double x) { inst_var_set(inst, name, mk_num(x)); }
void set_num_sid(int inst, int sid, double x)       { inst_var_set_sid(inst, sid, mk_num(x)); }

// 取实例变量，没有就给个默认值。
//   ⚠ 别写 `*inst_var(s, "xxx")` —— inst_var 在变量不存在时返回 NULL，
//     解引用就是空指针崩溃。GML 里读没设过的变量会报 "Variable not set"，
//     而这些"引擎内置"（bbox_*、hit_effect_2 这类由宿主补的）我们没法假定它一定在。
Value iv_or(int inst, const char* name, double dflt) {
    Value* p = inst_var(inst, name);
    return p ? *p : mk_num(dflt);
}

Value iv_or_sid(int inst, int sid, double dflt) {
    Value* p = inst_var_sid(inst, sid);
    return p ? *p : mk_num(dflt);
}


// 写实例变量，跟槽写入一个规矩：新值 retain、旧值 release
//   ⚠ call_ret / v_map / v_array 返回的值是"借来的"（引用留在 scratch 槽里，等下次复用才放），
//     要把它存进实例变量（或容器）就必须走这个；标量用 set_num 就行
void set_var(int inst, const char* name, Value v) {
    Value* p = inst_var(inst, name);
    Value old = p ? *p : Value{MEM_INT, 0};
    retain(v);
    if (old.t != v.t || old.v != v.v) release(old);
    inst_var_set(inst, name, v);
}

void set_var_sid(int inst, int sid, Value v) {
    Value* p = inst_var_sid(inst, sid);
    Value old = p ? *p : Value{MEM_INT, 0};
    retain(v);
    if (old.t != v.t || old.v != v.v) release(old);
    inst_var_set_sid(inst, sid, v);
}


// enemy_tbit_of(target_type)：敌人类型 → 命中位掩码
//   宏值见 scr_command_VM.gml:5567（HIT_NORMAL=1 … HIT_UNDERGROUND=32、HIT_OTHER=64）
bool str_is(Vm& vm, const Value* v, const char* s);   // 定义在下面

int enemy_tbit_of(Vm& vm, const Value* t) {
    if (str_is_sid(t, S_normal))      return 1;
    if (str_is_sid(t, S_air))         return 2;
    if (str_is_sid(t, S_dance))       return 4;
    if (str_is_sid(t, S_obstacle))    return 8;
    if (str_is_sid(t, S_diver))       return 16;
    if (str_is_sid(t, S_underground)) return 32;
    return 64;                                              // HIT_OTHER
}


double get_num(int inst, const char* name) { return num_of(inst_var(inst, name)); }
double get_num_sid(int inst, int sid)      { return num_of(inst_var_sid(inst, sid)); }

void add_num(int inst, const char* name, double d) { set_num(inst, name, get_num(inst, name) + d); }
void add_num_sid(int inst, int sid, double d)      { set_num_sid(inst, sid, get_num_sid(inst, sid) + d); }

// GML 的真值：手册写的是"<= 0.5 为假，> 0.5 为真"（整数 0 假、1 真，跟 !=0 一致；
// 差别只在浮点 0 < x <= 0.5 这种）
bool truthy(const Value* v) {
    if (!v) return false;
    if (v->t == MEM_UNDEFINED) return false;
    return num_of(v) > 0.5;
}


// 这个值是不是那个字符串（按池下标比：池是全局的，同下标必然同文本）
bool str_is_sid(const Value* v, int sid) {
    return v && v->t == MEM_STRING && v->v == sid;
}

// 这个值是不是那个字符串（池是全局的：先看下标，下标不同再比文本）
bool str_is(Vm& vm, const Value* v, const char* s) {
    (void)vm;                                       // 池全局化之后不用 Vm 了
    if (!v || v->t != MEM_STRING) return false;
    if (!str_ok(v->v)) return false;
    return str_at(v->v) == s;
}


// 读全局：**直接查全局表**，不绕 VM_GetProp（那个还是空壳，绕它拿到的恒为 0）。
//   别的实例的变量用 inst_var/get_num 直接读，同理。
//   ⚠ 老代码里有几处 v_getprop(vm, 0, "xxx") —— 那是错的，已经改掉
//   下标版（S_xxx）零哈希；名字版留给运行时才知道名字的地方。
Value gv_sid(int sid) {
    Value* p = global_var_sid(sid);
    if (p) return *p;
    Value u; u.t = MEM_UNDEFINED; u.v = 0;
    return u;
}

Value gv(Vm& vm, const char* name) {
    (void)vm;
    Value* p = global_var(name);
    if (p) return *p;
    Value u; u.t = MEM_UNDEFINED; u.v = 0;
    return u;
}

double gnumv_sid(int sid)            { Value v = gv_sid(sid); return num_of(&v); }
bool   gtrue_sid(int sid)            { Value v = gv_sid(sid); return truthy(&v); }
double gnumv(Vm& vm, const char* name) { return num_of(&gv(vm, name)); }
bool   gtrue(Vm& vm, const char* name) { Value v = gv(vm, name); return truthy(&v); }

// 两个字符串值是不是同一串文字
//   （池是全局的：**同下标必然同文本**，所以先比下标，不同再比文本）
bool str_eq(Vm& vm, const Value* a, const Value* b) {
    (void)vm;                                       // 池全局化之后不用 Vm 了
    if (!a || !b || a->t != MEM_STRING || b->t != MEM_STRING) return false;
    if (!str_ok(a->v) || !str_ok(b->v)) return false;
    return a->v == b->v || str_at(a->v) == str_at(b->v);
}


void set_iv(int inst, const char* name, int n) {
    Value v; v.t = MEM_INT; v.v = n;
    inst_var_set(inst, name, v);
}

void set_iv_sid(int inst, int sid, int n) {
    Value v; v.t = MEM_INT; v.v = n;
    inst_var_set_sid(inst, sid, v);
}


void add_iv(int inst, const char* name, int d) {
    set_iv(inst, name, iv_of(inst_var(inst, name)) + d);
}

void add_iv_sid(int inst, int sid, int d) {
    set_iv_sid(inst, sid, iv_of(inst_var_sid(inst, sid)) + d);
}


// 按名字调一个内部函数 / 游戏 API：值先写进 scratch 槽（槽号连续），再派发
//   vals 里放的是"值"，不是槽号；最多 8 个参数
// 还没进 native / game API 表的函数 —— 占位（这两张表以后要补就补，先别让硬编码块卡住）
//   按**池下标**判（原来是逐条 strcmp）
//   画图 / UI：空实现（真正画图是宿主/引擎的事）
static bool is_void_stub(int sid) {
    switch (sid) {
        case S_draw_set_alpha: case S_draw_set_color: case S_draw_set_colour:
        case S_draw_set_halign: case S_draw_set_valign: case S_draw_set_font:
        case S_draw_rectangle: case S_draw_text:
            return true;
        default:
            return false;
    }
}

//   项目脚本函数（还没搬过来）：返回 undefined —— 跟 GML 里"查不到"一个效果，
//   调用方的 `!= undefined` 判断会正常走"没数据"那条路，而不是拿到一堆 0 当真数据
static bool is_undef_stub(int sid) {
    switch (sid) {
        case S_get_plant_data: case S_get_card_info: case S_get_cookbook_data:
            return true;
        default:
            return false;
    }
}


// 值写进 scratch 槽（槽号连续），返回实际参数个数 —— call_fn_v / call_fn_sid 共用的前半截
static int fill_args(Vm& vm, int* slots, const Value* vals, int argc) {
    if (argc > 8) argc = 8;
    for (int i = 0; i < argc; i++) {
        slots[i] = SCRATCH_BASE + i;
        wr_v(vm, slots[i], vals[i]);
    }
    return argc;
}

// 按**池下标**调（sid 是编译期常量时用 SID_xxx）：没有名字、没有哈希，一次区间判断 + 数组下标
void call_fn_sid(Vm& vm, int sid, const Value* vals, int argc, int dst) {
    Fn fn = fn_of_sid(sid);
    if (!fn) { printf("[vm] call_fn_sid: 池下标 %d 不是内置函数\n", sid); return; }
    int slots[8];
    argc = fill_args(vm, slots, vals, argc);
    fn(vm, (const uint8_t*)slots, argc, dst);
}

void call_fn_v(Vm& vm, const char* name, const Value* vals, int argc, int dst) {
    int slots[8];
    argc = fill_args(vm, slots, vals, argc);
    // 名字 → 池下标（一次哈希），之后全是整数判断
    int sid = str_find(name);
    Fn fn = (sid < 0) ? NULL : fn_of_sid(sid);
    if (fn) { fn(vm, (const uint8_t*)slots, argc, dst); return; }
    if (sid >= 0 && is_void_stub(sid)) return;                           // 占位：什么都不做
    if (sid >= 0 && is_undef_stub(sid)) {                                // 占位：返回 undefined
        if (dst >= 0) { Value u; u.t = MEM_UNDEFINED; u.v = 0; wr_v(vm, dst, u); }
        return;
    }
    printf("[vm] call_fn: no such function '%s'\n", name);
}


void call_fn(Vm& vm, const char* name, const int* vals, int argc, int dst) {
    Value tmp[8];
    if (argc > 8) argc = 8;
    for (int i = 0; i < argc; i++) tmp[i] = mk_num((double)vals[i]);
    call_fn_v(vm, name, tmp, argc, dst);
}

void call_fn_int_sid(Vm& vm, int sid, const int* vals, int argc, int dst) {
    Value tmp[8];
    if (argc > 8) argc = 8;
    for (int i = 0; i < argc; i++) tmp[i] = mk_num((double)vals[i]);
    call_fn_sid(vm, sid, tmp, argc, dst);
}


// 调一个有返回值的函数：结果放在 scratch 里，读回来
//   ⚠ 返回值是"借来的"：引用在 scratch 槽里，**下一次 call_ret 就会把它冲掉**。
//     所以：拿到容器/结构体句柄之后，要么马上把字段读出来，要么用下面的 call_ret_held。
Value call_ret(Vm& vm, const char* name, const Value* vals, int argc) {
    int dst = SCRATCH_BASE + 8;
    Value z; z.t = MEM_INT; z.v = 0;
    wr_v(vm, dst, z);
    call_fn_v(vm, name, vals, argc, dst);
    return rd_v(vm, dst);
}


// 同上，但按池下标调（编译期常量 SID_xxx）—— 顺手能写出来的调用点都该用这个
Value call_ret_sid(Vm& vm, int sid, const Value* vals, int argc) {
    int dst = SCRATCH_BASE + 8;
    Value z; z.t = MEM_INT; z.v = 0;
    wr_v(vm, dst, z);
    call_fn_sid(vm, sid, vals, argc, dst);
    return rd_v(vm, dst);
}


// 同一个意思，但把结果**留住**（多 retain 一份），后面再 call_ret 也不会被冲掉；
//   用完必须自己 release()。容器/结构体要跨调用用的时候用这个。
Value call_ret_held(Vm& vm, const char* name, const Value* vals, int argc) {
    Value v = call_ret(vm, name, vals, argc);
    retain(v);
    return v;
}

Value call_ret_held_sid(Vm& vm, int sid, const Value* vals, int argc) {
    Value v = call_ret_sid(vm, sid, vals, argc);
    retain(v);
    return v;
}


// ── 取值链 helper ──
//   GML 里 a[i][j].k 这种，翻译器是拆成一串调用的；硬编码块里也一样，只是要自己接：
//     global.x        → v_getprop(vm, 0, "x")        （inst 0 = 全局）
//     arr[i]          → v_array(vm, arr, idx)
//     map[? k]        → v_map(vm, map, key)
//     struct[$ "k"]   → v_struct(vm, s, "k")
//     grid[# c, r]    → v_grid(vm, g, c, r)
//   每个都"传值进去、返回值出来"，可以随便套：v_array(vm, v_getprop(vm,0,"a"), i)
//   （值都是按值传递的，嵌套调用不会互相踩 scratch 槽）
Value v_getprop(Vm& vm, int inst, const char* name) {
    Value a[2]; a[0] = mk_num(inst); a[1] = mk_str(vm, name);
    return call_ret_sid(vm, S_VM_GetProp, a, 2);      // 池下标是编译期常量：不查名字
}

// 对"值"（结构体等）取成员：a.b → VM_GetProp(a, "b")（跟翻译器的 member → get_prop 一致）
Value v_member(Vm& vm, const Value& obj, const char* name) {
    Value a[2]; a[0] = obj; a[1] = mk_str(vm, name);
    return call_ret_sid(vm, S_VM_GetProp, a, 2);
}

bool is_undef(const Value& v) { return v.t == MEM_UNDEFINED; }
bool eq_num(const Value& v, int n) { return !is_undef(v) && num_of(&v) == (double)n; }
Value m_get(Vm& vm, const Value& map, const char* key);      // 定义在下面（结构体那节）
// 数组/列表取值、map/struct 取值：一律走**只读访问器**（不动引用计数、不碰 scratch 槽）
//   ⚠ 别用 call_ret("array_get"/"ds_map_find_value")：句柄常常是 call_ret 借来的，
//     那次调用会先把借来的那份引用放掉，取的就是个死容器
Value v_array(Vm& vm, const Value& arr, const Value& idx) {
    (void)vm;
    return container_at(arr, (int)num_of(&idx));
}

Value v_map(Vm& vm, const Value& m, const Value& key) {
    (void)vm;
    Value out; out.t = MEM_UNDEFINED; out.v = 0;
    map_find(m, key, &out);
    return out;
}

Value v_struct(Vm& vm, const Value& s, const char* key) {
    return m_get(vm, s, key);
}

Value v_grid(Vm& vm, const Value& g, const Value& c, const Value& r) {
    Value a[3]; a[0] = g; a[1] = c; a[2] = r;
    return call_ret_sid(vm, S_ds_grid_get, a, 3);
}


// ============================================================
// 公共小工具（父类调用 / 结构体字段 / 数组下标 / is-a 判定）
//   子对象的硬编码块要用 event_inherited()、with（父对象）这些 GML 语义。
// ============================================================

// 对象继承关系（GML 的 parentObjectId）。
//   ⚠ 翻译器现在把 event_inherited() 静默当成 0（gm2bin.py:728），
//     硬编码块里不能这么干：该跑父类的块就得跑。
//   ⚠ 还有一处语义：GML 的 with (obj_parent) / instance_exists(obj_parent)
//     **包含子对象**（obj_enemy_mod 也算 obj_enemy_parent）。这里靠 inst_is() 判。
const char* hot_parent_of(const char* obj) {
    if (obj == str_at(S_obj_card_mod)) return str_at(S_obj_card_parent).c_str();
    if (obj == str_at(S_obj_enemy_mod)) return str_at(S_obj_enemy_parent).c_str();
    if (obj == str_at(S_obj_player_character)) return str_at(S_obj_card_parent).c_str();
    if (obj == str_at(S_obj_bullet_mod)) return NULL;   // 无父对象
    return NULL;
}

// 跑父对象的同名块（obj 传自己的对象名，块名照原样）
bool hot_inherited(Vm& vm, const char* obj, const char* block) {
    const char* p = hot_parent_of(obj);
    if (!p) return false;
    return run_hot_block(vm, p, block);
}


// 这个实例的对象的"是不是 base 或者 base 的子对象"（GML 的 with/instance_exists 口径）
bool inst_is(int id, const char* base) {
    Instance* in = inst_get(id);
    if (!in) return false;
    const std::string* cur = &in->obj;
    for (int depth = 0; depth < 8; depth++) {
        if (*cur == base) return true;
        const char* p = hot_parent_of(cur->c_str());
        if (!p) return false;
        static std::string tmp;                  // 只用于比较，函数内不重入
        tmp = p;
        cur = &tmp;
    }
    return false;
}


// ── 数组下标读写（读走只读访问器；写走 array_set，它是内部函数、会 retain）──
Value array_v(Vm& vm, const Value& arr, int i) {
    (void)vm;
    return container_at(arr, i);
}

void array_setv(Vm& vm, const Value& arr, int i, const Value& v) {
    Value a[3]; a[0] = arr; a[1] = mk_num(i); a[2] = v;
    call_fn_sid(vm, S_array_set, a, 3, -1);
}

double array_n(Vm& vm, const Value& arr, int i) {
    Value v = array_v(vm, arr, i);
    return num_of(&v);
}

void array_setn(Vm& vm, const Value& arr, int i, double x) {
    Value v = mk_num(x);
    array_setv(vm, arr, i, v);
}


// ── 结构体（我们这边结构体 = MEM_MAP）取/写成员 ──
//   取：走只读的 map_find（**不碰 scratch 槽、不动引用计数**）—— 因为句柄常常是
//       call_ret 借来的，用 ds_map_find_value 的话 call_ret 会先把那份引用放掉
//   写：走 ds_map_set（它自己会 retain，安全）
Value m_get(Vm& vm, const Value& map, const char* key) {
    Value k; k.t = MEM_STRING; k.v = intern_str(key);
    Value out; out.t = MEM_UNDEFINED; out.v = 0;
    map_find(map, k, &out);
    return out;
}

void m_set(Vm& vm, const Value& map, const char* key, const Value& v) {
    Value a[3]; a[0] = map; a[1] = mk_str(vm, key); a[2] = v;
    call_fn_sid(vm, S_ds_map_set, a, 3, -1);
}

void m_setnum(Vm& vm, const Value& map, const char* key, double x) {
    Value v = mk_num(x);
    m_set(vm, map, key, v);
}

double m_num(Vm& vm, const Value& map, const char* key) {
    Value v = m_get(vm, map, key);
    return num_of(&v);
}


// ============================================================
// mod 单位那套（obj_card_mod / obj_enemy_mod / obj_bullet_mod 共用）
//   平台原型：src_mod.gml:2039 mod_base_tick / 2052 mod_inst_add / 2060 mod_inst_del
//   "这个实例属于哪个 mod 的 VM" = 实例上的 enemy_id / card_id / bullet_id 查 g_mod_vms
//
//   ⚠ 字符串池现在是**全局一张**（见 vm.h）：任何 VM 写进实例变量的字符串，
//     下标都是全局的，谁读都是同一串文字 —— 不用再分"谁的池"。
//     str_is_any() 那个双池判断因此退化成一次普通比较（签名留着，调用点不用改）。
// ============================================================

// 按全局池取文本（越界 → 空串）
std::string pool_text(const Value* v) {
    if (!v || v->t != MEM_STRING || !str_ok(v->v)) return std::string();
    return str_at(v->v);
}

// 这个值是不是某个字符串：池全局化之后只有一个池，直接按文字比
bool str_is_any(Vm& vm, Vm* other, const Value* v, const char* s) {
    (void)other;                                    // 另一个池已经不存在了
    return str_is(vm, v, s);
}


// 这个实例的 mod VM
Vm* mod_vm_of_inst(Vm& vm, int inst, const char* idvar) {
    Value* p = inst_var(inst, idvar);
    if (!p || p->t != MEM_STRING) return NULL;
    std::string id = pool_text(p);
    if (id.empty()) return NULL;                       // 池不认识这个下标 → 当没有
    return mod_vm(id);
}


// 跑 mod VM 里的一个块 —— 平台那边是：
//   if (ds_map_exists(_vm.blocks, blk)) { 切 __vm / _VM_cur_card; VM_Execute(_vm, _vm.blocks[? blk], blk); 还原 }
//   块不存在就不跑（这是"块存在才跑"的对应物）
bool mod_run_block(Vm* mv, int inst, const char* block, const char* cur_global) {
    if (!mv) return false;
    std::string obj = mv->primary_object();
    if (obj.empty()) return false;
    // 块不存在就不跑（跟 ds_map_exists(_vm.blocks, blk) 一个意思）；
    // 硬编码块也算"存在"，不然 mod VM 里挂到父对象上的块会被漏掉
    if (!has_hot_block(obj, block) && !mv->find_block(block)) return false;
    int bak_ins = mv->cur_instance;
    mv->cur_instance = inst;
    if (cur_global) global_var_set(cur_global, mk_num(inst));   // 块内 API 取当前实例
    bool ok = mv->run_named(block);
    mv->cur_instance = bak_ins;
    return ok;
}


// mod_inst_add / mod_inst_del：只维护计数器
//   "哪些单位"不用另记一份 —— 就是 mv->instances（实例生命周期在 inst_new/inst_kill 里维护）
void mod_inst_add_v(Vm* mv, int inst, bool with_list) {
    (void)inst; (void)with_list;
    if (!mv) return;
    mv->inst_count++;
}

void mod_inst_del_v(Vm* mv, int inst, bool with_list) {
    (void)inst; (void)with_list;
    if (!mv) return;
    if (mv->inst_count > 0) mv->inst_count--;
}


// mod_base_tick(_inst)：倒计时 > 0 时每帧 -1 并加透明度
void mod_base_tick_v(int inst) {
    if (!inst_get(inst)) return;
    if (!inst_var_sid(inst, S_mod_countdown) || !inst_var_sid(inst, S_mod_alpha_add)) return;
    if (get_num_sid(inst, S_mod_countdown) > 0) {
        add_num_sid(inst, S_mod_countdown, -1);
        add_num_sid(inst, S_image_alpha, get_num_sid(inst, S_mod_alpha_add));
    }
}


// `info.k ?? dflt`：字段是 undefined 就用 dflt（GML 的空合并）
double m_or_num(Vm& vm, const Value& map, const char* key, double dflt) {
    Value v = m_get(vm, map, key);
    return v.t == MEM_UNDEFINED ? dflt : num_of(&v);
}

// 这个值是数字吗（对 GML 的 is_real，只认真 int / float）
bool is_numv(const Value* v) { return v && (v->t == MEM_INT || v->t == MEM_FLOAT); }

// ============================================================
// obj_card_mod —— 通用 mod 植物（7 个事件，30 KB；Step 就占 16.5 KB）
//   和 obj_enemy_mod 一个套路，但 id 变量是 **plant_id**、注册表是 mod_card_vms，
//   而且索敌 / 开火窗口 / 批量改属性这些比敌人那边复杂得多。
//   常量：CARD_STATE.IDLE=0 / ATTACK=1、
//         HIT_NORMAL=1 HIT_OBSTACLE=8 HIT_DIVER=16 HIT_AIR=2 HIT_DANCE=4 HIT_UNDERGROUND=32
// ============================================================

// 字符串优先按 mod VM 的池解（mod VM 的数组/命名数组里存的字符串都是那个池的）
std::string text_pref_mod(Vm& vm, Vm* mv, const Value& v) {
    if (mv && mv != &vm) {
        std::string s = pool_text(&v);
        if (!s.empty()) return s;
    }
    return pool_text(&v);
}


// 给内核用的入口（vm.h 里声明）：跑事件时带事件继承。
//   event_user / 伤害事件 / 自定义事件都该沿父对象找同名块 ——
//   obj_enemy_mod 自己没有 UserEvent 0 时，跑的是 obj_enemy_parent 的那个（GML 一直这么干）
bool run_hot_event_inherited(Vm& vm, int inst, const char* block) {
    return inst_event_inherited(vm, inst, block);
}

// 跑某个实例的某个事件，**自己没定义就沿父对象往上找**（GML 的事件继承）。
//   Create 里的 event_user(0) 就是这个语义：obj_card_mod 自己没有 UserEvent 0
//   （它只有 Create/Step/Draw/Destroy/Mouse_*），实际跑的是 obj_card_parent 的那个
bool inst_event_inherited(Vm& vm, int inst, const char* block) {
    Instance* in = inst_get(inst);
    if (!in) return false;
    std::string obj = in->obj;
    for (int depth = 0; depth < 8; depth++) {
        Vm* owner = vm_of_object(obj);          // 这个对象的块在它自己的 Vm 里
        if (owner && (has_hot_block(obj, block) || owner->find_block(block)))
            return run_block_on(vm, obj, inst, block);
        const char* p = hot_parent_of(obj.c_str());
        if (!p) return false;
        obj = p;
    }
    return false;
}

// ============================================================
// 池下标版（名字是编译期常量 S_xxx 时用；调用点不再出现字面量）
//   map/struct：直接吃 sid；块名/对象名：内部 str_at 取文本（O(1)）
// ============================================================
Value m_get_sid(Vm& vm, const Value& map, int sid) {
    Value k; k.t = MEM_STRING; k.v = sid;
    Value out; out.t = MEM_UNDEFINED; out.v = 0;
    map_find(map, k, &out);
    return out;
}
Value v_struct_sid(Vm& vm, const Value& s, int sid) { return m_get_sid(vm, s, sid); }
Value v_member_sid(Vm& vm, const Value& obj, int sid) {
    Value a[2]; a[0] = obj; a[1] = mk_str_sid(sid);
    return call_ret_sid(vm, S_VM_GetProp, a, 2);
}
void m_set_sid(Vm& vm, const Value& map, int sid, const Value& v) {
    Value a[3]; a[0] = map; a[1] = mk_str_sid(sid); a[2] = v;
    call_fn_sid(vm, S_ds_map_set, a, 3, -1);
}
void   m_setnum_sid(Vm& vm, const Value& map, int sid, double x) { Value v = mk_num(x); m_set_sid(vm, map, sid, v); }
double m_num_sid(Vm& vm, const Value& map, int sid) { Value v = m_get_sid(vm, map, sid); return num_of(&v); }
double m_or_num_sid(Vm& vm, const Value& map, int sid, double dflt) {
    Value v = m_get_sid(vm, map, sid); return v.t == MEM_UNDEFINED ? dflt : num_of(&v);
}
bool mod_run_block_sid(Vm* mv, int inst, int sid, const char* cur_global) {
    return mod_run_block(mv, inst, str_at(sid).c_str(), cur_global);
}
bool inst_is_sid(int id, int sid)                        { return inst_is(id, str_at(sid).c_str()); }
bool inst_event_sid(Vm& vm, int inst, int sid)           { return inst_event(vm, inst, str_at(sid).c_str()); }
bool inst_event_inherited_sid(Vm& vm, int inst, int sid) { return inst_event_inherited(vm, inst, str_at(sid).c_str()); }
bool hot_inherited_sid(Vm& vm, int o, int b)             { return hot_inherited(vm, str_at(o).c_str(), str_at(b).c_str()); }
bool hot_enemymod_mouse_sid(Vm& vm, int sid)             { return hot_enemymod_mouse(vm, str_at(sid).c_str()); }
Vm*  mod_vm_of_inst_sid(Vm& vm, int inst, int sid)       { return mod_vm_of_inst(vm, inst, str_at(sid).c_str()); }

bool str_is_any_sid(Vm& vm, Vm* other, const Value* v, int sid) {
    return str_is_any(vm, other, v, str_at(sid).c_str());
}
