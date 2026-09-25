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

// updateLog 启动弹窗展示的本次更新内容（累计更新，尚未发布；按类别整理，不按追加顺序）
const updateLog = `【本次更新】

## 一、新增函数 · 关卡规则

- **VM_BanAllCard()**：禁用全部卡片（遍历全游戏卡片注册表）
- **VM_CannelBanCard("卡名")**：解除指定卡片的禁用
- **VM_BanWeapon() / VM_BanSuperWeapon() / VM_BanShield()**：禁止角色使用武器 / 超级武器 / 盾牌
- **VM_SetCardShapeCap(等级) / VM_SetCardSkillCap(等级)**：转职 / 技能等级上限，-1=不限制
- **VM_SetWaveAuto(0或1)**：波次自动推进开关，0=关闭后不自动出怪，由脚本 VM_SetWave 控制
- **VM_SetWave(波次,子波次)**：设置当前波次/子波次（只改计数，不出怪）
- **VM_SetShovelFlameRate(系数)**：铲子铲卡返还火苗系数，-1=原逻辑，0~1=直接用该系数
- **VM_GetTimeLimit() / VM_SetTimeLimit(帧数)**：获取 / 设置关卡倒计时（单位帧，60 帧=1s），
  无倒计时返回 undefined；设置时联机自动同步客户端

## 二、新增函数 · 地图与查询

- **VM_EnemyInRange(行1,行2,列1,列2,"类型")**：矩形范围是否存在敌人（前缀和），
  类型 normal/obstacle/diver/air/dance/underground/all
- **VM_GetInstancesInRange("数组名",行1,行2,列1,列2,"enemy"或"card","筛选")**：
  收集范围内实例 id 进指定 VM 命名数组，返回数量（卡片可按 plant_id / plant_type 筛）
- **VM_GetHomingTarget("类型")**：最左且血量最高的敌人 id（追踪索敌），每帧全场只扫一次
- **VM_CanPlace("卡名", 列, 行)**：按游戏正规种植规则判断该格能不能种这张卡——地形、障碍、
  水域/莲叶、护盾层、底座卡、替换开关全都算进去（就是玩家手牌点下去时走的那套规则），1=能 0=不能
- **VM_GetCardProp("卡名","属性名")**：按属性名读一张卡的单值。存档类 shape/level/skill/max_level/max_shape，
  卡池类 plant_type/feature_type/target_card/cost/cooldown；读不到返回 undefined（配合 VM_IsUndefined）

## 三、新增函数 · 创建与实例

- **VM_SpawnBatMouse(列,行)**：在指定格子生成蝙蝠鼠（含降落标记），返回实例 ID
- **VM_CreateInstance("对象名",x,y)**：按像素坐标创建实例（子弹等）
- **VM_DestroyInstance(实例ID)**：直接销毁实例，会触发它的 Destroy 事件
- **VM_RunStep(实例, 轮数)**：让该实例额外跑 N 轮 Step（一轮 = Begin Step + Step + End Step），
  用来做加速；同一个实例被重跑期间再调会被忽略，防无限递归，轮数夹在 1~60
- **VM_IsDestroyed(实例ID)**：判断实例是否已被销毁，1=已销毁 0=存在
- **VM_GetCurCard()**：当前块所属的 mod 卡实例 id

## 四、新增函数 · 弹幕与伤害

- **VM_BulletScreenAdd(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字)**：
  往屏幕弹幕管理器塞一颗子弹。管理器由 obj_battle 开局创建，全场只此一个实例，所有弹幕统一迭代与绘制、不占实例。
  打击范围=格子半径，0=只算本格；伤害计数=最多命中几个，-1=不限次数；存活帧数=倒计时，小于等于 0=不自动消失；
  销毁对象非空则在该子弹消失处创建
- **VM_HomingBulletAdd(贴图,缩放,x,y,速度,伤害,可命中类型,销毁对象,mod名字,销毁贴图,模式)**：
  往追踪弹管理器塞一颗会拐弯的子弹。每帧重新索敌（可以来回换目标），按 point_direction 转向并重写 vx/vy；
  模式 0=只追全场最左的可命中敌人，1=优先本行正前方 150 像素内血最多的敌人，找不到再退回全场最左。
  命中只判定它锁定的那个目标，像素距离 90x85 以内就结算并消失（不扫网格、不穿透），
  走敌人自己的受击事件所以护盾/闪白/音效都对，销毁对象生成在命中点。固定项：动画速度 1、每帧自转 6 度
- **VM_DamageEnemy(敌人ID, 伤害, 伤害类型)**：给敌人造成伤害。走敌人自己的受击事件，所以闪白、受击音效、
  护盾判定全都对（含自己重写了受击事件的 4 个敌人），比直接改 hp 正确。
  伤害类型：normal=有盾只打盾，pierce=盾血一起掉，其它=无视护盾
- **VM_DamageEnemyAsh(敌人ID, 伤害, 伤害类型)**：灰烬伤害。伤害接得下就正常结算，
  接不下就一击必杀、原地换成 obj_mouse_ash_death（不看护盾，同原版大力神）

## 五、新增函数 · 数组与表格

- **VM_ArrayExists("表名")**：这张全局表存不存在（存在且是数组），1=有 0=没有
- **VM_CellCount("表名", 列, 行)**：这一格里有几个元素
- **VM_CellItem("表名", 列, 行, 第几个)**：这一格里第 k 个元素（k 从 0 起）
- **VM_CellContains("表名", 列, 行, 值)**：这一格有没有这个值，1=有 0=没有
- **VM_ArrayContains("表名", 值)**：整张表里有没有这个值，1=有 0=没有
  表不存在 / 不是数组 / 列行越界一律返回 -1，所以先 n = VM_CellCount(...) 再 while 遍历是安全的
- **VM_InstArrayExists / Size / Item / Set / Add / Del / Clear / Contains(实例ID, "变量名", ...)**：
  读写挂在实例身上的一维数组（给每张卡自己存状态用）；失败或不存在返回 -1

## 六、新增函数 · 贴图与绘制

- **VM_LoadSpritePerm_Ex("文件名",帧数,原点X,原点Y)**：永久缓存贴图并指定原点，
  不被 bin 重载清理，可用 VM_FreeSpritePerm 释放
- **VM_SpriteExists("贴图名")**：这张贴图现在真的可用吗。按解析链查：项目资源 → VM 临时缓存 →
  VM 永久缓存 → 全局贴图缓存。注意 get_load_sprite 找不到时会塞一张空白占位图且永不失败，
  所以不能用"有没有返回"来判断；本函数会排除占位图，返回 1=可用 0=不可用
- **VM_AliasSpritePerm("新名","已有名")**：把新名永久指向已有名（写永久缓存，进房间不会被清）。
  已有名必须已在永久缓存里（VM_LoadSpritePerm / _Ex 加载过）。存的是精灵 id，所以 get_load_sprite
  直接返回真 id，可以喂给 sprite_index。[reloadmod] 时会自动释放所有这类别名（底下的真图不动），
  让重新加载的 bin 再挂一次。配合 VM_SpriteExists 就是「内置有就用内置（性能好）、内置没有才用外置覆盖」
- **VM_DrawSpriteExt("贴图名",子图,x,y,xscale,yscale,角度,alpha)**：直接 draw_sprite_ext
  （颜色固定白色），只能在 _OBJECT_DRAW 块里调用
- **VM_SetDrawSlotEx / VM_SetDrawSlotEx_front(槽位,贴图,x,y,alpha,角度,xscale,yscale)**：
  设置绘制槽位（含旋转缩放），角度/缩放传 -1 或 undefined 用默认值(0/1/1)

## 七、新增函数 · 工具与调试

- **VM_Floor(值) / VM_Ceil(值)**：对数字向下 / 向上取整，非数字返回 undefined
- **VM_IsUndefined(值)**：判断值是否为 undefined（返回 1=是 0=不是）
- **VM_GetKilledProp("属性名")**：读当前销毁事件对象的属性快照，
  仅 _VM_CARD_DESTROYED / _VM_ENEMY_KILLED 期间有效（mouse_id 空串时按对象名兜底）
- **VM_LoadSound("路径")**：加载本地音频文件（只支持 wav），返回音频ID（失败 -1），
  可用 VM_PlaySound 播放（暂时不能使用，等待修复）
- **VM_CallFunc("函数名", 参数1, 参数2, ...)**：按名字调用一张独立字典
  （global._VM_call_dict，在 scr_command_VM.gml 末尾维护）里的函数，返回它的返回值。
  名字对编译器只是字符串、不校验，所以往字典里加函数不用改编译器、不用重编编译器、不用跑 sync_vmfuncs.py。
  第一个参数是函数名，后面是实参（最多 15 个）；配套 **VM_FuncExists** 先判断存在、**VM_FuncDesc** 看说明。
  字典里预置了 **44 个和游戏内部无关的通用工具函数**（数学 / 位运算 / 字符串 / 数值杂项 / 随机），
  清单在 scripts/scr_command_VM_callFunction，见 help.md →「按名字调用函数」。
  ⚠️ 走字典的动态调用**比较慢**（查字典 + 参数地址转值 + 动态调用），只适合低频场景，
  别写进 _VM_FRAME / _OBJECT_STEP 这种每帧每实例跑的地方

## 八、语言语法与编辑器

- **自定义块 _DEFINE_BLOCK_xxx**：写成 _DEFINE_BLOCK_xxx { ... } 定义在最外层，_DEFINE_BLOCK_xxx() 调用。
  没有参数和返回值，变量和调用方共用；编译时把被调块的代码拷到调用块末尾，调用处跳过去、块尾 / exit 跳回来
  （同一个块调 N 次就拷 N 份）。只有事件块能调自定义块，自定义块里不能再调块；块内 exit = 返回调用处，
  事件块内 exit = 结束整个块
- **支持 && / || 逻辑运算**：优先级 || < && < 比较，短路求值（左边能定结果就不算右边），结果是 1 或 0。
  例：if (a == 1 || b == 1) { ... }
- **代码折叠**：以 { 结尾的花括号块（含 } else {、嵌套块）和连续的 // 注释段都能收起。
  行号右边有折叠槽，快捷键 Ctrl+Shift+[ 折叠 / Ctrl+Shift+] 展开 / Ctrl+Alt+[ 全部折叠 / Ctrl+Alt+] 全部展开
- **函数悬停提示**：鼠标移到函数名上，标题行直接显示**函数原型**（例如
  VM_HomingBulletAdd(贴图, 缩放, x, y, 速度, 伤害, 可命中类型, 销毁对象, mod名字, 销毁贴图, 模式)），
  下面一行灰色小字是参数个数，再下面是说明；原型从 help.md 的函数表自动提取，
  help.md 里没写成函数表行的函数退回显示「函数名（N 个参数）」
- **帮助文档重排**：分成「常用命令」和「mod 专属命令」两大块（后者不是私有命令，只是 mod 用得多），
  函数按功能归类，加了「自定义块」章节

## 九、帮助文档修正

- VM_EnemyInRange 参数应为 (行1,行2,列1,列2,"类型")，此前误写成 (左,上,右,下,"类型")
- VM_GetInstancesInRange 参数应为 ("数组名",行1,行2,列1,列2,"enemy"或"card","筛选")，
  此前误写成 ("类型",左,上,右,下,"筛选","筛选2")
- 属性表修正（x/y 世界坐标、grid_col/grid_row 格子坐标）
- 卡槽属性可修改项修正（cooldown / cooldown_timer / cost / vm_current_cost）
- VM_SetProp 增加 "object_name" 特殊属性说明
- 移除 VM_ApplyPlantLevel 相关描述
- 原 VM_GetCardSaveInfo 已由 VM_GetCardProp 取代

## 十、配套编译器与数据

- 编译器函数表 89~142（89~95 / 109~115 / 116~125 / 127~142 分批加入）
- 块名补全：_OBJECT_CFG / _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW / _OBJECT_DESTROY
- 敌人 ID 109 → 134 个（新增海底世界等 25 个敌人）
- 卡片 ID 70 → 79 个（新增 9 张卡片）
- 事件块补全：_VM_BOSS_STATE_CHANGE
- VM_CannelBanCard 纳入卡片名强制校验

## 十一、mod 对象鼠标块

- 块名补全：_OBJECT_MOUSE_ENTER / _OBJECT_MOUSE_LEAVE / _OBJECT_CLICK
- 对应 6 个 mod 对象（卡 / 武器 / 宝石 / 敌人 / 子弹 / 特效）的鼠标移入 / 移出 / 左键点击
- 语法检查、高亮补全、悬停提示、帮助文档已同步
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
