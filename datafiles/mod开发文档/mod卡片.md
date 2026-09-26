# 卡片 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

卡片放在**游戏目录下的 `mod/cards/`** 里（游戏目录 = 跑游戏的那个文件夹）：

```text
游戏目录/                       ← 运行游戏的那个文件夹
└─ mod/
   └─ cards/                    ← 卡片这一类都放这里
      ├─ my_card.json           卡配置（数值、形态、图标）—— 必须有，扫描入口就是它
      ├─ my_card.bin            逻辑（由 my_card.txt 编译出来，游戏只读这个）
      ├─ my_card.txt            源码（建议保留，游戏不读它）
      └─ tex/                   自带贴图（可选）：用哪张贴图就丢哪张进来
         └─ xxx.png
```

**也可以再分一层子目录归类**（只扫一层、不往下递归）：

```text
mod/cards/snake/shegengbao.json
mod/cards/snake/shegengbao.bin
mod/cards/snake/shegengbao.txt
mod/cards/snake/tex/xxx.png     ← tex 跟着 json 那一层放，不是放回 cards/ 根目录
```

- 平铺和子目录**可以混用**：`mod/cards/a.json` 和 `mod/cards/snake/b.json` 会一起加载
- id 只看 **json 的文件名**，子目录名随便取（中文也行），不写进 id
- 再深一层（`cards/snake/2025/xxx.json`）**扫不到**

## 一、JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` | ✅ | 卡名（图鉴 / 商店显示） |
| `shapes` | ✅ | 形态数组，至少 1 个；里面每个形态是一档"外形" |
| `shapes[].sprite` | ✅ | 卡面 / 预览贴图名（走 `get_load_sprite`） |
| `shapes[].shape` | | 形态序号，默认按数组下标 |
| `shapes[].name` / `shapes[].description` | | 形态名 / 形态说明，默认用卡名 |
| `shapes[].plant_type` | | 层级：`normal`（默认）/ `shield_inner` / `lilypad` / `shield_outer` / `coffee`。**同时决定"能不能落到占位格"和"会不会被替换掉"**，见下面的⚠️ |
| `shapes[].feature_type` | | 特性，默认 `normal`。取值见下面的 **`feature_type` 取值**：`normal` / `water` / `amphi` / `dwarf` / `low` / `upgrade` / `bun` / `king_bun` / `tbun` / `king_tbun` |
| `shapes[].target_card` | | 要选目标卡时填卡名，默认 `none` |
| `shapes[].hp` / `atk` / `range` / `cooldown` / `cycle` | | 数值，**17 档数组**（从最低等级往上排），也可以写单个数字＝所有等级一样 |
| `shapes[].flame_produce` | | 可选，产火苗量（17 档） |
| `skill` | | `{ "attr": "flame_produce", "values": [...] }` 技能每级加成（`attr` 可以是 `cycle` / `flame_produce` / `atk` …） |
| `shop` | | 商店 |
| `info_island` | | 图鉴说明文本 |

- **17 档**：写不满就用最后一个补齐（`[50, 60]` → 后面 15 档全是 60）
- JSON 只管卡面 / 数值；**本体形象要在 `_OBJECT_CREATE` 里自己设**：`VM_SetProp(self, "sprite_index", "xxx")`

> ⚠️ **`plant_type` 必须按卡的用途选，写错了卡会"吃掉目标却什么也没吃到"**
>
> 种植时用的是 **JSON 里的 `plant_type`**（卡池里存的那份），它管两件事：
>
> 1. **能不能落到占位格**：`normal` 在"替换放置"关掉时**不允许**种到已有卡上；`coffee` 两种模式都允许；`lilypad` 只认水面；`shield_outer` 只认陆地。
> 2. **目标卡会不会被替换掉**：开着替换放置时，游戏只替换掉**和本卡 `plant_type` 相同**的卡（`normal` 换 `normal`，`coffee` 层永远不动），而且这一步**发生在你被创建之前**——那张卡已经被销毁、并从格子里移走了。
>
> 所以「**落到已有卡上，再把那张卡吃掉/换掉**」的卡（例：蛇羹煲、替换类卡）：
>
> - 写 `normal` → 玩家开着替换放置时，目标卡在种植瞬间就被销毁并移出网格，你的 `_OBJECT_STEP` 里 `VM_GetInstancesInRange(..., "card", ...)` **只剩自己**（现象：`n == 1`，循环永远进不到处理分支）。
> - 写 `coffee` → 占位格允许落下，且通用替换不动它，目标卡还在格子里，自己再动手吃 / 换 / 加成。
>
> 注意：`_OBJECT_CREATE` 里的 `VM_SetProp(self, "plant_type", ...)` 只改**实例**的绘制层级和被敌人吃的优先级，**管不了种植判定与替换判定**——那两项只看 JSON。

### `feature_type` 取值

默认 `normal`。它决定**能不能种在水里 / 会不会被普通老鼠打 / 种下时吃不吃底座卡 / 和哪些卡算"同族"**。

| 取值 | 是什么 | 具体影响 |
|---|---|---|
| `normal` | 普通卡（默认） | 无特殊判定 |
| `water` | 水上卡（木盘子、水上茶杯、钢丝球…） | **陆地上不许种**；水面上可以压着莲叶/普通卡放（按"水上卡"那套判定）。`plant_type: "normal"` 配 `feature_type: "water"` 就是"种在水里的普通卡" |
| `amphi` | 两栖卡（章鱼烧、炭烧海星、咖喱龙虾炮…） | 陆地和水面**都能种**；水面上只要那格没有（非替换模式下的）普通卡就能落 |
| `dwarf` | 低矮 / 陷阱类（鱼刺、老鼠夹…） | **普通老鼠不会停下来打它**（直接踩过去），只有巨型老鼠才会打；**水里不许种** |
| `low` | 低矮（原版咖啡杯、老鼠夹在用） | 目前游戏里**没有任何判定读它**，实际效果 = `normal`；为了和原版一致可以照写 |
| `upgrade` | 升级 / 替换类（大火炉、钢鱼刺、机枪小笼包…） | 种下时**先吃掉格子里 `target_card` 指定的那张卡**（例：大火炉吃 `small_fire`）→ 所以必须同时写 `target_card`；开着替换放置时也能落到同类卡上 |
| `bun` / `king_bun` | 包子 / 国王包子 | 同一族内部**不互相替换**（国王小笼包还能"填装"吸收同族包子卡） |
| `tbun` / `king_tbun` | 三向包子 / 三向国王包子 | 同上，另一族 |

- 判定发生在**种植那一刻**，所以改 `feature_type` 要重启/重进游戏
- 水/陆的两套落点判定都只认**这一格**（地形 + 格子里已有的卡）
- 自己拿不准就写 `normal`；只有"水里用""矮到老鼠不打""吃底座升级"这三类才需要写别的

最小例子（数值都是 **17 档数组** = 等级 0~16）：

```json
{
  "name": "我的卡片",
  "shapes": [
    {
      "shape": 0,
      "name": "我的卡片",
      "description": "说明文字",
      "sprite": "spr_small_fire",
      "plant_type": "normal",
      "feature_type": "normal",
      "target_card": "none",
      "hp":       [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50],
      "cost":     [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50],
      "atk":      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      "range":    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      "cooldown": [420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420, 420],
      "cycle":    [1500, 1440, 1380, 1320, 1260, 1200, 1140, 1080, 1020, 960, 900, 840, 780, 720, 660, 600, 540]
    }
  ],
  "skill": { "attr": "flame_produce", "values": [25, 27, 29, 31, 34, 37, 40, 44, 48] },
  "shop": { "cost": "1000", "description": "商店里显示的文字" },
  "info_island": "图鉴里显示的说明"
}
```

- `hp` / `cost` / `atk` / `range` / `cooldown` / `cycle` / `flame_produce` 都是 **17 档**（0~16 级，17 个数）
- 写不满会自动用最后一个补齐（`[50, 60]` → 后面 15 档全是 60）；也可以只写一个数字（`"hp": 50`）＝所有等级一样
- `skill.values` 是 **9 档**（技能 1~9 级）；`shop` / `info_island` 不写就不进商店 / 图鉴

---

## 二、卡片实例上的内置字段

卡片实例一建出来就带这些属性，**大部分可以直接读写**。

### 基础字段（每个卡片实例都有）

| 字段 | 说明 |
|---|---|
| `plant_id` | 卡 id（= 文件名） |
| `net_player_id` | **联机：这张卡是谁种的**（房间内 id：房主 0、客机从 1 起；`0` 也可能是单机 / 关卡自摆的卡）。单机永远是 0。见 `help.md` 的「联机与房间 id」 |
| `name` / `description` / `cost` / `cooldown` | 卡名 / 说明 / 阳光 / 冷却 |
| `hp` / `max_hp` | 当前 / 最大血量（`hp <= 0` 会被核心销毁） |
| `atk` / `range` | 攻击力 / 攻击范围（格） |
| `cycle` / `attack_cycle` / `origin_cycle` | 攻击周期 / 当前周期（减速翻倍）/ 原始周期 |
| `attack_timer` | 攻击计时器（自己推进，核心不管） |
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

### mod 专用（进 VM 时机 / 索敌 / 批量改属性都靠它们）

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
| `mod_step_enter_var` | 可写 | `"mod"` = 间隔帧数；`"wait"` = 还要等几帧；`"hp_change_mod"` = 冷却帧数 |
| `mod_step_enter_arr` | 可写（数组） | 开火窗口表（`"norm"` / `"norm_attack"` 用）；**负数 = 从一轮末尾倒数**（`-35` → 一轮长度-35） |
| `mod_step_enter_index` | 只读 | 这帧命中的窗口下标（0 起）；不是窗口 = `-1` |
| `mod_countdown` | 可写 | **倒计时（帧）**：`> 0` 时每帧 `-1`（默认 0 = 不生效） |
| `mod_alpha_add` | 可写 | **每帧透明度变化量**（负数 = 渐隐）；只在 `mod_countdown > 0` 时加到 `image_alpha` 上 |
| `mod_hover_mask` | 可写 | `1` = **鼠标移入时显示白色半透明遮罩**（默认 0 = 不显示） |
| `mod_hover_alpha` | 可写 | 遮罩的透明度（默认 `0.35`，0~1，越大越白） |
| `mod_set_on` / `mod_set_id` / `mod_set_prop` / `mod_set_val` | 可写 | **批量改属性**：开关开着时每帧把第 i 个 id 的 `prop[i]` 设成 `val[i]`（三个数组按最短长度配对） |
| `mod_set2_on` / `mod_set2_id` / `mod_set2_prop` / `mod_set2_val` | 可写 | 第二套，行为一样，各自独立 |

**`mod_countdown` + `mod_alpha_add`：倒计时 + 淡出（**所有 mod 对象通用**，核心每帧帮你推，不用自己写 STEP）**

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "mod_countdown", 30)          // 30 帧后倒计时走完
    VM_SetProp(self, "mod_alpha_add", -1 / 30.0)   // 每帧 -1/30 → 30 帧正好淡到 0
}
```

- 核心逻辑：**倒计时 > 0** 时 → `mod_countdown -= 1` 且 `image_alpha += mod_alpha_add`（两个一起才生效）
- ⚠️ **两边都是整数时 `/` 走整除**（和 C 的 `int /` 一样，向零取整）：`-1 / 30` 算出来是 `0`，淡出会**一点效果都没有**。要小数就把其中一个写成小数：`-1 / 30.0`，或者直接写 `-0.0333`
- 想"淡入"就把变化量写正数；想中途改方向/停手，直接改这两个字段的当前值
- 倒计时走完（到 0）就不再动透明度了 —— 要消失就在 STEP 里判 `mod_countdown == 0` 自己销毁
- ⚠️ 值按**帧**算（不是按秒）：30 帧 = 0.5 秒

**`mod_hover_mask`：鼠标移入时戴一层白色半透明遮罩（卡片 / 敌人都有）**

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "mod_hover_mask", 1)        // 打开
    VM_SetProp(self, "mod_hover_alpha", 0.45)    // 想更白就调大（默认 0.35）
}
```

- 判定：鼠标**压在实例的贴图碰撞掩码上**才算移入（`position_meeting`）；贴图没有碰撞遮罩时按贴图的包围盒（bbox）兜底，两个都没命中才算没移入
- 做法：用项目的 `hit_effect_2` 着色器把同一张贴图再画一遍 —— 那个着色器把 RGB 换成顶点色、保留贴图的 alpha，
  所以 `c_white` + `mod_hover_alpha` 得到的就是**贴图形状的纯白半透明剪影**，盖在本体上
- 想改"跟着鼠标走的声音/其它表现"用 `_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` 块

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
    VM_InstArrayAdd(self, "mod_step_enter_arr", -35)
    VM_SetProp(self, "mod_step_enter_condition", "norm_attack")
}
```

---

## 三、STEP 的时机与执行顺序

### 什么时候跑

- **`_OBJECT_CREATE`**：卡片被种下当帧**立刻执行**
- **`_OBJECT_STEP`**：从这张卡第一个 Step 阶段开始，每帧一次，**但会被下面的条件挡住**
- **`_OBJECT_DESTROY`**：实例销毁时（血量归零、被铲、被清场）
- **`_OBJECT_MOUSE_ENTER` / `_OBJECT_MOUSE_LEAVE` / `_OBJECT_CLICK`**：鼠标移入 / 移出 / 左键点在本卡上（按实例判定，鼠标要压在这张卡的贴图上）
- **游戏暂停**时整块不跑；**冻住**（`is_frozen`）时不进 VM；`hp <= 0`、实例已销毁也不进

> 手牌点下去种的卡：鼠标事件在 Step 之前，所以**当帧就会跑第一次 STEP**。
> 用 `VM_SpawnPlant` 在别的卡 / 地图脚本的 Step 里种下来的卡，Step 阶段里新建的实例当帧不再跑自己的 Step，
> **第一次 STEP 落在下一帧** —— 需要"种下就立刻发生效果"的逻辑，写进 `_OBJECT_CREATE` 更稳。

### 每帧顺序（核心在什么时候跑你的块）

```text
1. 暂停就整个 exit
2. 核心的逻辑        → 跑核心那一套（动画 / 攻击 / 生产等）
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

> ⚠️ **别让卡片每帧进 VM**（不设就是每帧，也就是 `""`）。场上卡一多，这条就是掉帧最大的来源。
> - 射手类 → `"norm_attack"` + 一条**开火窗口**（只在开火那几帧进）
> - 生产 / 计时类 → `"norm"` + **生产那一帧**的窗口
> - 被打才有反应的卡 → `"hp_change"` / `"hp_change_mod"`（平时完全不进）
>
> 窗口写**从一轮末尾倒数的负数**；一次能算完的常量和贴图放 `_OBJECT_CFG`。详见 [mod开发工具说明.md](mod开发工具说明.md) 的「性能铁律」。

| 值 | 什么时候进 | 配合字段 |
|---|---|---|
| `""`（默认，写错也按这个兜底） | **每帧进** | — |
| `"mod"` | `battle_time % 间隔 == 0`（全场共享，同间隔的卡同帧进） | `mod_step_enter_var` = 间隔帧数 |
| `"norm"` | `mod_tick_cycle` 命中窗口表（**不看敌人**） | `mod_step_enter_arr` 窗口表 |
| `"norm_attack"` | **有敌人** 且命中窗口表；窗口表空 = 有敌人就每帧进 | `mod_step_enter_arr` + 索敌字段 |
| `"wait"` | `mod_step_enter_var` 每帧自减，减到 0 进（进之前保持 0，需自己再设） | `mod_step_enter_var` = 剩余帧数 |
| `"hp_change"` | **血量相对上一帧变了**就进（掉血、回血都算）；出生时基线 = 出生血量，所以出生当帧不进 | — |
| `"hp_change_mod"` | 同 `"hp_change"`，但**触发一次后进冷却**：`mod_step_enter_var` 帧内再掉血也不再进 | `mod_step_enter_var` = 冷却帧数（不写/写错 = 无冷却，等同 `"hp_change"`） |

- `"hp_change"` / `"hp_change_mod"` 读的是核心每帧维护的 `pre_hp`（上一帧末的血量），受伤当帧就能进 VM；
  冷却期内的伤害会被吞掉，出冷却后不会因为攒着的旧变化补触发一次

- 命中窗口时 `mod_step_enter_index` = **窗口下标**（0 起，一轮里有多个窗口时用它区分是第几次），否则 `-1`
- 窗口表里的负数从一轮末尾倒数：一轮 1500 帧时 `-35` = 第 1465 帧
- 怎么选：
  - **普通射手**（有敌人才打、按 cycle 节奏）→ `"norm_attack"` + 一条窗口表
  - **生产者 / 每帧逻辑**（火苗、光环）→ 不设（每帧进）或 `"norm"`
  - **全场统一节奏 / 想省性能**（大量同类卡）→ `"mod"` + 间隔
  - **一次性延时**（种下 60 帧后做某事）→ `"wait"` + 60
  - **事件驱动**（等敌人出现才开始动作、计时起点不固定）→ `"norm_attack"` + **空窗口表**

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

#### ⚠️ 窗口表写「从一轮末尾倒数」的负数，别写绝对帧号

窗口值是拿去和**当前**的 `mod_tick_cycle` 比对的，而一轮长度是 `mod_tick_max`（不设 = 卡的 `cycle`）。
**`cycle` 会被技能改**：JSON 里 `"skill": { "attr": "cycle", "values": [...] }` 会在升级时把它改掉
（实例：雷神 150 → 108、月神 180 → 102、大力神 180 → 72）。所以：

- ✅ 写 `-30`（= 一轮末尾往前 30 帧）、`1 - 13 * flash_speed`
- ❌ 写 `120`（= 第 120 帧）—— `cycle` 一旦降到 120 以下，这个窗口**永远轮不到**，表现就是**卡一发不发**

**硬规则：窗口换算成实际帧号后必须落在 `0 ~ 一轮长度-1` 之间。** 负数按 `一轮长度 + 值` 换算，
换算结果仍为负的话同样永不触发（例：一轮 72 帧时写 `-84` = `-12`）。
确实需要固定周期时，显式设 `mod_tick_max`（例：`VM_SetProp(self, "mod_tick_max", 176)`），这时写绝对帧号才安全。

自查：把 JSON 的 `cycle` 数组、`skill.values`（若 `attr == "cycle"`）里的**最小值**当作一轮长度，
逐个验窗口是否还在范围内。

#### ⚠️ `"wait"`：每次进 VM 都必须重设 `mod_step_enter_var`

引擎把 `mod_step_enter_var` 减到 0、放你进 VM 之后，它就**停在 0**。卡里必须马上设回正数：

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    ...
    VM_SetProp(self, "mod_step_enter_var", 600)   // 下一次等 600 帧；不设就会被清掉条件
}
```

不设的后果：引擎判定「值不合法」并把 `mod_step_enter_condition` 清成 `""` —— 条件被清掉后**再也回不到 wait**，
卡退化成每帧进 VM。**每一条 `exit` 路径都要先设好**。
`"wait"` 适合「等 N 帧做一件事」的卡（冷却、前摇、延时放技能），比自己每帧倒数省得多。

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
    VM_InstArrayAdd(self, "mod_step_enter_arr", -35)
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
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  8,    0, dmg, 1, -1, "normal", "", "", "normal", 5, 0)
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  6,  5.6, dmg, 1, -1, "normal", "", "", "normal", 5, 0)
    VM_BulletScreenAdd_Ex("spr_my_bullet", 1, 0, 1.8, 1, bx, by,  6, -5.6, dmg, 1, -1, "normal", "", "", "normal", 5, 0)

    // 会拐到目标行的（原版水管弹那种）：用 _Exs，最后多两个参数（目标行世界 y、靠拢比例）
    ty = -1                                   // -1 = 不渐变
    VM_BulletScreenAdd_Exs("spr_my_bullet", 1, 0, 1.8, 1, bx, by, 8, 0, dmg, 1, -1, "normal", "", "", "normal", 5, 0, ty, 0.15)

    VM_PlaySound("snd_shot")
    VM_SetProp(self, "state", 1)
}
```

- **`标志数值`（bit 掩码）**：这颗子弹还能接受哪些卡片效果（`bit1`=过火、`bit2`=解冻、`4/8/16…`=自定义），
  要和卡片侧 `bullet_flag` 对上才生效；不想要卡片联动就写 `0`
- **`伤害计数`**：`1` = 命中一个就消失，`-1` = 不限次数（范围内所有可命中敌人都结算）
- **`销毁对象`**：非空时在子弹消失处创建它（`mod名字` 指定 mod 特效的 `mod_type`），做命中特效最省事
- 要会拐弯追人的，用 `VM_HomingBulletAdd(...)`（每帧重新索敌、自转）

#### 卡片侧能吃子弹的字段

子弹带着 `标志数值` 进到**本卡所在格的中心带**时，会和这张卡的 `bullet_flag` 做与运算，`>0` 就应用效果并**消位** ——
所以同一类卡对同一颗子弹只生效一次。卡上能写的字段：

| 字段 | 默认 | 说明 |
|---|---|---|
| `bullet_flag` | — | **去重位**：本卡属于哪些类。`bit1`=过火、`bit2`=解冻、`4/8/16…`=自定义 |
| `bullet_mul_dmg` | `1` | 乘伤害（`子弹伤害 × 倍率 + 加值`） |
| `bullet_add_dmg` | `0` | 加伤害 |
| `bullet_flip_x` / `bullet_flip_y` | `0` | 反向（0/1） |
| `bullet_angle_add` | `0` | 画面旋转角度（原版布丁是 +180） |
| `bullet_freeze_mul` | `1` | 乘冰冻帧 |
| `bullet_freeze_add` | `0` | 加冰冻帧（命中时写给敌人的 `ice_timer`） |
| `bullet_scale` | `1` | **乘**缩放；只认正数 |
| `bullet_hits_add` | `0` | 加穿透次数；只认正数，**穿透弹（`hits = -1`）无效** |
| `bullet_destroy` | `0` | `0`=不启用；**负数**=立即销毁这颗子弹；**正数**=只减穿透次数（减到 0 自然销毁）。穿透弹无效 |
| `bullet_charge_self` | `0` | 置 `1` = 每颗子弹穿过本卡时，把它当时的伤害累加到 `bullet_charge_dmg` |
| `bullet_charge_dmg` | `0` | 【只读】被 `bullet_charge_self` 累计的伤害（**不自动清零**，自己读、自己归零） |
| `bullet_pass_count` | `0` | 【只读】穿过本卡的子弹次数，**自动累计、不用开关**（穿透弹也算；靠消位保证一弹一卡只 +1） |

> `bit1`（过火）/ `bit2`（解冻）有内置表现，见 `help.md` 里 `VM_BulletScreenAdd_Ex` 那一条；
> 自定义类（`4/8/16…`）没有内置表现，效果全看上表。
> ⚠️ 记得在 `_OBJECT_CREATE` 里先 `VM_SetProp` 设好默认值，不然读到的是 `undefined`。

---

## 五、其它要点

- **动画**：`idle_anim`（待机帧数）、`attack_anim`（攻击帧数）、`flash_speed`（几帧走一格，默认 6）、
  `state`（0/1）驱动 `image_index`；默认 `idle_anim = 0` 会**锁在第 0 帧不动**。
  ⚠️ 帧数要求是 **`idle_anim + attack_anim + 2 ≤ 精灵总帧数`**，不是 `+0`：待机播 `0..idle_anim`，
  攻击从 `idle_anim+1` 起跳、还会把 `idle_anim+attack_anim+1` 也显示一帧。
  超出的话攻击动画会回绕到待机帧，看着像「动画卡了 / 播了一半变回待机」
  - **动画是核心自动推的，别碰 `image_speed`**（固定为 0）；想自己控制就在 `_OBJECT_STEP` 里直接写 `image_index`
  - 卡片比武器多一条：`mod_step_enter_condition = "norm_attack"` 时，**命中开火窗口且有敌人会自动切 `state = 1`**，
    没敌人时自动收回待机 —— 用这套的卡不用自己管 `state`
- **联动 / 计数**：先想清楚这份数据**属于谁**，它决定 `[reloadmod]` 之后还在不在：

  | 数据挂在谁身上 | 写法 | `[reloadmod]` 之后 |
  |---|---|---|
  | **实例** | `VM_SetProp(self, "名字", 值)` / `VM_InstArraySet(self, "名字", k, 值)` | ✅ 保留（不会帮你重置） |
  | **VM**（命名数组，按 mod id 一份、全实例共享） | `VM_ArrayGet / Set / ADD` | ❌ **被清空**（`VM_ArrayGet` 返回 0） |
  | **全局** | `VM_GetProp(0, "名字")` 读、`VM_SetProp(0, ...)` 写 | ✅ 保留（但要小心撞名） |

  由此得出三条写法：

  1. **常量表**（按等级查的数值表）→ 建在 **`_OBJECT_CFG`**（每次加载/重载都跑），
     别放 `_OBJECT_CREATE` —— 重载**只重跑 `_VM_CONST_INIT` + `_OBJECT_CFG`，不重跑 CREATE**。

```gml
_OBJECT_CFG {
    // 常量表放这里：reloadmod 后数组被清空，而 CFG 每次加载/重载都会重跑，正好重建
    if (VM_ArraySize("my_tbl") < 16) {
        VM_ArrayClear("my_tbl")
        VM_ArrayADD("my_tbl", 10)
        // ...
    }
}
```

  2. **「场上同类有多少个」这类活计数器** → 别用命名数组记（重载清 0 后倍率就错了），
     用的时候**现场数**：`VM_GetInstancesInRange("arr", 0, 99, 0, 99, "card", "卡id")`。

```gml
_OBJECT_STEP {
    self = VM_GetCurCard()
    n = VM_GetInstancesInRange("my_same", 0, 99, 0, 99, "card", "my_card")   // 含自己
    mult = 1 + 0.05 * (n - 1)
}
```

  3. 一定要用命名数组、又想扛住重载 → 读之前用 `VM_ArraySize("名字")` 判断为空就重建。

- 常用查询：`VM_GetPlantAt(列,行,"层级")`、`VM_GetInstancesInRange(...)`、`VM_EnemyInRange(...)`、
  `VM_CanPlace("卡名",列,行)`、`VM_EnemyInRange` 判"这一行有没有敌人"
- ⚠️ **不要读自己没设过的属性**（会报错并中断整个块），要用先在 `_OBJECT_CREATE` 里设一份
  （`flame_produce` / `cycle` / `atk` / `x` / `y` / `grid_row` 这些是现成的，可以直接读）
- 「上一张种下的卡」用 `VM_GetLastCreatedCard()`（CREATE 里读到的还是上一张）
