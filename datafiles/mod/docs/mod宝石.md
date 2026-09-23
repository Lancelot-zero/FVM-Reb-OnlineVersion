# 宝石 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/gems/my_gem.json / .bin / tex/
```

## JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `name` / `description` | ✅ | 名字 / 说明（鼠标悬停时直接显示 `description`） |
| `icon` | ✅ | 图标精灵名（建议自带 82×82 的 PNG、原点 40,40） |
| `slot` | | 装在哪种武器上：`main_weapon`（默认）/ `super_weapon` / `secondary_weapon` |
| `passive` | | `true` = 纯被动宝石，**不创建实体**（`.bin` 不会被跑） |
| `cooldown` | | 冷却数组（按等级取，**单位帧**）；主动宝石必填 |
| `max_level` | | 最高等级（默认 15） |
| `first_cooldown` | | 首次冷却（没有 `cooldown` 数组时用） |
| `shop` | | 商店 |

除 `name` / `description` / `icon` / `slot` / `shop` / `passive` 以外的字段会**原样透传**进宝石数据。
实例上现成能读的：`gem_id`（宝石 id）、`cooldown`（按等级算好的冷却帧数）、`gem_level`（存档等级）。

## 主动宝石：点击 → 放技能 → 自动冷却

核心已经把冷却和点击都写好了，插件只要读一个标志：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "clickable", 1)                  // 声明"我是主动宝石"
    VM_SetProp(self, "sprite_index", "spr_my_gem")    // 战场贴图（背包图标用的是 JSON 的 icon）
    VM_SetProp(self, "image_xscale", 1.8)
    VM_SetProp(self, "image_yscale", 1.8)
}

_OBJECT_STEP {
    self = VM_GetCurCard()
    if (VM_GetProp(self, "clicked") == 1) {
        VM_SetProp(self, "clicked", 0)     // 自己清掉
        ...放技能...
        // 冷却不用管：点下去已经自动按 JSON 的 cooldown[等级] 重新走了
    }
}
```

核心（`obj_gem_mod`）的行为：

| 属性 | 谁写 | 含义 |
|---|---|---|
| `clickable` | 插件（`_OBJECT_CREATE` 置 1） | 主动宝石；不置（默认 0）＝点击无效 |
| `clicked` | 核心置 1，插件处理完清 0 | "刚被有效点击"；**冷却中不会置** |
| `cooldown_timer` | 核心每帧自减，点击时自动设回 `cooldown` | 剩余冷却帧；>0 时图标上画遮罩 + 秒数、说明框里加「正在冷却中」 |
| `on_click` | 核心（鼠标进 / 出） | 显示说明框用；也可以自己置 |

## 被动宝石：自己管冷却

`passive: true`（或干脆不置 `clickable`）时点击无反应，`cooldown_timer` 完全由插件自己写
—— 比如"每 3 秒产一次火苗"，产完之后：

```gml
VM_SetProp(self, "cooldown_timer", 180)     // 3 秒（60 帧/秒），图标上照样显示冷却
```

## 几个要点

- 冷却值来自 JSON 的 `cooldown` 数组、**按等级取、单位是帧**；`cooldown_timer` 每帧自减
- **图标有两道静默失败**：槽位列表没遍历到 → 图标栏里完全不出现；`icon` 名字解析不到 →
  出现了但是空白（`get_load_sprite` 会给占位图、不报错）。所以自带 PNG 要在 `_OBJECT_CFG` 里预加载
- 战场贴图自己设（`sprite_index` + 缩放），背包/槽位图标用 JSON 的 `icon`
- 宝石实例是被"放置逻辑"创建的（不是插件创建），所以只有 `_OBJECT_*` 那套块可用；
  另外核心还挂了鼠标事件：`Mouse_10/11`（进/出 → `on_click`）、`Mouse_4`（左键 → `clicked`）
