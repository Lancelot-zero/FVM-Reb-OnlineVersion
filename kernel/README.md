# kernel/ —— 构建视图（自动生成，别手改）

生成器：`transformGML2txt/make_kernel_view.py`

    kernel/<类型>/<对象名>/<事件>.gml     ← 从 objects/ 复制
    kernel/function/<函数名>.gml          ← 从 scripts/ 拆出来（一个文件一个函数）

⚠ **不是真源**：改这里不会改 objects/scripts；重跑生成器会**整个删掉重建**
（连转换产物 `.bin` / `.callbin` 一起没），要重新转换。改真源请去 `objects/`、`scripts/`。

---

## 一、重新生成视图

```bash
python transformGML2txt/make_kernel_view.py
```

收什么：能编成 bin 的对象 + `function/` 里那批（含**依赖闭包**）；
不收：hot 覆盖的 12 个对象、白名单父对象、UI/基础设施（menu / laboratory / basic / button / other）。

## 二、转换（**两遍**，顺序不能反）

```bash
# 第 1 遍：函数 → .callbin（对象侧编译时要先认得这些名字）
python transformGML2txt/gm2bin.py kernel/function --recursive --out kernel

# 第 2 遍：对象 → .bin，并把 callbin 目录喂进去
#   → 那批函数的调用点会编成 VM_CallFunc(...)，而不是落到 native 占位转发
python transformGML2txt/gm2bin.py kernel --recursive --out kernel --callbins kernel/function
```

⚠ **第 2 遍的 `--callbins` 不能漏** —— 漏了就报一堆 `function "X" has no builtin`，
因为已经编好的 callbin 名字没被认到。

## 三、产物布局（和源放一起，同名）

```
kernel/card/obj_aquarius_elve/
      Create_0.gml  Step_0.gml …          ← 源
      obj_aquarius_elve.bin               ← 产物（一个对象一个）
kernel/function/
      select_shovel.gml                   ← 源（一个函数一个）
      select_shovel.callbin               ← 产物
```

产物不入库：`.gitignore` 里有 `kernel/**/*.bin`、`kernel/**/*.callbin`。

## 四、递归转换模式本身

```bash
gm2bin.py <目录> --recursive [--out <输出目录>] [--callbins <目录>]
```
- 含 **单个 function** 的 .gml → 编成 `.callbin`（文件名 = 函数名）
- 含事件 .gml 的目录 → 编成一个对象 `.bin`（对象名 = 目录名）
- 输出镜像输入树（相对路径）

## 五、常用检查

```bash
transformGML2txt/vm.exe <某个.bin> _VM_CONST_INIT     # 装载空跑一遍
transformGML2txt/vm.exe --selftest-instbox            # 实例容器自检（数组/map 双模式 + 游标）
```
