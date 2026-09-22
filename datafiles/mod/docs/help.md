# LabMapCompiler 使用手册

## 部署到游戏

1. 在编辑器中点"编译"（Ctrl+B / F5），`.txt` 同目录下自动生成 `.bin`
2. 把 `.bin` 放到 `C:\Users\<用户名>\AppData\Local\FVM_Reborn\laboratory\`
3. 确保同目录有同名 `.json` 地图文件
4. 进入对应地图，`.bin` 自动加载
5. 修改脚本后替换 `.bin`，重新建房即可

---

## 语法

### 变量与运算

```
x = 10
y = x + 5
z = x * y
ok = (x > y)
```

支持 `+` `-` `*` `/` `%`，比较 `==` `!=` `>` `>=` `<` `<=`，逻辑 `&&` `||`。
优先级从低到高：`||` < `&&` < 比较 < `+ -` < `* / %`，括号可任意嵌套，例如
`(a && b || c) && (d || e)`。`&&` / `||` 是**短路**的（左边能定结果就不算右边），结果是 `1`/`0`。

**没有逻辑非 `!`**，请用 `x == 0` / `x != 0` 代替。

一行一句；`;` 视作换行（`x = 1; y = 2` 等同两行）。表达式必须在一行内写完，括号内也不允许跨行。
支持负数，包括负的浮点数（`-2.8804`）。

函数参数支持任意表达式：`VM_SetFlame(flame + 100)`、`VM_SetProp(id, "atk", atk * 2)` 直接可用。

**跨块变量**：不同块中同名变量共用同一个内存槽，可跨事件传值。例：`_VM_BATTLE_START` 中写 `cnt = 0`，`_VM_PLATFORM_IDLE_END` 中直接读写 `cnt`。

**保留字**：`if` `elif` `else` `while` `break` `continue` `halt` `exit` 不能用作变量名。`exit` 与 `halt` 同义（结束当前块执行）。

### 注释

```
// 这是注释
```

### 条件

花括号必须写。支持 `elif` 链和 `else`：

```
if (wave == 0) {
    VM_SpawnEnemy("normal_mouse", 0, 100)
} elif (wave == 1) {
    VM_SpawnEnemy("normal_mouse", 0, 200)
} elif (wave == 2) {
    VM_SpawnBoss("arno", 2, 80000)
} else {
    VM_ShellPrint("后续波次")
}
```

`else if` 与 `elif` 等价，可以混用。`elif`/`else` 绑定最近一个刚闭合的 `if`；游离的 `elif`/`else` 编译报错。

### 循环

花括号必须写。`while` 每次迭代重新求值条件；`break` 跳出最近一层循环；`continue` 跳到条件重算：

```
i = 0
while (i < 10) {
    i = i + 1
    if (i == 5) {
        continue
    }
    if (i == 8) {
        break
    }
    VM_ShellPrint("第 ", i, " 次")
}
```

`break`/`continue` 只能写在 `while` 循环内，循环外使用编译报错。⚠️ VM 没有执行步数上限，死循环会卡死游戏，请确保条件最终会变为假。

---

## 事件块

在块里写代码，游戏到对应时机自动执行。所有块可选。

| 块 | 触发时机 |
|---|---|
| `_VM_ROOM_READY_ENTRY` | 进入准备室（设规则） |
| `_VM_BATTLE_START` | 战斗开始（造地图） |
| `_VM_WAVE_START` | 新一波开始，用 `VM_GetWave()` 拿波数 |
| `_VM_WAVE_END` | 当前波结束，用 `VM_GetWave()` 拿波数 |
| `_VM_SUBWAVE_START` | 新子波开始，用 `VM_GetSubwave()` 拿子波数 |
| `_VM_SUBWAVE_END` | 子波结束，用 `VM_GetSubwave()` 拿子波数 |
| `_VM_CARD_CREATED` | 卡片被种下，用 `VM_GetLastCreatedCard()` 拿实例 |
| `_VM_CARD_DESTROYED` | 卡片被销毁，用 `VM_GetLastDestroyedCard()` 拿实例，用 `VM_GetKilledProp("属性名")` 读销毁瞬间属性快照 |
| `_VM_CARD_DAMAGED` | 卡片受伤 |
| `_VM_CARD_PREVIEW_PICKED` | 卡片被选取到手槽，用 `VM_GetPreviewCard()` 拿 card_id |
| `_VM_ENEMY_SPAWNED` | 敌人出现，用 `VM_GetLastCreatedEnemy()` 拿实例 |
| `_VM_ENEMY_KILLED` | 敌人死亡，用 `VM_GetLastKilledEnemy()` 拿实例，用 `VM_GetKilledProp("属性名")` 读销毁瞬间属性快照 |
| `_VM_ENEMY_DAMAGED` | 敌人受伤 |
| `_VM_BOSS_STATE_CHANGE` | BOSS 状态改变，用 `VM_GetLastBossStateChangeId()` / `VM_GetLastBossOldState()` / `VM_GetLastBossNewState()` 拿实例与新旧状态 |
| `_VM_PLAYER_DAMAGED` | 玩家受伤 |
| `_VM_PLATFORM_IDLE_END` | 平台空闲结束，用 `VM_GetLastIdlePlatform()` 拿实例 |
| `_VM_MOUSE_LEFT` | 鼠标左键按下（单帧） |
| `_VM_MOUSE_RIGHT` | 鼠标右键按下（单帧） |
| `_VM_KEY_PRESSED` | 键盘按键按下（单帧），用 `VM_GetKeyPressed()` 判断 |
| `_VM_BUTTON_CLICKED` | 按钮被点击，用 `VM_GetLastClickedButton()` 拿实例 |
| `_VM_FRAME` | ⚠️ 每帧执行，禁止写复杂逻辑 |
| `_VM_TIMER_5f` | 每 5 帧 |
| `_VM_TIMER_10f` | 每 10 帧 |
| `_VM_TIMER_15f` | 每 15 帧 |
| `_VM_TIMER_30f` | 每 30 帧 |
| `_VM_TIMER_60f` | 每 60 帧 |

---

## 函数

> 参数类型：`int` = 整数，`float` = 浮点数，`string` = 字符串（双引号）。`-1` 通常表示"全部"或"默认"。

### 规则设置

| 函数 | 说明 |
|---|---|
| `VM_BanCard("卡名")` | 禁用卡片 |
| `VM_BanGem("宝石名")` | 禁用宝石 |
| `VM_BanAllCard()` | 禁用全部卡片（遍历全游戏卡片注册表） |
| `VM_CannelBanCard("卡名")` | 解除指定卡片的禁用 |
| `VM_BanWeapon()` | 禁止角色武器使用 |
| `VM_BanSuperWeapon()` | 禁止角色超级武器使用 |
| `VM_BanShield()` | 禁止角色盾牌使用 |
| `VM_SetCardLevelCap(等级)` | 卡片等级上限 |
| `VM_SetCardShapeCap(等级)` | 最大转职（shape）等级限制，`-1`=不限制 |
| `VM_SetCardSkillCap(等级)` | 最大技能等级限制，`-1`=不限制 |
| `VM_SetMaxSlots(数量)` | 最大携带卡片数 |
| `VM_SpawnCats(0或1)` | 是否生成初始猫，默认 1 |
| `VM_SetWaveAuto(0或1)` | 波次自动推进开关，0=关闭后不自动出怪，由脚本 VM_SetWave 控制 |
| `VM_SetWave(波次,子波次)` | 设置当前波次/子波次（只改计数不出怪） |
| `VM_SetShovelFlameRate(系数)` | 铲子铲卡返还火苗系数，`-1`=原逻辑，`0~1`=直接用该系数 |

### 地图与地形

| 函数 | 说明 |
|---|---|
| `VM_SetTerrain(列, 行, "类型")` | 设地形。`-1`=全部。类型: `"normal"` `"water"` `"obstacle"` |
| `VM_SetRowFeature(行, "属性")` | 改行属性: `"land"` `"water"`，`-1`=所有行 |
| `VM_GetTerrain(列, 行)` | 获取地形: 0=normal 1=water 2=obstacle -1=超出 |

### 创建

| 函数 | 参数 | 说明 |
|---|---|---|
| `VM_CreatePlatform(列,行,宽,高,轴向,距离,停顿帧,贴图)` | 8个 | 创建平台。轴向: 0=上下 1=左右 |
| `VM_SpawnPlant("卡名",列,行,外形,星级,技能)` | 6个 | 种植物。`-1`=整行/整列 |
| `VM_SpawnEnemy("敌人类型",行,血量)` | 3个 | 刷敌人 |
| `VM_SpawnBoss("BOSS类型",行,血量)` | 3个 | 刷 BOSS |
| `VM_SpawnObject("物件名",列,行)` | 3个 | 刷对象: `obstacle` `lava` `wind_tunnel` `mouse_hole` 等（含 boss 产物） |
| `VM_SpawnBatMouse(列,行)` | 2个 | 在指定格子生成蝙蝠鼠（含降落标记），返回实例 ID |
| `VM_SpawnPlantsRandom(x,y,w,h, shape,level,skill, card1,...,card9)` | 16个 | 区域内随机种植。每格随机选卡，只种空格。后 9 个参数为卡片名，`"-1"`（字符串）=跳过。客户端不执行 |
| `VM_CreateButton(x,y,"精灵名",缩放, idle, hover, press)` | 7个 | 创建按钮。后三帧 `-1`=默认(0,1,2)。点击触发 `_VM_BUTTON_CLICKED` |
| `VM_CreateInstance("对象名",x,y)` | 3个 | 按像素坐标创建实例（子弹等） |
| `VM_BulletScreenAdd(贴图,总帧数,打击范围,缩放,动画速度,x,y,vx,vy,伤害,伤害计数,存活帧数,子弹类型,销毁对象,mod名字)` | 15个 | 往屏幕弹幕管理器塞一颗子弹。管理器 `obj_Bullet_Screen_Management` 由 `obj_battle` 开局创建，全场只此一个实例，所有弹幕统一迭代与绘制、**不占实例**。`打击范围`=格子半径，0=只算本格；`伤害计数`=最多命中几个，`-1`=不限次数（此时范围内所有可命中敌人都结算）；`存活帧数`=倒计时，`<=0`=不自动消失；`子弹类型`同 `can_hit`（all/normal/air/air_only/pierce/track/throw/rotate/d_fruit）；`销毁对象`非空则在该子弹消失处创建它，是 mod 对象时用 `mod名字` 指定 `mod_type`。出界（x<0 / x>2200 / y<0 / y>1200）静默删除、不生成销毁对象 |

### 属性

| 函数 | 说明 |
|---|---|
| `VM_GetProp(实例ID, "属性名")` | 读实例属性。字符串属性返回字符串，浮点属性返回浮点 |
| `VM_GetKilledProp("属性名")` | 任意 | 读当前销毁事件对象的属性快照（仅 _VM_CARD_DESTROYED / _VM_ENEMY_KILLED 期间有效；mouse_id 空串时按对象名兜底） |
| `VM_SetProp(实例ID, "属性名", 值)` | 改实例属性，联机同步。特殊属性 `"object_name"`：把实例切换为指定对象 |
| `VM_SetCardProp(列,行,"卡名","属性",值)` | 按格子和卡名改属性。`"all"`=全部 |
| `VM_SetEnemyProp("类型","属性",值)` | 按敌人类型改属性。`"all"`=全部（跳过 BOSS） |

**平台属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `move_axis` | string | `"y"`=上下 `"x"`=左右 |
| `move_distance` | int | 移动距离（格） |
| `move_direction` | int | 1=正向 -1=反向 |
| `current_offset` | int | 当前偏移（格） |
| `boundary_idle_duration` | int | 边界停顿（帧） |
| `start_col` / `start_row` | int | 初始格子 |
| `width` / `length` | int | 平台尺寸（格） |

**植物属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `hp` / `max_hp` | int | 当前/最大血量 |
| `atk` | int | 攻击力 |
| `range` | int | 攻击范围 |
| `attack_timer` | int | 攻击计时器 |
| `attack_cycle` | float | 攻击循环周期 |
| `cooldown` | int | 冷却时间 |
| `cooldown_timer` | int | 冷却计时器 |
| `frozen_timer` / `ice_timer` | int | 冰冻剩余帧数 |
| `is_frozen` | int | 0=否 1=是冻结中 |
| `awake_buff_timer` | int | 唤醒加速计时器 |
| `invincible` | int | 0=否 1=是无敌 |
| `plant_type` | string | 层级: `"normal"` `"shield_inner"` `"lilypad"` `"shield_outer"` `"coffee"` |
| `plant_id` | string | 卡片 ID，如 `"coffee_bean"` |
| `current_level` | int | 星级 |
| `skill` | int | 技能等级 |
| `shape` | int | 外形 |
| `state` | int | 状态 (CARD_STATE) |
| `cost` | int | 阳光消耗 |
| `x` / `y` | float | 世界坐标（像素） |
| `grid_col` / `grid_row` | int | 格子坐标（列,行） |
| `depth` | float | 绘制深度 |
| `can_shovel_remove` | int | 0=否 1=是可铲除 |

**敌人属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `hp` / `maxhp` | int | 当前/最大血量 |
| `atk` | int | 每次攻击伤害 |
| `atk_cycle` | int | 攻击间隔（帧） |
| `move_speed` / `move_speed_modify` | float | 移动速度 / 速度修正 |
| `attack_range` | int | 攻击范围 |
| `state` | int | 状态 (ENEMY_STATE) |
| `target_plant` | int | 当前攻击目标植物 ID |
| `target_type` | string | 目标类型: `"normal"` `"air_only"` `"ground_only"` |
| `mouse_id` | string | 敌人类型 ID |
| `is_boss` | int | 0=否 1=是 BOSS |
| `helmet_hp` / `helmet_max_hp` | int | 头盔血量 |
| `shield_hp` / `shield_max_hp` | int | 护盾血量 |
| `hurt_rate` | float | 进入受伤状态的血量比例 |
| `ice_timer` / `frozen_timer` | int | 冰冻剩余帧数 |
| `is_frozen` | int | 0=否 1=是冻结 |
| `is_stun` / `stun_timer` | int | 是否眩晕 / 眩晕剩余帧 |
| `immune_to_ash` | int | 0=否 1=是免疫煤渣 |
| `x` / `y` | float | 世界坐标（像素） |

### 查询

| 函数 | 返回 | 说明 |
|---|---|---|
| `VM_GetWave()` | int | 当前波数 |
| `VM_GetSubwave()` | int | 当前子波数 |
| `VM_GetFlame()` | int | 火苗数 |
| `VM_GetEnemyCount()` | int | 场上敌人数量 |
| `VM_GetPlantCount()` | int | 场上植物数量 |
| `VM_GetPlantCountAt(列,行,"类型")` | int | 指定格子某类型数量。`"all"`=全部 |
| `VM_GetPlantAt(列,行,"层级")` | int | 指定格子指定层第一个植物实例 ID。层级: `"normal"` `"shield_inner"` `"lilypad"` `"shield_outer"` `"coffee"`，`"all"`=任意。没找到返回 -1 |
| `VM_GetLastBoss()` | int | 最后刷的 BOSS ID |
| `VM_GetLastCreatedEnemy()` | int | 最后刷的敌人 ID |
| `VM_GetLastKilledEnemy()` | int | 最后死的敌人 ID |
| `VM_GetKilledProp("属性名")` | 任意 | 当前销毁事件对象的属性快照（仅销毁类 hook 期间有效） |
| `VM_GetLastCreatedCard()` | int | 最后种的卡片 ID |
| `VM_GetLastDestroyedCard()` | int | 最后销毁的卡片 ID |
| `VM_GetLastIdlePlatform()` | int | 最后结束空闲的平台 ID |
| `VM_GetLastClickedButton()` | int | 最后点击的按钮 ID，没有返回 -1 |
| `VM_GetLastBossStateChangeId()` | int | 最后改变状态的 BOSS ID |
| `VM_GetLastBossOldState()` | int | 最后改变状态的 BOSS 的旧状态 |
| `VM_GetLastBossNewState()` | int | 最后改变状态的 BOSS 的新状态 |
| `VM_GetCurCard()` | int | 当前块所属的 mod 卡实例 id |
| `VM_EnemyInRange(行1,行2,列1,列2,"类型")` | int | 矩形范围是否存在敌人（前缀和）。行列都是闭区间、可反序、自动夹取到场内。类型: normal/obstacle/diver/air/dance/underground，`""`或`"all"`=任意 |
| `VM_GetHomingTarget("类型")` | int | 最左且血量最高的敌人 id（追踪索敌），类型 ""/all=任意，每帧全场只扫一次 |
| `VM_GetInstancesInRange("数组名",行1,行2,列1,列2,"enemy"或"card","筛选")` | int | 收集范围内实例 id 进指定 VM 命名数组（会先清空），返回数量。筛选：敌人按 target_type，卡片按 plant_id/plant_type，`""`或`"all"`=不限 |
| `VM_GetCardProp("卡名","属性名")` | 任意 | 按属性名读一张卡的**单值**。存档类（玩家自己那张卡的进度）：`"shape"` `"level"` `"skill"` `"max_level"` `"max_shape"`；卡池类（卡片本身的配置）：`"plant_type"` `"feature_type"` `"target_card"` `"cost"` `"cooldown"`。读不到返回 undefined，用 `VM_IsUndefined` 判断 |
| `VM_CanPlace("卡名",列,行)` | int | 按**游戏正规种植规则**判断该格能不能种这张卡——地形、障碍、水域/莲叶、护盾层、底座卡、替换开关全都算进去（就是玩家手牌点下去时走的那套），1=能 0=不能 |
| `VM_CallFunc("函数名", 参数...)` | 任意 | 按名字调用**独立字典**（`global._VM_call_dict`）里的函数，返回它的返回值。**名字对编译器只是字符串、不校验**，所以往字典里加函数不用改编译器、不用重编编译器、不用同步编辑器。第一个参数是函数名，后面是实参，最多 15 个；名字不在字典里返回 undefined（先 `VM_FuncExists` 判断） |
| `VM_FuncExists("函数名")` | int | 字典里有没有这个函数，1=有 0=没有（`VM_CallFunc` 的配套） |
| `VM_FuncDesc("函数名")` | string | 返回字典里登记的该函数说明字符串，没登记返回空串（`VM_CallFunc` 的配套） |
| `VM_SpriteExists("贴图名")` | int | 这张贴图**现在真的可用吗**——按解析链查：项目资源 → VM 临时缓存 → VM 永久缓存 → 全局贴图缓存。注意 `get_load_sprite` 找不到时会塞一张**空白占位图**且永不失败，所以本函数会额外排除占位图，返回 1=可用 0=不可用 |
| `VM_AliasSpritePerm("新名","已有名")` | - | 把新名**永久**指向已有名（写永久缓存，**进房间不会被清**）。已有名必须已在永久缓存里（`VM_LoadSpritePerm` / `_Ex` 加载过）。存的是**精灵 id**，所以 `get_load_sprite` 直接返回真 id，可以喂给 `sprite_index`。`[reloadmod]` 时会自动释放所有这类别名（底下的真图不动），让重新加载的 bin 再挂一次。配合 `VM_SpriteExists` 就是"内置有就用内置、没有才外置覆盖" |

### BOSS 状态（BOSS_STATE）

`_VM_BOSS_STATE_CHANGE` 事件及 `VM_GetLastBossOldState()` / `VM_GetLastBossNewState()` 返回的状态数值即下表枚举（从 0 开始递增）：

| 状态 | 数值 | 说明 |
|---|---|---|
| `APPEAR` | 0 | 登场：BOSS 出现/现身动画，播完进入 IDLE |
| `IDLE` | 1 | 待机：站立等待，计时结束后随机选取技能 |
| `SKILL1` | 2 | 释放技能 1（具体效果因 BOSS 而异） |
| `SKILL2` | 3 | 释放技能 2 |
| `SKILL3` | 4 | 释放技能 3 |
| `SKILL4` | 5 | 释放技能 4 |
| `MOVE` | 6 | 移动：BOSS 在地图上行进 |
| `LAUNCH` | 7 | 升空：起飞离场（如安琪拉宝贝起飞） |
| `DROP` | 8 | 降落：从空中落回地面 |
| `DISAPPEAR` | 9 | 消失：隐身离场，随后可能在别处重新登场 |
| `STUN` | 10 | 眩晕：被控制，无法行动 |
| `DEATH` | 11 | 死亡：播放死亡动画，动画结束后销毁 |

### 数组

命名数组按名存取，跨块共享，每局（进房间）重置；值可为 int / float / string。

| 函数 | 返回 | 说明 |
|---|---|---|
| `VM_ArrayGet("数组名",下标)` | any | 读元素。数组不存在或下标越界返回 0 |
| `VM_ArraySet("数组名",下标,值)` | - | 写元素。数组不存在自动创建；下标超出当前长度自动补 0 扩容 |
| `VM_ArrayDel("数组名",下标)` | - | 删除元素，后面的元素前移；越界无操作 |
| `VM_ArrayADD("数组名",值)` | - | 末尾追加一个元素；数组不存在自动创建 |
| `VM_ArraySize("数组名")` | int | 数组长度；数组不存在返回 0 |
| `VM_ArrayClear("数组名")` | - | 清空数组，长度归 0；数组不存在无操作 |
| `VM_ArrayClearAll()` | - | 清空所有数组 |

### 鼠标与键盘

| 函数 | 返回 | 说明 |
|---|---|---|
| `VM_GetMouseX()` | int | 鼠标世界 X 坐标 |
| `VM_GetMouseY()` | int | 鼠标世界 Y 坐标 |
| `VM_GetMouseCol()` | int | 鼠标所在网格列 |
| `VM_GetMouseRow()` | int | 鼠标所在网格行 |
| `VM_GetMousePressed(按键)` | int | 按下返回 1。1=左 2=右 3=中 |
| `VM_GetKeyDown("键名")` | int | 按键按住返回 1 |
| `VM_GetKeyPressed("键名")` | int | 按键刚按下返回 1（单帧有效） |

> **键名**：字母 `"A"`~`"Z"`，数字 `"0"`~`"9"`，功能 `"f1"`~`"f12"`，方向 `"up"` `"down"` `"left"` `"right"`，特殊 `"space"` `"enter"` `"escape"` `"tab"` `"shift"` `"ctrl"` `"alt"` `"backspace"` `"delete"` `"home"` `"end"` `"pageup"` `"pagedown"`。不区分大小写。

### 区域操作

| 函数 | 说明 |
|---|---|
| `VM_ClearMapObjects(列,行,"物件名")` | 清除地图物件。`-1`=全部行列，`"all"`=全部物件 |
| `VM_ClearPlants(列,行)` | 清除格子植物。`-1`=全部 |
| `VM_ClearPlantsByType("卡名")` | 按卡名清除。`-1`=全部（跳过角色） |
| `VM_WakePlants(列,行)` | 唤醒睡眠卡片。`-1`=全部 |
| `VM_SwapPlants(列1,行1,列2,行2)` | 交换两格植物 |
| `VM_SwapPlantRects(x1,y1,w,h, x2,y2)` | 交换两个等大矩形区域植物 |
| `VM_CompactColumn(列)` | 列向上压缩。`-1`=所有列 |
| `VM_CompactColumnRev(列)` | 列向下压缩 |
| `VM_CompactRow(行)` | 行向左压缩。`-1`=所有行 |
| `VM_CompactRowRev(行)` | 行向右压缩 |

### 贴图加载

**临时加载**（bin 重载时自动清理）：

| 函数 | 参数 | 说明 |
|---|---|---|
| `VM_LoadSprite("文件名")` | 1个 | 加载单帧贴图到临时缓存 |
| `VM_LoadSpriteFrames("文件名", 帧数)` | 2个 | 加载多帧贴图，自动均分切割 |
| `VM_LoadSpriteFrames_Ex("文件名", 帧数, x, y)` | 4个 | 加载多帧贴图并指定原点，-1=默认0 |
| `VM_GetLoadedSpriteName(序号)` | 1个 | 获取已加载贴图文件名，越界返回 "" |
| `VM_AliasSprite("新名", "已有名")` | 2个 | 将新名指向已有贴图缓存，当已有名不存在时无操作 |

**永久加载**（bin 重载不会清理，用 `VM_FreeSpritePerm` 手动释放）：

| 函数 | 参数 | 说明 |
|---|---|---|
| `VM_LoadSpritePerm_Ex("文件名", 帧数, 原点X, 原点Y)` | 4个 | 永久缓存贴图并指定原点，`-1`=默认0，不被 bin 重载清理 |


### 平台

| 函数 | 说明 |
|---|---|
| `VM_SetPlatformParams(实例,轴,距离,停顿,方向)` | 重设平台移动参数 |
| `VM_RefreshPlatformSnapshots()` | 刷新平台快照 |

### 绘制

| 函数 | 说明 |
|---|---|
| `VM_SetMapBackground("贴图名", 步长)` | 渐变切换背景。步长如 0.02 |
| `VM_SetDrawSlot_front(槽位,"贴图名",x,y,alpha)` | 设置前景绘制槽。槽位 0~63，alpha 0~1，贴图名为空清除。配合 `_VM_FRAME` |
| `VM_SetDrawSlot(槽位,"贴图名",x,y,alpha)` | 设置背景绘制槽。同上，绘制在火焰 UI 后面的背景层 |
| `VM_SetDrawSlotEx(槽位,"贴图名",x,y,alpha,角度,xscale,yscale)` | 同上，可设置角度/缩放（-1 用默认值） |
| `VM_SetDrawSlotEx_front(槽位,"贴图名",x,y,alpha,角度,xscale,yscale)` | 同上，前景槽 |
| `VM_DrawSpriteExt("贴图名",子图,x,y,xscale,yscale,角度,alpha)` | 直接画一张贴图（等价 `draw_sprite_ext`，颜色固定白色）。**只能在 `_OBJECT_DRAW` 块里调用**，在 Step 里调画不出来。贴图名走 `get_load_sprite`，`VM_LoadSpritePerm_Ex` 加载的也能直接用 |

### 工具

| 函数 | 说明 |
|---|---|
| `VM_Random(min, max)` | 随机整数 [min, max] |
| `VM_SetFlame(数量)` | 设置火苗数 |
| `VM_PlaySound("音效名")` | 播放内置音效: `"snd_place1"` `"snd_card_lift"` 等 |
| `VM_LoadSound("路径")` | 加载本地音频文件（只支持 wav），返回音频ID，失败返回 -1（暂时不能使用，等待修复） |
| `VM_Floor(值)` | 对数字向下取整，非数字返回 undefined |
| `VM_Ceil(值)` | 对数字向上取整，非数字返回 undefined |
| `VM_GetTimeLimit()` | 获取关卡倒计时剩余帧数，无倒计时返回 undefined , 60 帧为1s|
| `VM_SetTimeLimit(帧数)` | 设置关卡倒计时帧数，无倒计时返回 undefined，联机自动同步, 60 帧为1s |
| `VM_IsUndefined(值)` | 判断值是否为 undefined，返回 1=是 0=不是 |
| `VM_IsDestroyed(实例ID)` | 判断实例是否已被销毁，返回 1=已销毁 0=存在 |
| `VM_DestroyInstance(实例ID)` | 直接销毁实例（会触发它的 Destroy 事件）。在 `_OBJECT_STEP` 里销毁自己时，本帧剩下的语句还会继续执行，要用 `VM_IsDestroyed` 兜底 |
| `VM_RunStep(实例, 轮数)` | 让该实例额外跑 N 轮 Step（一轮 = Begin Step + Step + End Step），用来做加速。同一个实例正在被重跑时再调会被忽略（防无限递归）。轮数夹在 1~60，返回实际跑了几轮 |
| `VM_SetEventEnabled(0或1)` | 事件系统开关，默认 1 |
| `VM_GameWin()` | 触发胜利 |
| `VM_GameLose()` | 触发失败 |

### 内置音效（VM_PlaySound 可用，仅 snd）

| 音效名 | 描述 |
|---|---|
| `snd_bottle_explode` | 瓶子爆炸 |
| `snd_bullet_burnt` | 子弹烧焦 |
| `snd_bullet_explode` | 子弹爆炸 |
| `snd_button` | 按钮点击 |
| `snd_card_lift` | 卡牌抬起 |
| `snd_chomp1` | 啃咬 1 |
| `snd_chomp2` | 啃咬 2 |
| `snd_chomp3` | 啃咬 3 |
| `snd_coffee_cup_attack` | 咖啡杯攻击 |
| `snd_coffee_pot_attack` | 咖啡壶攻击 |
| `snd_coke_bomb_explode` | 可乐炸弹爆炸 |
| `snd_egg_bullet` | 鸡蛋子弹 |
| `snd_enter_water` | 入水 |
| `snd_fire_hit` | 火苗命中 |
| `snd_flame_collect` | 火苗收集 |
| `snd_flour_sack` | 面粉袋 |
| `snd_flour_sack_find` | 面粉袋发现 |
| `snd_goblet_lamp_grow` | 高脚灯生长 |
| `snd_hamburger_eat` | 汉堡被吃 |
| `snd_hit1` | 受击 1 |
| `snd_hit2` | 受击 2 |
| `snd_hit3` | 受击 3 |
| `snd_kettle_bomb_explode` | 水壶炸弹爆炸 |
| `snd_lobster_cannon` | 龙虾炮 |
| `snd_lose` | 失败 |
| `snd_mouse_clip_explode` | 老鼠夹爆炸 |
| `snd_mouse_clip_ready` | 老鼠夹就绪 |
| `snd_mouse_explode` | 老鼠爆炸 |
| `snd_mouse_frozen` | 老鼠冰冻 |
| `snd_mouse_unfreeze` | 老鼠解冻 |
| `snd_mouse_wave_attack` | 老鼠波浪攻击 |
| `snd_place1` | 放置 1 |
| `snd_place2` | 放置 2 |
| `snd_salad_pilt_splash` | 沙拉叉溅射 |
| `snd_shot` | 射击 |
| `snd_shovel` | 铲子 |
| `snd_throw` | 投掷 |
| `snd_win` | 胜利 |
| `snd_wooden_cork` | 木塞 |

### 输出

| 函数 | 说明 |
|---|---|
| `VM_ShellPrint(...)` | 控制台输出（最多 16 个参数，自动拼接） |
| `VM_ShowNotice(...)` | 屏幕中央通知（参数个数可变） |
| `VM_ShowNoticeDur("消息", ..., 帧数)` | 自定义时长的屏幕通知，最后一个参数为帧数 |
| `VM_SetNoticeStyle(缩放, R, G, B)` | 设置公告样式：缩放浮点、颜色 0-255，-1 表示该项使用默认 |

### 卡槽操作

| 函数 | 说明 |
|---|---|
| `VM_SetCardSlotProp("name", "prop", val)` | 修改卡槽属性。name: `"all"`/`-1`=全部, 数字=指定 slot_index, 字符串=匹配 card_id |
| `VM_CalcCardSlotProp("name", "prop", op, val)` | 卡槽属性四则运算。op: 0=加 1=减 2=乘 3=除（仅数值属性生效） |
| `VM_GetCardSlotCount()` | 获取卡槽数量 |
| `VM_GetPreviewCard()` | 获取当前手牌 card_id，没有返回 -1 |
| `VM_conveyor_belt_able(0或1)` | 传送带模式开关：开启后卡槽按传送带排列，卡组不自动生成卡槽，用 `VM_Slot_add` 手动加卡 |
| `VM_Slot_add("卡名")` | 向传送带队列末尾添加一张卡（卡槽），游戏内注册的任意卡片均可，与玩家是否解锁无关；返回卡槽实例 ID |

**可修改的卡槽属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `cooldown` | int | 冷却时间（帧） |
| `cooldown_timer` | int | 冷却计时器（帧） |
| `cost` | int | 原始阳光消耗 |
| `vm_current_cost` | int | 当前阳光消耗（`-1`=不覆盖；`current_cost` 每帧由它重算，改价格请改这个） |

**其余属性直接改不生效**（每帧被游戏逻辑覆盖或只读）：`current_cost`、`is_ready`、`is_selected`、`clevel`、`cshape`、`cskill`、`hover_alpha`、`cooling_alpha`、`preview_alpha`、`slot_index`（只读）、`card_id`（只读）

> `VM_CalcCardSlotProp` 仅对可修改的数值属性生效（cooldown / cooldown_timer / cost / vm_current_cost）。

---

## 完整示例

```
// ==================================
//  准备室阶段
//  进入准备室时执行，用于设置游戏规则
// ==================================
_VM_ROOM_READY_ENTRY {
    // 禁用"小火苗"卡片，玩家将无法携带此卡
    VM_BanCard("small_fire")
    // 卡片星级上限设为 10，无法再往上强化
    VM_SetCardLevelCap(10)
    // 玩家最多携带 10 张卡片出战
    VM_SetMaxSlots(10)
}

// ==================================
//  战斗开始
//  战斗正式开始时执行一次，用于造地图和初始化
// ==================================
_VM_BATTLE_START {
    // 全图设为障碍地形，-1 表示"全部列/行"
    VM_SetTerrain(-1, -1, "obstacle")
    // 开局给 3000 火苗
    VM_SetFlame(3000)

    a_cnt = 0       // 平台 a 的状态计数器，控制移动模式轮换
    // 创建平台 a：列0行0、宽4高5、上下移动(0)、移动距离0(暂不移动)、停顿0帧、贴图"spr_raft_platform"
    a = VM_CreatePlatform(0, 0, 4, 5, 0, 0, 0, "spr_raft_platform")

    // 创建平台 b1：列6行0、宽5高1、左右移动(1)、移动1格、边界停顿480帧、贴图"spr_rainbow_1"
    b1 = VM_CreatePlatform(6, 0, 5, 1, 1, 1, 480, "spr_rainbow_1")
}

// ==================================
//  卡片被种下
//  玩家种下一张卡片时触发，在此修改植物属性
// ==================================
_VM_CARD_CREATED {
    // 获取刚种下的植物实例 ID
    id = VM_GetLastCreatedCard()
    // 读取植物卡片名
    name = VM_GetProp(id, "plant_id")

    // 煮鸡蛋攻击力翻倍（参数支持任意表达式）
    if (name == "egg_boiler_pult") {
        VM_SetProp(id, "atk", VM_GetProp(id, "atk") * 2)
    }

    // 咖啡壶额外加血
    if (name == "coffee_pot") {
        VM_SetProp(id, "hp", VM_GetProp(id, "hp") + 500)
    }
}

// ==================================
//  平台空闲结束
//  平台到达边界并完成停顿时触发，在此控制平台移动
// ==================================
_VM_PLATFORM_IDLE_END {
    // 判断触发事件的平台是否为木筏 a
    if (VM_GetLastIdlePlatform() == a) {
        // 循环切换 4 种移动模式：
        //   a_cnt=0: 上下移动，移 2 格，停顿 200 帧，正向(1)
        //   a_cnt=1: 左右移动，移 2 格，停顿 200 帧，正向(1)
        //   a_cnt=2: 上下移动，移 2 格，停顿 200 帧，反向(-1)
        //   a_cnt=3: 左右移动，移 2 格，停顿 200 帧，反向(-1)
        if (a_cnt == 0) { VM_SetPlatformParams(a, 0, 2, 200, 1) }
        if (a_cnt == 1) { VM_SetPlatformParams(a, 1, 2, 200, 1) }
        if (a_cnt == 2) { VM_SetPlatformParams(a, 0, 2, 200, -1) }
        if (a_cnt == 3) { VM_SetPlatformParams(a, 1, 2, 200, -1) }
        // 状态 +1，取模 4 实现 0→1→2→3→0 循环
        a_cnt = (a_cnt + 1) % 4
    }
}
```

---

## 参数校验附录

编译器校验以下函数的字符串参数。参数个数/类型/取值校验不通过时**编译直接失败**并报告行号（旧版本仅打印警告、继续产出字节码）。int 实参传入 float 参数时自动提升，不算错误。

### 敌人 ID（134 个）

```
normal_mouse  football_fan_mouse  iron_pan_mouse  skateboard_mouse
landlady_mouse  zombie_with_flower_pot  machine_mouse  ninja_mouse
minion_mouse  kangaroo  repairman_mouse  diver_mouse  paper_boat_mouse
duck_mouse  tropical_fish_mouse  lambo_mouse  butterfly_mouse
taro_toho_mouse  water_taro_toho_mouse  assault_mouse  frog_prince_mouse
roller_skating_mouse  giant_mouse  mario_mouse  arno  temple_pharaoh
engineering_vehicle_mouse  garbage_track_mouse  mole  glider_mouse
ice_residue  bat_mouse  rumble  abyss_pharaoh  cucumber_paper_boat_mouse
apple_duck_mouse  egg_tropical_fish_mouse  orange_prince_mouse
submarine_mouse  rowboat_mouse  water_penguin_mouse  pink_paul
cucumber_normal_mouse  apple_football_fan_mouse  egg_iron_pan_mouse
tangerine_skateboard_mouse  shy_landlady_mouse  zombie_with_wallnut
caribbean_mouse  penguin_mouse  arson_mouse  non_mainstream_mouse
flute_mouse  panda_mouse  can_mouse  blonde_mary  pete
dragon_boat_mouse  flagship_mouse  thug_submarine_mouse
kof_submarine_mouse  soar_mouse  jet_mouse  dentist_mouse
sawblade_mouse  warrior_mouse  naruto_mouse  hazelnut_cannon_mouse
landmine_vehicle_mouse  waste_flying_mouse  airbrone_explosive_mouse
priest_mouse  pope_mouse  wrestler_mouse  special_armour_mouse
magician_mouse  ghost_mouse  flight_barrier_mouse  hells_messenger
needle_baron  fog_julie  lieutenant_buzz  paratrooper_mouse
irritable_jack  hot_vajra  machine_normal_mouse  machine_football_fan_mouse
machine_iron_pan_mouse  machine_skateboard_mouse  machine_flag_mouse
mirror_mouse  trumpeter_mouse  huang_xiaoming  angelababy
mouse_train_1  soldier_mouse  machine_bomb_mouse  aircraft_carrier
kamikaze_glider_mouse  captain_america_mouse  iron_man_mouse
mouse_train_2  charge_spring_mouse  snail_mouse  machine_beehive_mouse
machine_bee  spider_man_mouse  hulk_mouse  mouse_train_3
clownfish_mouse  conch_mouse  eel_mouse  electric_jellyfish
hercules  iron_diver_mouse  lobster_knight  machine_shark_1
machine_shark_2  mermaid_mary  oyster_mouse  sardine_mouse
seahorse_mouse  swordfish_mouse  thor  undersea_can_mouse
undersea_captain_mouse  undersea_diver_mouse  undersea_panda_mouse
undersea_penguin_mouse  undersea_repairman_mouse  undersea_submarine_1
undersea_submarine_2  war_god  windmill_fish_mouse
```

### 卡片 ID（79 个）

```
xiao_long_bao  small_fire  toast_bread  flour_sack  double_long_bao
mouse_clip  coke_bomb  wooden_plate  ice_long_bao  goblet_lamp
coffee_cup  salad_pult  coffee_pot  chocolate_bread  water_tea_cup
ice_bucket_bomb  stinky_tofu_pult  cat_box  kettle_bomb  triple_wine_rack
brazier  large_fire  iron_fishbone  gatlin_long_bao  rotating_coffee_pot
takoyaki  wooden_cork  coffee_grounds  wine_bottle_bomb  double_water_pipe
melon_shield  steel_wool  sausage  fishbone  hamburger  oil_lamp
ventilation_fan  egg_boiler_pult  ice_egg_boiler_pult  chocolate_pult
chocolate_cannon  firework_dragon  double_ice_long_bao  cat_chest
cherry_pudding  skewer_bomb  gatlin_ice_long_bao  aquarius_elve
tar_sprayer  triple_long_bao  triple_ice_long_bao  hotdog_cannon
oden_pot  whisky_bomb  cotton_candy  durian  dragon_fruit
pineapple_explosive_bread  ice_cream  lightning_baguette  bull_firework
magic_chicken  xinjiang_fried_noodles  king_long_bao  king_triple_long_bao
chili_powder  tang_hu_lu  beef_hotpot  spicy_pot  pan_fried_bun
coal_starfish  curry_lobster_cannon  delicacy_firework  fruit_tart
horseshoe_crab_bread  pizza_oven  rabbit_lantern  soda_bubble
sugar_ball_pult
```

### 物件名（65 个）

```
obstacle  lava  seawater  wind_tunnel  barrier  mouse_hole
pharaoh_hole  buzz_wind  cloud  ladder  fog
arno_bullet  arno_bullet_effect  arson_bullet  blonde_mary_bullet
electric_jellyfish_bullet  hercules_laser  ice_residue_ball  ice_residue_bullet
iron_man_bullet  lobster_knight_bullet  mermaid_mary_bullet  paul_bullet
rumble_laser  rumble_missile  shark_1_bullet  mouse_train_1_bullet
mouse_train_2_bullet  julie_missile  pete_missile  pete_claw  pete_spike
baron_bats  baron_blade  baron_needle  messenger_ignis_fatuus  messenger_mace
messenger_poop  machine_shark_2_wind
angelababy_star  angelababy_summon  angelababy_target  arson_mouse
captain_rainbow  captain_shield  iron_man
irritable_jack_fire  irritable_jack_rock_skill_3  irritable_jack_rock_skill_4
mario_cave  mario_pipeline  mermaid_mary_music  mermaid_mary_wave
mouse_train_3_butter  mouse_train_3_explode  pharaoh_bandage  pharaoh_coffin
spider_man_mouse_web  vajra_lava  vajra_lightning  vajra_spike
war_god_duck  war_god_summon  war_god_wood  xiaoming_text
```

> 前 11 个为基础物件，其余为 boss 产物（子弹/弹道与召唤物/技能物件）。boss 本体（`pete` `aircraft_carrier` `mouse_train_*_body` 等）用 `VM_SpawnBoss`，小怪用 `VM_SpawnEnemy`。

### 宝石名（16 个）

| ID | 名称 | 槽位 | 可禁用 |
|---|---|---|---|
| `laser_gem` | 激光宝石 | 主武器 | ✓ |
| `bomb_gem` | 轰炸宝石 | 主武器 | ✓ |
| `cateye_gem` | 猫眼宝石 | 主武器 | ✓ |
| `freeze_gem` | 冰冻宝石 | 主武器 | ✓ |
| `flame_recover_gem` | 回火宝石 | 主武器 | ✓ |
| `starlight_gem` | 星光宝石 | 主武器 | ✓ |
| `attack_gem` | 攻击宝石 | 主武器 | |
| `gale_gem` | 疾风宝石 | 超级武器 | |
| `power_gem` | 强力宝石 | 超级武器 | |
| `transform_gem` | 转化宝石 | 超级武器 | |
| `health_gem` | 生命宝石 | 副武器 | |
| `produce_gem` | 生产宝石 | 副武器 | |
| `slow_down_gem` | 迟缓宝石 | 副武器 | |
| `bleed_gem` | 流血宝石 | 副武器 | |
| `guard_gem` | 守护宝石 | 副武器 | |
| `strength_gem` | 蓄力宝石 | 副武器 | |
