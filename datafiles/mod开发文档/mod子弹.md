# 子弹 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

子弹放在**游戏目录下的 `mod/bullets/`** 里：

```text
游戏目录/                       ← 运行游戏的那个文件夹
└─ mod/
   └─ bullets/                  ← 子弹这一类都放这里
      ├─ my_bullet.json         可以就是 {}
      ├─ my_bullet.bin          逻辑全在这里（由 my_bullet.txt 编译出来）
      ├─ my_bullet.txt          源码（建议保留）
      └─ tex/                   自带贴图（可选）
         └─ xxx.png
```

**也可以再分一层子目录归类**（只扫一层、不往下递归）：`mod/bullets/xxx/my_bullet.json`，贴图跟 json 同一层放 `mod/bullets/xxx/tex/`；id 只看 json 文件名。

子弹**不进任何池**，只是给 `obj_bullet_mod` 提供一套同名逻辑（`mod_type` 就是文件名）。
所以 json 没有固定字段，`{}` 也行，但**必须有这个 json**，否则扫描不到。

---

## 一、怎么创建（两步）

> **没有「按 mod 类型直接创建」的命令。** `VM_CreateInstance` 只认游戏对象名（`obj_xxx`）：
> 传 mod 类型名（如 `"my_bullet"`）会去找 `obj_my_bullet` → 找不到 → **返回 −1**，日志里打一行
> `[VM_CreateInstance] 对象不存在: obj_my_bullet`。所以流程永远是：
> **先创建通用的 `obj_bullet_mod`，再告诉它用哪套逻辑**。

### ✅ 方案 A（推荐）：创建【前】用全局指定类型 —— 当帧就动

```gml
VM_SetProp(0, "_mod_pending_bullet_id", "my_bullet")   // ① 先指定类型（= mod/bullets/ 下的文件名）
b = VM_CreateInstance("obj_bullet_mod", x, y)          // ② 再创建
VM_SetProp(0, "_mod_pending_bullet_id", "")            // ③ 用完清空（别省，见下面的坑）
VM_SetProp(b, "vx", 8)              // ⚠️ 必须显式设速度
VM_SetProp(b, "vy", 0)
VM_SetProp(b, "damage", 60)
VM_SetProp(b, "sprite_index", "spr_my_bullet")
```

- Create 里立刻按类型初始化 → **创建当帧**就能动、贴图和数值当帧生效
- **③ 清空不能省**：`_mod_pending_bullet_id` 是个**全局**，不清就会一直留着。下一个脚本只要
  用的是方案 B（创建后才写 `mod_type`），就会**继承这个脏值** —— 因为 Create 里已经按脏值把
  VM 初始化并把 `_mod_initialized` 置成 `true`，之后写 `mod_type` **再也改不回来**。
  症状是「A 发完子弹，B 的子弹跑成了 A 的逻辑」（雷神 / 哈迪斯镰刀 / 命运女神都栽过这个）

### ⚠️ 方案 B：创建【后】再设 `mod_type` —— 下一帧才动，且怕脏值

```gml
b = VM_CreateInstance("obj_bullet_mod", x, y)
VM_SetProp(b, "mod_type", "my_bullet")     // 靠下一帧的"延迟初始化"补上
```

只在**创建那一刻 `_mod_pending_bullet_id` 恰好是空的**时才可靠。
只要别的地方用了方案 A 却没清全局，这里就会静默跑成别人的脚本 —— 所以**优先用方案 A**，
真要用 B 的话，创建前先 `VM_SetProp(0, "_mod_pending_bullet_id", "")` 把它清干净。

---

## 二、子弹实例上的内置字段

| 字段 | 初值 | 说明 |
|---|---|---|
| `mod_type` | 全局 `_mod_pending_bullet_id`（默认 `""`） | 跑哪套逻辑 |
| `destroy_timer` | `-1` | `-1`=不自动销毁；`0`=立即销毁；`>0`=每帧 −1，到 0 销毁 |
| `vx` / `vy` | `0` / `0` | **每帧位移，必须显式设**（不设就停在原地） |
| `vector_x` | `0` | 备用字段（不影响自带走位） |
| `grid_col` / `grid_row` | 创建时算一次 | **当前**所在格（每帧 Step 开头更新） |
| `prev_grid_col` / `prev_grid_row` | 同上 | **上一帧**所在格（判跨越用） |
| `image_speed` | `0` | 要帧动画自己设 |
| `x` / `y` / `sprite_index` / `image_alpha` / `image_angle` / `image_xscale` / `image_yscale` | 普通实例属性 | 直接读写 |
| `damage` / `row` / … | 没有 | **插件自己设的**属性（`VM_SetProp` 设过才有） |

### 碰撞结算字段（**引擎自动做**，默认全 0 = 不结算）

撞到敌人时（`Collision_obj_enemy_parent`）核心会按下面这些字段**自动结算**，脚本不用自己写：

| 字段 | 默认 | 作用 |
|---|---|---|
| `bullet_damage` | `0` | 撞到敌人造成多少伤害（`0` = 不掉血） |
| `bullet_damage_type` | `"normal"` | `normal` 有盾只打盾 / `pierce` 盾血一起掉 / 其它无视护盾 |
| `bullet_collide` | `1` | `1` = 正常碰撞；**`0` = 关掉碰撞**：核心把它的碰撞掩码换成"空掩码"，引擎**直接跳过重叠检测**（真省性能） |
| `bullet_slow` | `0` | `1` = 命中就**减速**（写敌人 `ice_timer`：移动与攻速都减半） |
| `bullet_slow_frame` | `0` | 减速持续帧数 |
| `bullet_freeze_chance` | `0` | `0~1`：命中时**完全冻结**的概率（写 `frozen_timer`，整只不动） |
| `bullet_freeze_frame` | `0` | 冻结持续帧数 |
| `bullet_stun_chance` | `0` | `0~1`：命中时**眩晕**的概率（写 `stun_timer`，不动 + 冒星星） |
| `bullet_stun_frame` | `0` | 眩晕持续帧数 |
| `bullet_hits` | `-1` | 撞几次后消失；**`-1` = 不消耗（默认：撞上也不会消失）** —— 要让子弹"撞一次就没"必须显式写 `1` |

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "bullet_damage", 120)          // 命中掉 120
    VM_SetProp(self, "bullet_slow", 1)              // 顺带减速
    VM_SetProp(self, "bullet_slow_frame", 180)      // 3 秒
    VM_SetProp(self, "bullet_freeze_chance", 0.3)   // 30% 完全冻住
    VM_SetProp(self, "bullet_freeze_frame", 120)    // 冻 2 秒
    VM_SetProp(self, "bullet_hits", 1)              // 撞一次就消失（**默认是 -1 = 撞了不消失**）
}
```

- 减速 / 冻结 / 眩晕都是"**取更长的那一个**"，不会把敌人身上已有的更长状态缩短
- 伤害走敌人自己的受击事件（闪白 / 音效 / 护盾判定都对），和自己写 `VM_DamageEnemy` 一个口径
- 碰撞还会写 `mod_collision_enemies`（这一帧撞到的所有敌人，数组）/ `mod_collision_enemy`（最后一个）/
  `mod_collision_hit`，配合 `mod_step_enter_condition = "attack_collision"` 就能"撞上才进 VM"
- ⚠️ 子弹必须有**有效贴图**（`sprite_index`）才有碰撞体；空占位图撞不到敌人
- **不想让这颗子弹参与碰撞**：`bullet_collide = 0` —— 核心会把它的碰撞掩码换成一个**空掩码精灵**，
  **引擎直接跳过它的重叠检测**（这是真的省性能，不只是"不结算"）；伤害/状态/碰撞字段/`"attack_collision"`
  全都不做，命中判断完全交给脚本。
  - 恢复：把 `bullet_collide` 改回 `1`（掩码复位成自家贴图）
  - `place_meeting` 之类的检测也会一起失效（掩码是空的）
  - 兜底：万一引擎版本没有 `sprite_collision_mask`，会自动退回"碰撞事件里提前退出"（不结算，但检测仍在跑）
  - ⚠️ 掩码每帧在 Step 里维护，所以**在 `_OBJECT_CREATE` 或任意 STEP 里设都生效**（当帧就生效）
- ⚠️ 贴着敌人不动时**每帧**都会触发一次碰撞 → 伤害/状态也每帧结算一次
- ⚠️ 概率用 `random(1)`，联机下各客户端可能掷出不同结果（要严格同步就在 VM 里自己掷）

### 抛物线字段（**引擎自己飞**）

`bullet_parabola = 1` 就由核心按弧线推进（会**覆盖** `vx/vy` 的直线位移）：

| 字段 | 默认 | 作用 |
|---|---|---|
| `bullet_parabola` | `0` | `1` = 走抛物线 |
| `bullet_target` | `-1` | 目标**实例** id（优先；`-1` 或实例没了就用下面坐标） |
| `bullet_target_x` / `bullet_target_y` | `0` / `0` | 目标坐标 |
| `bullet_parabola_time` | `30` | 全程帧数（30 帧 = 0.5 秒） |
| `bullet_parabola_h` | `-1` | 弧线最高点高度（像素）；`-1` = 自动（起点到目标距离的 1/4） |

- 起点和目标是**起飞那一帧**取一次（目标是实例时也不追第二遍）
- 飞完（到目标点）就停下：`bullet_parabola` 置 `0`、`vx/vy` 清零、坐标钉在目标点，
  之后交给碰撞结算 / `destroy_timer` / 脚本自己处理

---

## 三、STEP 的时机与执行顺序

### 创建当帧 vs 下一帧（重要）

`VM_CreateInstance` 只是"创建一个 `obj_bullet_mod`"，它不认 `mod_type`。所以：

| 时机 | 发生什么 |
|---|---|
| **创建当帧** | 子弹刚建出来时 `mod_type` 还是空 → **不初始化**；随后你的 `VM_SetProp` 把 `mod_type` / 数值写进去。这帧还没有本体精灵，看不见是正常的 |
| **下一帧** | Step 开头发现 `mod_type` 有了 → **延迟初始化**：跑该子弹的 `_OBJECT_CREATE`，紧接着**同帧**跑 `_OBJECT_STEP` |
| 用 `_mod_pending_bullet_id` 提前写好再创建 | Create 里就直接初始化，**当帧**就能动 |

> 所以：在"创建前设好类型"就能当帧生效；只设 `mod_type` 也行，代价是下一帧才开始动。

### 每帧顺序（核心在什么时候跑你的块）

```text
1. 暂停 → exit
2. prev_grid_col/row = 当前格子；再用 get_grid_position_from_world 更新 grid_col/row
3. 延迟初始化（mod_type 有了就跑 _OBJECT_CREATE）
4. 判断这一帧进不进 VM（mod_step_enter_condition）
5. 跑子弹 .bin 的 _OBJECT_STEP（被上面条件挡住时不执行）
6. 实例没了（你在 VM 里销毁了自己）→ exit
7. 自动销毁：destroy_timer == 0 → 立即销毁；> 0 每帧 −1，到 0 销毁
8. 位移：x += vx; y += vy
9. 出界销毁：x > 2200 / y > 1200 / x < 0 / y < 0
```

### 进 VM 的时机（`mod_step_enter_condition`）

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享） | `mod_step_enter_var` = 间隔帧数 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |
| `"cell"` | **跨格**那一帧进（`grid_col` 或 `grid_row` 相对上一帧变了） | — |
| `"cell_row"` | **只跨行**那一帧进（`grid_row` 变了）；直线弹用不上，抛物弹有用 | — |
| `"cell_target_row"` | **在目标所在行里**就进（在着就一直进） | 目标取 `bullet_target`（实例）；没实例就用 `bullet_target_x/y` 换算格子；都没设 = 永不进 |
| `"cell_target_col"` | **在目标所在列里**就进 | 同上 |
| `"attack"` | **场上还有敌人才进** | `mod_has_enemy`（只读） |
| `"attack_cell"` | **子弹当前这一格有敌人才进**（查每帧重建的敌人表，比 `"attack"` 精确） | `mod_has_enemy`（只读） |
| `"attack_collision"` | **撞到敌人了才进**（碰撞事件把撞到的敌人攒成数组；**一帧撞几个也只进一次**） | `mod_collision_enemies`（只读，数组）= 这一帧撞到的所有敌人；`mod_collision_enemy`（只读）= 最后一个；`mod_collision_hit` = 1/0 |

- **子弹最常用 `"cell"`**：只有跨格那一帧进 VM，正好对上"路径碰撞"的需求
- ⚠️ **子弹是场上数量最多的东西，最忌每帧进 VM**（不设条件就是每帧）：九成情况 `"cell"` 就够；
  真的需要"每帧都动"的（抛物线、自转），才留空让自己每帧进 —— 而且那种弹优先考虑屏幕弹幕 `VM_BulletScreenAdd_Exs`（见 `help.md` 与 [mod开发工具说明.md](mod开发工具说明.md) 的「性能铁律」）
- ⚠️ `"cell"` 触发的是**进格后的第一帧** —— 格子坐标是在 `x += vx` **之前**算的，比实际越界晚一帧；
  一帧跨多格也只触发一次
- `"attack_collision"` 是"**撞上才进**"：碰撞事件把这一帧撞到的敌人**都攒进 `mod_collision_enemies`**（数组），
  Step 里只进**一次** VM —— 一帧撞 3 个也只进一次，脚本自己遍历；进完 VM 列表自动清空。
- ⚠️ **时序**：碰撞是 Step **跑完之后**才判定的，所以卡片是在**下一帧的 Step** 里收到这次碰撞。
  `bullet_hits` 用完的子弹会**等这一次 VM 跑完**才销毁（引擎把销毁推到子弹自己的下一步），
  不会出现"子弹先没了、脚本收不到通知"这种情况。
  只关心一个敌人就读 `mod_collision_enemy`（= 最后一个）。遍历写法：
  ```gml
  _OBJECT_STEP {
      self = VM_GetCurCard()
      n = VM_InstArraySize(self, "mod_collision_enemies")
      k = 0
      while (k < n) {
          e = VM_InstArrayItem(self, "mod_collision_enemies", k)
          // 对 e 做点什么……
          k = k + 1
      }
      VM_DestroyInstance(self)     // 撞完就没
  }
  ```
- `"cell_target_row"` / `"cell_target_col"` 是"**在**目标那一行/列里"就进（不是跨过去那一帧）：
  目标优先取 `bullet_target` 实例，其次按 `bullet_target_x/y` 换算格子；两个都没设就永不进。
  适合"飞到目标行/列里了才开始做点什么"（配合 `"attack_cell"` 用就是"同一行 + 同格"）

- `"attack"` 是**全局判定**（只看场上还有没有敌人），`"attack_cell"` 只看**自己脚下这一格** ——

**能不进 STEP 就别进：这些需求只靠预设字段就能做完（只在 `_OBJECT_CREATE` 里设好）**

| 需求 | 只靠这些字段 / 条件 |
|---|---|
| 直线飞 + 命中扣血 | `bullet_damage` / `bullet_damage_type` / `bullet_hits`（引擎在碰撞事件里自动结算） |
| 命中减速 / 冻结 / 眩晕 | `bullet_slow` + `bullet_slow_frame` / `bullet_freeze_chance` + `bullet_freeze_frame` / `bullet_stun_chance` + `bullet_stun_frame` |
| 抛物线（追实例或坐标） | `bullet_parabola` + `bullet_target`（或 `bullet_target_x/y`）+ `bullet_parabola_time` / `bullet_parabola_h` |
| 撞上才做额外事 | STEP 条件用 `"attack_collision"` —— 全程只在**命中那几帧**进 VM |
| 到目标行 / 列才做 | `"cell_target_row"` / `"cell_target_col"` |
| 只要命中特效 / 音效 | 同上：`"attack_collision"` 进一次就够，不用每帧跑 |

也就是说：**多数子弹连 `_OBJECT_STEP` 都不用写**（写了也就是命中/到位那一两帧跑），
子弹多的时候这就是帧率差别的来源。真需要每帧动的（追踪、自转），优先考虑
`VM_BulletScreenAdd_Exs` / `VM_HomingBulletAdd` 这类不占实例的接口。
  两个都是一次查表、零循环，比卡片那套按区域索敌便宜；要"飞到敌人身上才结算"就用 `"attack_cell"`

### 鼠标三个块

`_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK` 子弹也有，是按实例判定的
（鼠标要压在子弹的贴图上）。⚠️ 子弹飞得快、通常又没设 `sprite_index`，**实际很少用得上**。

两个由此而来的要点：

- **位移在第 7 步**，也就是**在你的 `_OBJECT_STEP` 之后**。所以 VM 里读到的 `x` / `y` 是**本帧移动前**的位置；
  想抛物线/自转就每帧改写 `vx` / `vy`（也可以直接改 `x` / `y`）
- **出界自动销毁**是第 8 步做好的，不用自己判（`destroy_timer = -1` 也不会飞出屏幕外赖着）
- **绘制**：`_OBJECT_DRAW` 写了就跑 VM 的绘制块（**不再自动画 sprite**），没写就 `draw_self()`；
  Draw 里也会补做一次延迟初始化，所以"还没 Step 就先画一帧"也不会漏

---

## 四、demo

### Demo 1：直线穿透弹（命中不消失）

```gml
_OBJECT_CFG {
    if (VM_SpriteExists("spr_my_bullet") == 0) {
        VM_LoadSpritePerm_Ex("tex/spr_my_bullet_0.png", 5, 87, 45)
        VM_AliasSpritePerm("spr_my_bullet", "tex/spr_my_bullet_0.png")
    }
}

_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "destroy_timer", -1)      // 不自动销毁（出界由对象处理）
    VM_SetProp(self, "image_xscale", 1.6)
    VM_SetProp(self, "image_yscale", 1.6)

    // sprite_index 是内置实例变量，永远"已定义"（没设时是 -1）→ 不能用 undefined 兜底
    spr = VM_GetProp(self, "sprite_index")
    if (VM_IsUndefined(spr) == 1) { spr = -1 }
    if (spr == -1) { VM_SetProp(self, "sprite_index", "spr_my_bullet") }

    // 别人没给的速度/伤害，这里给默认
    if (VM_IsUndefined(VM_GetProp(self, "vx")) == 1) { VM_SetProp(self, "vx", 8) }
    if (VM_IsUndefined(VM_GetProp(self, "vy")) == 1) { VM_SetProp(self, "vy", 0) }
    if (VM_IsUndefined(VM_GetProp(self, "damage")) == 1) { VM_SetProp(self, "damage", 0) }
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    r = VM_GetProp(self, "row")
    bc = VM_GetProp(self, "grid_col")

    // 打本行、子弹周围 3 格内的敌人；命中不销毁（穿透）
    n = VM_GetInstancesInRange("pb_hit", r, r, bc - 1, bc + 1, "enemy", "")
    k = 0
    while (k < n) {
        e = VM_ArrayGet("pb_hit", k)
        if (VM_GetProp(e, "hp") > 0) {
            VM_SetProp(e, "hp", VM_GetProp(e, "hp") - VM_GetProp(self, "damage"))
        }
        k = k + 1
    }
}
```

（完整版见 `mod/bullets/pierce_bullet.txt`：还带"目标行渐变"和"过火盆点燃"两段。）

### Demo 2：命中即消失（走敌人受击事件）

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    r = VM_GetProp(self, "grid_row")
    c = VM_GetProp(self, "grid_col")
    n = VM_GetInstancesInRange("hit", r, r, c, c, "enemy", "")
    if (n > 0) {
        e = VM_ArrayGet("hit", 0)
        VM_DamageEnemy(e, VM_GetProp(self, "damage"), "normal")   // 闪白/音效/护盾都对
        // 命中处放个特效（也可以直接用弹幕管理的"销毁对象"参数）
        fx = VM_CreateInstance("obj_effect_mod", VM_GetProp(self, "x"), VM_GetProp(self, "y"))
        VM_SetProp(fx, "mod_type", "my_hit_fx")
        VM_SetProp(fx, "destroy_timer", 20)
        VM_DestroyInstance(self)
    }
}
```

### Demo 3：抛物线 / 自转

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "vy", -8)         // 先往上
    VM_SetProp(self, "spin", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    // 重力：每帧把 vy 往下拉（位移由对象在跑完本块后做）
    VM_SetProp(self, "vy", VM_GetProp(self, "vy") + 0.4)
    // 自转（画面）
    VM_SetProp(self, "image_angle", VM_GetProp(self, "image_angle") + 12)
}
```

---

## 五、要点（踩过的坑）

- **创建 mod 子弹：优先用方案 A（创建【前】设全局 `_mod_pending_bullet_id`，创建后清空）**。
  `VM_CreateInstance` **不认 mod 类型名**，只能创建游戏对象；创建后写 `mod_type`（方案 B）只在
  "创建那一刻全局是空的"时才有效。全局不清 → 下一个用方案 B 的脚本继承脏值，静默跑成上一个
  mod 的逻辑（Create 里已置 `_mod_initialized = true`，之后改 `mod_type` 无效）
- **`vx` / `vy` 必须显式设**：Create 里就是 0，"不设就用默认"不成立（子弹一动不动）
- **`sprite_index` 永远"已定义"**（没设时是 `-1`），判断"别人给没给贴图"要用 `== -1`，不能用 undefined
- **默认不会自己消失**（`destroy_timer = -1`）：出界会自动销毁，但命中后要自己 `VM_DestroyInstance(self)`
  或者给自己一个寿命 `VM_SetProp(self, "destroy_timer", 90)`
- **命中结算可以交给引擎**：设好 `bullet_damage` / `bullet_slow` / `bullet_freeze_chance` /
  `bullet_stun_chance` / `bullet_hits` 这些字段（见「碰撞结算字段」），撞上敌人时核心会自动扣血、
  上状态、按次数消耗 —— 这种写法**一条 VM 代码都不用进**。
  想自己接管（例如命中后放范围爆炸），就用 `mod_step_enter_condition = "attack_collision"`：
  碰撞时核心已经把 `mod_collision_enemy`（撞到的敌人 id）写好，进 VM 后自己处理。
  ⚠️ 子弹必须有**有效贴图**（`sprite_index`）才有碰撞体；空占位图撞不到东西。
  ⚠️ 碰撞事件在 Step 之后跑，所以闸门是**撞到的下一帧**才放行；贴着敌人不动会每帧都撞、每帧都结算。
  不想用碰撞事件也可以自己查：`VM_GetInstancesInRange(..., "card", "brazier")` / `(..., "enemy", "")` / `VM_EnemyInRange`
- **大量同质子弹别用实例子弹**：改用屏幕弹幕管理器，不占实例、统一迭代绘制：
  - `VM_BulletScreenAdd_Ex(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度)`（18 个参数，带卡片效果位掩码）
  - `VM_BulletScreenAdd_Exs(...)`（多两个**行渐变**参数：目标行世界 y、靠拢比例 0.15）
  - `VM_HomingBulletAdd(...)`（会拐弯的追踪弹：每帧重新索敌、自转 6 度）
- **ID 与参数**：`子弹类型`（`can_hit`）= `all` / `normal` / `air` / `air_only` / `pierce` / `track` / `throw` / `rotate` / `d_fruit`；
  `伤害类型` = `normal`（有盾只打盾）/ `pierce`（盾血一起掉）/ 其它（无视护盾）
