# 武器 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/weapons/my_weapon.json / .bin / .txt / tex/
```

---

## 一、JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `sprite` | ✅ | 本体贴图名 |
| `icon` | | 装备图标（默认用 `sprite`） |
| `slot` | | `main_weapon`（默认）/ `super_weapon` / `secondary_weapon` |
| `atk` / `cycle` | | 攻击力 / 攻击周期（帧）；默认 0 / 60，**会挂到实例上**（见下节） |
| `name` / `description` | | 名称 / 说明（商店、背包显示） |
| `atk_impact` | | 可选，**攻击宝石**加成表：装备了 `attack_gem` 时 `atk` 改读这张表（按宝石等级取） |
| `cycle_impact` | | 可选，**攻速宝石**用的周期表（同样按宝石等级取） |
| `hp_increase` | | 可选，加血（副武器用；由 `obj_player_shield` 读，**不进 mod VM**） |
| `shop` | | 商店 |

> ⚠️ 除上面这些，**JSON 里的自定义字段不会挂到实例上**，要在 `_OBJECT_CREATE` 里自己 `VM_SetProp` 设一份
> （`hades_scythe` 的 `bullet_amount` / `ghost_shape` 就是这么处理的）。

例子：

```json
{
  "name": "我的武器",
  "description": "说明",
  "sprite": "spr_my_weapon",
  "icon": "spr_my_weapon_icon",
  "slot": "main_weapon",
  "atk": 230,
  "cycle": 300,
  "shop": { "cost": "150000", "description": "商店里显示的文字" }
}
```

---

## 二、三种槽位

| | 主武器 / 超级武器 | 副武器（盾牌） |
|---|---|---|
| 注册对象 | `obj_weapon_mod` | `obj_player_shield` **+** 额外一个 `obj_weapon_mod` |
| 谁创建实例 | 种卡时随卡片创建（放置逻辑） | 角色创建时（`obj_player_character/Mouse_53.gml`） |
| `.bin` 的 `CREATE/STEP/DRAW` | 会执行 | **会执行**（跑在额外那个 `obj_weapon_mod` 上） |
| 加血 `hp_increase` | — | 由 `obj_player_shield` 读，mod 管不到 |
| 内置盾（cookie / oreo / cut_cake） | — | **只建 `obj_player_shield`，不挂 mod 实例**（所以它们的 `.bin` 不会跑） |
| 超级武器 | 会跟随玩家（同主武器） | — |

副武器是**两个实例**：`obj_player_shield` 本体 `image_alpha = 0`、自己不画东西，只负责加血和原版 5 个盾宝石；
形象和自定义逻辑走额外那个 `obj_weapon_mod`（它读 `global._mod_pending_weapon_id` 拿副武器 id，
把 `sprite_index` 设成 `weapon_info.sprite`）。**只有注册过的 mod 盾才会挂这个实例。**

---

## 三、武器实例上的内置字段

`obj_weapon_mod` 的 Create 建好下面这些（`weapon_info` 来自 JSON 注册数据）：

| 字段 | 初值 | 说明 |
|---|---|---|
| `weapon_id` | 全局 `_mod_pending_weapon_id` | 武器 id（= 文件名） |
| `weapon_info` | `get_weapon_info(weapon_id)` | 注册数据（`atk` / `cycle` / `sprite` / `atk_impact` …） |
| `atk` | `weapon_info.atk` | 攻击力；**装备了 `attack_gem` 时改读 `atk_impact[宝石等级]`** |
| `cycle` | `weapon_info.cycle`（默认 60） | 攻击周期（帧） |
| `sprite_index` | `weapon_info.sprite` | 本体贴图 |
| `image_xscale` / `image_yscale` | `1.6` | 缩放（默认就比原版大一点） |
| `image_speed` | `0` | 动画**不由 image_speed 驱动**，见下 |
| `parent_player` | `noone` | 放置它的玩家（放置逻辑设的；副武器也设） |
| `grid_col` / `grid_row` | `0` | 每帧由 Step 从玩家同步 |
| `timer` / `flash_speed` | `0` / `5` | **核心动画用**（不要拿来当自己的计时器） |
| `idle_anim` / `attack_anim` | `0` / `0` | 待机帧数 / 攻击帧数 |
| `state` | `CARD_STATE.IDLE` | `0`=待机 `1`=攻击 |

现成可以读的还有 `x` / `y` / `depth`（每帧被核心写，见下）。

> ⚠️ **武器没有卡片那套 `mod_tick_*` / `mod_has_enemy` / `mod_step_enter_*` / `mod_set_*` 设施**
> —— 索敌、开火节奏、进 VM 时机全部要自己在 `_OBJECT_STEP` 里写（用自己设的属性当计时器，例如 `cd` / `atk_timer`）。

---

## 四、STEP 的时机与执行顺序

### 什么时候跑

- **`_OBJECT_CREATE`**：实例创建时立刻执行（主/超武在种卡的那一刻；副武器在角色创建时）
- **`_OBJECT_STEP`**：**每帧都跑**（没有卡那种"进 VM 条件"），只要没暂停
- **`_OBJECT_DRAW`**：写了就跑 VM 的绘制块（**不再自动 `draw_self()`**），没写就自动画 `sprite_index`
- **`_OBJECT_DESTROY`**：武器对象**不支持**（写了不会执行）

### 每帧顺序（`obj_weapon_mod/Step_0.gml`）

```text
1. 暂停 → 整个 exit
2. 跟随玩家（parent_player 存在时）：
      depth  = parent_player.depth - 1
      x      = parent_player.x - 10
      y      = parent_player.y - 100
      grid_row / grid_col = 玩家那格
3. 动画自动播放：timer/flash_speed 驱动 image_index，
      state=0 循环 0..idle_anim，state=1 循环 idle_anim+1 .. idle_anim+attack_anim
4. 跑武器 .bin 的 _OBJECT_STEP
```

两个由此而来的"特别用法"：

- **位置每帧被核心钉在玩家头上**（`x = 玩家.x - 10`、`y = 玩家.y - 100`）。
  想要别的站位/钉在固定位置，就在自己的 `_OBJECT_STEP` 里**每帧覆盖**（放在自己逻辑的开头）：

```gml
// 例子：冥王战镰——固定在 x=500，高度用自己的 lock_y，深度压到最上层
VM_SetProp(self, "x", 500)
VM_SetProp(self, "y", VM_GetProp(self, "lock_y"))
VM_SetProp(self, "depth", 0 - 500)
```

```gml
// 例子：主宰之盾——核心把 y 钉在 玩家.y-100，盾牌想再往下挪 100，就每帧补回来
VM_SetProp(self, "y", VM_GetProp(self, "y") + 100)
```

- **`timer` 是核心动画的**（每帧自增、到 `flash_speed` 清零），当自己的计时器会被打乱 —— 用别的属性。

---

## 五、demo

### Demo 1：同行有敌人才开火（原版子弹实例）

最简单的一类：索敌自己判、节奏用 `cycle`、子弹用**原版弹对象**（不用自己写子弹逻辑）。

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_long_bao_gun")
    VM_SetProp(self, "idle_anim", 7)       // 待机 0..7
    VM_SetProp(self, "attack_anim", 6)     // 攻击 8..14
    VM_SetProp(self, "flash_speed", 6)
    VM_SetProp(self, "cd", 0)              // 自己的计时器
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    r = VM_GetProp(self, "grid_row")
    c = VM_GetProp(self, "grid_col")
    // 本行 c..最右列有没有敌人
    if (VM_EnemyInRange(r, r, c, 15, "") == 1) {
        cd = VM_GetProp(self, "cd") + 1
        if (cd >= VM_GetProp(self, "cycle")) {
            cd = 0
            b = VM_CreateInstance("xiaolongbao_bullet", VM_GetProp(self, "x") + 40, VM_GetProp(self, "y") - 75)
            VM_SetProp(b, "damage", VM_GetProp(self, "atk"))
            VM_SetProp(b, "move_speed", 8)
            VM_SetProp(b, "row", r)
            VM_PlaySound("snd_shot")
        }
        VM_SetProp(self, "cd", cd)
    } else {
        VM_SetProp(self, "cd", 0)          // 没敌人 → 节奏重数
    }
}
```

要"三行连发"就多建两颗、`row` 改成 `r-1` / `r+1`（参考 `dev_test/weapons/triple_gun.txt`）。

### Demo 2：mod 子弹 + 完整开火节奏

用 `obj_bullet_mod`（子弹逻辑写在 `mod/bullets/` 里，适合抛物线 / 追踪 / 特殊命中）：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_my_weapon")
    VM_SetProp(self, "idle_anim", 20)
    VM_SetProp(self, "attack_anim", 16)
    VM_SetProp(self, "flash_speed", 5)
    // JSON 自定义字段不上实例 → 自己设一份
    VM_SetProp(self, "shot_count", 5)
    VM_SetProp(self, "atk_timer", 0)
    VM_SetProp(self, "fired", 0)
    VM_SetProp(self, "cooldown", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    ak = VM_GetProp(self, "atk")
    n = VM_GetProp(self, "shot_count")

    if (VM_GetProp(self, "state") == 0) {
        // ---- 待机：冷却中就走冷却；冷却好了且场上有敌人就开打 ----
        if (VM_GetProp(self, "cooldown") > 0) {
            VM_SetProp(self, "cooldown", VM_GetProp(self, "cooldown") - 1)
        } elif (VM_GetEnemyCount() > 0) {
            VM_SetProp(self, "state", 1)
            VM_SetProp(self, "atk_timer", 0)
            VM_SetProp(self, "fired", 0)
        }
    } else {
        // ---- 攻击：第 32 帧首发，之后每 10 帧一发 ----
        at = VM_GetProp(self, "atk_timer") + 1
        VM_SetProp(self, "atk_timer", at)
        fidx = VM_GetProp(self, "fired")
        if (fidx < n) {
            if (at >= 32 + fidx * 10) {
                b = VM_CreateInstance("obj_bullet_mod", VM_GetProp(self, "x") + 60, VM_GetProp(self, "y"))
                VM_SetProp(b, "mod_type", "my_bullet")
                VM_SetProp(b, "damage", ak)
                VM_PlaySound("snd_shot")
                VM_SetProp(self, "fired", fidx + 1)
            }
        }
        // 收招：attack_anim(16) x flash_speed(5) = 80 帧
        if (at >= 80) {
            VM_SetProp(self, "state", 0)
            VM_SetProp(self, "atk_timer", 0)
            VM_SetProp(self, "cooldown", VM_GetProp(self, "cycle"))
        }
    }
}
```

要点：
- **节奏全靠自己的属性**（`atk_timer` / `fired` / `cooldown`），武器没有卡片那套自动计时
- `state = 1` 才会播攻击动画（待机/攻击不会自动切）
- 瞬间多发、或者要"不占实例"的弹幕，用 `VM_BulletScreenAdd_Ex` / `VM_HomingBulletAdd`
  （参数与写法见 [mod卡片.md](mod卡片.md) 的 Demo 3）

### Demo 3：副武器（盾牌）

```json
{
  "name": "我的盾",
  "description": "说明",
  "sprite": "spr_my_shield",
  "icon": "spr_my_shield_icon",
  "slot": "secondary_weapon",
  "hp_increase": 800,
  "shop": { "cost": "150000", "description": "说明" }
}
```

```gml
// 贴图必须在注册前挂好（注册时按 JSON 的 sprite/icon 查名字，查不到会给空白图且不报错）
_OBJECT_CFG {
    if (VM_SpriteExists("spr_my_shield") == 0) {
        VM_LoadSpritePerm_Ex("tex/spr_my_shield_0.png", 1, 31, 53)
        VM_AliasSpritePerm("spr_my_shield", "tex/spr_my_shield_0.png")
    }
    if (VM_SpriteExists("spr_my_shield_icon") == 0) {
        VM_LoadSpritePerm_Ex("tex/spr_my_shield_icon_0.png", 1, 35, 35)
        VM_AliasSpritePerm("spr_my_shield_icon", "tex/spr_my_shield_icon_0.png")
    }
}

_OBJECT_CREATE {
    self = VM_GetCurCard()
    // 形象钉死，不依赖 obj_weapon_mod 的默认值
    VM_SetProp(self, "sprite_index", "spr_my_shield")
    VM_SetProp(self, "image_xscale", 1.6)
    VM_SetProp(self, "image_yscale", 1.6)
    VM_SetProp(self, "image_alpha", 1)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    // 核心把 y 钉在 玩家.y-100；盾牌想再往下挪 100 就每帧补
    VM_SetProp(self, "y", VM_GetProp(self, "y") + 100)
    ...需要的话在这里加自己的行为...
}
```

---

## 六、要点

- **动画四属性必须自己喂**，否则形象卡在第 0 帧：
  `idle_anim`（待机帧数）、`attack_anim`（攻击帧数）、`flash_speed`（几帧走一格）、`state`（0/1）；
  `idle_anim + attack_anim` 别超过精灵总帧数
- **动画是核心自动推的，别碰 `image_speed`**：`image_speed` 固定为 0，`image_index` 由核心每 `flash_speed`
  帧往前走一格；改 `image_speed` 只会和核心抢 `image_index`。想完全自己控制，就在 `_OBJECT_STEP` 里直接写
  `image_index`（VM 在核心动画之后跑，当帧覆盖）
- **待机 / 攻击不会自动切**：武器没有卡片那套"命中开火窗口自动切 ATTACK"，`state` 必须自己改
  （开火时设 1、收招时设回 0）。而且**攻击动画是循环的**，不是"播一遍停住" —— 要"播完歇冷却"就自己数帧
  （参考 `hades_scythe`：`at >= 80` 收招）
- **本体贴图**：`sprite_index` 已经由 JSON 的 `sprite` 设好；换形态/换贴图在自己的逻辑里覆盖
- **贴图预加载**：自带 PNG 要在 `_OBJECT_CFG` 里 `VM_LoadSpritePerm_Ex` + `VM_AliasSpritePerm`
  （注册武器时就会去查名字，晚一步就变成空白图）
- **攻击力**：直接用 `atk`（已经带上了攻击宝石的加成）；`cycle` 同理可以读注册值
- 想复刻原版武器，最完整的参考是 `mod/weapons/hades_scythe.txt`（超级武器：贴图别名、
  分阶段切换、完整开火节奏、mod 子弹），简单参考是 `dev_test/weapons/` 里的三把枪
