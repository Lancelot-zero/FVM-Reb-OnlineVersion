# 时装 Mod

> 通用前提（文件命名、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)。

## 文件

```text
mod/attires/my_attire.json     ← 只有 json，没有 bin / 没有逻辑
```

时装是**纯外观**，没有任何逻辑块可写。

## JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `target_card` | | `"player"`（默认，角色时装）/ 卡片 id（卡片时装） |
| `name` | | 名字 |
| `icon` | | 商店 / 列表图标 |
| `spr` | ✅ | 贴图：单个名字，**或名字数组**（多帧逐张加载） |
| `card_slot_icon` | | 可选，卡槽图标名字数组（卡片时装用） |
| `shop` | | 商店（角色时装进 tab5，卡片时装进 tab4） |

例子（角色时装）：

```json
{
  "name": "mod测试时装",
  "target_card": "player",
  "icon": "spr_player_attire_1",
  "spr": "spr_player_attire_1",
  "shop": { "cost": "1000", "description": "mod时装演示" }
}
```

## 要点

- 贴图同样可以在 `_OBJECT_CFG` 里预加载（时装没有实例，但 `.bin` 的 CFG 依然会跑）
  —— 不过时装一般只有 json，直接引用**内置精灵名**最省事
- 多帧写法：`"spr": ["spr_a", "spr_b", "spr_c"]`
- 时装 id 就是文件名；想改外观名/价格改 json 即可，`[reloadmod]` 生效
