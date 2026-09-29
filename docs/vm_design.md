# VM 设计

## 1. VM 是什么

**一个 bin = 一个 VM = 一个对象。**

VM 只负责执行这个对象的事件块。

**新代码写在新的文件夹 `DLL_VM` 里**（新旧**函数名保持一致**）；老代码留着参考和复用。

## 2. VM 的结构体（名字：`vm_bin`；**是 struct，不是 class**）

```cpp
struct vm_bin {
    Value                          mem[MEM_SIZE];   // 固定大小（参考 GML 的虚拟机 ✓）；直接是数组，不用指针
    std::unordered_map<int, Block> blocks;         // 事件名池下标 → 块（这个对象的全部代码）

    int                            inst_count;     // 实例个数
    std::vector<int>               arr;            // 数组：存**槽位**（槽位 = 内层地址）
    std::set<int>                  mp;             // 集合：存**槽位**（id 才是实例唯一标识）
    std::set<int>::iterator        cur;            // 集合迭代器：当前访问的位置
    int                            cur_rank;       // 当前访问在第几个位置上
};
```

`inst_count` / `arr` / `mp` / `cur` / `cur_rank` 是"名下的实例"这一份东西：

- **`arr`（数组）**：实例少的时候用它（<16）
- **`mp`（字典）**：实例多了用它（≥16），**转过去就不回来**
- **`cur` + `cur_rank`**：字典模式下的游标 —— 上次访问到哪个节点、是第几个；
  查"第 k 个"时从游标加加减减，跳得远才从头/尾重新走

### 还有 `vm_callbin`

```cpp
struct vm_callbin {
    Block block;   // 一个块
    int   value;   // 这个函数**需要的内存大小**（在公共内存上占的范围）
};
```

callbin 函数**没有自己的内存**：跑的时候直接在**公共内存**（`g_call_mem`）上操作，
`value` 就是它需要的那段范围；**函数调函数就在上面再加偏移** ✓

### 接口函数

```cpp
Value VM_RUN(Block& block, Value* mem);   // 入参：块 + mem 地址；返回 Value

Value VM_BIN_Load    (const uint8_t* data, size_t size, vm_bin&     out);   // 字节流 + 大小 + 出参
Value VM_CALLBIN_LOAD(const uint8_t* data, size_t size, vm_callbin& out);   // 同上

// 返回值：**成功 = 1（Value）；失败 = undefined**
// 出参：装载出来的结构体本身

// VM_RUN 的块有两种来源（**这部分还没完成**）：
//   ① 从 vm_bin.blocks 里按事件名取出来的块（跑对象的事件）
//   ② callbin 的块 + 它在公共内存上的那段偏移（跑函数调用）
//
// 上下文：用一个**栈**（数组模拟）——每次调用把"要调用的对象 / 实例"压栈，
//   于是每一层调用都知道自己这层是哪个实例 ✓
// callbin 需要多少槽位：**bin 里已经确定了**；调用栈的偏移由 VM_RUN 自己维护 ✓
//
// VM_RUN 的返回值：
//   跑 **bin 的块**     → **没有返回值**（跑完就行）
//   跑 **callbin 的块** → 返回**函数的返回值**（Value）
//   （参考老代码 + 现在的 callbin ✓）

// VM_RUN 的内容就是解释循环：
//   取指令 → 按 opcode 在 mem 上读写（槽号编译期定死）→ 跳转改 pc →
//   调用走表（OP_NATIVE → kNativeFns，OP_CALL → kGameFns）→
//   OP_HALT 时返回**槽 0** 里的那个 Value
```

**上面这些（`vm_bin` / `vm_callbin` 两个结构体 + `VM_RUN` / `VM_BIN_Load` / `VM_CALLBIN_LOAD` 三个函数）都写在同一个 `.cpp` 文件里。**

### 不要的字段（都在别层）

```
obj / cur_instance / collision_other / kind_sid / named_arrays / mod_card_data / last_err / owns_mem
```

## 3. 分层

分**四个文件夹**，清晰一点：

- **内层**：核心。**定下来之后就基本不动了**
- **外层**：主要是一堆**函数**（各类函数的实现），**可以一直加**
- **业务层**：**初始化 / 战斗开始 / 步入**（事件派发那些游戏语义）
- **测试层**：自检 / 用例

### 内层（核心，互相不掺杂，都不认识外层）

| # | 文件 | 职责 |
|---|---|---|
| 1 | 虚拟机 | 执行（解释循环 / 块派发）+ 上面那个结构体 |
| 2 | 实例 | 实例的一切操作（建 / 取 / 销 / 变量读写 / 容器） |
| 3 | 内存与池 | 全局变量数组、容器池、字符串池（含容量与计数） |
| 4 | 字符常量 | 单独文件：所有字符串常量的**唯一真源** |
| 5 | 函数调用机制 | 取参、切上下文、派发、回值、报错 |

### 宏定义与字符串池初始化（**同一个 `.cpp`**；**参考旧代码** —— 就是现在 `names.h` / `names.cpp` 那套）

- 一堆**宏定义**：opcode、各个池的下标、上限等常量
- **字符串池初始化要填的内容**：所有字符串常量的名单
  → 启动时按名单依次 `intern`，得到固定下标（`S_xxx`）
  → 之后 C++ 里不再出现字符串字面量，全用这些下标 ✓

### 全局池（**都在同一个 `.cpp` 里**）

```
字符串池        g_strings      文本数组
                g_str_map      文本 → 下标

全局变量表      g_globals      名字池下标 → Value        ← GML 的 global.x

实例表          g_instances    槽位表（下标 = 槽位）
                g_inst_free    槽位复用队列
                g_inst_alive   实例 id → 槽位（唯一权威）

对象名 → VM     g_vm_of_object 对象名池下标 → Vm*
mod id → VM     g_mod_vms      mod id 池下标 → Vm*

callbin         g_call_dict    函数名池下标 → CallBin
                g_call_mem     callbin 的共享内存（固定大小，首次调用时分配）

容器池          g_list_pool    ds_list
                g_map_pool     ds_map
                g_grid_pool    ds_grid
                g_arr_pool     数组
                （每个还带：引用计数 g_*_refs、时间戳 g_*_stamp、复用队列 g_*_used）
```

这些**不属于任何 VM**，所有 VM 共用。

### 实例层（**也在同一个 `.cpp` 里**）

```cpp
struct Instance {
    int                          obj;       // 对象名的**池下标**（描述名称）
    Value                        attr[FIELD_COUNT];   // 定长属性：**C++ 里预定义的那堆字段**（x / y / image_index …）
    std::unordered_map<int, Value> vars;    // 字典：其余属性（脚本动态的；属性名池下标 → Value）
    std::unordered_map<int, std::vector<Value>> arrays;  // 字典：数组（数组名池下标 → 数组）
};
```

函数：

```
创建        inst_new(…)                 建一个实例 → 返回实例 id
删除        inst_destroy(id)            唯一入口：跑 Destroy 事件 → 从 g_inst_alive 移除
                                        → 释放槽位 → 从 VM 的 arr/mp 摘掉
            inst_kill(id)               只回收，不跑事件（VM 析构时用）
是否存活    inst_alive(id)              查 g_inst_alive
            inst_get(id)                取实例本体（没有 / 已死 → NULL）
个数        obj 有几个 —— 看那个 vm_bin 的 inst_count 字段（一个 obj 就是一个 VM）
第 k 个     inst_kth(vm, k)             第 k 个实例 id（<16 数组直取 / ≥16 走游标）
            inst_at(obj, k)             按对象名问
变量        inst_var_sid / inst_var_set_sid      实例变量读写（名字用池下标）
容器内部    arr/mp 的增删查             <16 用数组、≥16 用 map+游标；**对外只暴露上面这些**
```


**规则：**

- **id 只增不减**（永不复用）；**槽位可复用**
- `g_inst_alive`：id → 槽位，**唯一权威**（没有 `alive` 字段）
- 每个 VM 一份实例容器：**<16 用数组 / ≥16 用 map + 游标**（转了就**不回**）
- **销毁唯一入口**：跑 Destroy 事件 → 从 `g_inst_alive` 移除 → 释放槽位 → 从 VM 的 `arr`/`mp` 摘掉；**带重入保护**（Destroy 里再销毁自己不再进来）
- **容器/变量的生命周期归 GML**；VM 只在明确拥有所有权时 `release`

### 外层（各类函数的**实现**，按类别分文件）

```
内置函数      GM 内建（数学 / 字符串 / 文件 / 相机 / 音频…）
游戏函数      game API（VM_*）
native        项目内部工具（数组 / ds_* / instance_* / 格子…）
热块          热点对象的整块事件
callbin       外部函数的执行入口
```

### 入口与工具（最外圈，内层看不见它们）

```
main       入口 / 参数解析 / 子命令
自检        只被 main 调
装载        bin / callbin 读取、字符串池重映射、装载
日志        错误码 → 文本（**字符串只在这个文件里格式化**）
```

### 规则

1. **外层 → 内层，单向**；内层不认识外层
2. **机制**（怎么调 / 怎么跑）和**表**（有哪些函数）分开
3. **内层不许有** `printf` / 文件 IO / `argv` / 自检
4. **不要过度封装**：能就地算的就地算；纯 getter 不要
   **不绕远路；名字要清晰**
5. 常量的分类查询**用查表**，不用 `switch` / `if` 链
6. **运行时只有池下标，没有文本**；字符串常量只在"字符常量"那个文件里
7. **装载期是唯一允许"文本 → 池下标"的地方**

## 4. 字符串

- 全局一张池；**文本只在池里**，别处一律池下标
- `names.h` 是唯一真源：`NAME_LIST` 展开成 `enum` + 名单 + 灌池
- 装载期 `intern` 一次，**运行时零哈希**

## 5. 接口表是契约

`native` / `game API` 两张表的**名字与顺序是契约**（已编译的 mod 依赖这套 id）——
**只加实现，不动顺序。**

## 6. 关键字不是函数

关键字 / 语句**不进 native 表**；翻译器遇到不认识的语法一律**响亮报错 + 给出改法**，
再由 GML 侧改成可转换的写法。

## 7. 定位

动态加载 / 热更原来就有；这一套是为了把**热点事件迁到 C++ 提速**。

做法：**一个 bin = 一个对象 = 一个 VM**，按**事件粒度**决定：
这个事件走 C++ 热块，还是留在字节码解释。

## 8. 核心业务层（外层）

把"游戏的语义"实现出来 —— 它**只通过内层接口干活**（VM_RUN / Load / 实例操作），
自己不碰字节码解码。

包含：**初始化函数** / **战斗开始函数** / **步入函数**

这三个就是**编译成 DLL 导出的接口**，由外部调用；参数都是**内存地址**（方便通信）——细节后面再说 ✓

宿主测可以搞一张**函数表**（不影响 C++ 的情况下）。

```
事件派发     run_block_on(vm_bin&, 实例 id, 事件名池下标)
               · 先查有没有 C++ 热块；有就走它，没有再把字节码块交给 VM_RUN
               · "当前实例是谁"作为**参数**传进去（不再挂在 VM 上）
               · 跨对象：g_vm_of_object[对象名下标] 找到另一个 vm_bin
             run_event_user / 碰撞事件 other 同理（other 也走参数）

实例生命周期 inst_new / inst_destroy（业务规则：先跑 Destroy 事件 → 再回收 → 重入保护）

对象注册     注册 / 注销：对象名池下标 → vm_bin*
mod 注册     mod id → vm_bin*；从实例身上读 id 找它的 mod VM

callbin 调用  函数名池下标 → vm_callbin → 在公共内存上加偏移 → VM_RUN

宿主接口     初始化 / 装载 bin / 装载 callbin / 派发事件 / 查询（个数、第 k 个）
```

**业务层的状态自己存**（实例表、注册表、调用栈偏移…），**不往 `vm_bin` 里塞** ✓

## 9. 实施顺序与兼容

1. **先写内层**；内层好了再考虑外层 ✓
2. **函数规则和之前对齐**：native / game API 两套表的名字与顺序照旧（契约 ✓）
3. **现在导出的 bin / callbin 要能直接读进来用** ✓（装载格式不变 ✓）
