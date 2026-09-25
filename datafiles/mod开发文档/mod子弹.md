# 子弹 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/bullets/my_bullet.json   ← 可以就是 {}
mod/bullets/my_bullet.bin    ← 逻辑全在这里
mod/bullets/my_bullet.txt    ← 源码（建议保留）
mod/bullets/tex/...          ← 自带贴图
```

子弹**不进任何池**，只是给 `obj_bullet_mod` 提供一套同名逻辑（`mod_type` 就是文件名）。
所以 json 没有固定字段，`{}` 也行，但**必须有这个 json**，否则扫描不到。

---

## 一、怎么创建（两步）

```gml
// ① 先创建通用对象
b = VM_CreateInstance("obj_bullet_mod", x, y)
// ② 再设 mod_type（= mod/bullets/ 下的文件名，去掉 .json）
VM_SetProp(b, "mod_type", "my_bullet")
VM_SetProp(b, "sprite_index", "spr_my_bullet")
VM_SetProp(b, "vx", 8)              // ⚠️ 必须显式设速度
VM_SetProp(b, "vy", 0)
VM_SetProp(b, "damage", 60)
```

也可以"先写全局再创建"（Create 时就初始化，见下面的时机表）：

```gml
VM_SetProp(0, "_mod_pending_bullet_id", "my_bullet")
b = VM_CreateInstance("obj_bullet_mod", x, y)
```

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

---

## 三、STEP 的时机与执行顺序

### 创建当帧 vs 下一帧（重要）

`VM_CreateInstance` 只是"创建一个 `obj_bullet_mod`"，它不认 `mod_type`。所以：

| 时机 | 发生什么 |
|---|---|
| **创建当帧** | `obj_bullet_mod` 的 Create 跑，`mod_type` 还是空 → **不初始化**；随后你的 `VM_SetProp` 把 `mod_type` / 数值写进去。这帧还没有本体精灵，看不见是正常的 |
| **下一帧** | Step 开头发现 `mod_type` 有了 → **延迟初始化**：跑该子弹的 `_OBJECT_CREATE`，紧接着**同帧**跑 `_OBJECT_STEP` |
| 用 `_mod_pending_bullet_id` 提前写好再创建 | Create 里就直接初始化，**当帧**就能动 |

> 所以：在"创建前设好类型"就能当帧生效；只设 `mod_type` 也行，代价是下一帧才开始动。

### 每帧顺序（`obj_bullet_mod/Step_0.gml`）

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
| `"attack"` | **场上还有敌人才进**（`instance_number(obj_enemy_parent) > 0`） | `mod_has_enemy`（只读） |

- **子弹最常用 `"cell"`**：只有跨格那一帧进 VM，正好对上"路径碰撞"的需求
- ⚠️ `"cell"` 触发的是**进格后的第一帧** —— 格子坐标是在 `x += vx` **之前**算的，比实际越界晚一帧；
  一帧跨多格也只触发一次
- `"attack"` 是**全局判定**（只看场上还有没有敌人），不像卡片那样按区域/类型索敌 —— 子弹数量多，这样便宜

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
    VM_SetProp(self, "destroy_timer", 0 - 1)      // 不自动销毁（出界由对象处理）
    VM_SetProp(self, "image_xscale", 1.6)
    VM_SetProp(self, "image_yscale", 1.6)

    // sprite_index 是内置实例变量，永远"已定义"（没设时是 -1）→ 不能用 undefined 兜底
    spr = VM_GetProp(self, "sprite_index")
    if (VM_IsUndefined(spr) == 1) { spr = 0 - 1 }
    if (spr == 0 - 1) { VM_SetProp(self, "sprite_index", "spr_my_bullet") }

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
    VM_SetProp(self, "vy", 0 - 8)         // 先往上
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

- **`vx` / `vy` 必须显式设**：Create 里就是 0，"不设就用默认"不成立（子弹一动不动）
- **`sprite_index` 永远"已定义"**（没设时是 `-1`），判断"别人给没给贴图"要用 `== -1`，不能用 undefined
- **默认不会自己消失**（`destroy_timer = -1`）：出界会自动销毁，但命中后要自己 `VM_DestroyInstance(self)`
  或者给自己一个寿命 `VM_SetProp(self, "destroy_timer", 90)`
- **碰撞要自己写**：`obj_bullet_mod` 没有碰撞事件（原版那些 `Collision_*` 联动一个都没有），
  常用 `VM_GetInstancesInRange(..., "card", "brazier")` / `(..., "enemy", "")` / `VM_EnemyInRange`
- **大量同质子弹别用实例子弹**：改用屏幕弹幕管理器，不占实例、统一迭代绘制：
  - `VM_BulletScreenAdd_Ex(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度)`（18 个参数，带卡片效果位掩码）
  - `VM_BulletScreenAdd_Exs(...)`（多两个**行渐变**参数：目标行世界 y、靠拢比例 0.15）
  - `VM_HomingBulletAdd(...)`（会拐弯的追踪弹：每帧重新索敌、自转 6 度）
- **ID 与参数**：`子弹类型`（`can_hit`）= `all` / `normal` / `air` / `air_only` / `pierce` / `track` / `throw` / `rotate` / `d_fruit`；
  `伤害类型` = `normal`（有盾只打盾）/ `pierce`（盾血一起掉）/ 其它（无视护盾）
