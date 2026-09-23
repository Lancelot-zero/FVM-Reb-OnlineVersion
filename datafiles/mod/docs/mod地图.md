# 地图 Mod

> 通用前提（文件命名、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> 关卡脚本的 VM 语言与全部函数见 `help.md`。

## 目录

```text
mod/maps/<地图id>/
  world.json              大地图 + 关卡列表
  share/                  这一张图共享的资源（可选）
  level_1/
    <关卡名>.json          关卡配置（实验室格式，编辑器导出）
    <关卡名>.bin           关卡脚本（LabMapCompiler 编译产物）
    xx.png / xx.ogg        关卡自带图片、音乐
```

`mod/maps/____shared____/` 放公共资源（默认背景、图标），所有 mod 地图都能用。

## world.json

| 字段 | 必填 | 说明 |
|---|---|---|
| `id` | | 地图 id，默认用目录名 |
| `name` | ✅ | 大地图显示名 |
| `sprite` | | 大地图贴图 |
| `icon` | | 大地图图标 |
| `share` | | 共享资源目录名（如 `"share"`） |
| `levels` | ✅ | 关卡数组 |
| `levels[].name` | | 关卡显示名 |
| `levels[].button_x` / `button_y` | | 关卡按钮在大地图上的位置 |
| `levels[].stage_json` | ✅ | 关卡配置相对路径，如 `"level_1/level.json"` |
| `levels[].sprite` / `icon` | | 关卡预览贴图 / 图标（可选） |

```json
{
  "id": "my_island",
  "name": "我的大地图",
  "icon": "spr_laboratory_icon",
  "share": "share",
  "levels": [
    { "name": "第一关", "button_x": 400, "button_y": 400, "stage_json": "level_1/level.json" }
  ]
}
```

## 关卡两部分

| 文件 | 作用 | 怎么来 |
|---|---|---|
| `<关卡名>.json` | 关卡配置：地图尺寸、地形、波次敌人、奖励、事件… | **实验室关卡格式**（`version` / `level_name` / `map_cols` / `map_rows` / `enemy_list` / `rewards` / `events`…），用实验室或地图编辑器导出/编辑，**不用手写** |
| `<关卡名>.bin` | 关卡脚本：摆地图、刷怪、设规则 | VM 脚本（`_VM_BATTLE_START` / `_VM_WAVE_START` / `_VM_FRAME` …），用 `LabMapCompiler` 或编辑器编译 |

关卡脚本常用块：

```text
_VM_ROOM_READY_ENTRY   进准备室时（设规则：禁卡、等级上限、卡槽数…）
_VM_BATTLE_START       战斗开始（造地图：地形、平台、物件）
_VM_WAVE_START/END     每波开始/结束
_VM_SUBWAVE_START/END  每个子波
_VM_ENEMY_KILLED       杀敌（奖励/惩罚）
_VM_FRAME              每帧（⚠️ 别写复杂逻辑）
```

## 进图流程

```text
mod 地图文件被复制到 laboratory/____mod_map____/
→ CustomStage
→ 关卡详情 → 进关
```

## 要点

- 关卡 json 里的 `enemy_list` 可以直接引用 mod 敌人（`mod/enemies/` 下的 id）
- 关卡脚本里也能用 mod 的东西：`VM_SpawnPlant("mod卡id", ...)`、`VM_SpawnEnemy("mod敌人id", ...)`
- 自带图片/音乐直接放在关卡文件夹里，json 里写文件名；`share/` 里的是整张图共享
- 关卡脚本报错会**静默吞掉**（`VM_Execute` 的已知行为），调不出效果时先在 `_VM_BATTLE_START` 里
  `VM_ShellPrint(...)` 打一行确认块跑了
