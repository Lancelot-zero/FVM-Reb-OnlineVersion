# 武器 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/weapons/my_weapon.json / .bin / tex/
```

## JSON 字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `sprite` | ✅ | 本体贴图名 |
| `icon` | | 装备图标（默认用 `sprite`） |
| `slot` | | `main_weapon`（默认）/ `super_weapon` / `secondary_weapon` |
| `atk` / `cycle` | | 攻击力 / 攻击周期（帧），默认 0 / 60 |
| `name` / `description` | | 名称 / 说明 |
| `atk_impact` / `cycle_impact` | | 可选，武器对卡片的加成 |
| `hp_increase` | | 可选，加血（副武器常用） |
| `shop` | | 商店 |

## 三种槽位的区别

| | 主武器 / 超级武器 | 副武器（盾牌） |
|---|---|---|
| 注册对象 | `obj_weapon_mod` | `obj_player_shield` + 额外一个 `obj_weapon_mod` |
| 进战场的时机 | 种卡时随卡片创建 | 角色创建时 |
| `.bin` 的 `CREATE/STEP/DRAW` | 会执行 | **会执行**（跑在额外那个 `obj_weapon_mod` 上） |
| 血量 / 原版盾宝石效果 | 不涉及 | 由 `obj_player_shield` 负责，mod 管不到 |

> 副武器的 `.bin` 是**额外挂的那个 `obj_weapon_mod`** 在跑（`obj_player_character/Mouse_53.gml` 里创建），
> 所以逻辑块照样能写；但盾牌本体（血量、原版盾宝石效果）不归 mod 管。

## BIN 特别用法

- **动画四属性必须自己喂**，否则形象卡在第 0 帧：

```gml
_OBJECT_CREATE {
    self = VM_GetCurCard()
    VM_SetProp(self, "idle_anim", 20)     // 待机循环 0..20
    VM_SetProp(self, "attack_anim", 16)   // 攻击循环 21..36（idle+attack 别超过总帧数）
    VM_SetProp(self, "flash_speed", 5)
    VM_SetProp(self, "state", 1)          // 0=待机 1=攻击
}
```

- 实例上现成可读：`weapon_id` / `weapon_info` / `atk` / `cycle` / `parent_player` /
  `grid_col` / `grid_row` / `timer` / `flash_speed` / `idle_anim` / `attack_anim` / `state`
- 发射物一般自己创建：`VM_CreateInstance("obj_bullet_mod", x, y)` + `mod_type`（见 [mod子弹.md](mod子弹.md)），
  或者更省的 `VM_BulletScreenAdd_Ex` / `VM_HomingBulletAdd`
- ⚠️ **JSON 里的自定义字段不会挂到实例上**；想用就在 `_OBJECT_CREATE` 里自己 `VM_SetProp` 设一份
- 副武器示例（JSON 里只有数据 + `.bin` 里只预加载贴图）：

```gml
// mod/weapons/master_shield.txt
_OBJECT_CFG {
    VM_LoadSpritePerm_Ex("tex/spr_master_shield_0.png", 1, 31, 53)
    VM_LoadSpritePerm_Ex("tex/spr_master_shield_icon_0.png", 1, 35, 35)
}
```
