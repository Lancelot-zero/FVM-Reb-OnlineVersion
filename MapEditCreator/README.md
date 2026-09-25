# 美食大战老鼠重生-实验室地图插件编辑工具

LabMapCompiler.exe 脚本的图形编辑器（Wails v2 + CodeMirror 6）。单 exe 分发，内置语法高亮、语法检查、编译输出面板与语法帮助。

## 功能

- **新建 / 打开 / 保存**：Ctrl+N / Ctrl+O / Ctrl+S，支持拖拽 .txt 文件到窗口打开，未保存修改时关闭有确认
- **编译**：F5 / Ctrl+B，编译前自动保存，调用 `LabMapCompiler.exe 脚本.txt`，输出 `.bin` 到脚本旁；输出面板中带行号的错误可点击跳转
- **语法检查**：编辑时实时检查（防抖 350ms），规则全部来自 help.md：
  - 事件块名、函数名、参数个数（~70 个函数表）
  - 字符串参数枚举（敌人 ID 109 / 卡片 ID 71 / 物件名 9 / 宝石名 16 / 地形 / 键名等）
  - 游离 `elif`/`else`、花括号配对、保留字变量名、块外语句
  - 放置规则为**警告**（编译器实测不强制，help.md 放置表已过时）
- **帮助**：内置 help.md 全文（深色 markdown 渲染）

## 编译器定位

1. 编辑器 exe 同目录的 `LabMapCompiler.exe`（优先，可放新版替换）
2. 否则使用内嵌副本（首次编译时自动释放到 `%LocalAppData%\..\Cache\MapEditCreator\`）

## 构建

本机 Go 与 Node 不在 PATH：

| 工具 | 位置 |
|---|---|
| Go 1.25.5 | `D:\Program Files\Go\bin` |
| Node 20 | `D:\Program Files\nodejs` |
| Wails v2.12 | `C:\Users\Lenovo\go\bin`（已在 PATH） |

```bash
# 开发调试
PATH="/d/Program Files/Go/bin:/d/Program Files/nodejs:$PATH" wails dev

# 发布构建 → build/bin/MapEditCreator.exe
PATH="/d/Program Files/Go/bin:/d/Program Files/nodejs:$PATH" wails build

# 后端测试
"D:/Program Files/Go/bin/go.exe" test ./...
```

## 新增 VM 函数同步流程

游戏侧新增 VM 函数后，本编辑器需要同步 6 处才能保持语法检查 / 高亮 / 帮助一致。

### 游戏侧（FVM-Reborn 项目）

1. `scripts/scr_command_VM/scr_command_VM.gml` 实现函数并用 `VM_RegisterFunction` 注册，**注册顺序即函数 ID**
2. `LabMapCompiler/compiler_defs.h` 加同名定义，**ID 必须与注册顺序一致**
3. 运行 `build.bat` 编译游戏

### 编辑器侧（本项目）

1. 编辑 `vmfuncs_spec.json`，声明新函数：

   ```json
   {
     "name": "VM_XXX",
     "args": ["int", "string"],
     "ret": "any",
     "desc": "一句话中文说明（显示在悬停提示）"
   }
   ```

   - 类型：`int` / `float` / `string` / `any`；无参省略 `args`，无返回省略 `ret`
   - 可选字段：`minArgs` / `maxArgs`（可变参数函数）、`testCall`（自定义测试调用行）

2. 双击 `sync_vmfuncs.bat`（或 `python sync_vmfuncs.py`；加 `--dry-run` 只预览不写入）

   脚本自动更新 4 个文件：

   | 文件 | 更新内容 |
   |---|---|
   | `compiler_defs.h` | 编译器函数表，函数 ID 按现有最大值自动递增 |
   | `linter.go` | 语法检查签名表（funcTable）+ 悬停中文描述（funcDescs） |
   | `frontend/src/lablang.js` | 函数高亮与自动补全 |
   | `allfuncs_test.go` | 全函数编译测试用例 |

   **幂等设计**：已同步过的函数自动跳过，可安全重复运行；修改已有函数的
   `desc` 后重跑，会更新 linter 悬停描述与 compiler_defs.h 注释（签名不变）。

3. 手动改两处（脚本不管，因为格式灵活）：

   - `help.md` — 帮助文档函数表，按函数用途加入对应小节
   - `app.go` — 启动更新日志，**累加追加**，不删旧条目（一直未发布）

4. 验证：`"D:/Program Files/Go/bin/go.exe" test ./...`，通过后 `wails build` 出包

## 目录结构

```
main.go          Wails 入口（embed 前端产物 + 内嵌编译器 + help.md）
app.go           后端绑定（文件读写 / 编译 / 语法检查 / 帮助 / 关闭确认 / 更新日志）
linter.go        语法检查器（纯 Go，可单测）
linter_test.go   18 个单测 + help.md 完整示例回归
compiler.go      编译器定位、执行（隐藏控制台窗口）、输出解码（UTF-8→GBK）
compiler_defs.h  编译器函数表（与游戏侧 LabMapCompiler/compiler_defs.h 一致）
sync_vmfuncs.py  新 VM 函数同步脚本（见"新增 VM 函数同步流程"）
sync_vmfuncs.bat 同步脚本入口（双击运行）
vmfuncs_spec.json 新函数声明（name/args/ret/desc）
embedded/        内嵌 LabMapCompiler.exe 副本
frontend/        CodeMirror 6 编辑器（vanilla JS + vite）
```

## 部署脚本产物

把 `MapEditCreator.exe` 与 `LabMapCompiler.exe` 放同一目录即可使用（不放也可，会用内嵌副本）。
