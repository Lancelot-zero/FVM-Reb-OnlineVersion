// 通用 mod 子弹对象
// 由插件 .bin 通过 VM_CreateInstance("obj_bullet_mod", x, y) 创建，
// 然后用 VM_SetProp(inst, "mod_type", "子弹类型名") 指定逻辑。
// destroy_timer: -1=不自动销毁, 0=立即销毁, >0=每帧减1，到0销毁
mod_type = variable_global_exists("_mod_pending_bullet_id") ? global._mod_pending_bullet_id : "";
destroy_timer = -1;
_mod_initialized = false;
_mod_vm = undefined;
image_speed = 0;
vector_x = 0
vx = 0
vy = 0

// ══════════════════════════════════════════════════════════════════════════
// 进 VM 的入口条件（卡在创建后用 VM_SetProp 写；不写 = 每帧进）
//   mod_step_enter_condition
//     "" / 认不出     每帧进（默认）
//     "mod"           按全场帧号对齐：battle_time % mod_step_enter_var == 0
//     "wait"          倒数：mod_step_enter_var 每帧 -1，减到 <= 0 进
//                      ⚠️ 进完之后要卡自己把 var 设回正数，否则此后每帧都进
//     "cell"          跨格才进：grid_col 或 grid_row 相对上一帧变了
//                     （两个值 obj 每帧已经算好，零成本；子弹最常用这个）
//                     ⚠️ 时机：格子坐标是在 x += vx 之前算的，所以触发的是
//                        "进格后的第一帧"，比实际越界晚一帧；一帧跨多格也只触发一次
//     "cell_row"      只跨行才进（grid_row 变了）；直线弹用不上，抛物弹有用
//     "cell_target_row" / "cell_target_col"
//                     **在**目标所在行 / 列里就进（是"在着就一直进"，不是只跨过去那一帧）；
//                     目标取 bullet_target 实例（没有实例就用 bullet_target_x / bullet_target_y
//                     换算格子；都没设 = 永不进）
//     "attack"        场上还有敌人才进（全局判定，不看具体位置）
//     "attack_cell"   子弹【当前这一格】有敌人才进（查每帧重建的敌人表，比 "attack" 精确）
//     "attack_collision"  和敌人【撞上】才进（由碰撞事件置标记；撞一次进一次）
//   mod_step_enter_var   "mod" = 间隔帧数；"wait" = 还要等几帧
// obj 每帧回写（卡只读）：
//   mod_has_enemy       "attack" / "attack_cell" 模式下：1 = 有敌人（其他模式恒为 0）
//   mod_collision_enemies "attack_collision"：这一帧撞到的**所有**敌人 id（数组，碰撞事件里追加）
//   mod_collision_enemy   同上，但只保留**最后一个**（方便只关心一个的脚本）
//   grid_col / grid_row / prev_grid_col / prev_grid_row  当前帧 / 上一帧格子坐标
// ══════════════════════════════════════════════════════════════════════════
mod_step_enter_condition = ""   // "" / "mod" / "wait" / "cell" / "cell_row" / "cell_target_row" / "cell_target_col" / "attack" / "attack_cell" / "attack_collision"
// 盯梢（实现见 Step）：插件用命名数组 mod_watch 声明"要盯的属性名"，任一属性值变了，这一帧也进 _OBJECT_STEP
mod_changed_prop = ""    // 【只读】这一帧第一个发生变化的盯梢属性名（没变化 = ""）
mod_watch_last  = []     // 【obj 内部】上一帧 mod_watch 各属性的值（和数组一一对应）
mod_watch_changed = []  // 【obj 内部】这一帧发生变化的盯梢属性名（每个各进一次 _OBJECT_STEP）
mod_step_enter_var       = 0    // "mod" = 间隔帧数；"wait" = 还要等几帧
mod_has_enemy            = 0    // 【只读】"attack" / "attack_cell" 模式下 1 = 有敌人

// 碰撞（Collision_obj_enemy_parent 事件里写；"attack_collision" 闸门下一帧进一次）
//   ⚠️ 一帧可能撞到好几个敌人 → 撞到的都进 mod_collision_enemies（数组，进完 VM 自动清空），
//      脚本里用 VM_InstArraySize / VM_InstArrayItem 遍历；只关心一个就读 mod_collision_enemy
mod_collision_enemies = []   // 【只读】这一帧撞到的所有敌人 id
mod_collision_enemy   = -1   // 【只读】最后一个撞到的敌人 id
mod_collision_hit     = 0    // 【只读】1 = 上一次碰撞过（进 VM 后自动清 0）

// ══════════════════════════════════════════════════════════════════════
// 碰撞结算（**引擎自动做**，全在卡的 _OBJECT_CREATE 里 VM_SetProp 设；默认全 0 = 不结算）
//   bullet_damage        撞到敌人造成多少伤害（0 = 不掉血）
//   bullet_collide       1 = 正常碰撞（默认）；**0 = 关掉碰撞**（不结算 / 不记录 / 不进 attack_collision）
//                        —— 想自己判命中（例如只打特定行、只打 BOSS）就关掉，全交给脚本
//   bullet_damage_type   伤害类型：normal 有盾只打盾 / pierce 盾血一起掉 / 其它无视护盾
//   bullet_slow          1 = 命中就减速（敌人移动/攻速减半）
//   bullet_slow_frame    减速持续帧数（写敌人的 ice_timer）
//   bullet_freeze_chance 0~1：命中时**完全冻结**的概率（写 frozen_timer，敌人整只不动）
//   bullet_freeze_frame  冻结持续帧数
//   bullet_stun_chance   0~1：命中时**眩晕**的概率（写 stun_timer，不动 + 冒星星）
//   bullet_stun_frame    眩晕持续帧数
//   bullet_hits          撞几次后消失；**-1 = 不消耗（默认：撞上也不会消失）**
//                        ⚠️ 默认必须是 -1：正式 mod 子弹的伤害靠卡片自己的 VM 逻辑
//                        （cell 闸门 + VM_DamageEnemy），子弹要是被碰撞事件提前销毁，
//                        就变成"到敌人面前消失、一点伤害都没有"。
//                        要让子弹"撞一次就没"，卡片里显式 VM_SetProp(self,"bullet_hits",1)
//   ⚠️ 减速/冻结/眩晕都是"取更长的那一个"（不会把敌人身上更长的状态缩短）
//   ⚠️ 概率用 random(1)，联机下各客户端可能不同步（要严格同步请自己在 VM 里掷）
// ══════════════════════════════════════════════════════════════════════
bullet_damage        = 0
bullet_damage_type   = "normal"
bullet_collide       = 1    // 0 = **关掉碰撞**（不结算、不记录、attack_collision 也不进）
bullet_slow          = 0
bullet_slow_frame    = 0
bullet_freeze_chance = 0
bullet_freeze_frame  = 0
bullet_stun_chance   = 0
bullet_stun_frame    = 0
bullet_hits          = -1   // 撞几次后消失；**-1 = 不消耗（默认）**，要"撞一次就没"写 1

// 【obj 内部】剩余可命中次数：第一次撞到才按 bullet_hits 初始化（那时脚本的 CREATE 早跑完了）
bullet_hits_left = 0
bullet_hits_init = 0

// ══════════════════════════════════════════════════════════════════════
// 抛物线（引擎自己飞；bullet_parabola = 1 才生效，会覆盖 vx/vy 的直线位移）
//   bullet_target        目标实例 id（优先；-1 或实例没了就用下面两个坐标）
//   bullet_target_x/y    目标坐标
//   bullet_parabola_time 全程帧数（默认 30 帧 = 0.5 秒）
//   bullet_parabola_h    弧线最高点高度（像素）；**-1 = 自动**（起点到目标的 1/4）
//   飞完（到目标点）就停下：bullet_parabola 置 0、vx/vy 清零，之后交给碰撞结算 / 自己销毁
//   ⚠️ 目标是实例时，位置只在**起飞那一帧**取一次（飞出去就不再追）
// ══════════════════════════════════════════════════════════════════════
bullet_parabola      = 0
bullet_target        = -1
bullet_target_x      = 0
bullet_target_y      = 0
bullet_parabola_time = 30
bullet_parabola_h    = -1

// 【obj 内部】抛物线状态
bullet_para_t    = -1   // 已飞帧数；-1 = 还没起飞
bullet_para_x0   = 0
bullet_para_y0   = 0
bullet_para_dist = 0

// ══════════════════════════════════════════════════════════════════════
// 路径数组（插件侧用 VM 命名数组描述"每帧位移"，**位移由本对象每帧执行**）：
//   插件在 _OBJECT_CREATE 里：VM_ArrayClear("dx_arr") + VM_ArrayADD("dx_arr", 每帧x位移)
//                            VM_ArrayClear("dy_arr") + VM_ArrayADD("dy_arr", 每帧y位移)
//   本对象每帧做：x += dx_arr[bullet_path_i mod 长度]；y += dy_arr[同一个下标]；然后 bullet_path_i += 1
//   两串长度不一致时**按短的算**；数组空 / 没建 = 这条子弹不用路径（照旧走 vx/vy）
//   执行时机在 vx/vy 和抛物线**之后** → 和其它移动是**叠加**关系，不会把别顶掉
// ══════════════════════════════════════════════════════════════════════
bullet_path_i = 0    // 【只读】路径计数器：已经走到数组第几项（插件可读它做动画/透明/销毁）

// 倒计时 + 每帧透明度变化（基础属性，所有 mod 对象都有）
mod_countdown = 0    // 倒计时（帧）：> 0 时每帧 -1
mod_alpha_add = 0    // 每帧加到 image_alpha 上的量（负数 = 渐隐）；只在倒计时 > 0 时加


// 当前帧/上一帧格子坐标（用于子弹路径碰撞检测）
grid_col = -1;
grid_row = -1;
prev_grid_col = -1;
prev_grid_row = -1;
var _gp = get_grid_position_from_world(x, y, true);
grid_col = _gp.col;
grid_row = _gp.row;
prev_grid_col = grid_col;
prev_grid_row = grid_row;

// 如果创建前已经通过 global._mod_pending_bullet_id 指定类型，则立即初始化
if (mod_type != "" && variable_global_exists("mod_bullet_vms") && ds_map_exists(global.mod_bullet_vms, mod_type)) {
  _mod_vm = global.mod_bullet_vms[? mod_type];
  _mod_initialized = true;
  // 子弹不进 instances 列表（量大且没人按类型枚举），只维护 VM 的活实例计数：见 Draw_0.gml 的 mod_inst_add(_mod_vm, id, false)

  if (ds_map_exists(_mod_vm.blocks, "_OBJECT_CREATE")) {
    var _bak_last = global._VM_last_created_card;
    global._VM_last_created_card = id;
    var _bak_cur = global._VM_cur_card;
    global._VM_cur_card = id;
    var _bak_vm = global.__vm;
    global.__vm = _mod_vm;
    VM_Execute(_mod_vm, _mod_vm.blocks[? "_OBJECT_CREATE"], "_OBJECT_CREATE");
    global.__vm = _bak_vm;
    global._VM_cur_card = _bak_cur;
    global._VM_last_created_card = _bak_last;
  }
}