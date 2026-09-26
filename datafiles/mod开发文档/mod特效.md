# 特效 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

特效放在**游戏目录下的 `mod/effects/`** 里：

```text
游戏目录/                       ← 运行游戏的那个文件夹
└─ mod/
   └─ effects/                  ← 特效这一类都放这里
      ├─ my_fx.json             可以就是 {}
      ├─ my_fx.bin              逻辑（由 my_fx.txt 编译出来，游戏只读这个）
      ├─ my_fx.txt              源码（建议保留）
      └─ tex/                   自带贴图（可选）
         └─ xxx.png
```

**也可以再分一层子目录归类**（只扫一层、不往下递归）：`mod/effects/xxx/my_fx.json`，贴图跟 json 同一层放 `mod/effects/xxx/tex/`；id 只看 json 文件名。

和子弹同一套路：**不进任何池，只提供逻辑**，json 可以是 `{}`（但必须有 json 才会被扫描）。
特效一般用在三处：**卡片/宝石/武器身上的光环**、**命中或死亡特效**、**纯表现（文字提示、爆炸）**。

---

## 一、怎么创建

```gml
fx = VM_CreateInstance("obj_effect_mod", x, y)
VM_SetProp(fx, "mod_type", "my_fx")          // = mod/effects/ 下的文件名
VM_SetProp(fx, "sprite_index", "spr_my_fx")
VM_SetProp(fx, "destroy_timer", 30)          // 30 帧后自动销毁
```

也可以"先写全局再创建"（Create 时就初始化）：

```gml
VM_SetProp(0, "_mod_pending_effect_id", "my_fx")
fx = VM_CreateInstance("obj_effect_mod", x, y)
VM_SetProp(0, "_mod_pending_effect_id", "")
```

---

## 二、特效实例上的内置字段

| 字段 | 初值 | 说明 |
|---|---|---|
| `mod_type` | 全局 `_mod_pending_effect_id`（默认 `""`） | 跑哪套逻辑 |
| `destroy_timer` | `-1` | `-1`=不自动销毁；`0`=立即销毁；`>0`=每帧 −1，到 0 销毁 |
| `image_speed` | `0` | 要帧动画自己设（或自己在 STEP 里推 `image_index`） |
| `x` / `y` / `sprite_index` / `image_alpha` / `image_angle` / `image_xscale` / `image_yscale` / `depth` | 普通实例属性 | 直接读写 |
| `parent_inst` / `rel_x` / `rel_y` / `frames` / `anim` | 没有 | **插件自己设的**（跟随类特效常用这套命名） |
| `mod_follow_instance` | `undefined` | 设成一个实例，本特效的 `x`/`y` **每帧贴到它身上**（跟随）；`undefined` / 目标已销毁 = 不跟随 |
| `mod_prestep_dx` / `mod_prestep_dy` | `0` / `0` | 每帧 Step **结束时**把这两个量加到 `x` / `y` 上 |
| `mod_step_enter_condition` | `""` | 什么时候进 VM：`""` 每帧 / `"mod"` 每 N 帧 / `"wait"` 倒数 |
| `mod_step_enter_var` | `0` | `"mod"` = 间隔帧数；`"wait"` = 还要等几帧 |

> 特效**没有** `vx`/`vy`、也没有出界销毁（那是子弹才有的）；位置完全由你自己控制。

**跟随与偏移的配合（注意区别）：**

```gml
// ① 固定偏移：每帧贴到主人身上，再往上偏 40 像素（x/y 每帧被跟随重置，所以偏移是固定的）
VM_SetProp(self, "mod_follow_instance", 主人实例)
VM_SetProp(self, "mod_prestep_dy", -40)

// ② 每帧位移：不设跟随 → x/y 没人重置，每帧加一次就是"匀速移动"
VM_SetProp(self, "mod_prestep_dx", 3)      // 每帧往右 3 像素
```

> ⚠️ `mod_prestep_dx` / `mod_prestep_dy` 的语义**取决于有没有跟随**：
> 设了 `mod_follow_instance` 就是**固定偏移**（和武器那两个字段一样）；没设就是**每帧位移**（等于速度）。

---

## 三、STEP 的时机与执行顺序

和子弹一样也有**延迟初始化**：

| 时机 | 发生什么 |
|---|---|
| **创建当帧** | Create 跑，`mod_type` 还是空 → 不初始化；随后你的 `VM_SetProp` 写进类型和数值 |
| **下一帧** | Step 开头发现 `mod_type` 有了 → 跑该特效的 `_OBJECT_CREATE`，紧接着**同帧**跑 `_OBJECT_STEP` |
| 用 `_mod_pending_effect_id` 提前写好再创建 | Create 里直接初始化，**当帧**就生效（宝石挂光环就是这么写的） |

每帧顺序：

```text
1. 暂停 → exit
2. 跟随目标：mod_follow_instance 有设且实例还在 → x = 目标.x; y = 目标.y
3. 延迟初始化（mod_type 有了就跑 _OBJECT_CREATE）
4. 判断这一帧进不进 VM（mod_step_enter_condition）
5. 跑特效 .bin 的 _OBJECT_STEP（被上面条件挡住时不执行）
6. 实例没了（你在 VM 里销毁了自己）→ exit
7. 自动销毁：destroy_timer == 0 → 立即销毁；> 0 每帧 −1，到 0 销毁
8. 偏移：x += mod_prestep_dx; y += mod_prestep_dy
```

### 进 VM 的时机（`mod_step_enter_condition`）

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享） | `mod_step_enter_var` = 间隔帧数 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |

> 特效没有 `hp`、也没有格子坐标，所以 `hp_change` / `cell` / 索敌那几类条件这里都没有。

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

### 鼠标三个块

`_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK` 特效也有，按实例判定
（鼠标要压在特效的贴图上）。

绘制：`_OBJECT_DRAW` 写了就跑 VM 的绘制块（**不再自动画 sprite**），没写就 `draw_self()`。

---

## 四、demo

### Demo 1：跟随光环（挂在卡片 / 宝石 / 角色身上）

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "rel_x", -15)      // 相对主人的偏移
    VM_SetProp(self, "rel_y", -5)
    VM_SetProp(self, "frames", 9)          // 贴图帧数（自己推 image_index 用）
    VM_SetProp(self, "anim", 0)
    VM_SetProp(self, "image_speed", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    p = VM_GetProp(self, "parent_inst")
    if (VM_IsUndefined(p) == 1) { VM_DestroyInstance(self) exit }
    if (VM_IsDestroyed(p) == 1) { VM_DestroyInstance(self) exit }

    // 跟着主人走
    VM_SetProp(self, "x", VM_GetProp(p, "x") + VM_GetProp(self, "rel_x"))
    VM_SetProp(self, "y", VM_GetProp(p, "y") + VM_GetProp(self, "rel_y"))

    // 自己推帧动画（image_speed = 0 时手工推进）
    a = VM_GetProp(self, "anim") + 1
    if (a >= VM_GetProp(self, "frames")) { a = 0 }
    VM_SetProp(self, "anim", a)
    VM_SetProp(self, "image_index", a)
}
```

主人侧怎么创建它（先写 `_mod_pending_effect_id`，当帧就初始化）见
[mod宝石.md](mod宝石.md) 的 Demo 3。

### Demo 2：一次性命中特效（放完就销毁）

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "image_speed", 0)
    VM_SetProp(self, "image_index", 0)
    VM_SetProp(self, "destroy_timer", 20)      // 20 帧后自动消失
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    i = VM_GetProp(self, "image_index") + 1
    if (i >= 6) { i = 5 }                      // 6 帧一次性播完，停在最后一帧
    VM_SetProp(self, "image_index", i)
    VM_SetProp(self, "image_alpha", 1 - i / 6) // 顺便淡出
}
```

创建处一般直接挂在弹幕/子弹上更省：屏幕弹幕的 `销毁对象` + `mod名字` 两个参数会在子弹消失处自动生成它。

### Demo 3：不占实例的表现（推荐做法）

纯表现、大量重复的特效，优先用**绘制槽**而不是特效实例：

```gml
// 在任意事件块的 _VM_FRAME 里：把图贴到屏幕某处
VM_SetDrawSlot(0, "spr_my_fx", 500, 300, 0.8)     // 槽位 0~63，alpha
// 不需要了就清掉
VM_SetDrawSlot(0, "", 0, 0, 1)
```

- `VM_SetDrawSlot(槽位,贴图,x,y,alpha)` = 背景层；`VM_SetDrawSlot_front(...)` = 前景层
  （都在 `_VM_FRAME` 里用，见 `help.md` 的「绘制」）
- 这条路的代价是"没有独立生命周期"，适合跟着 UI / 场景常驻的东西

---

## 五、要点

- **跟随类特效一定要判主人还在不在**（`VM_IsUndefined` / `VM_IsDestroyed`），否则主人没了光环还在原地
- **`_mod_pending_effect_id` 创建前写、创建后清**（不清的话下一个特效会被套上同一个类型）
- `destroy_timer = -1` 是**不自动销毁**，跟随类特效一般靠"主人没了就销毁"来控制，别忘了这条出路
- 大量同质表现优先用 `VM_SetDrawSlot` / 弹幕的 `销毁对象`，别堆一堆特效实例
