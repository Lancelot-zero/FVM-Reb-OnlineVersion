# DLL_VM 进度与待办（交接文档）

> 任务已**搁置**。下次接着干时先读这份，别从头摸。
> 仓库：`FVM-Reborn`（美食大战老鼠：重生）· 代码：`DLL_VM/` · 翻译器：`transformGML2txt/gm2bin.py`

---

## 0 一句话现状

**机制 / 通道 / 翻译 / 契约 都齐了；表里还剩 76 条 native 空壳；业务层三个导出没写 → 真实 bin 端到端还没跑通。**

---

## 1 怎么构建 / 验证（先跑这两条）

```bat
cd DLL_VM\tests
cmd /c "build.bat < nul"          rem 期望：BUILD OK（/W4，零 warning）
.\selftest.exe ..\..\kernel       rem 期望：== ALL OK (0 failed) ==（330+ 条断言）
```
体检行（每次改动都看它）：
```
native 358 条里空壳 76，game 150 条里空壳 0（合计 76/508）
```
翻译（两步；**第一步建议跑两遍**，见待办 P4）：
```bat
cd transformGML2txt
python gm2bin.py ..\kernel\function --recursive --out %TEMP%\fn
python gm2bin.py ..\kernel --recursive --out %TEMP%\full --callbins %TEMP%\fn\function
rem 期望：bin 433, callbin ~106, stop 10（全是"对象事件文件里带 function"的虚警）, skip 0
```

---

## 2 已完成（别再重做）

| 项 | 说明 |
|---|---|
| 内层 `core/` | 槽内存、字符串池、调用栈、实例系统、块派发、callbin 装载 |
| 两张表 + 冻结契约 | `core/const_str.h`：`NATIVE_FN_LIST(79)+NATIVE_IMPL_LIST(279)=358`、`GAME_FN_LIST(150)` |
| game 150 条 | 全部实现（自举 49 + A/B/C 101） |
| native | 276 条有条目：**125 真实现 + 151 转发壳**（79 转发 + 72 项目业务） |
| 名字 → 实现 双表 | `outer/fn_dll.cpp`：先 DLL 函数表（`VM_RegisterFunc`）→ 再 callbin 表；`VM_UnregisterFunc` / `VM_FuncApiVersion` |
| 平台 GML VM 残留 | 15 个 `.gml` + 7 个 stale `.callbin` 已删；kernel 里只剩 4 个真被调用的（`vm_hook_run` 17 / `VM_FlushHooks` 2 / `VM_ClientWrapId` 2 / `VM_PropIdOf` 1） |
| GML 里的平台 VM 调用 | 18 处 `VM_Execute(...)` 全删，残留 0 |
| 字符串真源归一 | 31 个名字进 `REST_NAME_LIST` 尾部（append-only），49 处字面量换 `S_xxx` |
| 内建常量 | `tools/gen_gm_builtin_consts.py` → `gm_builtin_consts.json`（200 个），gm2bin 翻译时内联 |
| 内置常量表 `CALL_BY_NAME` | gm2bin 里"按名字调用"的表；命中的函数编成 `OP_CALL VM_CallFunc`，**不进 native 表** |
| 入站包（引擎→VM） | `host/host_packet.cpp` + `outer/packet.cpp`：`host_packet_apply` / `VM_ApplyPacket` / `VM_InternText` / `VM_SidOf` |
| 出站请求流（VM→引擎） | `host/host_shared.cpp`：连续内存 + 游标 + `host_shm_push_i32/_f32/_bytes`；`outer/host_push.cpp` 的 `VM_HostPush` |
| 共享内存契约 | `tools/gen_host_layout.py` → `host/host_layout.h` + `.json`（B1/B2/A + `HOST_LAYOUT_VERSION` + `host_layout_version()`） |
| B1 输入 6 条 | `outer/host_state_in.cpp`：键盘按下位图、鼠标三掩码、GUI 坐标；`keyboard_get_string` 契约无字段 → 仍是空壳 |
| 4 条网格/伤害 | 改写成 GML callbin（`kernel/function/*.gml`）+ 进 `CALL_BY_NAME` |
| **字符串拼接** | `core/vm.cpp` 的 `str_concat()`（任一侧是字符串就拼，结果 `intern_str` 回池）+ `core/pool.cpp` 的 `value_to_text()`（值→文本唯一实现） |

---

## 3 待办（按优先级，每条带验收线）

### P0 业务层三个导出 ← **端到端唯一阻塞**
- 要写：`business/` 里的 **初始化 / 战斗开始 / 步入** 三个导出 + **mod 块装载口**（名字 → 块的表）+ 事件派发接线。
- 现在：`business/native_biz.cpp` 只有那 4 条（已转 GML callbin 的）占位定义，三个导出 0 行。
- 验收：拿真实 `kernel/**/*.bin` 跑一场：创建实例 → 跑 Step → 事件派发命中字节码块，全程 `g_vm_err == VM_OK`。

### P1 第二批 native：写 **A 请求流**（约 30 条）
- 音频 12 / 相机 6 / 窗口 6 / 房间 3 / 网络 5 / 系统·IME 8。
- 做法：照 `host_layout.h` 的请求号 + 字段布局，用 `host_shm_push_i32/_f32/_bytes` 写；**表不动**（这些名字本来就在 native 表里，只是把空壳换成实现）。
- 验收：自检里读那块内存，**逐字节**校验请求号 + 字段；空壳数应下降。

### P2 第三批 native：写 **B2 命令区**（约 20 条）
- 绘制那批（`draw_sprite` / `draw_rectangle` / `draw_text_ext` / `draw_set_*` / `surface_*` …）。
- 做法：写进 B2 的定长记录（512 条 × 8 槽，`[0]cmd [1][2]int [3..7]float`），计数由**生产方每帧第一次写之前清 0**。
- 验收：自检读计数 + 记录逐字段校验。

### P3 `VM_RunBlock`（外部可触发的"跑块"口）+ 19 处命名钩子
- 现在 `run_block_on` 是 C++ 内部函数，形状是 `(vm_bin&, obj_sid, inst, block_sid)`；GML/入站包都调不到它。
- 做法：包一层**符合 `Fn` 形状**的 `VM_RunBlock(Value* mem, args, argc, dst)` → 内部 `run_block_on(*cur_vm(), …, run_inst(), 块名)`；`VM_RegisterFunc("VM_RunBlock", …)`（表不动）。两个前提要响亮报错：没有当前 vm / 没有有效实例。
- 然后那 19 处命名钩子（`vm_hook_run` 17 + `VM_FlushHooks` 2）才有"执行块"的能力 —— 它们体内的 `VM_Execute` 调用已被删，现在只记账不执行。

### P4 收尾小事
- `game_b.cpp:1049/1052`：`asset_of("spr_win")` / `play_sound_asset("snd_win"/"snd_lose")` —— **真·两个真源**（`snd_lose` 名单里已有）。
- `game_a.cpp` 的 `intern_once` 那 26 个名字 + `kAllNames` → `tools/gen_rest_names.py --write` 一条命令能收（它也会动别的批次用到的名字，做前先看 diff）。
- 两步翻译的**第一步跑两遍**（收敛）：单跑第一遍会有 15 个"先编的文件调不到还没编出来的 callee"的连坐 stop。
- `tools/gen_gm2bin_callbyname.py` 重跑会按 `HOST_BIZ_LIST` 重建 `CALL_BY_NAME`，**会丢掉 `VM_HostPush`**（现在它第二个锚点匹配不上、写文件前就 assert 中止，所以暂时安全）→ 给它加个白名单。

### P5 热块（搁置中）
- `outer/hot/hot_table.cpp` 是空缝（一律返回 false → 全部回落字节码）。
- 老代码 `transformGML2txt/hot/` 14 个文件约 6800 行没搬。
- 用户当时的想法：热块做成**独立 DLL**（主项目留注册口：热函数列表 + 函数入口）。

---

## 4 契约速查（VM ↔ 宿主引擎）

**唯一真源**：`tools/gen_host_layout.py`（顶部 `LAYOUT` dict）→ 生成 `host_layout.h`（C++ constexpr）+ `host_layout.json`（别的语言）。改了要 +`HOST_LAYOUT_VERSION`。

| 块 | 方向 | 大小 | 说明 |
|---|---|---|---|
| **B1 状态区** | 引擎写 → VM 读 | 64 槽 / 512 字节 | 覆盖式每帧写，不用清。0 帧号 · 1-4 鼠标(世界 x/y + GUI x/y) · 5-7 按住/刚按下/刚松开 掩码 · 8 滚轮 · 9 焦点 · 10 IME · 11-12 窗口宽高 · 13-14 帧 ms/累计 · 16-23/24-31/32-39 键盘三张位图 · 40-63 预留（IME 文本区） |
| **B2 命令区** | VM 写 → 引擎读 | 4097 槽 / 32776 字节 | 计数 1 + 512 条 × 8 槽；**生产方每帧第一次写前清 0**，消费方只读不许清 |
| **A 请求流** | VM 写 → 引擎读 | `host_shm_*`（容量从 `host_shm_size()` 拿，不复制进契约） | 1 播放音频 · 2 音频停/暂停/恢复 · 3 相机 · 4 窗口 · 5 网络发送 · 6 系统 · 7+ 预留 |

细节：槽 = **8 字节**（`Value{int32 t; int32 v;}`）；位图 `word = 基址 + (vk>>5)`、位 = `vk&31`、低位在前；鼠标掩码 **bit0 左 / bit1 右 / bit2 中**（GM 的 `mb_*` 是 1/2/3，**不是位号**，代码里显式映射）。
包里（A 与入站包）**不带类型信息**，靠两边事先约定。

**入站包两条规则**：
```
① ["名字"][值]                       → 存全局变量
② ["CALL"][函数名][argc][参数…]      → 按名字调一次（内置 / game / DLL 注册 / callbin 都支持）
坏包一律响亮：VM_E_BAD_PACKET + 返回 -1（a1 = PK_* 原因）
```
**引擎侧怎么接**：① 按名字注册 C++ 实现（`VM_RegisterFunc`）；② 填 host 表（`g_host_in/draw/net` 现在是安全默认值，`g_host_audio` 还没建）；③ 定期读那块内存 / 写 B1。引擎启动时必须调一次 `outer_init()`（或直接 `host_packet_set_handler(VM_ApplyPacket)`）——**没有静态初始化注册**。

---

## 5 红线（不许动）

1. **`core/const_str.h` 的三张名单**：`NATIVE_FN_LIST` / `NATIVE_IMPL_LIST` / `GAME_FN_LIST` —— 顺序 = fid = 已编译 bin 的命根子；**只许在尾部 append**，老下标一个都不能动。
2. **表只减不增**：新函数一律走 `CALL_BY_NAME` + `VM_RegisterFunc` / callbin，**不加 native 表项**。
3. **字符串字面量只许出现在 `core/const_str.cpp`**（字符常量名单唯一真源）；别处只用 `S_xxx` 池下标。
4. 跨 DLL 只走 **POD**（`Value` / `int` / `uint8_t*` / `const char*`）；别把 `std::string`/`std::vector` 放进注册签名。
5. `DLL_VM/tests/build.bat` **保持纯 ASCII**（cmd 按 GBK 读批处理）。
6. 不要两套真源（同一个语义别有两个实现/两份表）。
7. 别静默失败：没有就 `vm_set_err` 响亮报错。

---

## 6 判断题：已定 / 待定

**已定**
- 表冻结；新函数走"按名字调用 + 注册"。
- 高频（绘制/输入）走**内存直读**（B1/B2），低频（音频/网络/窗口）走**请求流**（A）。
- 内建常量在**编译期内联**（gm2bin 查 `gm_builtin_consts.json`）。
- 字符串拼接结果**回池**（同文本同 sid）。
- 入站包格式：`["CALL"][函数名][argc][参数…]`（argc **不含**名字）；`"CALL"` 严格大小写。

**待定（下次问用户）**
- `NET_SEND` 的长度字段：`u32` 还是 `i32`（`push_bytes` 用的是 u32）。
- B2 与 A 职责重叠 → 是否把界线写死进契约（建议：高频 B2 / 低频 A），或把低频也塞进 B2。
- 浮点转文本格式：现在沿用 `%.2f`（`2.0f` → `"2.00"`）。若要改成 GM 的 `"2"`，会动到**全游戏** `string()` / `draw_text` 输出，要单独一条。
- `string + 容器/undefined`：现在照 `string()` 口径（`"u=undefined"` / `"c=[array]"`），可翻成响亮报错。
- 字符串拼接 1 MB 上限（`STR_CONCAT_MAX`）是拍的值。
- `keyboard_get_string` 契约里没有字段（IME 文本区留在 B1 槽 40-63 未定义）。

---

## 7 踩过的坑（别重踩）

1. **宽度契约**：`VM_INT`/`VM_FLOAT` 的宽度是跨 DLL 契约（`Value` 8 字节），改它要 +版本号 + 重编插件。
2. **callbin 帧只有 `cb.value` 个槽**：别往 `SCRATCH_BASE` 那种高位槽写 —— `OP_NATIVE` 也会从 callbin 帧里发出来，写越界会踩公共内存（`native_forward` 就是因此改成"零临时槽"的）。
3. **`str_at` 的引用会被 `intern_str` 扩容搞悬空** → 拼接前先把文本拷成 `std::string`。
4. **中文路径**：`fopen` 在 Windows 按 ANSI 解释 UTF-8 → 一律 `_wfopen(fs::u8path(p).c_str(), L"…")`。
5. **名单重名会让池下标漂移** → 加名字后要校验展开项数/无重名。
6. 生成器管的文件别手改：`outer/outer_split.h`（`tools/gen_outer_split.py`）、`outer/host_fn.cpp` 的注释块（`tools/gen_host_comment.py`）、`const_str.h` 的两段（`tools/gen_name_lists.py`）、`host_layout.*`（`tools/gen_host_layout.py`）。
7. **静态初始化期别碰字符串池**（跨 TU 顺序未定义）→ 注册改成"首次查表时登记 + `outer_init()` 显式调"。
8. `host_file_open` 之类的 host 真原语**不要**去 include 内层头（分层）；要用同布局 POD + `static_assert` 钉住。

---

## 8 关键文件索引

```
DLL_VM/core/          vm.cpp(解释器/OP_ADD 拼接) vm_def.h(全部内层声明/错误码) pool.cpp(value_to_text/intern_str)
                      instance.cpp(实例系统) call.cpp(callbin 调用) const_str.h/.cpp(名字名单=字符串唯一真源)
DLL_VM/outer/         fn_table.cpp(表体+patch) fn_dll.cpp(名字→实现双表+注册口) forward.cpp(转发)
                      dispatch.cpp(run_block/run_block_on/run_event_user) packet.cpp(入站包解包)
                      host_push.cpp(VM_HostPush) host_state_in.cpp(B1 输入 6 条)
                      game_*.cpp / builtin.cpp / container.cpp / inst_fn.cpp / host_fn.cpp(空壳) / host_real.cpp
DLL_VM/host/          host.h(接口总表) host_shared.cpp(出站流+契约版本) host_state.cpp(B1) host_packet.cpp(入站入口)
                      host_layout.h/.json(契约生成物) host_file/buffer/draw/input/net.cpp
DLL_VM/business/      native_biz.cpp（只有占位；三个导出待写）
DLL_VM/tests/         build.bat(纯 ASCII) selftest.cpp(330+ 断言)
tools/                gen_host_layout.py gen_name_lists.py gen_outer_split.py gen_host_comment.py
                      gen_gm_builtin_consts.py gen_gm2bin_callbyname.py scan_gm_consts.py scan_missing_kernel_builtins.py
transformGML2txt/     gm2bin.py(翻译器) gml_consts.json(项目常量) gm_builtin_consts.json(GM 内建常量)
docs/                 本文件 · mod_plugin_guide.md
```
