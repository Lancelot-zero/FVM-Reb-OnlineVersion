# Mod 插件开发与分析指南

本文面向需要分析、复刻、编写和调试本 Mod 系统的智能体/开发者。

目标：让分析者能够从现有项目代码出发，判断一个功能能否做成 Mod 插件，并知道应该写哪些文件、放在哪里、如何编译和测试。

---

## 1. Mod 系统总览

当前 Mod 系统支持以下类型：

```text
mod/cards/      卡片
mod/weapons/    武器
mod/gems/       宝石
mod/attires/    时装
mod/enemies/    敌人
mod/bullets/    子弹 VM
mod/effects/    特效 VM
mod/maps/       自定义地图
```

一个 Mod 单元通常由：

```text
xxx.json
xxx.bin
可选资源文件：png / ogg / bin
```

组成。

### JSON 负责

- 名称
- 描述
- 贴图名或相对图片路径
- 属性数值
- 成长曲线
- 商店配置
- 图鉴文本
- 其他静态配置

### BIN 负责

- 创建逻辑
- 每帧逻辑
- 绘制逻辑
- 销毁逻辑
- 特殊行为

BIN 由 `.txt` 源码通过 `LabMapCompiler.exe` 编译得到。

---

## 2. 目录结构与加载入口

常见入口文件：

```text
scripts/src_mod/src_mod.gml
```

关键初始化函数：

```gml
src_mod_init()
src_mod_weapons_init()
src_mod_gems_init()
src_mod_attires_init()
src_mod_enemies_init()
src_mod_bullets_init()
src_mod_effects_init()
src_mod_maps_init()
src_mod_maps_load_dir()
```

`obj_game_init/Other_2.gml` 中根据 `global.mod_enabled` 决定是否扫描 Mod 目录。

---

## 3. VM 基础概念

Mod 插件脚本不是 GML，而是 VM 字节码源码。

### 3.1 常用 VM 块

```text
_OBJECT_CFG
_OBJECT_CREATE
_OBJECT_STEP
_OBJECT_DRAW
_OBJECT_DESTROY
```

不同对象支持的块名可能不同。写之前先看对应 `obj_*_mod` 对象执行的块名。碰撞通常要在 `_OBJECT_STEP` 里自己判断，不存在通用的 `_OBJECT_HIT` / `_OBJECT_KILL`。

**`_OBJECT_CFG` 的执行时机和别的块不一样**：它不由 `obj_*_mod` 执行，而是在 **`.bin` 加载时执行一次**
（紧跟 `_VM_CONST_INIT` 之后），执行完就从块表里删掉。所以：

- 适合放一次性配置，比如 `VM_LoadSpritePerm_Ex` 预加载永久贴图
- **不要**在里面写依赖"当前实例"的逻辑 —— 那时还没有任何实例
- 此时 `mod_dir` 已经写好，所以里面用相对文件名能正确解析到本 mod 目录

各 `obj_*_mod` 实际执行的块：

```text
obj_card_mod     _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW / _OBJECT_DESTROY
obj_weapon_mod   _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW
obj_bullet_mod   _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW / _OBJECT_DESTROY
obj_effect_mod   _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW / _OBJECT_DESTROY
obj_enemy_mod    _OBJECT_CREATE / _OBJECT_STEP / _OBJECT_DRAW / _OBJECT_DESTROY
```

### 3.2 取当前实例

```gml
self = VM_GetCurCard()
```

虽然名字叫 Card，但在多数 Mod 对象块里代表当前对象实例。

### 3.3 读写属性

```gml
VM_SetProp(self, "hp", 100)
hp = VM_GetProp(self, "hp")
```

### 3.4 创建实例

```gml
b = VM_CreateInstance("xiaolongbao_bullet", x, y)
VM_SetProp(b, "damage", 100)
```

目标对象必须已存在。

### 3.5 加载图片

单帧：

```gml
_OBJECT_CFG {
    VM_LoadSpritePerm_Ex("body.png", 1, 0, 0)
}
```

动画条带：

```gml
_OBJECT_CFG {
    VM_LoadSpritePerm_Ex("body.png", 18, 0, 0)
}
```

然后设置属性：

```gml
VM_SetProp(self, "sprite_index", "body.png")
```

### 3.6 范围敌人查询

```gml
VM_EnemyInRange(r1, r2, c1, c2, type)
```

返回 `1/0`。

### 3.7 获取范围实例

```gml
VM_GetInstancesInRange("arr", r1, r2, c1, c2, "enemy", type)
n = VM_ArraySize("arr")
for (i = 0; i < n; i++) {
    eid = VM_ArrayGet("arr", i)
}
```

### 3.8 索敌

```gml
target = VM_GetHomingTarget("air")
```

### 3.9 播放声音

```gml
VM_PlaySound("snd_shot")
```

---

## 4. 分析一个现有功能，判断能不能 Mod 化

分析任何原版卡片、武器、敌人、宝石时，按以下步骤。

### 第一步：找注册入口

搜索：

```text
register_card(
register_plant_lite(
register_weapon(
register_gem(
register_attire(
register_enemy(
```

找到它注册的：

- ID
- 对象名
- 精灵
- 属性
- 专属宝石
- 商店配置

### 第二步：找对象事件

打开对应对象目录，例如：

```text
objects/obj_xxx/
  Create_0.gml
  Step_0.gml
  Draw_0.gml
  Destroy_0.gml
  Other_*.gml
```

重点看：

- Create：初始化、依赖数据
- Step：行为、冷却、状态机
- Other：事件分发、攻击、碰撞
- Draw：自定义绘制

### 第三步：列出依赖

把功能拆成：

```text
本体
入场对象
子弹对象
特效对象
专属宝石
音效/图片
地图/界面依赖
网络依赖
```

### 第四步：判断实现层级

分为三类：

#### A. JSON 可完成

只依赖数值、贴图、商店。

例如：

```text
普通主武器
普通副武器
简单宝石
时装
```

#### B. JSON + BIN 可完成

依赖逻辑，但逻辑能用 VM 函数实现。

例如：

```text
普通子弹逻辑
范围攻击
追踪
简单 AI
简单入场特效
```

#### C. 需要新增对象或引擎支持

依赖专用对象、复杂绘制、特殊碰撞、特殊网络同步。

例如：

```text
阿拉丁神灯固定轨迹
冥王战镰 ∞ 形轨迹
专用超级武器入场对象
特殊子弹对象
```

这类通常要：

- 扩展 Mod 注册器
- 新增 `obj_*_mod`
- 或写更复杂的 BIN 逻辑

---

## 5. 编写一个卡片 Mod

目录：

```text
mod/cards/my_card.json
mod/cards/my_card.bin
```

JSON 参考：

```json
{
  "name": "我的卡片",
  "shapes": [
    {
      "shape": 0,
      "name": "形态1",
      "description": "说明",
      "sprite": "spr_small_fire",
      "plant_type": "normal",
      "feature_type": "normal",
      "target_card": "none",
      "hp": [50, 50, 50],
      "cost": [50, 50, 50],
      "atk": [0, 0, 0],
      "range": [0, 0, 0],
      "cooldown": [420, 420, 420],
      "cycle": [1500, 1500, 1500]
    }
  ]
}
```

BIN 示例：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    VM_SetProp(self, "cd", VM_GetProp(self, "cd") + 1)
}
```

---

### 5.1 卡片对象与可用块

所有 mod 卡共用 `obj_card_mod`（继承 `obj_card_parent`），**文件名即卡 id**：

```text
_OBJECT_CFG        .bin 加载时执行一次（见 3.1）
_OBJECT_CREATE     卡片被种下时
_OBJECT_STEP       每帧
_OBJECT_DESTROY    卡片被销毁时
_OBJECT_DRAW       不写则自动 draw_self()
```

`obj_card_mod` 的 Create 里是 `plant_id = global._mod_pending_card_id`，
所以卡种下时这个全局必须是当前卡的 id —— 放置逻辑（`obj_card_slot`）和
`VM_SpawnPlant` 都会负责写，一般不用自己管。

### 5.2 JSON 字段

```json
{
  "name": "卡名标题",
  "shapes": [
    {
      "shape": 0,
      "name": "形态名",
      "description": "形态描述",
      "sprite": "xxx.png",
      "plant_type": "normal",
      "feature_type": "normal",
      "target_card": "none",
      "hp":       [17 档],
      "cost":     [17 档],
      "atk":      [17 档],
      "range":    [17 档],
      "cooldown": [17 档],
      "cycle":    [17 档],
      "flame_produce": [17 档]
    }
  ],
  "skill": { "attr": "cycle", "values": [9 档] },
  "shop": { "cost": "100", "description": "商店描述" },
  "info_island": "图鉴说明文本"
}
```

⚠️ **数值数组只认 17 档**。原版很多卡是 19 档，`src_mod_stat_17()` 会把多的截掉、
少的用最后一个补齐。复刻 19 档的卡时填**前 17 个值**。

⚠️ **`sprite` 一个字段两用**：既当手牌卡面，也当放置预览（加载器里写死 `place_preview: _spr`）。
原版能分开（卡面用 icon、预览用本体），mod 卡分不开。

### 5.3 卡片实例上的属性与自毁

`obj_card_parent` / `obj_card_mod` 会设好这些，BIN 里可以直接读：

```text
plant_id  plant_type  hp / max_hp  atk  range  cost
grid_col  grid_row    x / y        depth
current_level  skill  shape  state
timer  flash_speed  idle_anim  attack_anim
```

**自毁**：VM 没有销毁函数，卡片把 `hp` 设成 0 就行 —— `obj_card_parent/Step_0.gml` 里有
`if hp <= 0 { instance_destroy() }`：

```gml
VM_SetProp(self, "hp", 0)
```

> 对比子弹：子弹用 `destroy_timer = 0`（见 9.2），卡片用 `hp = 0`，两者的销毁属性不一样。

### 5.4 卡片动画

和武器**同一套**（见 6.1）：靠父对象用 `idle_anim` / `attack_anim` / `flash_speed` / `state`
驱动 `image_index`，默认 `idle_anim = 0` 会把形象锁在第 0 帧。

```gml
VM_SetProp(self, "idle_anim", 9)
VM_SetProp(self, "attack_anim", 16)
VM_SetProp(self, "flash_speed", 5)
VM_SetProp(self, "state", 1)          // 0=待机 1=攻击
```

本体贴图要自己设 —— JSON 的 `sprite` 只管卡面和预览：

```gml
VM_SetProp(self, "sprite_index", "xxx.png")
```

### 5.5 联动：用命名数组，别依赖全局

要按"场上有几个同类卡"算加成，用**本卡 VM 的命名数组**最稳（见 14.10）：

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

// STEP 里取用
n = VM_ArrayGet("my_count", 0)
```

好处：不依赖某个全局变量存在（见 14.13），作用域也正好是"这张卡的所有实例"。

### 5.6 「上一张种下的卡」怎么拿

mod 系统里的**复制类卡**（梵天 / 百变蛇）用的机制：

```gml
// mod_on_card_placed(卡id, 形态)：由 obj_mod_battle_manager 在放置后调用
global.last_placed_card_id    = 卡id;
global.last_placed_card_shape = 形态;
```

黑名单 `["brahma", "ice_cream", "magic_chicken", "baibianshe"]` —— 这几张**不参与记录**，
所以复制类卡不会互相复制。

复刻这类卡时，在 `_OBJECT_CREATE` 里读（那时记录的还是上一张）：

```gml
tg = VM_GetProp(0, "last_placed_card_id")     // 实例 id 传 0 = 读全局
```

⚠️ **黑名单挡不住你自己。** 你新做的卡不在那 4 个里，放完第一张全局就变成你的 id 了，
**再放第二张 target 就指向自己** → 生成时会出问题（见 14.11）。必须把自家 id 也排除。

⚠️ 这个机制依赖 `obj_mod_battle_manager` 和那两个全局，**不一定在你手上的构建里**（见 14.13）。

---

## 6. 编写一个武器 Mod

目录：

```text
mod/weapons/my_gun.json
mod/weapons/my_gun.bin
```

JSON：

```json
{
  "name": "我的枪",
  "description": "发射子弹",
  "sprite": "spr_long_bao_gun",
  "icon": "spr_long_bao_gun_icon",
  "slot": "main_weapon",
  "atk": 10,
  "cycle": 78,
  "atk_impact": [12, 14, 16, 18, 20],
  "shop": { "cost": "10000", "description": "商店说明" }
}
```

BIN：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    r = VM_GetProp(self, "grid_row")
    c = VM_GetProp(self, "grid_col")

    if (VM_EnemyInRange(r, r, c, 15, "") == 1) {
        cd = VM_GetProp(self, "cd") + 1
        if (cd >= VM_GetProp(self, "cycle")) {
            cd = 0
            b = VM_CreateInstance("xiaolongbao_bullet", VM_GetProp(self, "x"), VM_GetProp(self, "y"))
            VM_SetProp(b, "damage", VM_GetProp(self, "atk"))
            VM_SetProp(b, "row", r)
        }
        VM_SetProp(self, "cd", cd)
    } else {
        VM_SetProp(self, "cd", 0)
    }
}
```

超级武器使用：

```json
"slot": "super_weapon"
```

无子弹版超级武器：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "parent_player", "")
    VM_SetProp(self, "x", 400)
    VM_SetProp(self, "y", 400)
    VM_SetProp(self, "cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    VM_SetProp(self, "x", 400)
    VM_SetProp(self, "y", 400)
    VM_SetProp(self, "cd", VM_GetProp(self, "cd") + 1)
}
```

### 6.1 武器动画：必须自己喂这四个属性

`obj_weapon_mod` 的动画是**它自己每帧驱动 `image_index`** 的（不是靠 `image_speed`），
驱动参数是实例上的四个属性。它们的默认值是 `idle_anim = 0` / `attack_anim = 0`，
**会让形象死死卡在第 0 帧 —— 表现就是"形象不会动"**：

| 属性 | 含义 |
|---|---|
| `idle_anim` | 待机帧数，待机循环在 `0 .. idle_anim` |
| `attack_anim` | 攻击帧数，攻击循环在 `idle_anim+1 .. idle_anim+attack_anim` |
| `flash_speed` | 几帧走一格（默认 5） |
| `state` | `0` = `CARD_STATE.IDLE` 待机，`1` = `CARD_STATE.ATTACK` 攻击 |

```gml
// 例：待机 20 帧 + 攻击 16 帧的镰刀（两者相加 = 精灵总帧数 36）
VM_SetProp(self, "idle_anim", 20)
VM_SetProp(self, "attack_anim", 16)
VM_SetProp(self, "flash_speed", 5)
VM_SetProp(self, "state", 1)     // 切攻击动画；不设就永远播待机
```

三个要点：

- `idle_anim + attack_anim` 应当等于（或小于）精灵总帧数，超出去会画到别人的帧
- **待机 / 攻击不会自动切换**，要靠自己设 `state`
- "先播一段入场特效再切本体"这种效果，用自己的计数器到点改 `sprite_index` 和这三个属性即可

### 6.2 武器实例上现成有哪些属性

`obj_weapon_mod` 的 Create 会设好下面这些，BIN 里可以直接读：

```text
weapon_id  weapon_info  atk  cycle
image_xscale  image_yscale  image_speed
parent_player  grid_col  grid_row
timer  flash_speed  idle_anim  attack_anim  state
```

**JSON 里的其它自定义字段不会挂到实例上**（详见 14.9），要用的值请在 `_OBJECT_CREATE` 里自己设。

### 6.3 副武器（盾牌）没有 VM

副武器和主 / 超武走的是两条完全不同的路：

| | 主武器 / 超级武器 | 副武器（盾牌） |
|---|---|---|
| 注册时 `obj` | `obj_weapon_mod` | `obj_player_shield` |
| 谁创建实例 | 放置逻辑 | **角色创建时直接建** |
| 是否挂 mod VM | 是 | **否** |
| `_OBJECT_CREATE` / `_OBJECT_STEP` / `_OBJECT_DRAW` | 会执行 | **不会执行** |

所以副武器的 `.bin` 里**写逻辑块是没用的** —— 没有任何代码去执行它们。
唯一仍然有效的是 `_OBJECT_CFG`：它在 **`.bin` 加载时**由 `src_mod_weapons_init` 执行一次，与实例无关。

**能给的只有 JSON 数据。** 盾牌实例是角色创建时顺手建的，数值直接读注册数据：

```gml
// obj_player_character/Mouse_53.gml
if (global.save_data.equipped_items.secondary_weapon.id != "" && global._VM_ban_shield == false) {
    var s_inst = instance_create_depth(x, y, depth, obj_player_shield)
    s_inst.parent_player = id
    var main_info = get_weapon_info(global.save_data.equipped_items.secondary_weapon.id)
    hp += main_info.hp_increase
    max_hp += main_info.hp_increase
}
```

逻辑全在内置 `obj_player_shield` 里（内置宝石：produce / slow_down / bleed / guard / strength），
mod 侧唯一有意义的数据是 **`hp_increase`**（还有 `name` / `description` / `shop`）。

**贴图不受 mod 控制。** 盾牌实例的 `sprite_index` 由 `obj_player_shield` 自己决定：有的版本会读
`get_weapon_info(装备的副武器 id).sprite` 赋给它（这样 JSON 里的 `sprite` 才生效），
有的版本根本不设（那就永远看不见）。因为盾牌没有 VM，**mod 层没有任何入口能给它设 `sprite_index`**，
这一条只能看游戏本体怎么实现。

一个能跑的最小副武器 —— `.bin` 里只有 CFG，没有逻辑块：

```gml
// mod/weapons/master_shield.txt
_OBJECT_CFG {
    VM_LoadSpritePerm_Ex("spr_master_shield_0.png", 1, 31, 53)
    VM_LoadSpritePerm_Ex("spr_master_shield_icon_0.png", 1, 35, 35)
}
```

```json
// mod/weapons/master_shield.json
{
  "name": "主宰之盾",
  "description": "主宰之盾：增加800生命值",
  "sprite": "spr_master_shield_0.png",
  "icon": "spr_master_shield_icon_0.png",
  "slot": "secondary_weapon",
  "hp_increase": 800,
  "shop": { "cost": "150000", "description": "主宰之盾：增加800生命值" }
}
```
---

## 7. 编写一个宝石 Mod

目录：

```text
mod/gems/my_gem.json
mod/gems/my_gem.bin
```

JSON：

```json
{
  "name": "我的宝石",
  "description": "效果说明",
  "icon": "spr_attack_gem_icon",
  "slot": "main_weapon",
  "max_level": 15,
  "cooldown": [60, 58, 56, 54, 52]
}
```

BIN 负责效果逻辑。

### 7.1 宝石对象是「按槽位遍历装备栏」创建的

宝石图标不是自动挂上去的，由 `obj_player_character/Mouse_53.gml` 显式创建。
**这段循环只遍历主武器**：

| 位置 | 条件 | 遍历的是谁 |
|---|---|---|
| `Mouse_53.gml:33` | `global.network.mode == "client"` | `main_weapon.gems` |
| `Mouse_53.gml:64` | 本地 / 服务端 | `main_weapon.gems` |

```gml
var gem_list = global.save_data.equipped_items.main_weapon.gems
for (var i = 0; i < array_length(gem_list); i++) {
    var gem_id = gem_list[i]
    if (array_get_index(global.banned_gems_online, gem_id) != -1) continue;
    var gem_info = get_gem_info(gem_id)
    if (gem_info.obj != noone) {
        global._mod_pending_gem_id = gem_id;
        instance_create_depth(390, 213 + gem_index * 80, -500, gem_info.obj)
        gem_index++
    }
}
```

四个要点：

- **只跟主武器有关**：`secondary_weapon.gems` / `super_weapon.gems` **没有任何代码去遍历**，
  所以给盾牌 / 超武镶宝石，图标栏里**什么都不会出现**
- **创建成什么对象**取决于宝石自己的 `obj` 字段，跟武器类型无关
- **位置是固定 UI 坐标** `(390, 213 + gem_index*80, -500)`，不挂在武器身上
- 在 `global.banned_gems_online` 里的宝石会被跳过（联机禁用）

### 7.2 宝石对象怎么知道自己是哪颗

靠全局变量传，和武器的 `_mod_pending_weapon_id` 一个套路：

```gml
global._mod_pending_gem_id = gem_id;      // 先写全局
instance_create_depth(..., gem_info.obj)  // 再创建
```

通用 mod 宝石对象 `obj_gem_mod` 的 Create 里读回来，顺便把图标设上：

```gml
gem_id = variable_global_exists("_mod_pending_gem_id") ? global._mod_pending_gem_id : ""
gem_info = get_gem_info(gem_id)
...
sprite_index = gem_info.icon            // ← 图标是在这里设的
```

### 7.3 passive：纯被动宝石不创建实体

`src_mod_register_gem` 里：

```gml
obj: (_c[$ "passive"] == true) ? noone : obj_gem_mod,
```

JSON 写 `"passive": true` → `obj = noone` → **不创建对象**（纯数值 / 被动宝石）。
不写或 `false` → 用 `obj_gem_mod`。

另外，mod 宝石 JSON 里除 `name` / `description` / `icon` / `slot` / `shop` / `passive`
之外的字段会**原样透传**进宝石数据（`max_level` / `first_cooldown` / `cooldown` 数组等），
`obj_gem_mod` 就靠 `gem_info.cooldown` 取冷却值。

> ⚠️ 这点跟武器**不一样**：mod 武器 JSON 的自定义字段**不会**挂到实例上（见 14.9），
> 宝石这边是原样透传的。

### 7.4 宝石图标有两道「静默失败」

| 关卡 | 失败表现 |
|---|---|
| 槽位列表没被遍历（盾牌 / 超武的宝石） | 图标栏里**完全不出现** |
| `icon` 名字解析不到 | 出现了但**是空白的**（`get_load_sprite` 兜底返回占位图，不报错） |

所以 mod 宝石的 `icon` 建议按 §10 方式 B 自带 PNG，并在 `_OBJECT_CFG` 里
`VM_LoadSpritePerm_Ex` 预加载 —— 注册时 `get_load_sprite` 才能命中永久缓存。
---

## 8. 编写一个敌人 Mod

目录：

```text
mod/enemies/my_mouse.json
mod/enemies/my_mouse.bin
```

JSON：

```json
{
  "name": "我的老鼠",
  "description": "说明",
  "spr": "spr_mouse",
  "hp": 100,
  "shield": 0,
  "speed": 0.3,
  "atk": 10,
  "cycle": 36,
  "range": 90,
  "ash_proof": false,
  "feature": "land"
}
```

BIN 可以写：

```text
_OBJECT_CREATE
_OBJECT_STEP
_OBJECT_DESTROY
```

---

## 9. 编写一个子弹 Mod

目录：

```text
mod/bullets/my_bullet.json
mod/bullets/my_bullet.bin
mod/bullets/my_bullet.txt
```

`mod/bullets/` 下**每个 `.json` 都会被扫描**，id = 文件名去掉 `.json`，同名 `.bin` 提供逻辑。
json 内容可以是空对象 `{}`，但**扫描入口必须是 json**（没有 json 就不会被加载）。

### 9.1 通用子弹对象 obj_bullet_mod

子弹统一用内置的 `obj_bullet_mod` 创建，再用 `mod_type` 指定跑哪套逻辑。

**必须两步走：先创建实例，再设名称。** `VM_CreateInstance` 只是个纯对象创建器，它不认 mod 子弹的名字，内部就是 `asset_get_index(obj_name)`，所以第一步只能传 `obj_bullet_mod`：

```gml
// ① 先创建通用子弹对象
b = VM_CreateInstance("obj_bullet_mod", x, y)
// ② 再设名称，告诉它跑哪套逻辑（名字 = mod/bullets/ 下的文件名，去掉 .json）
VM_SetProp(b, "mod_type", "my_bullet")
VM_SetProp(b, "damage", 100)
```

`mod_type` 是**创建之后**才设置的：obj_bullet_mod 的 Create 执行时 `mod_type` 还是空串，
所以真正的 `_OBJECT_CREATE` 不会在创建当帧跑 —— 它会在 Create / Step / Draw 里做"延迟初始化"，
读到 `mod_type` 后再补执行。时序如下：

| 时机 | 发生什么 |
|---|---|
| 创建当帧 | obj_bullet_mod 的 Create 跑，`mod_type` 为空 → 不初始化；随后 `VM_SetProp` 把名称和数值写进去。这帧还没有本体精灵，看不见是正常的 |
| 下一帧 | 延迟初始化命中 → 执行该子弹的 `_OBJECT_CREATE`，紧接着**同帧**执行 `_OBJECT_STEP` 开始动 |

所以在创建后立刻 `VM_SetProp(b, "damage", ...)` 是安全的 —— 等 `_OBJECT_STEP` 跑起来时早就设好了。

两个补充：

- 对象名**不写 `obj_` 前缀也可以**，`VM_CreateInstance` 会自动补，`"bullet_mod"` 与 `"obj_bullet_mod"` 等价
- 对象不存在时它会打印 `[VM_CreateInstance] 对象不存在: xxx` 并返回 `-1`，控制台可直接看到

> 想省掉那一帧延迟，可以在创建**之前**先写全局：`VM_SetProp(0, "_mod_pending_bullet_id", "my_bullet")`
> —— `VM_SetProp` 的实例 id 传 `0` 表示写全局变量。这样 Create 一进去就能拿到类型，当帧就初始化。

### 9.2 销毁：destroy_timer

mod 子弹**没有销毁函数**（VM 里没有 `VM_Destroy` 之类的接口），销毁靠实例上的 `destroy_timer` 属性：

| 值 | 行为 |
|---|---|
| `-1` | 不自动销毁（默认） |
| `0` | **立即销毁**（当帧 Step 结束后） |
| `>0` | 每帧减 1，减到 0 时销毁 |

```gml
// 飞到终点就销毁
VM_SetProp(self, "destroy_timer", 0)
exit
```

### 9.3 可用块

```text
_OBJECT_CREATE
_OBJECT_STEP
_OBJECT_DESTROY
_OBJECT_DRAW        // 不写则自动 draw_self() 画本体精灵
```

### 9.4 子弹身上的属性

obj_bullet_mod 自带：

| 属性 | 说明 |
|---|---|
| `mod_type` | 要执行的子弹类型名（= `mod/bullets/` 下的文件名） |
| `destroy_timer` | 见 9.2 |
| `grid_col` / `grid_row` | 当前所在格子，每帧 Step 开头自动更新 |
| `prev_grid_col` / `prev_grid_row` | 上一帧所在格子 |

`x` / `y` / `sprite_index` / `image_alpha` / `image_speed` 等是普通实例属性，直接读写。
注意 obj_bullet_mod 的 Create 里 `image_speed = 0`，要动的话自己设。

### 9.5 碰撞要自己写

VM 里没有通用的子弹碰撞，要在 `_OBJECT_STEP` 里自己判定。常用套路：

```gml
// 1. 取子弹所在格子
bc = VM_GetProp(self, "grid_col")
br = VM_GetProp(self, "grid_row")

// 2. 取该格上的敌人（第一个参数是收结果的数组名）
n = VM_GetInstancesInRange("cell", br, br, bc, bc, "enemy", "")

// 3. 逐个算 x/y 距离，命中就扣血
k = 0
while (k < n) {
    e = VM_ArrayGet("cell", k)
    if (VM_GetProp(e, "hp") > 0) {
        VM_SetProp(e, "hp", VM_GetProp(e, "hp") - VM_GetProp(self, "damage"))
    }
    k = k + 1
}
```

伤害就是直接改敌人 `hp`（obj_enemy_parent 里 `hp <= 0` 会进入死亡）。
这样会**跳过护盾 / 护甲判定**，也不会触发 `_VM_ENEMY_DAMAGED` 钩子。

### 9.6 VM 没有数学函数

只有 `VM_Floor` / `VM_Ceil`，**没有 `sqrt` / `sin` / `cos` / `pow` / `abs` / `min` / `max`**。
需要曲线轨迹的话只有两条路：

- 离线算好存进 VM 数组，运行时 `VM_Floor` + 线性插值（推荐，零运行时开销）
- 用 `while` 自己实现（牛顿迭代算 `sqrt`、泰勒级数算 `sin`）

⚠️ 编译器不支持负的浮点字面量，详见 **14.8**。

---

### 9.7 敌人类型过滤：子弹能打哪些类型

`VM_GetInstancesInRange` 的类型参数是**按敌人的 `target_type` 筛**，合法值就是敌人身上那几种：
`normal` / `air` / `dance` / `obstacle` / `diver` / `underground`。传 `""` 或 `"all"` = 不筛，全打。

原版子弹不是直接写死类型，而是先给自己定一个 `target_type`，再用
`get_hittable_enemy_types(子弹类型)` 换算成可打列表：

| 子弹类型 | 可打敌人类型 |
|---|---|
| `all` / `rotate` / `d_fruit` | normal, diver, air, dance, obstacle, underground（全 6） |
| **`pierce`** | **normal, dance, obstacle**（3） |
| `normal` | normal, obstacle |
| `air` | normal, air |
| `air_only` | air |
| `track` | normal, diver, air |
| `throw` | normal, diver |

⚠️ **一次只能筛一个类型。** `get_hittable_enemy_types` 返回的是一串，而 VM 的类型参数只接受
**一个**字符串 —— 照原版行为就得**对每个类型各调一次** `VM_GetInstancesInRange`（各用一个数组名）：

```gml
n = VM_GetInstancesInRange("c1", br, br, bc, bc, "enemy", "normal")
... 循环扣血 ...
n = VM_GetInstancesInRange("c2", br, br, bc, bc, "enemy", "dance")
... 循环扣血 ...
n = VM_GetInstancesInRange("c3", br, br, bc, bc, "enemy", "obstacle")
... 循环扣血 ...
```

3 个类型 = 3 次调用，是**性能代价**（见 §15）。

> 顺带：`VM_EnemyInRange` 的类型名是**另一套**（走 `global.has_enemy_*` 前缀和数组），
> 名字像但不是一回事。它的合法值是 `normal` / `obstacle` / `diver` / `air` / `dance` / `underground`。

## 10. 编写地图 Mod

目录：

```text
mod/maps/<map_id>/
  world.json
  share/
  level_1/
    level.json
    level.bin
    xxx.png
    xxx.ogg
```

`world.json`：

```json
{
  "id": "my_island",
  "name": "我的大地图",
  "icon": "spr_laboratory_icon",
  "share": "share",
  "levels": [
    {
      "name": "第一关",
      "button_x": 400,
      "button_y": 400,
      "stage_json": "level_1/level.json"
    }
  ]
}
```

进入流程：

```text
复制到 laboratory/____mod_map____/
→ CustomStage
→ StageDetail.on_create_room()
```

---

## 11. 图片资源策略

### 方式 A：引用内置精灵

```json
"sprite": "spr_long_bao_gun"
```

前提：目标项目里必须有这个精灵资源。

### 方式 B：Mod 自带 PNG

把 PNG 放进 Mod 目录，然后在 BIN 里加载：

```gml
_OBJECT_CFG {
    VM_LoadSpritePerm_Ex("body.png", 1, 0, 0)
}
```

创建时：

```gml
VM_SetProp(self, "sprite_index", "body.png")
```

注意：

- `VM_LoadSpritePerm_Ex` 是永久缓存
- 第 2 个参数是帧数
- 后续可以替换占位图，不需要改逻辑
- 注册 JSON 里的 `sprite`/`icon` 可能在 BIN 执行前就需要使用；如果加载器不支持相对 PNG，可以先填一个已有精灵名作为占位，BIN 里再覆盖实际 `sprite_index`

---

### 11.1 get_load_sprite 找不到时给空白图，不报错

注册时 JSON 里的 `sprite` / `icon` 都走 `get_load_sprite(名字)`，查找顺序：

```text
① asset_get_index(名字)         引擎内置资源
② global._VM_sprite_temp_cache  VM 临时缓存（VM_LoadSprite / VM_LoadSpriteFrames）
③ global._VM_sprite_cache       VM 永久缓存（VM_LoadSpritePerm_Ex）
④ global._sprite_cache          异步文件加载
⑤ 都找不到 → 拿 _ph_empty.png 加一张空白占位图返回
```

**⑤ 是关键：找不到不报错，只给一张空白图。** 表现是"卡/武器/宝石在界面上是白的"，
而不是崩溃 —— 又一处静默失败（对比 14.9）。

所以自带 PNG 的 mod，`icon` / `sprite` 写的名字必须和 `VM_LoadSpritePerm_Ex` 里传的**完全一致**，
而且那个块要在**注册之前**跑过 —— `_OBJECT_CFG` 就是干这个的（加载时执行，见 3.1）。

---

## 12. 分析和复刻的实际流程

写一个功能前，先输出一份分析表：

```text
功能名：
原对象：
注册函数：
依赖精灵：
依赖对象：
依赖宝石：
依赖音效：
是否涉及子弹：
是否涉及网络：
可否 JSON 完成：
可否 BIN 完成：
是否需要新增对象：
```

然后按顺序：

1. 找注册代码
2. 找 Create/Step/Other
3. 找子弹/特效对象
4. 列出所有属性
5. 判断能否纯 Mod
6. 写 JSON
7. 写 BIN
8. 编译
9. 测试
10. 记录哪些行为暂未实现

---

### 12.1 把原版 GML 翻译成 VM 的常用对照

复刻 mod 卡/子弹时，原版写的都是 GML，很多函数 VM 里没有。常用替代：

| 原版 GML | VM 里怎么写 |
|---|---|
| `instance_create_depth(x, y, d, obj_xxx)` | `VM_CreateInstance("obj_xxx", x, y)`（对象不存在返回 -1） |
| `deck_get_card_data()` + `instance_create_depth` 生成卡 | `VM_SpawnPlant(卡名, 列, 行, 外形, 星级, 技能)`（`-1` = 用默认） |
| `instance_destroy()` 自毁（子弹） | `VM_SetProp(self, "destroy_timer", 0)` |
| `instance_destroy()` 自毁（卡片） | `VM_SetProp(self, "hp", 0)`（`obj_card_parent` 里 `hp<=0` 即销毁） |
| `global.xxx` 读 | `VM_GetProp(0, "xxx")`（读不到返回 `undefined`，要兜底） |
| `global.xxx` 写 | `VM_SetProp(0, "xxx", 值)` |
| 用 `global.xxx` 做计数 | **建议换成本 VM 的命名数组**，不依赖那个全局是否存在 |
| `can_place_at_position(...)` 挑空格 | `VM_GetPlantCountAt(列, 行, "all") < 1` |
| `with (obj_enemy_parent) { ... }` 遍历敌人 | `VM_GetInstancesInRange(...)` 收进数组再遍历；只问"有没有"用 `VM_EnemyInRange` |
| `get_hittable_enemy_types(子弹类型)` | VM 一次只筛一个类型，只能按类型各调一次（见 9.7） |
| `bbox` 真实碰撞 | 中心距矩形近似 / 同格判定（见 9.5） |
| `sprite_get_number(sprite_index)` 拿帧数 | **VM 里拿不到**，帧数/原点要事先量好（见 14.5、§10 方式 B） |
| `sound_play(...)` | `VM_PlaySound("snd_xxx")`（只能用 §内置音效表里的名字） |
| `event_user(n)` | 没有通用调用，把逻辑内联 |
| `get_world_position_from_grid(row, col)` | 拿不到，固定像素坐标代替 |
| `get_gem_index("xxx_gem")` / `get_gem_level(...)` | 拿不到宝石状态，联动只能自己记 |

**找不到等价物时**：先看它是"挑格子/收集实例/生成实例"的哪一类，
多半能用 `VM_GetPlantCountAt` / `VM_GetInstancesInRange` / `VM_SpawnPlant` 拼出来。

## 13. 编译 BIN

Mod 源码先写 `.txt`：

```text
mod/weapons/my_gun.txt
```

用 `LabMapCompiler.exe` 编译成：

```text
mod/weapons/my_gun.bin
```

如果编译器参数不明，观察现有 Mod 的 `.txt` 和 `.bin` 配对，保持同名。

一个 `.txt` 文件对应一个 `.bin`。

---

## 14. 常见坑

### 14.1 同名注册覆盖

例如 `master_shield` 可能被注册两次：

```text
第一次 hp_increase = 800
第二次 hp_increase = 700
```

如果注册函数是 `ds_map_add`，第二次会失败；如果是 replace，则第二次生效。

分析时一定要确认注册函数是否允许覆盖。

### 14.2 超级武器会跟随玩家

`obj_weapon_mod` 会尝试跟随 `parent_player`。  
无子弹固定超武要写：

```gml
VM_SetProp(self, "parent_player", "")
```

否则会被拖到玩家身上。

### 14.3 本地 PNG 和内置精灵名混用

```text
spr_xxx      = 内置精灵
xxx.png      = Mod 自带图片
```

不要混成 `spr_xxx.png`。

### 14.4 VM_GetCurCard 名字有误导

虽然叫 `Card`，但多数 Mod 对象都用它拿当前实例。

### 14.5 帧数

`VM_LoadSpritePerm_Ex("xxx.png", N, ox, oy)`

这里的 `N` 是动画帧数，不是图片宽度。  
如果是单帧图，写 `1`。

### 14.6 路径

Mod 地图中的相对路径：

```json
"stage_json": "level_1/level.json"
```

资源同目录用：

```json
"map_sprite": "./sprite.png"
```

### 14.7 兼容性

复刻前必须确认目标项目里有哪些：

```text
对象
精灵
音效
注册表
VM 函数
```

没有的东西不能只靠 JSON 写出来。

---

### 14.8 编译器不支持负的浮点字面量

`LabMapCompiler.exe` 处理负的浮点字面量有 bug，**整个文件直接编译失败**：

```text
Line 44: internal: literal pool miss 'f:-1070049159'
```

实测对照：

| 写法 | 结果 |
|---|---|
| `VM_ArrayADD("t", 2.8804)` | OK |
| `VM_ArrayADD("t", -2.8804)` | **失败** |
| `VM_ArrayADD("t", -1.5)` | **失败** |
| `VM_ArrayADD("t", -0.5)` | **失败** |
| `VM_SetProp(self, "x", -500)` | OK（**负整数**没问题） |
| `VM_ArrayADD("t", 0 - 2.8804)` | OK |

绕过办法是写成 `0 - X`：

```gml
VM_ArrayADD("t", 0 - 2.8804)    // 等价于 -2.8804
```

`0.0000` / `-0.0000` 这类浮点零也会踩到同一个错，直接写整数 `0`。

⚠️ 批量生成数据表时特别容易中招：**表里只要有一个负浮点，整个文件都编译不过**，
而报错行号指向的是表中间某一行，不容易一眼看出规律。

---

### 14.9 不要读自己没设过的属性

**这是最阴的一个坑：不报错，只是静默失效。**

实例上的属性有三种来源，可靠性完全不同：

| 来源 | 例子 | 能不能读 |
|---|---|---|
| 对象 Create 里设的 | `atk` `cycle` `state` `grid_col` `grid_row` `timer` | ✅ 能 |
| 自己在 `_OBJECT_CREATE` 里 `VM_SetProp` 设的 | 自己命名的 `cd` / `phase` / `shot_count` | ✅ 能 |
| **JSON 里的自定义字段** | `bullet_amount` `hit_range` | ❌ **不能，不会挂到实例上** |

典型翻车现场：JSON 里写了 `"bullet_amount": 1`，BIN 里读它：

```gml
n = VM_GetProp(self, "bullet_amount")   // → undefined
if (n < 1) { n = 1 }                    // undefined < 1 不成立，n 还是 undefined
...
if (fidx < n) { 开火 }                   // fidx < undefined 不成立 → 一发都不发
```

**全都不报错，就是没子弹。** 同理 `VM_GetProp` 的兜底写法（`if (x < 1) { x = 默认值 }`）
也救不了 —— 它只在属性**存在但值不对**时有用，属性根本不存在时读到的是 `undefined`，
比较运算不会按预期成立。

**正确写法：先在 `_OBJECT_CREATE` 里自己设一份，再读。**

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    // 先设默认值，这样后面读才是安全的
    VM_SetProp(self, "shot_count", 1)
    VM_SetProp(self, "hit_range", 60)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    n = VM_GetProp(self, "shot_count")   // 现在一定读得到
    ...
}
```

**为什么 JSON 字段不上实例？** 注册器（`src_mod_register_weapon` 等）读 JSON 时只挑固定字段
（`sprite`/`icon`/`slot`/`atk`/`cycle`/`atk_impact`/`cycle_impact`/`hp_increase`/`shop`）转成注册数据，
`obj_weapon_mod` 的 Create 也只设 `weapon_id`/`weapon_info`/`atk`/`cycle`/`grid_col`/`grid_row`/
`timer`/`flash_speed`/`idle_anim`/`attack_anim`/`state` 这些。JSON 里的**其它键一律被忽略**。

排查手段：在 `_OBJECT_CREATE` 里 `VM_ShellPrint` 打一行，能看到就说明 CREATE 跑了；
武器侧 `VM_ShellPrint("fire b=", b)` 打 `VM_CreateInstance` 的返回值，`-1` 表示对象不存在。

---
### 14.10 命名数组是「整个 mod 单元」共享的，不是实例级

VM 的命名数组存在 **VM 上**，不是实例上 —— 源码里所有数组操作都是这个形态：

```gml
function VM_ArraySet(name_addr, index_addr, val_addr) {
    var _name = vm_read_mem(global.__vm, name_addr);
    var _vm = global.__vm;
    if (!ds_map_exists(_vm.arrays, _name)) ds_map_add(_vm.arrays, _name, []);
    ...
}
```

`global.__vm` 是**当前正在执行的那个 mod 单元的虚拟机**（一个 `.bin` 一个）。所以：

- 可见范围是「这一个 mod 单元」，**同类对象的所有实例共享同一份**
- 进房间时重置
- **没有实例级数组**；而且绕路也堵死了 —— 数组名是运行时从 VM 内存读的字符串，
  而 VM 没有任何字符串拼接 / 子串函数，拼不出 `"hits_" + self` 这种动态名字

#### 坑：共享表 + 实例 id 回收 = 静默错判

GameMaker **会回收实例 id**。如果把 `(实例id, 目标id)` 记在共享表里做去重：

```gml
// 子弹 A 打中敌人，记下
VM_ArrayADD("hits", self)
VM_ArrayADD("hits", e)
// 子弹 A 销毁后 id 被后来的子弹 B 复用
// → B 查表看到 (自己的id, 该敌人) 的旧记录 → 以为打过 → 直接跳过伤害
```

表现是「**打中不掉血**」，而且越到后期越频繁。同时表只增不减，线性查找也会越来越慢。

#### 两个可行做法

**① 共享表 + 台账（用完就清）**

```gml
// _OBJECT_CREATE：进场登记
VM_ArraySet("live", 0, VM_ArrayGet("live", 0) + 1)
if (VM_ArrayGet("live", 0) < 2) { VM_ArrayClear("hits") }

// _OBJECT_DESTROY：最后一颗死掉就清空整张表
VM_ArraySet("live", 0, VM_ArrayGet("live", 0) - 1)
if (VM_ArrayGet("live", 0) < 1) { VM_ArrayClear("hits") }
```

表里只会有"当前还活着"的实例的记录，而活着的实例 id 必然唯一 → id 回收问题消失，表也不再无限增长。

**② 用编号实例属性当定长数组**

实例属性是**真·实例级**的，可以当固定长度数组用：

```gml
// _OBJECT_CREATE 里必须先把槽位设好，否则后面读不安全（见 14.9）
VM_SetProp(self, "hit_n", 0)
VM_SetProp(self, "hit_0", 0)
VM_SetProp(self, "hit_1", 0)
```

- ✅ 真正的实例级存储，天然不受 id 回收影响，也不需要清理
- ❌ 属性名不能动态拼，槽位只能**展开成一串 if/elif 链**，代码会长
- ❌ 槽位有上限，超出只能放弃去重或不管

**取舍**：目标少、生命周期短（子弹这类）用 ① 就够；目标多、要长期记录再用 ②。

---

### 14.11 VM_SpawnPlant 遇到卡组里没有的卡会崩

`VM_SpawnPlant(卡名, 列, 行, 外形, 星级, 技能)` 内部：

```gml
var _card_data = deck_get_card_data(card_id, shape);
if (is_undefined(_card_data)) return -1;      // ← 只判了 undefined
var _obj = _card_data[? "obj"];
```

而 `deck_get_card_data` **遍历的是 `global.player_deck`**，找不到时返回的是 **`noone`**：

```gml
return noone;
```

于是 `noone[? "obj"]` **直接报错**。

**什么时候会撞上**：要生成的卡不在玩家卡组里。最容易撞的是"复制类卡复制到自己" ——
卡明明在场上，但按卡名去卡组里找不一定找得到（见 5.6）。

**结论**：调 `VM_SpawnPlant` 前，自己先把不该生成的目标排除掉。

### 14.12 VM_Execute 会吞掉块内的错误

```gml
function VM_Execute(vm, buf, name) {
    if (!buffer_exists(buf)) return 0;
    try {
        ...执行字节码...
```

`_OBJECT_CREATE` / `_OBJECT_STEP` 里**只要有一句抛错，后面的语句全部不执行**，
而且外面只会看到"某个属性是 undefined"，看不到真正的出错位置。

**排查手段：在块里逐句打点。**

```gml
_OBJECT_CREATE {
    VM_ShellPrint("cg CREATE 0 enter")
    self = VM_GetCurCard()
    VM_ShellPrint("cg CREATE 1 got self")
    ...每一步后面都打一句...
}
```

**最后打出来的是几号，问题就在它的下一句。** 反过来，第一句就打不出来 = 这个块压根没被调用。

### 14.13 目标构建里可能没有你要的 VM 函数 / 全局

同一份源码有两条线，**要合并起来才能编译**：

```text
main   插件系统线（src_mod、obj_bullet_mod、obj_card_mod、VM 函数表…）
mod    玩法线（卡、武器、敌人、地图…）
```

所以会出现：源文件里明明有，但**打包好的构建里没有**。实测撞到的：

| 名字 | 情况 |
|---|---|
| `last_placed_card_id` / `last_placed_card_shape` | main 新加的全局，构建里没有 |
| `VM_IsUndefined` | main 新加的 VM 函数，构建里没有 → 块内一调就抛错被吞 |
| `obj_hades_scythe_bullet` | 不在构建里 → `VM_CreateInstance` 返回 -1，子弹不出现 |
| `spr_master_shield_effect_1~4` | 不在 `sprites_join_mod`，也不在 `removed_sprites.json` |

**排查手段：直接扫 `data.win`**（对象名、精灵名、全局名都在字符串池里）。
16MB 分块扫，命中就提前结束：

```powershell
$enc=[System.Text.Encoding]::GetEncoding(28591)
$s=$enc.GetString($buf,0,$n)
if($s.IndexOf("VM_IsUndefined") -ge 0){ ...找到了... }
```

**写脚本前先扫一遍要用到的对象 / 精灵 / 函数 / 全局名**，比写完再猜快得多。

### 14.14 main 和 mod 分支的差异

同一个功能两条线可能不一样，**别只看一边**：

| | `main` | `mod`（工作区） |
|---|---|---|
| `obj_weapon_mod` / `obj_bullet_mod` / `obj_card_mod` | 有 | 有 |
| `obj_player_shield` | **不设贴图**，盾牌看不见 | 读 `weapon_info.sprite`，盾牌可见 |
| `Mouse_53.gml` 的宝石创建循环 | 只有主武器 | 主 / 副 / 超武三个都有 |
| `obj_divine_*` 等专属宝石对象 | **只有引用，对象不存在** | 有 |
| 专属宝石数量 | 9 颗 | 29 颗 |

判断"某个东西在不在"之前，**先说清楚问的是哪个分支**。

---

## 15. 性能：VM 脚本的开销模型

Mod 脚本是**解释执行**的字节码，比原版 GML 慢得多。实例一多就会明显掉帧。

### 15.1 瓶颈不是判定逻辑，是「进 VM 解释器」的次数

每帧总开销 ≈ Σ(每个在场实例)［宿主对象 Step 的固定活 + `VM_Execute` 进入开销 + 你的脚本］

| 开销 | 内容 | 能否省 |
|---|---|---|
| ① 宿主对象 Step 的固定活 | 如 `obj_bullet_mod` 每帧每个实例都要 `get_grid_position_from_world()` 算格子、若干 `ds_map_exists` 判延迟初始化、`ds_list` 操作 | **改不掉** |
| ② `VM_Execute` 进入开销 | `buffer_seek` → 读 opcode → 分派，外面套着 `try/catch`，进出还要切 `global.__vm` / `_VM_cur_card` | **改不掉** |
| ③ 你脚本里的语句 | `VM_GetProp` / `VM_SetProp` / 各种查询 | 能省，但**不是大头** |

**①② 是"进场费"** —— 只要实例还活着、Step 还在跑，每帧就得交一次，和脚本长短无关。

一个真实的量级（里格子弹）：

```text
每颗子弹每帧原本 ~15 次 VM 函数调用
2 轮 × 5 发 / 78 帧，每发活 ~165 帧  →  约 21 发同时在场（只摆一个里格）
                                      →  每帧 300+ 次 VM 调用
```

### 15.2 能省的三处

**① 移动交给引擎**

```gml
// 不要每帧这样：
VM_SetProp(self, "x", VM_GetProp(self, "x") + vx)

// 改成 CREATE 里设一次：
VM_SetProp(self, "hspeed", vx)
VM_SetProp(self, "vspeed", vy)
```

引擎每帧自己推 x/y，**每帧省 6 次 VM 调用**。代价见 15.4。

**② 把「每 N 帧才做的事」提到 STEP 最前面**

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    tick = VM_GetProp(self, "tick") + 1
    if (tick < 5) {
        VM_SetProp(self, "tick", tick)
        exit                    // 4/5 的帧只跑这 3 次调用
    }
    VM_SetProp(self, "tick", 0)
    ... 后面才是重活：出屏判断、范围查询、扣血 ...
}
```

出屏判断、超时销毁这类也可以一起挪进"每 N 帧"里做，不用每帧查。

**③ 范围查询前先用前缀和预检**

```gml
if (VM_EnemyInRange(br, br, bc, bc, "") == 1) {   // 前缀和，便宜
    ... VM_GetInstancesInRange ...                // 贵，能不跑就不跑
}
```

大部分帧格子里是空的，这样能直接跳过收集。

### 15.3 真正的杠杆：减少在场实例数

进场费省不掉，所以**实例数是乘数**。以里格为例：一发活 165 帧、每 78 帧打 10 发，
稳态就是 21 发在场；摆 5 个里格 → 105 发 → 每帧 100+ 次 `VM_Execute`。

可控手段（都是**设计取舍**，不是代码能绕的）：

- 少发几轮 / 拉长 `cycle`
- 缩短寿命（出屏即死、别设大 `destroy_timer`）
- 合并同类子弹（比如 5 个方向的弹合成一个对象，方向靠属性传）

### 15.4 用 hspeed/vspeed 会失去什么

`VM_SetProp` 设 `x` / `y` 时有两个副作用：

```gml
if (prop == "x" || prop == "y") { update_plant_bindings(inst_id); }   // 卡片跟随移动平台用
// 另外还会走联机同步 MSG_MODIFY_PROP
```

改用 `hspeed` / `vspeed` 后这两个都不触发。**子弹一般无所谓，卡片 / 平台相关的要留意。**

还有一点：出屏判断如果改成每 N 帧才查，子弹会多飞几帧（最多 `速度 × N` 像素）才消失。

---

## 16. 给智能体的最终检查清单

编写或复刻前：

- [ ] 找到注册入口
- [ ] 找到对象目录
- [ ] 找到所有事件
- [ ] 找到所有依赖对象
- [ ] 找到所有依赖精灵
- [ ] 判断能否 JSON 完成
- [ ] 判断能否 BIN 完成
- [ ] 判断是否需要新增对象
- [ ] 写 JSON
- [ ] 写 BIN
- [ ] 编译 BIN
- [ ] 测试普通模式
- [ ] 测试联机模式
- [ ] 记录未实现部分

完成标准：

```text
不修改原版代码时，Mod 能独立注册并运行；
需要修改时，优先新增，不要破坏原对象；
无法复刻的部分要明确列出。
```