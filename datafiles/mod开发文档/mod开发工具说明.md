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

**配套工具**：编辑器（`MapEditCreator.exe`）内置了 `help.md`（点「帮助」看语法与函数表），
带语法检查、函数补全与悬停说明、代码折叠、一键编译（`Ctrl+B` / `F5`）；
编译器 `LabMapCompiler.exe` 也可以单独命令行用（`LabMapCompiler.exe 你的脚本.txt`）。
写脚本时的分工：**语言和函数查 help.md / 编辑器帮助，各类 mod 怎么组查这几份文档**。
旁边的 `.html` 是转出来的（仓库里 `node tools/md2html.js`，用编辑器前端的 marked 渲染，不联网）——
**改了 md 记得重跑一次**，html 要和 md 一起提交（yyp 里两种都登记了）。
卡片 / 武器 / 宝石 / 敌人 / 子弹 / 特效这几份都按同一格式写：**文件结构 → JSON 字段 →
实例上的内置字段 → 块执行时机与顺序 → 可直接抄的 demo → 要点**。

源文件都在 `FVM-Reborn/datafiles/mod/`；构建时整个 `datafiles/` 会覆盖到游戏运行目录，
所以游戏实际读的是 `<游戏目录>/mod/`（两棵树内容应保持一致）。

---

## 一、能做哪几种 Mod

| 类别 | 目录 | 需要的文件 | 注册成什么 | 逻辑对象 | 分文件 |
|---|---|---|---|---|---|
| **卡片** | `mod/cards/` | `.json` + `.bin`（+`tex/`） | 卡池 + 植物数据 | `obj_card_mod` | [mod卡片.md](mod卡片.md) |
| **武器** | `mod/weapons/` | `.json` + `.bin`（+`tex/`） | 主武器 / 超级武器 / 副武器 | 主·超武用 `obj_weapon_mod`；副武是 `obj_player_shield` + 额外一个 `obj_weapon_mod` | [mod武器.md](mod武器.md) |
| **宝石** | `mod/gems/` | `.json` + `.bin`（+`tex/`） | 宝石（按装备槽位） | `obj_gem_mod`（`passive` 为真则没有实体） | [mod宝石.md](mod宝石.md) |
| **敌人** | `mod/enemies/` | `.json` + `.bin`（+`tex/`） | 敌人池 | `obj_enemy_mod` | [mod敌人.md](mod敌人.md) |
| **子弹** | `mod/bullets/` | `.json` + `.bin` | 不进任何池，只提供逻辑 | `obj_bullet_mod` | [mod子弹.md](mod子弹.md) |
| **特效** | `mod/effects/` | `.json` + `.bin` | 同上 | `obj_effect_mod` | [mod特效.md](mod特效.md) |
| **时装** | `mod/attires/` | 只要 `.json` | 角色 / 卡片时装（纯外观） | 无 | [mod时装.md](mod时装.md) |
| **地图** | `mod/maps/<地图id>/` | `world.json` + 每关 `json`/`bin` | 实验室大地图 | 无（关卡脚本用 LabMapCompiler） | [mod地图.md](mod地图.md) |

三条硬规则：

1. **文件名（去掉扩展名）就是 id**：`<id>.json` / `<id>.bin` / `<id>.txt` 是一组
2. **子弹 / 特效的 json 可以只是 `{}`**，但**必须有 json** —— 扫描入口是 json，没有它就找不到同名 bin
3. **时装只有 json**，没有逻辑块

---

## 二、大概怎么做（五步）

1. **建文件**：`mod/<类别>/<id>.json`、`<id>.txt`，要自带贴图再加 `tex/`
2. **写 txt**（VM 脚本，只用 VM 语言 + `help.md` 里的 `VM_*` 函数）→ **编译出 `<id>.bin`**
   - 编辑器：`Ctrl+B` / `F5`；命令行：`LabMapCompiler.exe <id>.txt`
   - `.txt` 只是源码，游戏只读 `.bin`
3. **写 json**：字段见对应分文件
4. **贴图**：内置名直接用，自带 PNG 放 `tex/`（见下）
5. **生效方式**：
   - 改**已有文件** → 控制台 `[reloadmod]`（只重载载入的 bin，不动注册表）
   - 只改某一个 mod → `[reloadmod <数字id | id | 名称>]`（数字id 就是 `[listmod]` 里那个编号；重读那个 json/bin，数值重新注册；**贴图换了还是要用全量 `reloadmod`**，因为贴图别名是所有 mod 共用一批登记的）
   - **新增文件** → `[reloadallmod]`（重扫所有目录，只注册新增的；不用重开游戏了）
   - 看现在加载了什么 → `[listmod]`（每行：`[数字id] [类别] id  名称  路径  简介`，简介取 json 的 `description`，没有就取 `shop.description`）
   - 改**核心 GML** → 要 GM 重编译（mod 层改动不需要）

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
obj_card_mod     CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
obj_weapon_mod   CREATE / STEP / DRAW           / 移入 / 移出 / 点击
obj_bullet_mod   CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
obj_effect_mod   CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
obj_enemy_mod    CREATE / STEP / DRAW / DESTROY / 移入 / 移出 / 点击
obj_gem_mod      CREATE / STEP / DRAW           / 移入 / 移出 / 点击
```

- 鼠标三个块 **6 个 mod 对象全都有**；`_OBJECT_DESTROY` 只有武器和宝石没有（写了不会执行）
- 鼠标三个块是**按实例**判定的 —— 走 GameMaker 的实例鼠标事件，鼠标压在**这个实例的贴图（碰撞掩码）**上才算，
  所以**没设 `sprite_index` 的实例收不到**（子弹/特效尤其注意）
- 想判"全场任意位置按了左键"用 `_VM_MOUSE_LEFT` 事件块（地图脚本），不要用 `_OBJECT_CLICK`
- 这三个块执行完都会 `event_inherited()`，所以宝石那种核心自己也处理鼠标的对象，核心逻辑照常跑

- 写块之前先确认对应对象支持它 —— 写了没人执行的块等于没写
- 取当前实例：`self = VM_GetCurCard()`（名字叫 Card，实际指"当前对象实例"）
- 碰撞没有通用块（不存在 `_OBJECT_HIT`），要在 `_OBJECT_STEP` 里自己判

### 实例字段（GML 内置，读写都用 VM_GetProp / VM_SetProp）

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
| 贴图空白但不报错 | `get_load_sprite` 找不到会给占位图；用 `VM_SpriteExists` 判断，自带图记得在 `_OBJECT_CFG` 预加载 |
| 每次进房间临时贴图没了 | 临时缓存（`VM_LoadSprite`）进房间会清；常驻用 `VM_LoadSpritePerm_Ex` |
| `VM_CallFunc` 那批工具函数叫不动 | 走字典的动态调用**较慢**，且只适合低频；先 `VM_FuncExists("名字")` 判断 |
| 某些 mod 函数叫不动 | 确认**目标构建**里有没有这个函数（编辑器帮助 / `help.md` 能查），主线与 mod 分支的函数表不一定一样 |
| 存档里的旧 mod 变成透明卡 / 空位 | 卸载或改名后，存档里还留着那个 id；游戏会把它当"未注册"处理（摘出来不写坏档） |
| 掉帧 | 瓶颈是**进 VM 解释器的次数**：别在 `_VM_FRAME` / 每帧每实例里写重逻辑；大量同质子弹改用屏幕弹幕管理器（`VM_BulletScreenAdd_Ex`），少用 `VM_CallFunc` |

遇到"原版是怎么做的"这类问题，可以直接翻 `datafiles/mod/dev_test/` 里的样例，
或对照游戏源码里对应的 `obj_*` 对象事件。
