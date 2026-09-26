# Mod 开发工具说明

写给"做 mod 的人"的总说明。**先看本文**（能做哪些、通用流程与约定），再按类别看对应的分文件。

| 文档 | 内容 |
|---|---|
| **本文** | 能做哪几种、通用流程、块名与贴图约定、**实例字段（x/y/image_* 等）**、常见坑 |
| [mod卡片.md](mod卡片.md) | 卡片：JSON 字段（17 档数值等）、动画、联动计数 |
| [mod武器.md](mod武器.md) | 武器：三种槽位、JSON 字段、动画四属性 |
| [mod宝石.md](mod宝石.md) | 宝石：被动/主动、点击与冷却契约、图标两道静默失败 |
| [mod敌人.md](mod敌人.md) | 敌人：JSON 字段、行为要自己写 |
| [mod子弹.md](mod子弹.md) | 子弹：两步创建、`vx/vy`、碰撞自己写、更省的两个弹幕接口 |
| [mod特效.md](mod特效.md) | 特效：跟随实例、自动销毁 |
| [mod时装.md](mod时装.md) | 时装：只有 json 的纯外观 |
| [mod地图.md](mod地图.md) | 地图：`world.json` 与关卡（实验室格式 + 关卡脚本） |
| `help.md` | VM 脚本语言：语法、事件块、**全部 VM 函数** |

---

## 零、先弄明白这几个词

| 词 | 意思 |
|---|---|
| **mod** | 你自己加进游戏的东西：一张卡、一把武器、一个敌人……一个 mod = 一个 id |
| **id** | 名字，同时也是**文件名**。`my_card.json` 的 id 就是 `my_card`，游戏里认的就是它 |
| **json** | **配置**（数值、贴图名、商店价格…），用记事本/编辑器直接改 |
| **txt / bin** | **逻辑**。`txt` 是你写的源码，`bin` 是编译出来的产物，**游戏只读 bin** |
| **VM 脚本** | txt 里写的东西，语法见 `help.md`（不是 C / JS，一共十来条语法 + 一堆 `VM_*` 函数） |
| **块** | txt 里的 `_OBJECT_STEP { ... }` 这种段落。**写进哪个块，就决定它什么时候跑** |
| **编译** | 把 txt 变成 bin：编辑器 `Ctrl+B` / `F5`，或命令行 `LabMapCompiler.exe my_card.txt` |
| **tex** | 自带贴图放这个文件夹，PNG 丢进去，json / 脚本里写文件名 |
| **注册** | 游戏开局前把你的 mod 记进卡池 / 敌人池等表里。**新加的 mod 得重开游戏或 `[reloadallmod]` 才会被扫到** |
| **热重载** | 文件名没变、只改内容时，控制台 `[reloadmod]` 就能生效，不用重开 |

## 最短上手路径（照着做一遍就会了）

目标：加一张自己的卡 `my_fire`（拿游戏内置的小火炉贴图凑合用）。

1. 在**游戏目录**（跑游戏的那个文件夹）里建 `mod/cards/`
2. 写 `mod/cards/my_fire.json` —— 配置（数值全是 **17 档数组**，对应等级 0~16，数一下是 17 个）：

   ```json
   {
     "name": "我的小火炉",
     "shapes": [
       {
         "shape": 0,
         "name": "我的小火炉",
         "description": "我的小火炉：生产火苗",
         "sprite": "spr_small_fire",
         "plant_type": "normal",
         "feature_type": "normal",
         "target_card": "none",
         "hp":       [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50],
         "cost":     [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50],
         "atk":      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
         "range":    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
         "cooldown": [420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420],
         "cycle":    [1500, 1440, 1380, 1320, 1260, 1200, 1140, 1080, 1020, 960, 900, 840, 780, 720, 660, 600, 540]
       }
     ],
     "shop": { "cost": "1000", "description": "测试卡" }
   }
   ```

   - 每一档 = 一个等级（第 1 个数是 0 级，最后一档是 16 级）；写不满也能跑（缺的按最后一个补齐），但**要改数值就得写满**
   - 想让某个属性所有等级都一样，也可以只写一个数字（`"hp": 50`），引擎会自动铺成 17 档
   - 想加技能/图鉴，再加 `"skill": { "attr": "flame_produce", "values": [25, 27, 29, 31, 34, 37, 40, 44, 48] }`（9 档）和 `"info_island": "说明"`

3. 写 `mod/cards/my_fire.txt` —— 逻辑，先只写最小的一段（**先让形象出来**）：

   ```text
   _OBJECT_CREATE {
       self = VM_GetCurCard()
       VM_SetProp(self, "sprite_index", "spr_small_fire")   // 用游戏内置贴图当形象
   }
   ```

4. **编译**：编辑器里按 `Ctrl+B`（命令行就是 `LabMapCompiler.exe my_fire.txt`）→ 同目录出现 `my_fire.bin`
5. 进游戏，**按 `F1` 打开控制台**，敲 `[reloadallmod]`（**新文件必须重扫**）→ 再敲 `[listmod]`
   - 列表里出现 `my_fire` ＝ 加载成功
6. 以后每次改 txt：重新按 `Ctrl+B` → 控制台 `[reloadmod my_fire]` → 立刻能看到改动

> 控制台：游戏里按 **`F1`** 开关（再按一次 `F1` 或 `Esc` 关），命令直接敲、回车执行。

> 想省事、又搞不清"为什么没生效"的时候：**改完直接重启游戏**，一定是最新的。

## ⚠️ 性能铁律：让 STEP 尽量少进 VM（卡片 / 子弹尤其重要）

解释一遍字节码是要花时间的，而且是**每帧 × 每个实例**都要花。卡片和子弹是场上的多数，
`_OBJECT_STEP` 写得勤，就是掉帧最大的来源。

**所以卡片 / 子弹的第一原则：能不进就不进 —— 用"进 VM 的时机"控制，别用默认的每帧进。**

| 情形 | 推荐写法 |
|---|---|
| 卡片 · 射手（有敌人才打、按节奏开火） | `mod_step_enter_condition = "norm_attack"` + `mod_step_enter_arr` 里只写开火那几帧 |
| 卡片 · 生产 / 计时 | `"norm"` + 只在"要生产那一帧"的窗口 |
| 卡片 · 被打才有反应 | `"hp_change"` / `"hp_change_mod"`（平时完全不进） |
| 子弹 | `"cell"`（**跨格那一帧**才进，正好对上路径碰撞） |

配套要点：

- **窗口表一律写"从一轮末尾倒数"的负数**（例 `-30`）。写绝对帧号，一吃技能（`skill.attr = "cycle"` 会把 `cycle` 改小）就永远命中不了 —— 见「常见坑速查」
- **一次能算完的别放 STEP**：常量、预加载贴图放 `_OBJECT_CFG`（它只在加载/重载时跑一次）
- **别在 STEP 里做每帧都要查的重活**：结果没变就存成实例字段，下一帧直接读；数组/查询能挪到窗口里做就挪
- **大量同质子弹优先用屏幕弹幕**（`VM_BulletScreenAdd_Exs`）：整批由管理器一次迭代，**不占实例、不各跑一个 VM**，比"一颗子弹一套 VM"便宜得多
- **全场的每帧逻辑别塞进 `_OBJECT_STEP`**：真要每帧做就写地图脚本的 `_VM_FRAME`，而且那里也只放轻活
- 判断标准很简单：**这个字段/这段代码，是不是只有那 1~2 帧用得上？** 是 → 就别每帧跑
- **子弹更狠一点：很多子弹连 STEP 都不用写** —— 伤害、减速、冻结、眩晕、命中几次消失、抛物线
  这些都有预设字段（`bullet_*`），设好就由引擎在碰撞事件里自动结算；真要写逻辑，也用
  `"attack_collision"` / `"cell_target_row"` 这类条件只在关键那几帧进（见 `mod子弹.md`）

### 没效果 / 报错了，去哪看

| 看哪里 | 能看到什么 |
|---|---|
| **控制台**（游戏里按 `F1`） | VM 报错信息、`VM_ShellPrint` 打的字、`[reloadmod]` 这类命令的回显 —— **先看这里** |
| **`mod/mod_errors.log`**（游戏目录的 `mod/` 下） | 控制台刷太快错过的报错会记到这里（时间 + 内容，只留最近 2000 行；同一个错连续刷屏时每 60 次才记一条） |
| 游戏窗口的 debug 输出 | `show_debug_message` 的内容（编辑器里跑的时候才方便看） |

几个**不会报错**的坑，只能自己查：

- **贴图找不到** → 给一张占位图，静默；用 `VM_SpriteExists("名字")` 判断，自带图记得在 `_OBJECT_CFG` 里预加载
- **音效名写错** → 静音，什么都不提示（必须是内置的 `snd_*`）
- **读了没设过的属性** → **报错并中断整个块**（这一帧后面都不跑了），先在 `_OBJECT_CREATE` 里给一份默认值
- **地图脚本**里的报错会被吞掉，调不出效果时先在 `_VM_BATTLE_START` 里 `VM_ShellPrint("进来了")` 确认块跑了

---

## 一、能做哪几种 Mod

| 类别 | 目录 | 需要的文件 | 注册成什么 | 分文件 |
|---|---|---|---|---|
| **卡片** | `mod/cards/` | `.json` + `.bin`（+`tex/`） | 卡池 + 植物数据 | [mod卡片.md](mod卡片.md) |
| **武器** | `mod/weapons/` | `.json` + `.bin`（+`tex/`） | 主武器 / 超级武器 / 副武器 | [mod武器.md](mod武器.md) |
| **宝石** | `mod/gems/` | `.json` + `.bin`（+`tex/`） | 宝石（按装备槽位） | [mod宝石.md](mod宝石.md) |
| **敌人** | `mod/enemies/` | `.json` + `.bin`（+`tex/`） | 敌人池 | [mod敌人.md](mod敌人.md) |
| **子弹** | `mod/bullets/` | `.json` + `.bin` | 不进任何池，只提供逻辑 | [mod子弹.md](mod子弹.md) |
| **特效** | `mod/effects/` | `.json` + `.bin` | 同上 | [mod特效.md](mod特效.md) |
| **时装** | `mod/attires/` | 只要 `.json` | 角色 / 卡片时装（纯外观） | [mod时装.md](mod时装.md) |
| **地图** | `mod/maps/<地图id>/` | `world.json` + 每关 `json`/`bin` | 实验室大地图（关卡脚本用 LabMapCompiler 编译） | [mod地图.md](mod地图.md) |

三条硬规则：

1. **文件名（去掉扩展名）就是 id**：`<id>.json` / `<id>.bin` / `<id>.txt` 是一组
2. **子弹 / 特效的 json 可以只是 `{}`**，但**必须有 json** —— 扫描入口是 json，没有它就找不到同名 bin
3. **时装只有 json**，没有逻辑块

**可以在类别目录下再建一层子目录分类收纳**（mod 多了的时候好用）：

```
mod/cards/snake/shegengbao.json      ← 卡片按系列分：cards/god/、cards/zodiac/、cards/snake/ …
mod/cards/snake/shegengbao.bin
mod/cards/snake/tex/xxx.png          ← 贴图跟着【json 所在目录】走，不是跟着 cards/ 根目录
```

- 每个类别（cards / weapons / gems / enemies / bullets / effects / attires / maps）都支持，**只扫一层、不递归**（`mod/cards/a/b/x.json` 扫不到）
- `tex/` 认的是**同一层**，所以放进子目录就得建 `<子目录>/tex/`
- 子目录名随便取（中文也行），**不影响 id**（id 只看 json 文件名）
- 平铺和子目录混用没问题：`mod/cards/a.json` 和 `mod/cards/snake/b.json` 会一起加载

---

## 二、大概怎么做（五步）

1. **建文件**：`mod/<类别>/<id>.json`、`<id>.txt`，要自带贴图再加 `tex/`
2. **写 txt**（VM 脚本，只用 VM 语言 + `help.md` 里的 `VM_*` 函数）→ **编译出 `<id>.bin`**
   - 编辑器：`Ctrl+B` / `F5`；命令行：`LabMapCompiler.exe <id>.txt`
   - `.txt` 只是源码，游戏只读 `.bin`
3. **写 json**：字段见对应分文件
4. **贴图**：内置名直接用，自带 PNG 放 `tex/`（见下）
5. **生效方式**（下面这些命令都在游戏里的控制台敲：**按 `F1` 开关**）：
   - 改**已有文件** → 控制台 `[reloadmod]`（只重载载入的 bin，不动注册表）
   - 只改某一个 mod → `[reloadmod <数字id | id | 名称>]`（数字id 就是 `[listmod]` 里那个编号；重读那个 json/bin，数值重新注册；**贴图换了还是要用全量 `reloadmod`**，因为贴图别名是所有 mod 共用一批登记的）
   - **新增文件** → `[reloadallmod]`（重扫所有目录，只注册新增的；不用重开游戏了）
   - 看现在加载了什么 → `[listmod]`（每行：`[数字id] [类别] id  名称  路径  简介`，简介取 json 的 `description`，没有就取 `shop.description`）
   - **重启游戏** → 一定生效（最省心的一招）：卡池 / 商店 / 图鉴这类**注册过的东西**改了，热重载不一定跟得上，重启准没错

建议起手方式：从 `datafiles/mod/dev_test/` 里抄一份最接近的样例改，
或者在编辑器里新建文件（模板里事件块都摆好了，删掉行首注释即启用）。

---

## 三、通用约定

### 块名与执行时机

| 块 | 什么时候跑 |
|---|---|
| `_OBJECT_CFG` | `.bin` **加载时**执行一次（不在任何实例上，只适合预加载贴图/常量），执行完就删掉 |
| `_OBJECT_CREATE` | 实例创建时 |
| `_OBJECT_STEP` | 每帧（会被 `mod_step_enter_condition` 挡住，见各对象文档） |
| `_OBJECT_DRAW` | 绘制时（不写就自动 `draw_self()`） |
| `_OBJECT_DESTROY` | 实例销毁时 |
| `_OBJECT_MOUSE_ENTER` | 鼠标移入本实例 |
| `_OBJECT_MOUSE_LEAVE` | 鼠标移出本实例 |
| `_OBJECT_CLICK` | 鼠标左键在本实例上按下 |

各对象实际支持：

```text
卡片     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
武器     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
子弹     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
特效     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
敌人     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
宝石     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
```

- 鼠标三个块与 `_OBJECT_DESTROY` **6 个 mod 对象全都有**（武器 / 宝石的 Destroy 事件是后补的，现在写了会执行）
- 鼠标三个块是**按实例**判定的 —— 鼠标压在**这个实例的贴图（碰撞掩码）**上才算数，
  所以**没设 `sprite_index` 的实例收不到**（子弹/特效尤其注意）
- 想判"全场任意位置按了左键"用 `_VM_MOUSE_LEFT` 事件块（地图脚本），不要用 `_OBJECT_CLICK`
- 这三个块执行完，**核心自己的鼠标处理照常进行**（宝石这类核心也处理鼠标的对象不会被打断）
- 写块之前先确认对应对象支持它 —— 写了没人执行的块等于没写
- 取当前实例：`self = VM_GetCurCard()`（名字叫 Card，实际指"当前对象实例"）
- 碰撞没有通用块（不存在 `_OBJECT_HIT`），要在 `_OBJECT_STEP` 里自己判

### 全局挂载点（`_VM_*`）

`_VM_*` 是游戏在关键时机广播的**全局挂载点**，和上面那些对象自己的 `_OBJECT_*` 块是两回事。
mod 插件和地图脚本**都能挂**：事件类（`_VM_BATTLE_START` / `_VM_CARD_CREATED` / `_VM_MOUSE_LEFT` …）、
`_VM_FRAME`（每帧）、5f / 10f / 15f / 30f / 60f 定时器。

- **只有"场上有它的实例"时才跑**：卡片 / 武器 / 宝石 / 敌人 / 子弹 / 特效一样，该类单位一个实例都没有时挂载点不触发
  （`_OBJECT_CREATE` 都还没跑的新局、准备界面同理）。热重载换 bin、旧实例销毁都会自动跟上。
- **卡片类事件不区分是谁的卡**：`_VM_CARD_CREATED` / `_VM_CARD_DESTROYED` 对关卡自摆的卡、玩家角色也照样广播，
  想只处理自己的卡就在块里用 `VM_GetLastCreatedCard()` / `VM_GetLastDestroyedCard()` + `plant_id` 自己筛。
- **高频点慎用**：`_VM_FRAME` / 5f / 10f / 15f 是全场级别的开销，每帧要做的事一般写在 `_OBJECT_STEP` 里更省
  （那里有 `mod_step_enter_condition` 闸门，没轮到的实例不进 VM）。
- 想让某个 mod **不受"有没有实例"这层过滤**（调试用），见 `help.md` 文末「附：`__hookall__`」——**特殊用法，非必要不要用**。

### 实例字段（内置字段，读写都用 VM_GetProp / VM_SetProp）

```gml
x = VM_GetProp(self, "x")          // 读
VM_SetProp(self, "x", x + 40)      // 写（浮点直接写，如 1.8）
```

**位置 / 尺寸**

| 字段 | 说明 |
|---|---|
| `x` / `y` | 实例坐标（像素）。**武器每帧跟玩家、宝石停在创建点**——这些是核心写的，想偏移就在 STEP 里自己加（`VM_SetProp(self,"y",VM_GetProp(self,"y")+50)`） |
| `depth` | 深度，越小越靠前（武器默认跟随玩家 -1） |
| `grid_col` / `grid_row` | 所在格子；卡/武器/宝石由核心写，子弹还有 `prev_grid_col` / `prev_grid_row` |
| `sprite_width` / `sprite_height` | 贴图尺寸，**已经乘过 image_xscale/yscale**，别再乘一次 |

**外观**

| 字段 | 说明 |
|---|---|
| `sprite_index` | 贴图。可以直接写**字符串名**（内置名，或 `VM_AliasSpritePerm` 挂的名字）。⚠️ 它永远"已定义"（没设时是 `-1`），判断"别人给没给贴图"要用 `== -1`，不能用 undefined |
| `image_index` | 当前帧（0 起）。核心动画会自动推，想自己控制就直接写（VM 在核心动画之后跑，当帧生效） |
| `image_speed` | **固定 0，别碰**——核心自己推 `image_index`，改了只会互相抢 |
| `image_xscale` / `image_yscale` | 缩放。武器默认 1.6、宝石默认 1（`obj_gem_mod` 里常设 1.8）、子弹按各自对象默认 |
| `image_angle` | 旋转角度（度）。子弹/光环自转就靠它，如 `image_angle = 0 - timer * 6` |
| `image_alpha` | 透明度 0~1 |
| `visible` | 是否绘制（`false` = 不画但还在跑逻辑） |

**动画配置**（核心读这几个来推 `image_index`，各类文档里有详表）

| 字段 | 说明 |
|---|---|
| `timer` / `flash_speed` | 每帧计数 / 每格停留几帧 |
| `idle_anim` / `attack_anim` | 待机帧范围、攻击帧范围（`idle_anim` = 待机最后一帧的下标） |
| `state` | `0` = 待机、`1` = 攻击（走 `attack_anim` 那段） |

**这些字段是核心在维护的，别硬覆盖**（想改就在对应文档里找"允许改"的写法）：

- 武器 / 副武器：`x` / `y` / `depth` / `grid_*`（每帧跟玩家）
- 卡 / 敌人：`hp` / `maxhp`（敌人还含 `shield_hp` / `move_speed` / `atk_cycle`…，来自注册表或服务器）
- 宝石：`cooldown_timer` 每帧自减（插件触发后自己重写，见 mod宝石.md）
- 子弹：`x += vx`、`y += vy` 是**核心在 VM 之后**做的，所以它们在 VM 里读到的是"这一帧移动前"的位置

### 贴图

**方式 A：用内置精灵名**（数量最多、最省事）

```json
"sprite": "spr_small_fire"
```

**方式 B：自带 PNG**（放 `mod/<类别>/tex/`，在 `_OBJECT_CFG` 里加载并挂别名）

```gml
_OBJECT_CFG {
    if (VM_SpriteExists("spr_my_card") == 0) {
        VM_LoadSpritePerm_Ex("tex/spr_my_card_0.png", 1, 40, 40)
        VM_AliasSpritePerm("spr_my_card", "tex/spr_my_card_0.png")
    }
}
```

- 多帧就写帧数（`VM_LoadSpritePerm_Ex(文件, 帧数, 原点X, 原点Y)`），图按横向均分
- 原点是图里的像素坐标，居中就是 `宽/2, 高/2`
- 文件命名习惯 `<精灵名>_0.png`（和内置一致）
- 内置优先 + 外置覆盖：先 `VM_SpriteExists` 判断，内置有就不覆盖

### 商店与解锁（卡片 / 武器 / 宝石 / 时装通用）

```json
"shop": { "cost": "50000", "description": "商店里显示的文字" }
```

写了才进商店；`cost` 是字符串。

### 数值档位

卡片数值是 **17 档数组**（从最低等级往上排），写不满就用最后一个补齐。

### 倒计时与淡出（**所有 mod 对象通用**：卡片 / 武器 / 宝石 / 敌人 / 子弹 / 特效）

每个 mod 实例都带这两个属性，核心每帧帮你推，**不用自己写 STEP**：

| 属性 | 作用 |
|---|---|
| `mod_countdown` | 倒计时（帧）。`> 0` 时每帧 `-1` |
| `mod_alpha_add` | 每帧加到 `image_alpha` 上的变化量（负数 = 渐隐，正数 = 渐显） |

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()          // 其它对象里是当前实例；子弹/特效写 `self = ...` 同理
    VM_SetProp(self, "mod_countdown", 30)          // 30 帧后走完
    VM_SetProp(self, "mod_alpha_add", -1 / 30.0)   // 每帧变化量 → 30 帧正好淡到 0
}
```

- 判定顺序：**`mod_countdown > 0`** 时才 `-1` 并把变化量加到 `image_alpha`（两个一起才生效，默认都是 0 = 不动）
- ⚠️ **两边都是整数时 `/` 走整除**（同 C 的 `int /`，向零取整）：写成 `-1 / 30` 结果是 `0`，等于不淡。要小数就写 `-1 / 30.0` 或 `-0.0333`
- 变化量就是**每帧**的量，不用除 60；30 帧 = 0.5 秒
- 走完（倒计时到 0）就不再动透明度；要在淡完时销毁，就在 `_OBJECT_STEP` 里判 `mod_countdown == 0` 自己 `VM_DestroyInstance(self)`
- 游戏暂停时不推进；卡片被冻住（`is_frozen`）时也不推进
- 中途想改方向/停手：直接改这两个属性的当前值就行

---

## 四、常见坑速查

| 现象 | 原因 / 处理 |
|---|---|
| 新加的 mod 在游戏里找不到 | 控制台 `[reloadallmod]` 重扫注册；`[reloadmod]` 只重载**已有**文件 |
| 改了某个 mod 想单独生效 | `[reloadmod <数字id>]`（数字id 见 `[listmod]`）；改了贴图仍要用全量 `[reloadmod]` |
| 不知道现在加载了哪些 mod | `[listmod]` 列出来（数字id / 类别 / id / 名称 / 路径 / 简介） |
| 卡片/武器形象卡在第 0 帧不动 | 没喂 `idle_anim` / `attack_anim` / `flash_speed` / `state` |
| 设了 `x` / `y` 但位置没变 | 武器每帧跟玩家、宝石停在创建点——核心会覆盖，要在 `_OBJECT_STEP` 里自己加偏移（见「实例字段」一节） |
| 改了 `image_speed` 动画反而乱 | 核心固定 `image_speed = 0` 自己推 `image_index`；要自己控制就直接写 `image_index` |
| 遮罩/判定框比贴图大一截 | `sprite_width` / `sprite_height` **已含缩放**，再乘一次 `image_xscale` 就放大了 |
| 子弹/实例一动不动 | `vx` / `vy` 默认就是 0，**必须显式设** |
| 脚本跑到一半就不执行了 | 读了没设过的属性会报错并中断**整个块**；要用先在 CREATE 里设一份 |
| 两个卡加的同一个全局计数串了 | 命名数组是**整个 mod 单元共享**的，不是实例级；同名注册还会互相覆盖 |
| 发完 A 的 mod 子弹后，B 的子弹行为/贴图变成了 A 的（例：双刃蛇打完，雷神/哈迪斯镰刀的子弹跑成刀刃弹） | `_mod_pending_bullet_id` 是**全局**且创建后没人清。B 若"创建后才写 `mod_type`"，Create 里已经按脏值初始化并把 `_mod_initialized` 置成 true，之后再改 `mod_type` 无效。改法：创建【前】用全局指定类型、创建后清空（见 `mod子弹.md`「一、怎么创建」的方案 A） |
| 想直接 `VM_CreateInstance("my_bullet", x, y)` 创建 mod 子弹 | 不支持：该命令只认游戏对象名（会去找 `obj_my_bullet`，找不到返回 −1）。必须创建 `obj_bullet_mod` 再指定类型 |
| `[reloadmod]` 之后数值全变 0：宝石每帧狂刷火苗、撕裂/神罚打不出伤害、护盾没减伤、联动倍率固定不动 | 重载会**清空该 VM 的全部命名数组**，但**不重跑 `_OBJECT_CREATE`**。所以数值表要建在 `_OBJECT_CFG`（每次加载/重载都跑），或读之前用 `VM_ArraySize` 判空重建；「场上同类有几个」这类活计数器别用命名数组记，改成现场 `VM_GetInstancesInRange`（详见 `mod卡片.md`「五、其它要点」的重载对照表） |
| 卡升级/吃到技能后**完全没反应**（一发不发） | 开火窗口表写成了**绝对帧号**，而 `skill.attr = "cycle"` 会把 `cycle` 改小（例：雷神 150→108）→ 窗口落在 ticker 范围外**永不触发**。窗口一律写「从一轮末尾倒数」的负数（`-30`），或用 `mod_tick_max` 固定周期 |
| 攻击动画闪一下就回待机 / 播不完 / 缺帧 | 帧数要求是 `idle_anim + attack_anim + 2 ≤ 精灵总帧数`（待机播 `0..idle_anim`，攻击从 `idle_anim+1` 起跳、还会多显示一帧） |
| `VM_PlaySound` 没声音、也不报错 | 音效名必须是内置表里的 `snd_*`（见 `help.md`「内置音效」）。写不存在的名字是**静音**，不会报错、日志也没有提示 |
| 看到 `VM_GetProp(0, "grid_rows")` 以为是写错了 | 这是**合法**写法：`inst_id == 0` 时走 `variable_global_get`，读的就是全局变量（写全局同理用 `VM_SetProp(0, ...)`） |
| 「落到已有卡上再吃/换那张卡」的卡，进 VM 后**只查到自己**（`VM_GetInstancesInRange(..., "card", ...)` 返回 1） | JSON 的 `plant_type` 写成了 `normal`：开着替换放置时，游戏在**建你这张卡之前**就把同 `plant_type` 的卡销毁、并从格子里移走了——目标早没了。改成 `coffee`（占位格允许落、替换不会动 coffee 层）。**种植/替换判定只看 JSON**，`_OBJECT_CREATE` 里的 `VM_SetProp(self,"plant_type",...)` 只改实例层级，改不了这个（详见 `mod卡片.md`「一、JSON 字段」） |
| `mod_step_enter_condition` 设成 `"wait"` 后卡反而每帧都在跑 | `"wait"` 减到 0 放你进 VM 后停在 0，卡里**必须马上设回正数**；不设引擎会把条件清成 `""`，之后就再也回不到 wait 了 |
| 贴图空白但不报错 | `get_load_sprite` 找不到会给占位图；用 `VM_SpriteExists` 判断，自带图记得在 `_OBJECT_CFG` 预加载 |
| 每次进房间临时贴图没了 | 临时缓存（`VM_LoadSprite`）进房间会清；常驻用 `VM_LoadSpritePerm_Ex` |
| `VM_CallFunc` 那批工具函数叫不动 | 走字典的动态调用**较慢**，且只适合低频；先 `VM_FuncExists("名字")` 判断 |
| 某些 mod 函数叫不动 | 确认**目标构建**里有没有这个函数（编辑器帮助 / `help.md` 能查），主线与 mod 分支的函数表不一定一样 |
| 存档里的旧 mod 变成透明卡 / 空位 | 卸载或改名后，存档里还留着那个 id；游戏会把它当"未注册"处理（摘出来不写坏档） |
| 掉帧 | 瓶颈是**进 VM 解释器的次数**：别在 `_VM_FRAME` / 每帧每实例里写重逻辑；大量同质子弹改用屏幕弹幕管理器（`VM_BulletScreenAdd_Ex`），少用 `VM_CallFunc` |

遇到"原版是怎么做的"这类问题，可以直接翻 `datafiles/mod/dev_test/` 里的样例照着改。
