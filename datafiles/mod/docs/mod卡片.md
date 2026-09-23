# 卡片 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/cards/my_card.json     卡配置（数值、形态、图标）
mod/cards/my_card.bin      逻辑（VM 脚本编译产物）
mod/cards/my_card.txt      源码（建议保留）
mod/cards/tex/...          自带贴图（可选）
```

## 一、JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` | ✅ | 卡名（图鉴 / 商店显示） |
| `shapes` | ✅ | 形态数组，至少 1 个；里面每个形态是一档"外形" |
| `shapes[].sprite` | ✅ | 卡面 / 预览贴图名（走 `get_load_sprite`） |
| `shapes[].shape` | | 形态序号，默认按数组下标 |
| `shapes[].name` / `shapes[].description` | | 形态名 / 形态说明，默认用卡名 |
| `shapes[].plant_type` | | 层级：`normal`（默认）/ `shield_inner` / `lilypad` / `shield_outer` / `coffee` |
| `shapes[].feature_type` | | 特性，默认 `normal`（水、替换类卡等会看它） |
| `shapes[].target_card` | | 要选目标卡时填卡名，默认 `none` |
| `shapes[].hp` / `atk` / `range` / `cooldown` / `cycle` | | 数值，**17 档数组**（从最低等级往上排），也可以写单个数字＝所有等级一样 |
| `shapes[].flame_produce` | | 可选，产火苗量（17 档） |
| `skill` | | `{ "attr": "flame_produce", "values": [...] }` 技能每级加成（`attr` 可以是 `cycle` / `flame_produce` / `atk` …） |
| `shop` | | 商店 |
| `info_island` | | 图鉴说明文本 |

- **17 档**：写不满就用最后一个补齐（`[50, 60]` → 后面 15 档全是 60）
- JSON 只管卡面 / 数值；**本体形象要在 `_OBJECT_CREATE` 里自己设**：`VM_SetProp(self, "sprite_index", "xxx")`

最小例子：

```json
{
  "name": "我的卡片",
  "shapes": [
    {
      "shape": 0,
      "sprite": "spr_small_fire",
      "hp": 50, "cost": 50, "atk": 0, "range": 0,
      "cooldown": 420, "cycle": 1500
    }
  ]
}
```

---

## 二、卡片实例上的内置字段

卡片实例（`obj_card_mod`，继承 `obj_card_parent`）一建出来就带这些属性，**大部分可以直接读写**。

### 基础（来自 `obj_card_parent`）

| 字段 | 说明 |
|---|---|
| `plant_id` | 卡 id（= 文件名） |
| `name` / `description` / `cost` / `cooldown` | 卡名 / 说明 / 阳光 / 冷却 |
| `hp` / `max_hp` | 当前 / 最大血量（`hp <= 0` 会被父对象销毁） |
| `atk` / `range` | 攻击力 / 攻击范围（格） |
| `cycle` / `attack_cycle` / `origin_cycle` | 攻击周期 / 当前周期（减速翻倍）/ 原始周期 |
| `attack_timer` | 攻击计时器（自己推进，父对象不管） |
| `attack_type` | `ATTACK_TYPE` 枚举（`PRODUCER` 等） |
| `plant_type` / `feature_type` / `target_card` / `target_type` | 层级 / 特性 / 目标卡 / 可打的敌人类型 |
| `shape` / `skill` / `current_level` | 外形 / 技能等级 / 星级 |
| `state` | `0`=待机 `1`=攻击（也见 `CARD_STATE`） |
| `timer` / `first_produce` / `first_produce_delay` | 帧计时 / 是否已首产 / 首产延时 |
| `flame_produce` | 产火苗量（来自 JSON / 技能表，可直接读） |
| `is_frozen` / `frozen_timer` / `ice_timer` / `is_slowdown` / `awake_buff_timer` | 冰冻 / 减速 / 唤醒状态 |
| `invincible` / `can_shovel_remove` | 无敌 / 能被铲 |
| `grid_col` / `grid_row` | 所在格子（列, 行） |
| `x` / `y` | 世界坐标（像素） |
| `depth_value` / `depth_group` | 绘制深度 / 分组（0 前景 1 中景 2 背景） |
| `image_index` / `image_speed` / `flash_speed` / `idle_anim` / `attack_anim` | 动画 |
| `pre_hp` | 上一帧血量（受伤判定用） |

### mod 专用（来自 `obj_card_mod`，进 VM 时机/索敌/批量改属性都靠它们）

| 字段 | 读写 | 说明 |
|---|---|---|
| `mod_tick_cycle` | 只读 | 一轮计时：每帧 +1，到一轮长度归零 |
| `mod_tick_max` | 可写 | 一轮多少帧；**不设 = 用卡的 `cycle`**（技能改 cycle 会自动跟上） |
| `mod_tick_enemy` | 只读 | 有敌人才 +1（没敌人清 0） |
| `mod_has_enemy` | 只读 | 索敌区域里有没有敌人（0/1） |
| `mod_enemy_check` | 可写 | `1` = 让对象每帧帮这张卡索敌（`norm_attack` 会自动开） |
| `mod_step_enemy_area` | 可写（数组） | 索敌区域，**每 4 个值一组** `[上, 下, 左, 右]`（以自己那格为中心各扩几格）；可写多组，任一组有敌人就算有。不设 = `[1,1,0,99]`（本行 ±1、自己那格往右） |
| `mod_enemy_types` | 可写（数组） | 索敌只看哪几类：`normal` / `obstacle` / `diver` / `air` / `dance` / `underground`；空 = 任意 |
| `mod_step_enter_condition` | 可写 | 什么时候进 VM，见下节 |
| `mod_step_enter_var` | 可写 | `"mod"` = 间隔帧数；`"wait"` = 还要等几帧 |
| `mod_step_enter_arr` | 可写（数组） | 开火窗口表（`"norm"` / `"norm_attack"` 用）；**负数 = 从一轮末尾倒数**（`-35` → 一轮长度-35） |
| `mod_step_enter_index` | 只读 | 这帧命中的窗口下标（0 起）；不是窗口 = `-1` |
| `mod_set_on` / `mod_set_id` / `mod_set_prop` / `mod_set_val` | 可写 | **批量改属性**：开关开着时每帧把第 i 个 id 的 `prop[i]` 设成 `val[i]`（三个数组按最短长度配对） |
| `mod_set2_on` / `mod_set2_id` / `mod_set2_prop` / `mod_set2_val` | 可写 | 第二套，行为一样，各自独立 |

写数组只能用 VM 的实例数组接口：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    // 索敌：只看本行往右 → 区域 [0,0,0,99]
    VM_InstArrayClear(self, "mod_step_enemy_area")
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 99)
    // 只看空中敌人
    VM_InstArrayClear(self, "mod_enemy_types")
    VM_InstArrayAdd(self, "mod_enemy_types", "air")
    // 开火窗口：一轮里第 0 帧和第 (一轮-35) 帧（负数从末尾倒数）
    VM_InstArrayClear(self, "mod_step_enter_arr")
    VM_InstArrayAdd(self, "mod_step_enter_arr", 0)
    VM_InstArrayAdd(self, "mod_step_enter_arr", 0 - 35)
    VM_SetProp(self, "mod_step_enter_condition", "norm_attack")
}
```

---

## 三、STEP 的时机与执行顺序

### 什么时候跑

- **`_OBJECT_CREATE`**：卡片被种下时，在 `obj_card_mod` 的 Create 里**立刻执行**（当帧）
- **`_OBJECT_STEP`**：从这张卡第一个 Step 阶段开始，每帧一次，**但会被下面的条件挡住**
- **`_OBJECT_DESTROY`**：实例销毁时（血量归零、被铲、被清场）
- 暂停（`global.is_paused`）时**整块不跑**；冻住（`is_frozen`）时**不进 VM**；`hp <= 0`、实例已销毁也不进

> 手牌点下去种的卡：鼠标事件在 Step 之前，所以**当帧就会跑第一次 STEP**。
> 用 `VM_SpawnPlant` 在别的卡 / 地图脚本的 Step 里种下来的卡，Step 阶段里新建的实例当帧不再跑自己的 Step，
> **第一次 STEP 落在下一帧** —— 需要"种下就立刻发生效果"的逻辑，写进 `_OBJECT_CREATE` 更稳。

### 每帧顺序（`obj_card_mod/Step_0.gml`）

```text
1. 暂停就整个 exit
2. event_inherited()      → 跑父对象那一套（动画 / 攻击 / 生产等）
3. 实例没了 / hp<=0 / is_frozen → exit（冻住不进 VM）
4. 一轮计时 mod_tick_cycle +1，到一轮长度（mod_tick_max，不设用 cycle）归零
5. 索敌（仅当 mod_enemy_check=1 或条件为 norm_attack）
   → 算 mod_has_enemy；没敌人时顺手把 state 收回 IDLE
6. mod_tick_enemy：有敌人才 +1
7. 判断这一帧进不进 VM（mod_step_enter_condition）
   → 命中开火窗口且确实有敌人时，自动把 state 切成 ATTACK
8. 进 VM：执行卡的 _OBJECT_STEP
9. 批量改属性：mod_set_on / mod_set2_on（在 VM 之后，所以卡当帧写的数组当帧就生效）
```

也就是说：**卡的 `_OBJECT_STEP` 里不用自己判"有没有敌人"、也不用自己切攻击动画**
（索敌和切动画由第 5、7 步做），只要设置好索敌区域和进 VM 条件。

### 进 VM 的时机（`mod_step_enter_condition`）

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享，同间隔的卡同帧进） | `mod_step_enter_var` = 间隔帧数 |
| `"norm"` | `mod_tick_cycle` 命中窗口表（**不看敌人**） | `mod_step_enter_arr` 窗口表 |
| `"norm_attack"` | **有敌人** 且命中窗口表；窗口表空 = 有敌人就每帧进 | `mod_step_enter_arr` + 索敌字段 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |

- 命中窗口时 `mod_step_enter_index` = **窗口下标**（0 起，一轮里有多个窗口时用它区分是第几次），否则 `-1`
- 窗口表里的负数从一轮末尾倒数：一轮 1500 帧时 `-35` = 第 1465 帧
- 怎么选：
  - **普通射手**（有敌人才打、按 cycle 节奏）→ `"norm_attack"` + 一条窗口表
  - **生产者 / 每帧逻辑**（火苗、光环）→ 不设（每帧进）或 `"norm"`
  - **全场统一节奏 / 想省性能**（大量同类卡）→ `"mod"` + 间隔
  - **一次性延时**（种下 60 帧后做某事）→ `"wait"` + 60

---

## 四、三个常用 demo

### Demo 1：产火苗

两种写法，按需要选：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_my_producer")
    VM_SetProp(self, "idle_anim", 12)
    VM_SetProp(self, "attack_anim", 20)
    VM_SetProp(self, "flash_speed", 5)
    VM_SetProp(self, "attack_timer", 0)
    VM_SetProp(self, "first_produce", 0)
    // 不设 mod_step_enter_condition → 每帧进 VM
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    at = VM_GetProp(self, "attack_timer")
    cyc = VM_GetProp(self, "cycle")
    fp = VM_GetProp(self, "first_produce")
    fire = 0

    if (fp == 0) {
        // 种下 60 帧先产一次
        at = at + 1
        if (at >= 60) { fire = 1; fp = 1; at = 0 }
    } else {
        at = at + 1
        if (at >= cyc) { fire = 1; at = 0 }
    }

    if (fire == 1) {
        n = VM_GetProp(self, "flame_produce")
        if (VM_IsUndefined(n) == 1) { n = 25 }
        // ① 真实火苗实例（会飘、会自动拾取，和原版一样）
        fx = VM_GetProp(self, "x")
        fy = VM_GetProp(self, "y") - 60
        i = 0
        while (i < 12) {
            fi = VM_CreateInstance("flame", fx, fy)
            VM_SetProp(fi, "value", n)
            i = i + 1
        }
        // ② 想要"直接入账、没有飞行动画"就换成一句：
        // VM_SetFlame(VM_GetFlame() + n * 12)
    }

    VM_SetProp(self, "attack_timer", at)
    VM_SetProp(self, "first_produce", fp)
}
```

- 产量读 `flame_produce`（JSON / 技能表给的），火苗实例的 `value` 就是每朵的火苗数
- 想连开火动画一起演，就在产的那一帧 `VM_SetProp(self, "state", 1)`（`norm_attack` 会自动切）

### Demo 2：实例子弹（`obj_bullet_mod`）

适合需要**自己控制运动/碰撞**的子弹（抛物线、追踪、弹跳…）。

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_my_shooter")
    VM_SetProp(self, "idle_anim", 8)
    VM_SetProp(self, "attack_anim", 12)
    VM_SetProp(self, "flash_speed", 5)
    VM_SetProp(self, "attack_timer", 0)

    // 有敌人才进 VM，一轮里第 0 帧和第 (一轮-35) 帧开火
    VM_InstArrayClear(self, "mod_step_enter_arr")
    VM_InstArrayAdd(self, "mod_step_enter_arr", 0)
    VM_InstArrayAdd(self, "mod_step_enter_arr", 0 - 35)
    VM_SetProp(self, "mod_step_enter_condition", "norm_attack")
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    bx = VM_GetProp(self, "x") + 20
    by = VM_GetProp(self, "y") - 40
    dmg = VM_GetProp(self, "atk")

    b = VM_CreateInstance("obj_bullet_mod", bx, by)
    VM_SetProp(b, "mod_type", "my_bullet")        // = mod/bullets/my_bullet
    VM_SetProp(b, "sprite_index", "spr_my_bullet")
    VM_SetProp(b, "image_xscale", 1.8)
    VM_SetProp(b, "image_yscale", 1.8)
    VM_SetProp(b, "damage", dmg)
    VM_SetProp(b, "vx", 8)                        // ⚠️ 必须显式给速度
    VM_SetProp(b, "vy", 0)

    VM_PlaySound("snd_shot")
    VM_SetProp(self, "state", 1)
}
```

子弹那一侧（`mod/bullets/my_bullet.txt`）负责运动与命中：

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    // 每帧位移由 obj_bullet_mod 自己做（x += vx）——这里只管碰撞
    row = VM_GetProp(self, "grid_row")
    col = VM_GetProp(self, "grid_col")
    n = VM_GetInstancesInRange("hit", row, row, col, col, "enemy", "")
    if (n > 0) {
        e = VM_ArrayGet("hit", 0)
        VM_DamageEnemy(e, VM_GetProp(self, "damage"), "normal")
        VM_DestroyInstance(self)
    }
}
```

> 子弹默认**不会自己消失**（`destroy_timer = -1`），要出界/命中后销毁自己用 `VM_DestroyInstance(self)`，
> 或者 `VM_SetProp(self, "destroy_timer", 90)` 定个寿命。

### Demo 3：弹幕子弹（屏幕弹幕管理器）

比实例子弹省很多（不占实例、统一迭代与绘制），适合**大量同质子弹**。

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    bx = VM_GetProp(self, "x") + 30
    by = VM_GetProp(self, "y") - 50
    dmg = VM_GetProp(self, "atk")

    // VM_BulletScreenAdd_Ex(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,
    //                       存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度)
    // 三发平射（打击范围 0 = 只算本格；伤害计数 -1 = 不限次数；存活帧数 -1 = 不自动消失）
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  8,    0, dmg, 1, 0 - 1, "normal", "", "", "normal", 5, 0)
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  6,  5.6, dmg, 1, 0 - 1, "normal", "", "", "normal", 5, 0)
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  6, 0 - 5.6, dmg, 1, 0 - 1, "normal", "", "", "normal", 5, 0)

    // 会拐到目标行的（原版水管弹那种）：用 _Exs，最后多两个参数（目标行世界 y、靠拢比例）
    ty = 0 - 1                                   // -1 = 不渐变
    VM_BulletScreenAdd_Exs("spr_my_bullet", 1, 0, 1.8, 1, bx, by, 8, 0, dmg, 1, 0 - 1, "normal", "", "", "normal", 5, 0, ty, 0.15)

    VM_PlaySound("snd_shot")
    VM_SetProp(self, "state", 1)
}
```

- **`标志数值`（bit 掩码）**：这颗子弹还能接受哪些卡片效果（`bit1`=过火、`bit2`=解冻、`4/8/16…`=自定义），
  要和卡片侧 `bullet_flag` 对上才生效；不想要卡片联动就写 `0`
- **`伤害计数`**：`1` = 命中一个就消失，`-1` = 不限次数（范围内所有可命中敌人都结算）
- **`销毁对象`**：非空时在子弹消失处创建它（`mod名字` 指定 mod 特效的 `mod_type`），做命中特效最省事
- 要会拐弯追人的，用 `VM_HomingBulletAdd(...)`（每帧重新索敌、自转）

---

## 五、其它要点

- **动画**：`idle_anim`（待机帧数）、`attack_anim`（攻击帧数）、`flash_speed`（几帧走一格，默认 6）、
  `state`（0/1）驱动 `image_index`；默认 `idle_anim = 0` 会**锁在第 0 帧不动**，
  `idle_anim + attack_anim` 别超过精灵总帧数
- **联动 / 计数**用本卡 VM 的命名数组（`VM_ArrayGet` / `VM_ArraySet` / `VM_ArrayADD`，每局重置），
  别依赖全局变量 —— 命名数组是**整个 mod 单元共享**的：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_ArraySet("my_count", 0, VM_ArrayGet("my_count", 0) + 1)
}

_OBJECT_DESTROY {
    self = VM_GetCurCard()
    n = VM_ArrayGet("my_count", 0) - 1
    if (n < 0) { n = 0 }
    VM_ArraySet("my_count", 0, n)
}
```

- 常用查询：`VM_GetPlantAt(列,行,"层级")`、`VM_GetInstancesInRange(...)`、`VM_EnemyInRange(...)`、
  `VM_CanPlace("卡名",列,行)`、`VM_EnemyInRange` 判"这一行有没有敌人"
- ⚠️ **不要读自己没设过的属性**（会报错并中断整个块），要用先在 `_OBJECT_CREATE` 里设一份
  （`flame_produce` / `cycle` / `atk` / `x` / `y` / `grid_row` 这些是现成的，可以直接读）
- 「上一张种下的卡」用 `VM_GetLastCreatedCard()`（CREATE 里读到的还是上一张）
