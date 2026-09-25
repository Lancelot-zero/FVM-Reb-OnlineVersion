# 敌人 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/enemies/my_mouse.json / .bin / .txt / tex/
```

---

## 一、JSON 字段

| 字段 | 必填 | 默认 | 说明 |
|---|---|---|---|
| `spr` | ✅ | | 贴图名 |
| `name` | | 文件名 | 名字 |
| `hp` | | 100 | 血量（同时设 `maxhp`） |
| `shield` | | 0 | 护盾值（同时设 `shield_max_hp`） |
| `speed` | | 0.3 | 移动速度 → 实例的 `move_speed` |
| `atk` | | 10 | 攻击力 |
| `cycle` | | 36 | 攻击间隔（帧）→ 实例的 `atk_cycle` |
| `range` | | 90 | 攻击距离 → 实例的 `attack_range` |
| `ash_proof` | | false | 免疫灰烬 → 实例的 `immune_to_ash` |
| `feature` | | `"land"` | `"land"`=陆行 / `"water"`=水生；其它值＝不限行列类型 |
| `description` | | "" | 说明 |

最小例子：

```json
{
  "name": "我的老鼠",
  "spr": "spr_mouse",
  "hp": 100, "shield": 0, "speed": 0.3,
  "atk": 10, "cycle": 36, "range": 90,
  "ash_proof": false, "feature": "land"
}
```

---

## 二、敌人实例上的内置字段

mod 敌人用 `obj_enemy_mod`（**继承 `obj_enemy_parent`**，移动 / 攻击 / 死亡 / 受击这些基础行为全部复用父类）。
Create 时按上面 JSON 套用数值，所以实例上一建出来就有：

| 字段 | 来源 | 说明 |
|---|---|---|
| `enemy_id` | 全局 `_mod_pending_enemy_id` | 敌人 id（= 文件名） |
| `sprite_index` | JSON `spr` | 贴图 |
| `hp` / `maxhp` | JSON `hp` | 当前 / 最大血量（`hp <= 0` 由父类走死亡流程） |
| `shield_hp` / `shield_max_hp` | JSON `shield` | 护盾 |
| `move_speed` | JSON `speed` | 移动速度（父类用它推进 `x`） |
| `atk` | JSON `atk` | 攻击力 |
| `atk_cycle` | JSON `cycle` | 攻击间隔（帧） |
| `attack_range` | JSON `range` | 攻击距离 |
| `immune_to_ash` | JSON `ash_proof` | 免疫灰烬 |
| `feature` | JSON `feature` | 陆行 / 水生 |
| `x` / `y` / `grid_col` / `grid_row` | 父类 | 位置与所在格（每帧更新） |
| `state` | 父类 | `ENEMY_STATE`（移动 / 攻击 / 死亡…） |
| `target_plant` / `target_type` / `mouse_id` / `is_boss` | 父类 | 索敌与身份 |
| `ice_timer` / `is_frozen` / `is_stun` / `stun_timer` / `hurt_rate` | 父类 | 冰冻 / 眩晕 / 受伤状态 |
| `helmet_hp` / `helmet_max_hp` | 父类 | 头盔 |

**父类已经做好的事**：按 `move_speed` 往左推进、到卡片旁边停下攻击（走 `atk_cycle`）、
掉血 / 死亡动画 / 灰烬判定。所以 mod 敌人**基本只需要写"额外行为"**。

---

## 三、STEP 的时机与执行顺序

### 什么时候跑

- **`_OBJECT_CREATE`**：实例创建时立刻执行（刷怪时）
- **`_OBJECT_STEP`**：每帧；暂停时不跑，**且会被 `mod_step_enter_condition` 挡住**（默认 `""` 才是每帧进）
- **`_OBJECT_DRAW`**：父类先画，然后跑 VM 的绘制块（**叠加绘制**，不会替代父类的画）
- **`_OBJECT_DESTROY`**：父类处理完，再跑 VM 的 `_OBJECT_DESTROY`，最后把实例移出列表
- **`_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK`**：鼠标移入 / 移出 / 左键点在敌人身上（按实例判定，鼠标要压在敌人的贴图上）

### 每帧顺序

```text
1. 暂停 → exit
2. 记下上一帧格子坐标：prev_grid_col / prev_grid_row = 当前 grid_col / grid_row
3. event_inherited()   → 父类：移动 / 攻击 / 死亡 / 受击状态推进（并刷新 target_plant / grid_col / grid_row）
4. 判断这一帧进不进 VM（mod_step_enter_condition）
5. 跑敌人 .bin 的 _OBJECT_STEP（被上面条件挡住时不执行）
```

> 因为父类先跑，VM 里读到的是**父类推进之后**的位置/状态；想改速度就改 `move_speed`（下一帧生效），
> 想改位置直接写 `x` / `y`。

### 进 VM 的时机（`mod_step_enter_condition`）

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享，同间隔的敌人同帧进） | `mod_step_enter_var` = 间隔帧数 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |
| `"cell"` | **跨格**那一帧进（`grid_col` 或 `grid_row` 相对上一帧变了） | — |
| `"hp_change"` | **血量相对上一帧变了**就进（掉血、回血都算） | — |
| `"hp_change_mod"` | 同 `"hp_change"`，但触发一次后进 `mod_step_enter_var` 帧冷却 | `mod_step_enter_var` = 冷却帧数 |
| `"card"` | **父类索敌当前有目标才进**（`target_plant` 有效） | — |
| `"card_mod"` | 同 `"card"`，但触发一次后进 `mod_step_enter_var` 帧冷却 | `mod_step_enter_var` = 冷却帧数 |

| 字段 | 读写 | 说明 |
|---|---|---|
| `mod_step_enter_condition` | 可写 | 见上表 |
| `mod_step_enter_var` | 可写 | `"mod"` = 间隔帧数；`"wait"` = 还要等几帧；`*_mod` = 冷却帧数 |
| `mod_tick_cool` | 只读 | `*_mod` 模式下的冷却剩余帧数 |
| `mod_has_card` | 只读 | `"card"` / `"card_mod"` 模式下 1 = 父类索敌有目标 |
| `prev_grid_col` / `prev_grid_row` | 只读 | 上一帧格子坐标（`"cell"` 用） |

- `"cell"` 触发的是**进格后的第一帧**（格子坐标在父类 Step 里算，比移动晚一帧）
- `"hp_change"` 读的是父类的 `pre_hp`（写在 **End Step**，而这段 Step 先跑），所以拿到的是上一帧末的血量
- `"card"` 用的是**父类自己那套索敌**（和 "有没有卡片能打" 同一口径），不用自己再查一遍

---

## 四、demo

### Demo 1：冲刺型敌人（改速度做节奏）

父类管移动，VM 只做节奏（`dev_test/enemies/mod_dash_mouse.txt`）：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "dash_timer", 0)
    VM_SetProp(self, "move_speed", 0.9)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    t = VM_GetProp(self, "dash_timer") + 1
    if (t == 180) {                       // 每 180 帧（3 秒）冲一次
        VM_SetProp(self, "move_speed", 1.6)
    }
    if (t == 330) {                       // 冲 150 帧后回普通速度
        VM_SetProp(self, "move_speed", 0.9)
        t = 0
    }
    VM_SetProp(self, "dash_timer", t)
}
```

### Demo 2：远程 / 投掷攻击

父类的攻击是"贴身打卡片"；想远程就自己找目标、自己发子弹：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "throw_cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    r = VM_GetProp(self, "grid_row")
    c = VM_GetProp(self, "grid_col")
    // 左边这一行有没有卡片（列 0..c）
    n = VM_GetInstancesInRange("e_tgt", r, r, 0, c, "card", "")
    if (n > 0) {
        cd = VM_GetProp(self, "throw_cd") + 1
        if (cd >= 120) {
            cd = 0
            b = VM_CreateInstance("obj_bullet_mod", VM_GetProp(self, "x"), VM_GetProp(self, "y") - 20)
            VM_SetProp(b, "mod_type", "my_enemy_shot")
            VM_SetProp(b, "damage", VM_GetProp(self, "atk"))
            VM_SetProp(b, "vx", 0 - 5)          // 往左飞
            VM_SetProp(b, "vy", 0)
            VM_PlaySound("snd_throw")
        }
        VM_SetProp(self, "throw_cd", cd)
    } else {
        VM_SetProp(self, "throw_cd", 0)
    }
}
```

子弹侧照 [mod子弹.md](mod子弹.md) 写；打**卡片**用 `VM_SetProp(卡片实例, "hp", ...)` 或让子弹自己判。

### Demo 3：被打时的额外反应

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    // 血量掉到一半以下 → 换贴图 / 加速（父类负责真正的死亡）
    if (VM_GetProp(self, "hp") <= VM_GetProp(self, "maxhp") / 2) {
        VM_SetProp(self, "sprite_index", "spr_my_mouse_angry")
        VM_SetProp(self, "move_speed", 0.6)
    }
}

_OBJECT_DESTROY {
    self = VM_GetCurCard()
    // 死亡时掉一地特效（VM_GetInstancesInRange 之类的清理记得别在这里做，实例马上就没了）
    fx = VM_CreateInstance("obj_effect_mod", VM_GetProp(self, "x"), VM_GetProp(self, "y"))
    VM_SetProp(fx, "mod_type", "my_death_fx")
    VM_SetProp(fx, "destroy_timer", 30)
}
```

---

## 五、要点

- **刷敌人**：地图脚本里 `VM_SpawnEnemy("敌人id", 行, 血量)`（id 就是 `mod/enemies/` 下的文件名）；
  关卡 JSON 的 `enemy_list` 也能直接引用
- **别重复实现父类已有的东西**（移动、贴身攻击、死亡动画）；VM 里改 `move_speed` / `hp` / `state` 就行
- 敌人受伤走自己的受击事件（闪白 / 音效 / 护盾判定）；插件主动给别人伤害用
  `VM_DamageEnemy(敌人id, 伤害, 伤害类型)`，灰烬那套用 `VM_DamageEnemyAsh(...)`
- `feature` 只有 `land` / `water` 有意义（决定刷在哪种行），其它值＝不限
- 敌人有 `mod_step_enter_*` 那套进 VM 条件（`""` / `mod` / `wait` / `cell` / `hp_change` / `hp_change_mod` / `card` / `card_mod`），
  用法见上面「进 VM 的时机」；**没有**卡片那套 `mod_tick_*` / `mod_enemy_check` / `mod_step_enemy_area` 区域索敌
  （要"有卡片才动"直接用 `"card"`）
