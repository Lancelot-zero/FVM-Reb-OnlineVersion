# 宝石 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

宝石放在**游戏目录下的 `mod/gems/`** 里：

```text
游戏目录/                       ← 运行游戏的那个文件夹
└─ mod/
   └─ gems/                     ← 宝石这一类都放这里
      ├─ my_gem.json            宝石配置（被动/主动、数值、图标）
      ├─ my_gem.bin             逻辑（由 my_gem.txt 编译出来，游戏只读这个）
      ├─ my_gem.txt             源码（建议保留）
      └─ tex/                   自带贴图（可选）
         └─ xxx.png
```

**也可以再分一层子目录归类**（只扫一层、不往下递归）：`mod/gems/xxx/my_gem.json`，贴图跟 json 同一层放 `mod/gems/xxx/tex/`；id 只看 json 文件名。

---

## 一、JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` / `description` | ✅ | 名字 / 说明（鼠标悬停时直接显示 `description`） |
| `icon` | ✅ | 图标精灵名（建议自带 **82×82** 的 PNG、原点 40,40） |
| `slot` | | 装在哪种武器上：`main_weapon`（默认）/ `super_weapon` / `secondary_weapon` |
| `passive` | | `true` = 纯被动宝石，**不创建实体**（`.bin` 不会被跑） |
| `cooldown` | | 冷却数组（**按等级取、单位帧**）；主动宝石和"自己走冷却"的宝石都要 |
| `max_level` | | 最高等级（默认 15） |
| `first_cooldown` | | 首次冷却（没有 `cooldown` 数组时用） |
| `shop` | | 商店 |

除 `name` / `description` / `icon` / `slot` / `shop` / `passive` 以外的字段会**原样透传**进宝石数据
（要在 `.bin` 里用就自己 `VM_SetProp` 设一份到实例上，或者读实例上现成的那几个，见下）。

---

## 二、宝石实例上的内置字段

宝石实例一建出来就带好这些：

| 字段 | 初值 | 说明 |
|---|---|---|
| `gem_id` | 全局 `_mod_pending_gem_id` | 宝石 id（= 文件名） |
| `gem_info` | `get_gem_info(gem_id)` | 注册数据（`description` / `icon` / `cooldown` / 透传字段…） |
| `cooldown` | `gem_info.cooldown[等级]` | **按等级取好的冷却帧数**（没有数组时用 `first_cooldown`，再没有就 60） |
| `sprite_index` | `gem_info.icon` | 默认是图标；战场形象自己 `VM_SetProp` 覆盖 |
| `parent_player` | `noone` | **保持 noone**（原版语义，别占用） |
| `mod_point_parent_player` | 放置它的角色实例 | 插件宝石定位用（放置逻辑挂的） |
| `mod_point_grid_row` / `mod_point_grid_col` | 放置方所在格子 | 同上 |
| `mod_point_gem_level` | 存档等级 | 同上 |
| `mod_has_enemy` / `mod_has_card` | 只读 | "区域里有敌人 / 有卡片"的结果（`"enemy_area*"` / `"card_area*"` 条件用） |
| `mod_tick_cool` | 只读 | `"*_area_mod"` 的冷却剩余帧数 |
| `image_speed` / `timer` / `flash_speed` | `0` / `0` / `5` | 动画（同卡片那套 `state`/`idle_anim`/`attack_anim`） |
| `state` / `idle_anim` / `attack_anim` | IDLE / 0 / 0 | 动画状态 |

**预留显示属性**（核心负责渲染，插件写值就行）：

| 属性 | 谁写 | 含义 |
|---|---|---|
| `clickable` | 插件（`_OBJECT_CREATE` 置 1） | 声明"我是主动宝石"；不置（默认 0）＝点击无效 |
| `clicked` | 核心置 1，插件处理完清 0 | "刚被有效点击"；**冷却中不会置** |
| `cooldown_timer` | 核心每帧自减；点击时自动设回 `cooldown` | 剩余冷却帧；>0 时画遮罩 + 秒数、说明里加「正在冷却中」 |
| `on_click` | 核心（鼠标进 / 出） | 显示说明框用；也可以自己置 |
| `gem_level` | 存档等级 | >0 时画等级星星 |
| `tooltip_text` | 插件 | 追加在说明后面的自定义文本 |

---

## 三、STEP 的时机与执行顺序

### 什么时候跑

- **`_OBJECT_CREATE`**：实例建好、放置逻辑把 `mod_point_parent_player` / `mod_point_grid_row` /
  `mod_point_grid_col` / `mod_point_gem_level` 写完之后立刻执行（放置逻辑会为这颗 mod 宝石
  **调一次它的步**；插件自己建的宝石则在第一次 Step 里补跑）。
  也就是说：**`_OBJECT_CREATE` 里读得到那四个 `mod_point_*` 归属字段**。
- **`_OBJECT_STEP`**：每帧（暂停时整块不跑），**且会被 `mod_step_enter_condition` 挡住**（默认 `""` 才是每帧进）
- **`_OBJECT_DRAW`**：写了就跑 VM 的绘制块（**不再自动画 sprite**）；没写则由核心画：
  半透明冷却遮罩 + 剩余秒数、悬停说明框、等级星星
- **`_OBJECT_DESTROY`**：宝石被销毁时（块内用 `VM_GetCurCard()` 拿自己，`VM_GetLastDestroyedCard()` 也能拿到）
- **`_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK`**：鼠标移入 / 移出 / 左键点在宝石图标上（按实例判定，鼠标要压在宝石贴图上）

### 每帧顺序（核心在什么时候跑你的块）

```text
1. 暂停 → 整个 exit
2. cooldown_timer 每帧自减 1（到 0 停）
3. 动画自动播放（timer/flash_speed 驱动 image_index，state 选待机/攻击区间）
4. 判断这一帧进不进 VM（mod_step_enter_condition）
5. 跑宝石 .bin 的 _OBJECT_STEP（被上面条件挡住时不执行）
```

> 动画和武器一样是**核心自动推**的：`image_speed` 固定 0，别去改；待机/攻击不会自动切，`state` 自己改。
> 想自己控制就在 `_OBJECT_STEP` 里直接写 `image_index`（VM 在核心动画之后跑，当帧生效）。

### 进 VM 的时机（`mod_step_enter_condition`）

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享） | `mod_step_enter_var` = 间隔帧数 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |
| `"cool"` | **冷却结束才进**（`cooldown_timer <= 0`） | — |
| `"enemy_area"` | **索敌区域里有敌人**就进 | `mod_step_enemy_area`（区域）/ `mod_enemy_types`（类型筛选）；`mod_has_enemy`（只读） |
| `"enemy_area_mod"` | 同上，但**进一次后要等冷却**再进 | 再加 `mod_step_enter_var` = 冷却帧数；`mod_tick_cool`（只读） |
| `"card_area"` | **指定区域里有卡片**就进（玩家底座不算） | `mod_step_card_area`（区域）；`mod_has_card`（只读） |
| `"card_area_mod"` | 同上，但**进一次后要等冷却**再进 | 再加 `mod_step_enter_var` = 冷却帧数；`mod_tick_cool`（只读） |

**区域怎么写**（`mod_step_enemy_area` / `mod_step_card_area` 都是同一套）：**每 4 个值一组** `[上, 下, 左, 右]`，
以"**锚点格**"为中心各扩几格；可以写多组，任一组命中就算命中。不设 / 凑不满 4 个 = 默认 `[1,1,0,99]`。

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    // 只看本行往右 → [0,0,0,99]
    VM_InstArrayClear(self, "mod_step_enemy_area")
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 0)
    VM_InstArrayAdd(self, "mod_step_enemy_area", 99)
    // 只看空中敌人
    VM_InstArrayClear(self, "mod_enemy_types")
    VM_InstArrayAdd(self, "mod_enemy_types", "air")
    // 有敌人就进，进一次后至少等 30 帧
    VM_SetProp(self, "mod_step_enter_condition", "enemy_area_mod")
    VM_SetProp(self, "mod_step_enter_var", 30)
}
```

- **锚点格**：优先用"**放置这颗宝石的角色**"当前所在格（角色会动，区域跟着走）；
  角色不在了就退回创建时记下的 `mod_point_grid_row` / `mod_point_grid_col`
- `mod_enemy_types` 可写 `normal` / `obstacle` / `diver` / `air` / `dance` / `underground`；空 = 任意类型
- 这两种区域检测**只在用这些条件时才算**，其它条件零开销

> 宝石**没有** `hp`、图标也不移动（钉在 `390, 213 + 序号*80`），所以卡片/敌人那套
> `hp_change` / `cell` 这里没有。`"cool"` 是给主动宝石用的：冷却中不进 VM，冷却一结束就进。

### 还有一条路：盯属性变化（`mod_watch`）

除了上面的条件，还能让"某个属性的值变了"成为进 VM 的理由。在 `.bin` 里声明一个**命名数组** `mod_watch`，
里面写要盯的**属性名**（这个对象自己的任何实例属性都行，比如 `hp` / `atk` / `x` / 你自己 `VM_SetProp` 加的属性）：

```gml
VM_ArrayClear("mod_watch")
VM_ArrayAdd("mod_watch", "hp")
VM_ArrayAdd("mod_watch", "atk")
```

对象每帧会**遍历**这个数组，用**等号**比每个属性的当前值和上一帧的值：

- 这一帧进几次 = **变化的属性个数** + **（`mod_step_enter_condition` 也成立 ? `1` : `0`）**；
- 每次进去读 `p = VM_GetProp(self, "mod_changed_prop")` 就知道**这次是谁**：变化的那几次是**属性名**（按数组顺序一个接一个），最后“条件”那次是 `""`；
- 同一帧多个属性变化 → **各进一次**（快照会全部更新，下帧不会重复报）；每轮之间会检查实例还在不在（插件把自己销毁了就中断）；
- 也能当**对象之间 / 插件之间的信号**用：约定一个属性名（比如自己 `VM_SetProp` 加的 `mod_signal`），一边改它，另一边就会因为 `mod_watch` 变化被触发进入，读 `mod_changed_prop` 就知道是谁变了 —— 相当于互相传消息；
- ⚠️ 声明 / 改列表那一帧只对齐快照、不触发；条件那一路和 watch 那一路**互不影响**（watch 每帧照常比对）。
- ⚠️ 声明 / 改列表那一帧只对齐快照、不触发；同一帧多个属性变化只报第一个（快照仍会全部更新，下帧不会重复报）。

### 鼠标事件（核心挂的，插件不用写）

| 事件 | 核心做什么 |
|---|---|
| `Mouse_10`（鼠标进入） | `on_click = 1` → 显示说明框 |
| `Mouse_11`（鼠标离开） | `on_click = 0` |
| `Mouse_4`（左键按下） | `clickable` 为 1 且 `cooldown_timer <= 0` 时：`clicked = 1`，并自动 `cooldown_timer = cooldown` |

所以**主动宝石**只要在 `_OBJECT_STEP` 里读 `clicked`；**被动宝石**（`clickable` 不置）点击完全没反应，
`cooldown_timer` 由插件自己写。

> 想自己接管鼠标，就写 `_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK` 块 ——
> 核心那套照常跑（三个块跑完，核心自己的鼠标处理还会继续），不会互相顶掉。

---

## 四、demo

### Demo 1：主动宝石（点击放技能 → 自动冷却）

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "clickable", 1)                  // 声明主动宝石
    VM_SetProp(self, "sprite_index", "spr_my_gem")    // 战场形象（背包图标用 JSON 的 icon）
    VM_SetProp(self, "image_xscale", 1.8)
    VM_SetProp(self, "image_yscale", 1.8)
    VM_SetProp(self, "image_speed", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    if (VM_GetProp(self, "clicked") == 1) {
        VM_SetProp(self, "clicked", 0)                // 自己清掉
        // ↓ 放技能（这里示例：全场敌人各吃一发伤害）
        r = 0
        while (r < 7) {
            n = VM_GetInstancesInRange("g_hit", r, r, 0, 15, "enemy", "")
            k = 0
            while (k < n) {
                e = VM_ArrayGet("g_hit", k)
                VM_DamageEnemy(e, 300, "normal")
                k = k + 1
            }
            r = r + 1
        }
        VM_PlaySound("snd_shot")
        // 冷却不用管：点下去已经自动按 JSON 的 cooldown[等级] 重新走了
    }
}
```

### Demo 2：被动宝石（自己走节奏 + 自己管冷却）

适合"有敌人就自动开火"这类；`cooldown` 数组照写，冷却自己设：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_attack_gem_icon")
    VM_SetProp(self, "cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    // 场上有敌人时累计冷却；到点就发射
    if (VM_EnemyInRange(0, 5, 0, 15, "") == 1) {
        cd = VM_GetProp(self, "cd") + 1
        if (cd >= VM_GetProp(self, "cooldown")) {
            cd = 0
            b = VM_CreateInstance("xiaolongbao_bullet", VM_GetProp(self, "x") + 30, VM_GetProp(self, "y"))
            VM_SetProp(b, "damage", 25)
            VM_SetProp(b, "move_speed", 8)
            VM_SetProp(b, "row", 2)
            VM_PlaySound("snd_shot")
            // 想让图标上也显示冷却，就顺手写：
            VM_SetProp(self, "cooldown_timer", VM_GetProp(self, "cooldown"))
        }
        VM_SetProp(self, "cd", cd)
    } else {
        VM_SetProp(self, "cd", 0)
    }
}
```

（这就是 `dev_test/gems/sentry_gem.txt` 的写法。）

### Demo 3：给宝石挂一个跟随光环

宝石侧创建**特效**（`mod/effects/` 那一套），把角色实例交给它跟着走：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "fx", 0)          // 记光环实例
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    p = VM_GetProp(self, "mod_point_parent_player")
    fx = VM_GetProp(self, "fx")

    // 拿到角色、且光环还没建（0 或已被销毁）→ 建一个
    need = 0
    if (VM_IsUndefined(p) == 0) {
        if (VM_IsDestroyed(p) == 0) {
            if (fx == 0) { need = 1 }
            if (VM_IsDestroyed(fx) == 1) { need = 1 }
            if (need == 1) {
                VM_SetProp(0, "_mod_pending_effect_id", "my_aura")     // 特效类型名
                fx = VM_CreateInstance("obj_effect_mod", VM_GetProp(p, "x"), VM_GetProp(p, "y"))
                VM_SetProp(0, "_mod_pending_effect_id", "")            // 用完清掉
                VM_SetProp(fx, "parent_inst", p)                       // 交给特效自己跟随
                VM_SetProp(fx, "sprite_index", "spr_my_aura")
                VM_SetProp(fx, "rel_x", -15)
                VM_SetProp(fx, "rel_y", -5)
                VM_SetProp(self, "fx", fx)
            }
        }
    }
}
```

特效侧怎么跟随，见 [mod特效.md](mod特效.md) 的 Demo 1。

---

## 五、要点

- **图标有两道静默失败**：槽位列表没遍历到 → 图标栏里完全不出现；`icon` 名字解析不到 →
  出现了但是空白（`get_load_sprite` 会给占位图、不报错）。自带 PNG 要在 `_OBJECT_CFG` 里预加载，
  而且**要在注册之前**挂好
- `cooldown` 数组**按等级取、单位是帧**；`cooldown_timer` 每帧自减
- 战场形象自己设（`sprite_index` + `image_xscale/yscale`），背包/槽位图标用 JSON 的 `icon`
- 宝石实例由**放置逻辑**创建，插件创建不了宝石；要跟角色/格子就靠 `mod_point_*`
- `parent_player` 保持 `noone`（原版语义），别拿它当"发射方"
- ⚠️ **按等级查的数值表要建在 `_OBJECT_CFG`，别放 `_OBJECT_CREATE`**：
  `[reloadmod]` 会**清空这个 VM 的全部命名数组**，但**不会重跑 `_OBJECT_CREATE`**（实例是保留的）。
  放 CREATE 的表重载后全空，`VM_ArrayGet` 静默返回 **0**，症状很隐蔽：
  产火苗的宝石会把「`t >= 0×60`」判成恒真 → **每帧狂刷火苗**；伤害/减伤类则变成**打不出伤害、没有护盾**。
  活计数器（场上同类数量）同理，别用命名数组记，用的时候现场 `VM_GetInstancesInRange`。
  详见 `mod开发工具说明.md`「四、常见坑速查」
