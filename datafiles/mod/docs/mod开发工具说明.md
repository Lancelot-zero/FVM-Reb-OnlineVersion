# Mod 开发工具说明

写给"做 mod 的人"的总说明。**先看本文**（能做哪些、通用流程与约定），再按类别看对应的分文件。

| 文档 | 内容 |
|---|---|
| **本文** | 能做哪几种、通用流程、块名与贴图约定、常见坑 |
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
   - 改**已有文件** → 控制台 `[reloadmod]`
   - **新增文件** → **必须重开游戏**（各 `src_mod_*_init` 只在 `obj_game_init` 里跑一次）
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
| `_OBJECT_STEP` | 每帧 |
| `_OBJECT_DRAW` | 绘制时（不写就自动 `draw_self()`） |
| `_OBJECT_DESTROY` | 实例销毁时 |

各对象实际支持：

```text
obj_card_mod     CREATE / STEP / DRAW / DESTROY
obj_weapon_mod   CREATE / STEP / DRAW
obj_bullet_mod   CREATE / STEP / DRAW / DESTROY
obj_effect_mod   CREATE / STEP / DRAW / DESTROY
obj_enemy_mod    CREATE / STEP / DRAW / DESTROY
obj_gem_mod      CREATE / STEP / DRAW（另外还有鼠标事件，见 mod宝石.md）
```

- 写块之前先确认对应对象支持它 —— 写了没人执行的块等于没写
- 取当前实例：`self = VM_GetCurCard()`（名字叫 Card，实际指"当前对象实例"）
- 碰撞没有通用块（不存在 `_OBJECT_HIT`），要在 `_OBJECT_STEP` 里自己判

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
| 新加的 mod 在游戏里找不到 | 新增文件要**重开游戏**（`[reloadmod]` 只重载已有文件） |
| 卡片/武器形象卡在第 0 帧不动 | 没喂 `idle_anim` / `attack_anim` / `flash_speed` / `state` |
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
