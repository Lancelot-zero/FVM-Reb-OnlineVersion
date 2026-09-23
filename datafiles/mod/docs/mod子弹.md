# 子弹 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/bullets/my_bullet.json   ← 可以就是 {}
mod/bullets/my_bullet.bin    ← 逻辑全在这里
mod/bullets/my_bullet.txt    ← 源码（建议保留）
```

子弹**不进任何池**，只是给 `obj_bullet_mod` 提供一套同名逻辑（`mod_type` 就是文件名）。
所以 json 没有固定字段，`{}` 也行，但**必须有这个 json**，否则扫描不到。

## 怎么创建（两步）

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

也可以用"先写全局再创建"的写法（Create 时立刻初始化）：

```gml
VM_SetProp(0, "_mod_pending_bullet_id", "my_bullet")
b = VM_CreateInstance("obj_bullet_mod", x, y)
```

## 内置属性

| 属性 | 说明 |
|---|---|
| `mod_type` | 跑哪套逻辑 |
| `destroy_timer` | `-1`=不自动销毁（默认）；`>0`=每帧 −1，到 0 销毁；`0`=立即销毁 |
| `vx` / `vy` | 每帧位移。**Create 里就是 0，不设就停在原地** |
| `grid_col` / `grid_row` | 当前格子（每帧自动更新）；`prev_grid_col` / `prev_grid_row` 是上一帧 |
| `image_speed` | Create 里是 0，要动自己设 |

## 特别用法

- **碰撞要自己写**：`obj_bullet_mod` 没有碰撞事件（原版那些 `Collision_*` 联动一个都没有）。常用套路：
  - 找本格卡片：`VM_GetInstancesInRange("a", 行, 行, 列-1, 列+1, "card", "brazier")`
  - 打敌人：`VM_GetInstancesInRange("a", 行, 行, 列, 列, "enemy", "")` 再 `VM_DamageEnemy(...)`
  - 判"有没有敌人"（更省）：`VM_EnemyInRange(行1,行2,列1,列2,"类型")`
- **命中伤害走敌人受击事件**：`VM_DamageEnemy(敌人id, 伤害, 伤害类型)`（`normal` 有盾只打盾 /
  `pierce` 盾血一起掉 / 其它无视护盾），闪白、音效、护盾判定都对
- **要抛物线 / 自转 / 弹跳**：在 `_OBJECT_STEP` 里每帧重写 `vx` / `vy` / `image_angle`
- **更省的两种选择**（不占实例、管理器统一迭代与绘制，适合大量同质子弹）：
  - `VM_BulletScreenAdd_Ex(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字,伤害类型,标志数值,角度)`
    —— 18 个参数，带卡片效果位掩码
  - `VM_BulletScreenAdd_Exs(...)` —— 多两个**行渐变**参数（目标行 y、靠拢比例 0.15，原版水管弹那种）
  - `VM_HomingBulletAdd(...)` —— 会拐弯的追踪弹（每帧重新索敌、自转 6 度）
- 卡片要"打出子弹"：在卡片 STEP 里 `VM_PlaySound("snd_shot")` + 创建上面的东西即可
