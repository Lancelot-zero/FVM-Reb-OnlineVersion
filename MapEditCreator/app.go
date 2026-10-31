package main

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"time"

	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// Diagnostic 语法检查诊断信息
type Diagnostic struct {
	Line     int    `json:"line"`     // 行号（1 起）
	Severity string `json:"severity"` // error / warning / info
	Message  string `json:"message"`
}

// CompileResult 编译结果
type CompileResult struct {
	Output   string `json:"output"`   // 编译器 stdout+stderr 合并输出
	ExitCode int    `json:"exitCode"` // 编译器退出码，0 = 成功
	Duration string `json:"duration"` // 耗时，如 "123ms"
	BinPath  string `json:"binPath"`  // 预期的输出 .bin 路径
}

// App 应用后端
type App struct {
	ctx   context.Context
	mu    sync.Mutex
	dirty bool // 编辑器是否有未保存修改（由前端 SetDirty 同步）
}

// NewApp 创建 App 实例
func NewApp() *App {
	return &App{}
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
}

// beforeClose 关闭窗口前：有未保存修改时阻止关闭，通知前端弹确认框。
// 注意 wails 约定：返回值 prevent = true 表示阻止关闭。
func (a *App) beforeClose(ctx context.Context) bool {
	a.mu.Lock()
	dirty := a.dirty
	a.mu.Unlock()
	if dirty {
		runtime.EventsEmit(a.ctx, "confirm-close")
		return true // 阻止关闭，等前端确认
	}
	return false
}

// SetDirty 前端同步未保存状态
func (a *App) SetDirty(dirty bool) {
	a.mu.Lock()
	a.dirty = dirty
	a.mu.Unlock()
}

// LogJsError 前端未捕获错误 → 后端日志（调试用）
func (a *App) LogJsError(msg string) {
	println("[JS-ERROR]", msg)
}

// updateLog 启动弹窗展示的本次更新内容（v3.0）
const updateLog = `# v3.0 更新

## 一、新增函数 · 卡槽与绘制

- **VM_RemoveCardSlot(i)**：删掉第 i 个战斗卡槽（i 从 0 起），其余槽重新编号并重排位置。
  只动本场战斗槽，卡组数据 selected_deck 不动
- **VM_InsertCardSlot(i, "卡id")**：在第 i 个位置插入一张卡的战斗卡槽（i 越界自动夹到首/尾；
  任意已注册卡 id，与玩家是否解锁无关），插入即冷却完成，其余槽重排
- **VM_SpawnCardSlot("卡id", x, y)**：创建一个游离卡槽（不属于卡槽组）：不进重排/插槽/删槽/21 上限，
  没有组内快捷键（鼠标点选可用），位置停在指定像素坐标，返回实例 id
- **VM_DrawText(x, y, "文字")**：在 _OBJECT_DRAW 里画一行文字（font_yuan）；
  字号用 VM_DrawSetFontSize、颜色用 VM_DrawSetColour，不设 = 默认字号 + 白色
- **VM_DrawSetFontSize(字号)** / **VM_DrawSetColour(r, g, b)**：设置后续 VM_DrawText 的字号 /
  颜色（r,g,b 各 0~255，任一 -1=恢复白色），设一次一直生效
- **VM_DrawSpritePartExt(贴图,子图,left,top,宽,高,x,y,xscale,yscale,颜色,alpha)**：
  画贴图的指定矩形区域（颜色 -1=白色不染色），只能在 _OBJECT_DRAW 里用
- **卡槽动画参数（VM_SetProp 设置，自动驱动）**：destroy_timer 倒计时自动销毁
  （手里拿着这张卡时倒计时暂停）、destroy_on_plant 种卡后自毁、blink_start 销毁前闪烁、
  jump_on / jump_* 跳跃动画

## 二、mod 直接生成与多 bin 地图

- **VM_SpawnPlant / VM_SpawnEnemy 直接支持 mod 名**：卡名 / 敌名不在内置名单时，编译只警告
  不报错；运行时自动查 mod 注册表（本图卡 bin / 已装 mod），命中就按 mod 卡 / mod 敌人创建。
  不用再手动写 pending 全局 + VM_CreateInstance 那套握手
- **多 bin 实验室地图**：图目录下放 <图名>.<类型>.<名称>.bin（类型 card / enemy / weapon /
  gem / bullet / effect），进图自动扫描装载、同名覆盖旧条目、离图还原；联机时各 bin 自动拼在
  地图 bin 后下发，客机按清单切段
- 编辑器帮助补全：六种 mod 对象总表（各对象怎么创建、身份怎么给）、VM_CreateInstance 的
  mod_type 用法、卡槽操作说明、多 bin 部署说明
`

// GetUpdateLog 返回本次更新内容（前端启动时以网页弹窗展示）
func (a *App) GetUpdateLog() string {
	return updateLog
}

// CheckCode 语法检查（前端防抖调用）
func (a *App) CheckCode(code string) []Diagnostic {
	return lintScript(code)
}

// BlockHint 事件块提示（前端展示用）
type BlockHint struct {
	Name  string   `json:"name"`  // 块名
	Desc  string   `json:"desc"`  // 触发时机
	Funcs []string `json:"funcs"` // 该块中可调用的函数
}

// FuncMeta 函数元信息（悬停提示 + 补全）
type FuncMeta struct {
	Desc string `json:"desc"` // 中文描述（help.md 函数表）
	Args int    `json:"args"` // 固定参数个数；-1 = 变长；0 = 无参数
	Sig  string `json:"sig"`  // 函数原型，例如 VM_GetProp(实例ID, "属性名")；空 = help.md 没写函数表行
}

// EditorMeta 编辑器元数据（前端启动时一次性拉取）
type EditorMeta struct {
	Blocks map[string]BlockHint `json:"blocks"`
	Funcs  map[string]FuncMeta  `json:"funcs"`
}

// GetEditorMeta 返回全部函数与事件块的元信息
func (a *App) GetEditorMeta() EditorMeta {
	meta := EditorMeta{
		Blocks: make(map[string]BlockHint, len(blockHints)),
		Funcs:  make(map[string]FuncMeta, len(funcTable)),
	}
	for name, h := range blockHints {
		meta.Blocks[name] = BlockHint{Name: name, Desc: h.Desc, Funcs: h.Funcs}
	}
	for name, spec := range funcTable {
		argc := -1
		if spec.min == spec.max {
			argc = spec.min
		}
		meta.Funcs[name] = FuncMeta{Desc: funcDescs[name], Args: argc, Sig: funcSigs[name]}
	}
	return meta
}

// GetBlockInfo 返回第 line 行所在事件块的提示（不在块内返回空）
func (a *App) GetBlockInfo(code string, line int) BlockHint {
	ev := enclosingEventAt(code, line)
	if ev == nil {
		return BlockHint{}
	}
	h := blockHints[ev.name]
	return BlockHint{Name: ev.name, Desc: h.Desc, Funcs: h.Funcs}
}

// ReadFile 读取文本文件，自动识别 UTF-8（含 BOM）/GBK
func (a *App) ReadFile(path string) (string, error) {
	if path == "" {
		return "", fmt.Errorf("路径为空")
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return "", fmt.Errorf("读取文件失败: %w", err)
	}
	return decodeText(data), nil
}

// WriteFile 写文本文件，始终 UTF-8
func (a *App) WriteFile(path, content string) error {
	if path == "" {
		return fmt.Errorf("路径为空")
	}
	dir := filepath.Dir(path)
	if dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return fmt.Errorf("创建目录失败: %w", err)
		}
	}
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		return fmt.Errorf("保存文件失败: %w", err)
	}
	return nil
}

// Compile 编译脚本：调用 LabMapCompiler.exe 脚本.txt（输出同名 .bin 在脚本旁）
func (a *App) Compile(scriptPath string) (CompileResult, error) {
	if scriptPath == "" {
		return CompileResult{}, fmt.Errorf("脚本路径为空")
	}
	if _, err := os.Stat(scriptPath); err != nil {
		return CompileResult{}, fmt.Errorf("脚本文件不存在: %s", scriptPath)
	}

	compilerPath, err := a.locateCompiler()
	if err != nil {
		return CompileResult{}, err
	}

	start := time.Now()
	out, exitCode, err := runCompiler(compilerPath, scriptPath)
	result := CompileResult{
		Output:   out,
		ExitCode: exitCode,
		Duration: time.Since(start).Round(time.Millisecond).String(),
		BinPath:  binPathFor(scriptPath),
	}
	if err != nil {
		return result, err
	}
	return result, nil
}

// GetHelp 返回语法帮助（help.md 全文）
func (a *App) GetHelp() string {
	return helpMarkdown
}

// GetTemplate 新建文件时的模板：完整脚本骨架，语句全部注释，删除行首 // 即启用
func (a *App) GetTemplate() string {
	return `// ============================================================
//  LabMap 脚本完整模板
//
//  使用方法：
//  1. 所有事件块（_VM_*）都已就位，块内语句默认全部注释
//  2. 删除语句行首的 // 即可启用该语句
//  3. 事件块只有到对应时机才会自动执行（见各块上方注释）
//
//  语法速查：
//  - 赋值/运算：x = 10    y = x + 5    ok = (x > y)
//    支持 + - * / %，比较 == != > >= < <=，逻辑 && ||
//    优先级：|| < && < 比较 < + - < * / %；没有逻辑非 !，用 x == 0 代替
//  - 条件：if (a == 1) { ... } elif (a == 2) { ... } else { ... }
//    （else if 与 elif 等价；elif/else 必须紧跟刚闭合的 if）
//  - 注释：// 单行注释（字符串内的 // 不算注释）
//  - 结束当前块：halt（与 exit 同义）
//  - 一行一句；分号 ; 视作换行；表达式不能跨行
//  - 跨块变量共享：不同块中同名变量共用同一个内存槽，可跨事件传值
//
//  提示：打开"帮助"按钮可查看完整语法手册
// ============================================================

// ============================================================
//  _VM_ROOM_READY_ENTRY · 进入准备室时执行（设置规则）
// ============================================================
_VM_ROOM_READY_ENTRY {
//    VM_BanCard("small_fire")            // 禁用"小火苗"卡片，玩家无法携带
//    VM_BanGem("laser_gem")              // 禁用"激光宝石"
//    VM_SetCardLevelCap(10)              // 卡片等级上限设为 10
//    VM_SetMaxSlots(10)                  // 玩家最多携带 10 张卡片
//    VM_SpawnCats(1)                     // 是否生成初始猫（0=不生成，默认 1）
//    VM_SetEventEnabled(1)               // 事件系统开关（默认 1）
}

// ============================================================
//  _VM_BATTLE_START · 战斗开始时执行一次（造地图/初始化）
// ============================================================
_VM_BATTLE_START {
//    VM_SetFlame(3000)                   // 开局火苗数
//    VM_SetTerrain(-1, -1, "normal")     // 全图地形（-1=全部；normal/water/obstacle）
//    VM_SetRowFeature(3, "water")        // 某一行改为水路（-1=所有行；land/water）
//
//    // 创建平台：列,行,宽,高,轴向(0=上下 1=左右),移动距离,边界停顿帧,贴图
//    a = VM_CreatePlatform(0, 0, 4, 5, 0, 2, 200, "spr_raft_platform")
//
//    // 刷地图元素
//    VM_SpawnObject("obstacle", 2, 2)    // 刷物件（obstacle/lava/wind_tunnel 等）
//    VM_SpawnPlant("small_fire", 3, 3, 0, 0, 0)  // 种植物（卡名,列,行,外形,星级,技能）
//    VM_SpawnEnemy("normal_mouse", 0, 100)       // 刷敌人（敌人类型,行,血量）
//    VM_SpawnBoss("arno", 2, 80000)              // 刷 BOSS（BOSS类型,行,血量）
//    VM_SpawnPlantsRandom(0, 0, 3, 2, 0, 0, 0, "small_fire", "-1", "-1", "-1", "-1", "-1", "-1", "-1", "-1")  // 区域内随机种植（后9参数为卡名，"-1"=跳过）
//
//    // 贴图与背景
//    VM_LoadSprite("spr_some_image")     // 加载贴图到临时缓存（bin 重载自动清理）
//    VM_SetMapBackground("bg_name", 0.02) // 渐变切换背景（贴图名,步长）
//
//    // 创建按钮：x,y,精灵名,缩放,idle帧,hover帧,press帧
//    b = VM_CreateButton(1, 1, "spr_btn", 1.0, 0, 1, 2)
}

// ============================================================
//  _VM_WAVE_START · 新一波开始时执行（用 VM_GetWave() 拿波数）
// ============================================================
_VM_WAVE_START {
//    w = VM_GetWave()                    // 当前波数（从 0 开始）
//    if (w == 0) {                       // 第 1 波
//        VM_ShellPrint("第一波开始")
//    } elif (w == 1) {                   // 第 2 波
//        VM_ShellPrint("第二波开始")
//    }
}

// ============================================================
//  _VM_WAVE_END · 当前波结束时执行（用 VM_GetWave() 拿波数）
// ============================================================
_VM_WAVE_END {
//    w = VM_GetWave()                    // 刚结束的波数
//    VM_SetFlame(VM_GetFlame() + 500)    // 每波结束奖励 500 火苗
}

// ============================================================
//  _VM_SUBWAVE_START · 新子波开始时执行（用 VM_GetSubwave() 拿子波数）
// ============================================================
_VM_SUBWAVE_START {
//    sw = VM_GetSubwave()                // 当前子波数
//    VM_ShellPrint("子波: ", sw)
}

// ============================================================
//  _VM_SUBWAVE_END · 子波结束时执行（用 VM_GetSubwave() 拿子波数）
// ============================================================
_VM_SUBWAVE_END {
//    sw = VM_GetSubwave()                // 刚结束的子波数
}

// ============================================================
//  _VM_CARD_CREATED · 卡片被种下时执行（用 VM_GetLastCreatedCard() 拿实例）
// ============================================================
_VM_CARD_CREATED {
//    id = VM_GetLastCreatedCard()        // 刚种下的植物实例 ID
//    name = VM_GetProp(id, "plant_id")   // 读植物卡片名（字符串属性返回字符串）
//
//    // 按卡名强化：煮鸡蛋攻击力翻倍
//    if (name == "egg_boiler_pult") {
//        VM_SetProp(id, "atk", VM_GetProp(id, "atk") * 2)
//    }
//
//    // 咖啡壶额外加血
//    if (name == "coffee_pot") {
//        VM_SetProp(id, "hp", VM_GetProp(id, "hp") + 500)
//    }
//
//    // 改完等级/技能/外形后调用，刷新数值
//    VM_ApplyPlantLevel(id)
}

// ============================================================
//  _VM_CARD_DESTROYED · 卡片被销毁时执行（用 VM_GetLastDestroyedCard() 拿实例）
// ============================================================
_VM_CARD_DESTROYED {
//    id = VM_GetLastDestroyedCard()      // 刚销毁的植物实例 ID
//    plant = VM_GetKilledProp("plant_id") // 销毁瞬间属性快照（hp/grid_col/grid_row 等）
//    VM_ShellPrint("卡片被销毁: ", id)
}

// ============================================================
//  _VM_CARD_DAMAGED · 卡片受伤时执行
// ============================================================
_VM_CARD_DAMAGED {
//    VM_ShellPrint("卡片受伤")
}



// ============================================================
//  _VM_CARD_PREVIEW_PICKED · 卡片被选取到手槽时执行（用 VM_GetPreviewCard() 拿 card_id）
// ============================================================
_VM_CARD_PREVIEW_PICKED {
//    card = VM_GetPreviewCard()          // 当前手牌 card_id（没有返回 -1）
//    VM_ShellPrint("选中卡片: ", card)
}

// ============================================================
//  _VM_ENEMY_SPAWNED · 敌人出现时执行（用 VM_GetLastCreatedEnemy() 拿实例）
// ============================================================
_VM_ENEMY_SPAWNED {
//    id = VM_GetLastCreatedEnemy()       // 刚刷出的敌人实例 ID
//    VM_SetEnemyProp("normal_mouse", "hp", 200)  // 按类型改属性（"all"=全部，跳过 BOSS）
}

// ============================================================
//  _VM_ENEMY_KILLED · 敌人死亡时执行（用 VM_GetLastKilledEnemy() 拿实例）
// ============================================================
_VM_ENEMY_KILLED {
//    id = VM_GetLastKilledEnemy()        // 刚死亡的敌人实例 ID
//    mouse = VM_GetKilledProp("mouse_id") // 销毁瞬间属性（空串时按对象名兜底）
//    VM_SetFlame(VM_GetFlame() + 50)     // 击杀奖励 50 火苗
}

// ============================================================
//  _VM_ENEMY_DAMAGED · 敌人受伤时执行
// ============================================================
_VM_ENEMY_DAMAGED {
//    VM_ShellPrint("敌人受伤")
}

// ============================================================
//  _VM_PLAYER_DAMAGED · 玩家受伤时执行
// ============================================================
_VM_PLAYER_DAMAGED {
//    VM_ShellPrint("玩家受伤")
}



// ============================================================
//  _VM_PLATFORM_IDLE_END · 平台到达边界并结束停顿时执行
//  （用 VM_GetLastIdlePlatform() 拿实例）
// ============================================================
_VM_PLATFORM_IDLE_END {
//    // 判断是不是平台 a，然后轮换移动模式
//    if (VM_GetLastIdlePlatform() == a) {
//        if (a_cnt == 0) { VM_SetPlatformParams(a, 0, 2, 200, 1) }   // 上下、2格、停顿200帧、正向
//        if (a_cnt == 1) { VM_SetPlatformParams(a, 1, 2, 200, 1) }   // 左右、2格、停顿200帧、正向
//        if (a_cnt == 2) { VM_SetPlatformParams(a, 0, 2, 200, -1) }  // 上下、2格、停顿200帧、反向
//        if (a_cnt == 3) { VM_SetPlatformParams(a, 1, 2, 200, -1) }  // 左右、2格、停顿200帧、反向
//        a_cnt = (a_cnt + 1) % 4         // 状态 +1 循环（0→1→2→3→0）
//    }
}

// ============================================================
//  _VM_MOUSE_LEFT · 鼠标左键按下时执行（单帧）
// ============================================================
_VM_MOUSE_LEFT {
//    col = VM_GetMouseCol()              // 鼠标所在网格列
//    row = VM_GetMouseRow()              // 鼠标所在网格行
//    VM_ClearPlants(col, row)            // 点击清除该格植物（-1=全部）
}

// ============================================================
//  _VM_MOUSE_RIGHT · 鼠标右键按下时执行（单帧）
// ============================================================
_VM_MOUSE_RIGHT {
//    col = VM_GetMouseCol()              // 鼠标所在网格列
//    row = VM_GetMouseRow()              // 鼠标所在网格行
//    VM_ShellPrint("右键点击: ", col, ", ", row)
}

// ============================================================
//  _VM_KEY_PRESSED · 键盘按键按下时执行（单帧）
//  （键名：A~Z、0~9、f1~f12、up/down/left/right、space/enter 等，不区分大小写）
// ============================================================
_VM_KEY_PRESSED {
//    if (VM_GetKeyPressed("space")) {    // 空格刚按下（单帧有效）
//        VM_ShellPrint("空格被按下")
//    }
//    if (VM_GetKeyDown("shift")) {       // Shift 按住中
//        VM_ShellPrint("Shift 按住中")
//    }
}

// ============================================================
//  _VM_BUTTON_CLICKED · 按钮被点击时执行（用 VM_GetLastClickedButton() 拿实例）
// ============================================================
_VM_BUTTON_CLICKED {
//    b = VM_GetLastClickedButton()       // 最后点击的按钮 ID（没有返回 -1）
//    if (b >= 0) {
//        phase = phase + 1               // 跨块变量：phase 在其他块中也有效
//        if (phase > 2) { phase = 0 }    // 单行 if：按钮相位循环 0→1→2→0
//        VM_ShellPrint("按钮: ", b, " 相位: ", phase)
//    }
}

// ============================================================
//  _VM_FRAME · 每帧执行 ⚠️ 禁止写复杂逻辑
// ============================================================
_VM_FRAME {
//    x = VM_GetMouseX()                  // 鼠标世界 X 坐标（轻量查询可用）
//    y = VM_GetMouseY()                  // 鼠标世界 Y 坐标
//    // 需要每帧绘制的可配合：
//    // VM_SetDrawSlot_front(0, "spr_ui", x, y, 1.0)  // 前景绘制槽（槽位0~7,贴图,x,y,alpha 0~1）
}

// ============================================================
//  _VM_TIMER_5f / 10f / 15f / 30f / 60f · 定时器（每 N 帧执行一次）
// ============================================================
_VM_TIMER_5f {
//    t5 = t5 + 1                         // 每 5 帧 +1
}
_VM_TIMER_10f {
//    t10 = t10 + 1                       // 每 10 帧 +1
}
_VM_TIMER_15f {
//    t15 = t15 + 1                       // 每 15 帧 +1
}
_VM_TIMER_30f {
//    t30 = t30 + 1                       // 每 30 帧 +1
}
_VM_TIMER_60f {
//    t60 = t60 + 1                       // 每 60 帧 +1（约每秒）
}

// ============================================================
//  _VM_BOSS_STATE_CHANGE · BOSS 状态改变时执行
//  （用 VM_GetLastBossStateChangeId() 拿实例）
// ============================================================
_VM_BOSS_STATE_CHANGE {
//    id = VM_GetLastBossStateChangeId()  // 刚改变状态的 BOSS 实例 ID
//    old = VM_GetLastBossOldState()      // 旧状态
//    new = VM_GetLastBossNewState()      // 新状态
//    VM_ShellPrint("BOSS ", id, " 状态: ", old, " -> ", new)
}

// ============================================================
//  对象块（_OBJECT_*）
//
//  这 5 个块是给 mod 对象脚本用的 —— 写 mod 卡 / 武器 / 子弹 / 特效 / 敌人的
//  .txt 时才用得上（一个 .bin 一个对象），地图脚本里用不到。
//  放在模板里只是让编辑器认得它们、不报「未知事件块」。
// ============================================================

// 配置块：bin 加载时执行一次，通常用来预加载贴图
_OBJECT_CFG {
//    VM_LoadSpritePerm_Ex("spr_xxx.png", 36, 202, 235)  // 贴图名,总帧数,原点X,原点Y
}

// 创建时执行一次
_OBJECT_CREATE {
//    self = VM_GetCurCard()              // 本实例
//    VM_SetProp(self, "cd", 0)           // 属性要先自己设默认值再读，别读没设过的属性
//    VM_SetProp(self, "sprite_index", "spr_xxx.png")
}

// 每帧执行
_OBJECT_STEP {
//    t = VM_GetProp(self, "cd") + 1      // 读
//    VM_SetProp(self, "cd", t)           // 写
}

// 绘制时执行 —— 只有这个块里才能画东西（Step 里调绘制函数画不出来）
_OBJECT_DRAW {
//    VM_DrawSpriteExt("spr_xxx.png", 0, 100, 200, 1, 1, 0, 1)  // 贴图,子图,x,y,xscale,yscale,角度,alpha
}

// 销毁时执行
_OBJECT_DESTROY {
//    VM_ShellPrint("对象被销毁")
}

// 鼠标移入本实例（6 个 mod 对象都支持）
_OBJECT_MOUSE_ENTER {
}

// 鼠标移出本实例
_OBJECT_MOUSE_LEAVE {
}

// 鼠标左键点击本实例（按实例判定，和全局的 _VM_MOUSE_LEFT 不同）
_OBJECT_CLICK {
//    VM_ShellPrint("被点了一下")
}
`
}

// GetCompilerInfo 返回当前使用的编译器路径
func (a *App) GetCompilerInfo() string {
	p, err := a.locateCompiler()
	if err != nil {
		return err.Error()
	}
	return p
}

// OpenInExplorer 在资源管理器中定位文件；路径为空时打开程序所在目录
func (a *App) OpenInExplorer(path string) error {
	if path == "" {
		exe, err := os.Executable()
		if err != nil {
			return fmt.Errorf("路径为空")
		}
		return exec.Command(explorerExe(), filepath.Dir(exe)).Start()
	}
	return openInExplorer(path)
}

var txtFileFilters = []runtime.FileFilter{
	{DisplayName: "LabMap 脚本 (*.txt)", Pattern: "*.txt"},
	{DisplayName: "所有文件 (*.*)", Pattern: "*.*"},
}

// PickOpenFile 弹出"打开脚本"对话框，返回所选路径（取消返回空串）
func (a *App) PickOpenFile() (string, error) {
	path, err := runtime.OpenFileDialog(a.ctx, runtime.OpenDialogOptions{
		Title:   "打开脚本",
		Filters: txtFileFilters,
	})
	if err != nil {
		return "", fmt.Errorf("打开对话框失败: %w", err)
	}
	return path, nil
}

// PickSaveFile 弹出"保存脚本"对话框，返回所选路径（取消返回空串）
func (a *App) PickSaveFile(defaultName string) (string, error) {
	if defaultName == "" {
		defaultName = "script.txt"
	}
	path, err := runtime.SaveFileDialog(a.ctx, runtime.SaveDialogOptions{
		Title:           "保存脚本",
		DefaultFilename: defaultName,
		Filters:         txtFileFilters,
	})
	if err != nil {
		return "", fmt.Errorf("保存对话框失败: %w", err)
	}
	return path, nil
}
