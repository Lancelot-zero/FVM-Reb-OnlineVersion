// ============================================================
// host_layout.h —— **VM ↔ 宿主引擎 的共享内存布局契约**（由 tools/gen_host_layout.py 生成）
//
//   ⚠⚠ 这是**生成物，别手改** ⚠⚠
//     改布局去改 tools/gen_host_layout.py 里的 LAYOUT，然后重跑：
//         python tools/gen_host_layout.py
//
//   为什么要有这份东西：
//     新接口是"共享内存 + 事先约定"——**不链接、不传函数指针**，任何语言 / 引擎
//     只要按约定读写这几块内存就能当宿主。代价是"约定"本身成了接口：
//     两边各抄一份、抄岔了不会有任何编译错误，只会静默读出垃圾。所以：
//       · 约定只有**唯一真源**（tools/gen_host_layout.py），两边都用它的产物；
//       · 下面的偏移全部是 **constexpr**（编译期常量）——508 条内置函数表已冻结，
//         每个函数要用哪块、哪一槽在编译期就定得下来，运行时查表只多一次内存访问
//         和一个"表没初始化"的失败模式，换不来任何好处。
//
//   ⚠ 改布局的规矩：**改一次就 +HOST_LAYOUT_VERSION，两边重新对齐**。
//     引擎启动时比一下 host_layout_version()（见 host.h），版本不同就别开工，
//     不要"凑合读"——错开的偏移读出来的是垃圾，不会报错。
//
//   ⚠ 字节语义：槽 = 8 字节（Value 是 {int32 类型, int32 值}），字节序**全部小端**。
//     跨语言对齐按 16 字节算（宿主的 Value 若带 padding 也不至于错位）。
// ============================================================
#pragma once

#include <cstdint>

// 契约版本号。B1/B2/A 任何一块布局改了都要 +1（生成器里改 LAYOUT['version']）
constexpr int HOST_LAYOUT_VERSION = 1;

// 一个槽的字节数 / 跨语言对齐（别手写 4：槽里是"类型 + 值"两个 int32）
constexpr int HOST_LAYOUT_VALUE_BYTES = 8;
constexpr int HOST_LAYOUT_SLOT_ALIGN  = 16;


// ============================================================
// B1 状态区（引擎**写** -> VM **读**）—— 覆盖式每帧写，不需要清
//
//   引擎每帧把整块重写一遍：所以没有"清"这个动作，也不会有上一帧的残留
//   （这就是选覆盖式、不选增量式的理由）。
//   ⚠ VM 侧读之前先比 HOST_LAYOUT_STATE_FRAME：帧号没变 = 还没有新的一帧，
//     别把同一帧读两遍（同一个输入被当成两次点击，就是这么来的）。
// ============================================================
constexpr int HOST_LAYOUT_STATE_SLOTS = 64;
constexpr int HOST_LAYOUT_STATE_BYTES = HOST_LAYOUT_STATE_SLOTS * HOST_LAYOUT_VALUE_BYTES;

// ⚠ 下面这些常量全是**槽号**（都是 int）：槽里存什么类型（int / float）写在每条注释里。
//   别把"槽号"当成 float 用 —— 槽号是"第几个槽"，不是槽里的值。

// —— 每个字段的槽号 ——
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W0 = 16;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W1 = 17;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W2 = 18;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W3 = 19;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W4 = 20;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W5 = 21;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W6 = 22;
constexpr int HOST_LAYOUT_STATE_KEY_DOWN_WORDS_W7 = 23;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W0 = 24;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W1 = 25;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W2 = 26;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W3 = 27;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W4 = 28;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W5 = 29;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W6 = 30;
constexpr int HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W7 = 31;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W0 = 32;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W1 = 33;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W2 = 34;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W3 = 35;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W4 = 36;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W5 = 37;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W6 = 38;
constexpr int HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W7 = 39;

constexpr int HOST_LAYOUT_STATE_FRAME = 0;
constexpr int HOST_LAYOUT_STATE_MOUSE_WORLD_X = 1;
constexpr int HOST_LAYOUT_STATE_MOUSE_WORLD_Y = 2;
constexpr int HOST_LAYOUT_STATE_MOUSE_GUI_X = 3;
constexpr int HOST_LAYOUT_STATE_MOUSE_GUI_Y = 4;
constexpr int HOST_LAYOUT_STATE_MOUSE_BUTTONS = 5;
constexpr int HOST_LAYOUT_STATE_MOUSE_PRESSED = 6;
constexpr int HOST_LAYOUT_STATE_MOUSE_RELEASED = 7;
constexpr int HOST_LAYOUT_STATE_SCROLL_DELTA = 8;
constexpr int HOST_LAYOUT_STATE_FOCUS = 9;
constexpr int HOST_LAYOUT_STATE_IME_ON = 10;
constexpr int HOST_LAYOUT_STATE_WINDOW_W = 11;
constexpr int HOST_LAYOUT_STATE_WINDOW_H = 12;
constexpr int HOST_LAYOUT_STATE_FRAME_MS = 13;
constexpr int HOST_LAYOUT_STATE_TIME_TOTAL = 14;
constexpr int HOST_LAYOUT_STATE_RESERVED_15 = 15;
constexpr int HOST_LAYOUT_STATE_RESERVED_40 = 40;
constexpr int HOST_LAYOUT_STATE_RESERVED_41 = 41;
constexpr int HOST_LAYOUT_STATE_RESERVED_42 = 42;
constexpr int HOST_LAYOUT_STATE_RESERVED_43 = 43;
constexpr int HOST_LAYOUT_STATE_RESERVED_44 = 44;
constexpr int HOST_LAYOUT_STATE_RESERVED_45 = 45;
constexpr int HOST_LAYOUT_STATE_RESERVED_46 = 46;
constexpr int HOST_LAYOUT_STATE_RESERVED_47 = 47;
constexpr int HOST_LAYOUT_STATE_RESERVED_48 = 48;
constexpr int HOST_LAYOUT_STATE_RESERVED_49 = 49;
constexpr int HOST_LAYOUT_STATE_RESERVED_50 = 50;
constexpr int HOST_LAYOUT_STATE_RESERVED_51 = 51;
constexpr int HOST_LAYOUT_STATE_RESERVED_52 = 52;
constexpr int HOST_LAYOUT_STATE_RESERVED_53 = 53;
constexpr int HOST_LAYOUT_STATE_RESERVED_54 = 54;
constexpr int HOST_LAYOUT_STATE_RESERVED_55 = 55;
constexpr int HOST_LAYOUT_STATE_RESERVED_56 = 56;
constexpr int HOST_LAYOUT_STATE_RESERVED_57 = 57;
constexpr int HOST_LAYOUT_STATE_RESERVED_58 = 58;
constexpr int HOST_LAYOUT_STATE_RESERVED_59 = 59;
constexpr int HOST_LAYOUT_STATE_RESERVED_60 = 60;
constexpr int HOST_LAYOUT_STATE_RESERVED_61 = 61;
constexpr int HOST_LAYOUT_STATE_RESERVED_62 = 62;
constexpr int HOST_LAYOUT_STATE_RESERVED_63 = 63;
// —— 键盘位图：位号 = vk 键码 0..255 ——
//   每个位图 8 个 int 槽（W0..W7，W = word），共三张（当前按住 / 本帧刚按下 / 本帧刚松开）。
//   取法：word = 槽 (KEY_*_W0 + (vk >> 5))，按下 = (word >> (vk & 31)) & 1。
//   为什么低位在前：vk 连续时读起来也连续；倒过来的话每个字内部要倒序，读写两边都得多想一步。
constexpr int HOST_LAYOUT_STATE_KEY_WORDS  = 8;
constexpr int HOST_LAYOUT_STATE_BITS_PER_WORD = 32;
constexpr int HOST_LAYOUT_STATE_VK_MAX     = 255;
static_assert(HOST_LAYOUT_STATE_KEY_WORDS * HOST_LAYOUT_STATE_BITS_PER_WORD
              == HOST_LAYOUT_STATE_VK_MAX + 1, "键盘位图要刚好盖住 vk 0..255");

// vk -> 位图里的槽 / 位。故意**不做边界裁剪**：vk 越界是调用方的 bug，不该在这儿被吞掉
//   （引擎那边 vk 一定在 0..255，拿别的数来查说明上层已经算错了，越早现形越好）
constexpr int HOST_LAYOUT_STATE_VK_WORD(int vk) { return vk >> 5; }
constexpr int HOST_LAYOUT_STATE_VK_BIT (int vk) { return vk & 31; }

// 顺手给宿主一个"从一整块状态里读某位"的便利函数（纯 constexpr，没有运行时查表）
//   words 指向该位图的第 0 槽（例如状态块首地址 + HOST_LAYOUT_STATE_KEY_DOWN_W0）
constexpr bool HOST_LAYOUT_STATE_BIT(const int32_t* words, int vk) {
    return ((words[HOST_LAYOUT_STATE_VK_WORD(vk)] >> HOST_LAYOUT_STATE_VK_BIT(vk)) & 1) != 0;
}


// ============================================================
// B2 命令区（VM **写** → 引擎**读**，高频：绘制 / 坐标这类）—— 计数 + 定长记录数组
//
//   槽 0 是计数，后面是记录区：每条记录 8 槽，
//   字段顺序 [0] 命令号(int) [1][2] int 参 [3..7] 5 个 float 参。
//   ⚠ 计数的规矩（两个写者 = 一定踩，所以只说一遍）：
//       生产方 VM：每帧**第一次写之前**清 0，然后只往后追；
//       消费方引擎：只读 [0, 计数)，**不许**自己清计数。
//   ⚠ 计数 > 上限 → 当作写不下（响亮报错 / 丢弃），**不回绕**覆盖前面的记录。
// ============================================================
constexpr int HOST_LAYOUT_CMD_MAX_RECORDS     = 512;
constexpr int HOST_LAYOUT_CMD_SLOTS_PER_RECORD = 8;
constexpr int HOST_LAYOUT_CMD_RECORD_BYTES    = HOST_LAYOUT_CMD_SLOTS_PER_RECORD * HOST_LAYOUT_VALUE_BYTES;
constexpr int HOST_LAYOUT_CMD_COUNT_SLOT      = 0;   // 计数所在的槽
constexpr int HOST_LAYOUT_CMD_RECORD_BASE     = 1;  // 记录区起始槽（= 计数槽 + 1）
constexpr int HOST_LAYOUT_CMD_SLOTS = HOST_LAYOUT_CMD_RECORD_BASE
                                    + HOST_LAYOUT_CMD_MAX_RECORDS * HOST_LAYOUT_CMD_SLOTS_PER_RECORD;
constexpr int HOST_LAYOUT_CMD_BYTES = HOST_LAYOUT_CMD_SLOTS * HOST_LAYOUT_VALUE_BYTES;

// 第 i 条记录的第一个槽（i 从 0 起；调用方自己保证 i < MAX_RECORDS —— 这里不裁）
constexpr int HOST_LAYOUT_CMD_RECORD_SLOT(int i) {
    return HOST_LAYOUT_CMD_RECORD_BASE + i * HOST_LAYOUT_CMD_SLOTS_PER_RECORD;
}
// 记录内部：0 命令号 / 1、2 int 参 / 3..7 float 参
constexpr int HOST_LAYOUT_CMD_FIELD_CMD     = 0;
constexpr int HOST_LAYOUT_CMD_FIELD_INT_A   = 1;
constexpr int HOST_LAYOUT_CMD_FIELD_INT_B   = 2;
constexpr int HOST_LAYOUT_CMD_FIELD_FLOAT_A = 3;
constexpr int HOST_LAYOUT_CMD_FIELD_FLOAT_B = 4;
constexpr int HOST_LAYOUT_CMD_FIELD_FLOAT_C = 5;
constexpr int HOST_LAYOUT_CMD_FIELD_FLOAT_D = 6;
constexpr int HOST_LAYOUT_CMD_FIELD_FLOAT_E = 7;


// ============================================================
// A 请求流（VM **写** → 引擎**读**，低频 / 事件型）—— 沿用 host_shared.cpp 的 host_shm_*
//
//   ⚠ 这一块**没有固定偏移**：它是一条"顺序追加 + 写游标"的流。
//     容量 / 游标从这里拿：
//         uint8_t* base = host_shm_base();      // 引擎每次消费前**重新**取（扩容会换地址）
//         int      cur  = host_shm_cursor();    // 读 [0, cur)
//     初始 64KB / 不够翻倍 / 上限 16MB —— 具体值归 host_shared.cpp，这里**不复制**
//     （复制了就是同一件事两个真源，早晚对不上）。
//
//   下面只定"请求号 → 字段顺序"：字段顺序即字节顺序，全部小端，
//   i32/f32/u32 长度都是 4 字节，bytes 是裸字节，**包里没有类型字节**
//   （传错顺序/类型不会被发现，这是设计前提，见 host_shared.cpp 的 ★编码格式★）。
// ============================================================
// 1 AUDIO_PLAY（播放音频）: i32(snd) + i32(loop) + f32(gain) + f32(pitch)
constexpr int HOST_LAYOUT_REQ_AUDIO_PLAY = 1;

// 2 AUDIO_CTRL（停止/暂停/恢复音频）: i32(snd) + i32(action)
constexpr int HOST_LAYOUT_REQ_AUDIO_CTRL = 2;

// 3 CAMERA（相机/视图）: i32(action) + f32(value)
constexpr int HOST_LAYOUT_REQ_CAMERA = 3;

// 4 WINDOW（窗口/显示）: i32(action) + i32(value)
constexpr int HOST_LAYOUT_REQ_WINDOW = 4;

// 5 NET_SEND（网络发送）: i32(channel) + u32(len) + bytes(data)
constexpr int HOST_LAYOUT_REQ_NET_SEND = 5;

// 6 SYSTEM（系统）: i32(action)
constexpr int HOST_LAYOUT_REQ_SYSTEM = 6;

constexpr int HOST_LAYOUT_REQ_RESERVED_FROM = 7;   // 这个号起预留（往尾部加，不插中间）

constexpr int HOST_LAYOUT_AUDIO_ACTION_STOP = 0;        // 停
constexpr int HOST_LAYOUT_AUDIO_ACTION_PAUSE = 1;       // 暂停
constexpr int HOST_LAYOUT_AUDIO_ACTION_RESUME = 2;      // 恢复

constexpr int HOST_LAYOUT_CAMERA_ACTION_SET_X = 0;      // 设 x
constexpr int HOST_LAYOUT_CAMERA_ACTION_SET_Y = 1;      // 设 y
constexpr int HOST_LAYOUT_CAMERA_ACTION_SET_ZOOM = 2;   // 设缩放

constexpr int HOST_LAYOUT_WINDOW_ACTION_FULLSCREEN = 0; // 全屏 0/1
constexpr int HOST_LAYOUT_WINDOW_ACTION_SIZE = 1;       // 设长宽
constexpr int HOST_LAYOUT_WINDOW_ACTION_TITLE = 2;      // 设标题

constexpr int HOST_LAYOUT_SYSTEM_ACTION_EXIT = 0;       // 退出
constexpr int HOST_LAYOUT_SYSTEM_ACTION_FULLSCREEN = 1; // 切全屏
constexpr int HOST_LAYOUT_SYSTEM_ACTION_DIALOG = 2;     // 弹对话框
constexpr int HOST_LAYOUT_SYSTEM_ACTION_CLIPBOARD = 3;  // 剪贴板


// ============================================================
// 编译期自检：这些数一旦被改坏，在这里就报出来（而不是运行时读出垃圾）
// ============================================================
static_assert(HOST_LAYOUT_VERSION >= 1, "版本号从 1 起");
static_assert(HOST_LAYOUT_STATE_SLOTS == 64, "B1 状态区 64 槽");
static_assert(HOST_LAYOUT_STATE_FRAME == 0, "B1 帧号必须在槽 0");
static_assert(HOST_LAYOUT_CMD_COUNT_SLOT == 0, "B2 计数必须在槽 0");
static_assert(HOST_LAYOUT_CMD_RECORD_BASE == 1, "B2 记录区紧跟计数");
static_assert(HOST_LAYOUT_CMD_FIELD_FLOAT_E == HOST_LAYOUT_CMD_SLOTS_PER_RECORD - 1,
              "每条记录 8 槽：命令号 + 2 int + 5 float");
static_assert(HOST_LAYOUT_STATE_SLOTS > 0 && HOST_LAYOUT_CMD_SLOTS > 0, "两块都得有大小");
