# 卡片 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/cards/my_card.json     卡配置
mod/cards/my_card.bin      逻辑（VM 脚本编译产物）
mod/cards/my_card.txt      源码（建议保留）
mod/cards/tex/...          自带贴图（可选）
```

## JSON 字段

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
| `shapes[].flame_produce` | | 可选，产火苗（17 档） |
| `skill` | | `{ "attr": "cycle", "values": [78, 75, ...] }` 技能每级加成 |
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

## BIN 特别用法

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "sprite_index", "spr_small_fire")
    VM_SetProp(self, "cd", 0)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    VM_SetProp(self, "cd", VM_GetProp(self, "cd") + 1)
}
```

- **动画**（和武器同一套）：靠 `idle_anim`（待机帧数）、`attack_anim`（攻击帧数）、
  `flash_speed`（几帧走一格，默认 5）、`state`（0=待机 1=攻击）驱动 `image_index`；
  默认 `idle_anim = 0` 会把形象**锁在第 0 帧不动**。待机/攻击**不会自动切换**，自己改 `state`：

```gml
VM_SetProp(self, "idle_anim", 9)
VM_SetProp(self, "attack_anim", 16)     // idle_anim + attack_anim 别超过精灵总帧数
VM_SetProp(self, "flash_speed", 5)
VM_SetProp(self, "state", 1)            // 切攻击
```

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

- 位置 / 目标：`grid_col` / `grid_row` / `x` / `y`；销毁自己用 `VM_DestroyInstance(self)`
- 常用查询：`VM_GetPlantAt(列,行,"层级")`、`VM_GetInstancesInRange(...)`、`VM_EnemyInRange(...)`、
  `VM_CanPlace("卡名",列,行)`
- ⚠️ **不要读自己没设过的属性**（会报错并中断整个块），要用先在 `_OBJECT_CREATE` 里设一份
- 「上一张种下的卡」用 `VM_GetLastCreatedCard()`（注意它在 CREATE 时读到的还是上一张）
