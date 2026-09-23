# 敌人 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/enemies/my_mouse.json / .bin / tex/
```

## JSON 字段

| 字段 | 必填 | 默认 | 说明 |
|---|---|---|---|
| `spr` | ✅ | | 贴图名 |
| `name` | | 文件名 | 名字 |
| `hp` | | 100 | 血量 |
| `shield` | | 0 | 护盾值 |
| `speed` | | 0.3 | 移动速度 |
| `atk` | | 10 | 攻击力 |
| `cycle` | | 36 | 攻击间隔（帧） |
| `range` | | 90 | 攻击距离 |
| `ash_proof` | | false | 是否免疫灰烬（灰烬伤害接不下就一击必杀那条规则） |
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

## BIN 特别用法

可用块：`_OBJECT_CFG` / `_OBJECT_CREATE` / `_OBJECT_STEP` / `_OBJECT_DRAW` / `_OBJECT_DESTROY`

- **移动、攻击、受击表现都在 STEP 里自己写**；不写就是"站着不动、也不打人"
- 实例上现成属性：`hp` / `maxhp` / `x` / `y` / `grid_row` / `grid_col` / `state` /
  `ice_timer` / `is_frozen` 等（完整表见 `help.md` 的「敌人属性」）
- 被卡片打时走的是敌人自己的受击事件；插件想让别的敌人受伤用 `VM_DamageEnemy(敌人id, 伤害, 伤害类型)`
  （闪白 / 音效 / 护盾判定都对），灰烬那套用 `VM_DamageEnemyAsh(...)`
- 想"按行推进"就用 `VM_GetProp(self, "grid_row")` + 自己算 `x`；要带动画就设 `sprite_index` /
  `image_speed` / `image_index`
- 刷敌人：地图脚本里 `VM_SpawnEnemy("敌人id", 行, 血量)`（id 就是 `mod/enemies/` 下的文件名）
