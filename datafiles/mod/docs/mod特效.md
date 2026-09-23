# 特效 Mod

> 通用前提（文件命名、块名与时机、贴图、生效方式）见 [mod开发工具说明.md](mod开发工具说明.md)；
> VM 语言与全部函数见 `help.md`。

## 文件

```text
mod/effects/my_fx.json / .bin
mod/effects/my_fx.txt        ← 源码（建议保留）
mod/effects/tex/...          ← 自带贴图
```

和子弹同一套路：**不进任何池，只提供逻辑**，json 可以是 `{}`（但必须有 json 才会被扫描）。

## 怎么创建

```gml
fx = VM_CreateInstance("obj_effect_mod", x, y)
VM_SetProp(fx, "mod_type", "my_fx")          // = mod/effects/ 下的文件名
VM_SetProp(fx, "sprite_index", "spr_my_fx")
VM_SetProp(fx, "destroy_timer", 30)          // 30 帧后自动销毁
```

也可以"先写全局再创建"（Create 时立刻初始化）：

```gml
VM_SetProp(0, "_mod_pending_effect_id", "my_fx")
fx = VM_CreateInstance("obj_effect_mod", x, y)
```

## 内置属性

| 属性 | 说明 |
|---|---|
| `mod_type` | 跑哪套逻辑 |
| `destroy_timer` | `-1`=不自动销毁（默认）/ `>0` 每帧 −1 到 0 销毁 / `0` 立即销毁 |
| `sprite_index` / `image_speed` | 贴图与动画（Create 里 `image_speed = 0`，要动自己设） |
| `x` / `y` / `image_alpha` / `image_angle` / `image_xscale` / `image_yscale` | 普通实例属性，随便改 |

## 常见用法：挂在卡片/宝石上的光环

```gml
// 特效的 _OBJECT_CREATE：记住主人和偏移
_OBJECT_CREATE {
    self = VM_GetCurCard()
    p = VM_GetProp(self, "parent_inst")
    VM_SetProp(self, "rel_x", 0 - 15)
    VM_SetProp(self, "rel_y", 0 - 5)
}

// 特效的 _OBJECT_STEP：跟随；主人没了就自己销毁
_OBJECT_STEP {
    self = VM_GetCurCard()
    p = VM_GetProp(self, "parent_inst")
    if (VM_IsUndefined(p) == 1) { VM_DestroyInstance(self) }
    else {
        if (VM_IsDestroyed(p) == 1) { VM_DestroyInstance(self) }
        else {
            VM_SetProp(self, "x", VM_GetProp(p, "x") + VM_GetProp(self, "rel_x"))
            VM_SetProp(self, "y", VM_GetProp(p, "y") + VM_GetProp(self, "rel_y"))
        }
    }
}
```

- 死亡/命中特效也走这个对象：`VM_CreateInstance("obj_effect_mod", x, y)`
  + `mod_type`（或者直接用屏幕弹幕的 `销毁对象` / `mod名字` 两个参数，让它自动生成）
- 逐帧推 `image_index` 做帧动画，或用 `destroy_timer` 控制存活时间
