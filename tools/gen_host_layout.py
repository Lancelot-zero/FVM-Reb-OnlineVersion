# -*- coding: utf-8 -*-
# ============================================================
# gen_host_layout.py —— "VM ↔ 宿主引擎" 共享内存布局的**唯一真源**
#
#   生成两个文件（同一份数据、两种呈现）：
#     DLL_VM/host/host_layout.h    给 C++ 宿主 / VM 侧：编译期常量（constexpr）
#     DLL_VM/host/host_layout.json 给别的语言 / 引擎（C#、Rust、脚本…）：字段名 → 槽号
#
#   为什么要生成、而不是两边各写一份：
#     新接口是"共享内存 + 事先约定"——**不链接、不传函数指针**，引擎按约定读写几块内存就行。
#     代价是"约定"本身成了接口：两边各抄一份、抄岔了不会有任何编译错误，
#     只会静默读出垃圾。所以约定必须有**唯一真源**（本脚本），两边都用它的产物。
#
#   为什么偏移全部写死（constexpr / 宏），不做运行时查表：
#     508 条内置函数表已冻结，每个函数要用到的数据可以在**编译期**就定下偏移。
#     运行时要查表 = 多一次内存访问 + 多一个"表没初始化好"的失败模式，
#     换不来任何好处。所以下面这些数字直接烘成编译期常量。
#
#   幂等：本脚本不用时间 / 随机数 / 环境变量，同一份真源跑两次 → 产物逐字节相同。
#         改布局 → 必须同时把 LAYOUT['version'] +1（脚本会把它写进两个产物）。
#
#   用法： python tools/gen_host_layout.py
#          （脚本按自己的位置定位仓库根，所以在哪个目录敲都一样）
# ============================================================
import io
import json
import os

# ============================================================
# ★★★ 唯一真源 ★★★ —— 要改布局只改这个 dict
#
#   一块内存 = 连续的一串 Value 槽。Value 的定义在 DLL_VM/core/vm_def.h：
#     struct Value { int32_t t; int32_t v; };   → **8 字节**
#   ⚠ 别凭印象写"一个值 4 字节"：槽里是 {类型, 值} 两个 int32，就是 8 字节。
#     所以 "N 个槽" = N×8 字节，下面凡是字节数都**从槽数算出来**，不手写。
# ============================================================
LAYOUT = {
    'version': 1,

    # ---- 块 1：状态区（引擎写 -> VM 读）-------------------------------
    # 覆盖式每帧写：引擎每帧把**整块**重写一遍，所以不需要"清"这个动作，
    # 也不会有"上一帧的残留"——这正是选覆盖式、而不是增量式的理由。
    'state': {
        'name': 'HOST_LAYOUT_STATE',
        'slots': 64,
        'producer': '引擎（宿主）',
        'consumer': 'VM',
        # (槽号或 (起,止), 名字后缀, C++ 类型, 说明)
        'fields': [
            (0,      'FRAME',            'int',   '帧号（引擎自己数，只增）：VM 用来判"这一帧读过了没"'),
            (1,      'MOUSE_WORLD_X',    'float', '鼠标：房间/世界 x'),
            (2,      'MOUSE_WORLD_Y',    'float', '鼠标：房间/世界 y'),
            (3,      'MOUSE_GUI_X',      'float', '鼠标：GUI 层 x'),
            (4,      'MOUSE_GUI_Y',      'float', '鼠标：GUI 层 y'),
            (5,      'MOUSE_BUTTONS',    'int',   '鼠标**按住**位掩码：bit0 左 / bit1 右 / bit2 中'),
            (6,      'MOUSE_PRESSED',    'int',   '本帧**刚按下**位掩码（位同上）'),
            (7,      'MOUSE_RELEASED',   'int',   '本帧**刚松开**位掩码（位同上）'),
            (8,      'SCROLL_DELTA',     'float', '本帧滚轮增量（上正下负，值域由引擎定）'),
            (9,      'FOCUS',            'int',   '窗口焦点 0/1'),
            (10,     'IME_ON',           'int',   '输入法开关 0/1'),
            (11,     'WINDOW_W',         'int',   '窗口宽（像素）'),
            (12,     'WINDOW_H',         'int',   '窗口高（像素）'),
            (13,     'FRAME_MS',         'float', '本帧耗时 ms'),
            (14,     'TIME_TOTAL',       'float', '累计时间（秒或 ms 由引擎定，只要一致）'),
            (15,     'RESERVED_15',      'int',   '预留（保持 0；要用先改这里再 +版本）'),
            ((16, 23), 'KEY_DOWN_WORDS',  'int',   '键盘"当前按住"位图：8 槽 = 256 位，位号 = vk 键码 0..255'),
            ((24, 31), 'KEY_PRESSED_WORDS','int',  '键盘"本帧刚按下"位图：8 槽'),
            ((32, 39), 'KEY_RELEASED_WORDS','int', '键盘"本帧刚松开"位图：8 槽'),
            ((40, 63), 'RESERVED',        'int',   '预留 24 槽：留给以后（输入法文本 / 手柄 / 触摸 / 剪贴板…）'),
        ],
        # 位图规则（写进注释与 JSON：不说清就一定会两边各解一套）
        'bitmap': {
            'words': 8,
            'bits_per_word': 32,
            'vk_max': 255,
            'rule': ('vk → 槽 HOST_LAYOUT_STATE_KEY_*_W0 + (vk >> 5)，位 (vk & 31)；'
                     '取法 (word >> (vk & 31)) & 1。vk 不在 0..255 一律当"没按"。'
                     '选"低位在前"，是因为这样 vk 连续时读起来也是连续的；'
                     '反过来的话每个字内部要倒序，写的人和读的人都要多想一步。'),
        },
    },

    # ---- 块 2：命令区（VM 写 -> 引擎读，高频：绘制 / 坐标这类）---------
    # 计数 + 定长记录数组：引擎一次扫完，不用回调、不用分配。
    'command': {
        'name': 'HOST_LAYOUT_CMD',
        'slots': 1 + 512 * 8,       # 计数 1 槽 + 记录区 512×8 槽
        'max_records': 512,
        'slots_per_record': 8,
        'producer': 'VM',
        'consumer': '引擎（宿主）',
        'record_fields': [
            (0, 'CMD',        'i32',   '命令号（本契约的命令清单见 JSON 的 commands 段）'),
            (1, 'INT_A',      'i32',   '整数参数 1'),
            (2, 'INT_B',      'i32',   '整数参数 2'),
            (3, 'FLOAT_A',    'f32',   'float 参数 1（x）'),
            (4, 'FLOAT_B',    'f32',   'float 参数 2（y）'),
            (5, 'FLOAT_C',    'f32',   'float 参数 3（w）'),
            (6, 'FLOAT_D',    'f32',   'float 参数 4（h）'),
            (7, 'FLOAT_E',    'f32',   'float 参数 5（alpha 这类）'),
        ],
        'rules': [
            '生产方（VM）在**每帧第一次写之前**把计数清 0，然后只往后追；计数 = 这帧已经写了几条。',
            '消费方（引擎）只读 [0, 计数)，**不许**自己清计数——清了就是两个写者，会踩。',
            '计数 > 上限：当作写不下（响亮报错 / 丢弃并计数），**不**回绕覆盖前面的记录。',
        ],
    },

    # ---- 块 3：请求流（VM 写 -> 引擎读，低频 / 事件型）------------------
    # 这一块**不在这里定偏移**：它沿用 host_shared.cpp 的 host_shm_*（顺序追加 + 游标），
    # 容量和游标都从 host_shm_size() / host_shm_cursor() 拿，本契约只定"请求号 → 字段顺序"。
    'request': {
        'producer': 'VM',
        'consumer': '引擎（宿主）',
        'transport': 'DLL_VM/host/host_shared.cpp 的 host_shm_*（初始 64KB / 不够翻倍 / 上限 16MB）',
        'capacity_note': ('容量和游标**不在这里写死**：那是 host_shm_size() / host_shm_cursor() 的事，'
                          '写进契约就等于同一件事有两个真源。这里只定"请求号 -> 字段顺序"。'),
        'signature_note': ('每条请求的字段顺序即字节顺序，全部小端；'
                           'i32 = 4 字节，f32 = 4 字节，u32 长度 = 4 字节，bytes = 裸字节，无类型字节。'),
        'ids': [
            (1, 'AUDIO_PLAY', '播放音频',  [('snd', 'i32'), ('loop', 'i32'), ('gain', 'f32'), ('pitch', 'f32')]),
            (2, 'AUDIO_CTRL', '停止/暂停/恢复音频', [('snd', 'i32'), ('action', 'i32')]),
            (3, 'CAMERA',     '相机/视图',  [('action', 'i32'), ('value', 'f32')]),
            (4, 'WINDOW',     '窗口/显示',  [('action', 'i32'), ('value', 'i32')]),
            (5, 'NET_SEND',   '网络发送',   [('channel', 'i32'), ('len', 'u32'), ('data', 'bytes')]),
            (6, 'SYSTEM',     '系统',       [('action', 'i32')]),
        ],
        # 请求号 7 起预留
        'reserved_from': 7,
        # 动作枚举（值 = 引擎和 VM 共用的约定）
        'actions': {
            'AUDIO_ACTION': [(0, 'STOP', '停'), (1, 'PAUSE', '暂停'), (2, 'RESUME', '恢复')],
            'CAMERA_ACTION': [(0, 'SET_X', '设 x'), (1, 'SET_Y', '设 y'), (2, 'SET_ZOOM', '设缩放')],
            'WINDOW_ACTION': [(0, 'FULLSCREEN', '全屏 0/1'), (1, 'SIZE', '设长宽'), (2, 'TITLE', '设标题')],
            'SYSTEM_ACTION': [(0, 'EXIT', '退出'), (1, 'FULLSCREEN', '切全屏'), (2, 'DIALOG', '弹对话框'),
                              (3, 'CLIPBOARD', '剪贴板')],
        },
    },
}

# ============================================================
# 从真源算出来的那些数（全部从上面推导，不重复手写）
# ============================================================
VALUE_BYTES = 8                    # struct Value { int32 t; int32 v; } —— 见 DLL_VM/core/vm_def.h
SLOT_ALIGN = 16                    # 跨语言的字节对齐：宿主那边的 Value 可能带 padding，按 16 对齐最保险

COMMAND_COUNT_SLOT = 0
COMMAND_RECORD_BASE = 1


def state_bytes():
    return LAYOUT['state']['slots'] * VALUE_BYTES


def command_slots():
    c = LAYOUT['command']
    return 1 + c['max_records'] * c['slots_per_record']


def command_bytes():
    return command_slots() * VALUE_BYTES


def record_bytes():
    return LAYOUT['command']['slots_per_record'] * VALUE_BYTES


# ============================================================
# 产物 1：host_layout.h
# ============================================================
HEADER_INTRO = '''// ============================================================
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
constexpr int HOST_LAYOUT_VERSION = %(version)d;

// 一个槽的字节数 / 跨语言对齐（别手写 4：槽里是"类型 + 值"两个 int32）
constexpr int HOST_LAYOUT_VALUE_BYTES = %(value_bytes)d;
constexpr int HOST_LAYOUT_SLOT_ALIGN  = %(slot_align)d;

'''

HEADER_STATE = '''
// ============================================================
// B1 状态区（引擎**写** -> VM **读**）—— 覆盖式每帧写，不需要清
//
//   引擎每帧把整块重写一遍：所以没有"清"这个动作，也不会有上一帧的残留
//   （这就是选覆盖式、不选增量式的理由）。
//   ⚠ VM 侧读之前先比 HOST_LAYOUT_STATE_FRAME：帧号没变 = 还没有新的一帧，
//     别把同一帧读两遍（同一个输入被当成两次点击，就是这么来的）。
// ============================================================
constexpr int HOST_LAYOUT_STATE_SLOTS = %(state_slots)d;
constexpr int HOST_LAYOUT_STATE_BYTES = HOST_LAYOUT_STATE_SLOTS * HOST_LAYOUT_VALUE_BYTES;

// ⚠ 下面这些常量全是**槽号**（都是 int）：槽里存什么类型（int / float）写在每条注释里。
//   别把"槽号"当成 float 用 —— 槽号是"第几个槽"，不是槽里的值。

// —— 每个字段的槽号 ——
%(state_slot_consts)s
// —— 键盘位图：位号 = vk 键码 0..255 ——
//   每个位图 8 个 int 槽（W0..W7，W = word），共三张（当前按住 / 本帧刚按下 / 本帧刚松开）。
//   取法：word = 槽 (KEY_*_W0 + (vk >> 5))，按下 = (word >> (vk & 31)) & 1。
//   为什么低位在前：vk 连续时读起来也连续；倒过来的话每个字内部要倒序，读写两边都得多想一步。
constexpr int HOST_LAYOUT_STATE_KEY_WORDS  = %(bitmap_words)d;
constexpr int HOST_LAYOUT_STATE_BITS_PER_WORD = %(bits_per_word)d;
constexpr int HOST_LAYOUT_STATE_VK_MAX     = %(vk_max)d;
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

'''

HEADER_CMD = '''
// ============================================================
// B2 命令区（VM **写** → 引擎**读**，高频：绘制 / 坐标这类）—— 计数 + 定长记录数组
//
//   槽 0 是计数，后面是记录区：每条记录 %(slots_per_record)d 槽，
//   字段顺序 [0] 命令号(int) [1][2] int 参 [3..7] 5 个 float 参。
//   ⚠ 计数的规矩（两个写者 = 一定踩，所以只说一遍）：
//       生产方 VM：每帧**第一次写之前**清 0，然后只往后追；
//       消费方引擎：只读 [0, 计数)，**不许**自己清计数。
//   ⚠ 计数 > 上限 → 当作写不下（响亮报错 / 丢弃），**不回绕**覆盖前面的记录。
// ============================================================
constexpr int HOST_LAYOUT_CMD_MAX_RECORDS     = %(max_records)d;
constexpr int HOST_LAYOUT_CMD_SLOTS_PER_RECORD = %(slots_per_record)d;
constexpr int HOST_LAYOUT_CMD_RECORD_BYTES    = HOST_LAYOUT_CMD_SLOTS_PER_RECORD * HOST_LAYOUT_VALUE_BYTES;
constexpr int HOST_LAYOUT_CMD_COUNT_SLOT      = %(count_slot)d;   // 计数所在的槽
constexpr int HOST_LAYOUT_CMD_RECORD_BASE     = %(record_base)d;  // 记录区起始槽（= 计数槽 + 1）
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

'''

HEADER_REQ = '''
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
%(req_consts)s
'''

HEADER_TAIL = '''
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
'''


def build_state_consts():
    """槽号常量。全是 int（槽号就是"第几个槽"）。

    ⚠ 名字里的 W0..W7 = word（哪一槽），不是"第几个位图"。
    ⚠ 顺序有讲究：*_WORDS 那三张位图的槽常量从真源里**先**挑出来排在前面，
       因为下面的位图规则常量（KEY_WORDS / VK_* / STATE_BIT）在注释里引用它们；
       其余字段按槽号排。排错了读起来就得来回翻。
    """
    fields = LAYOUT['state']['fields']
    bitmap_first = [f for f in fields if f[1].endswith('_WORDS')]
    rest = [f for f in fields if not f[1].endswith('_WORDS')]

    def emit(rows, lines):
        for rng, name, ty, _cmt in rows:
            if isinstance(rng, tuple):
                a, b = rng
                # 位图槽名 = <字段名>_W0..W7（W = word，不是"第几张位图"）——
                #   ⚠ 有意保留 _WORDS 这个中间词：别的任务（outer/host_state_in.cpp）
                #     已经按 HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W0 在写，改名 = 单方面毁约。
                for k in range(a, b + 1):
                    nm = ('%s_W%d' % (name, k - a)) if name.endswith('_WORDS') else '%s_%d' % (name, k)
                    lines.append('constexpr int HOST_LAYOUT_STATE_%s = %d;' % (nm, k))
            else:
                lines.append('constexpr int HOST_LAYOUT_STATE_%s = %d;' % (name, rng))

    lines = []
    emit(bitmap_first, lines)
    lines.append('')
    emit(rest, lines)
    return '\n'.join(lines)


def build_request_consts():
    out = []
    for num, nm, cn, fields in LAYOUT['request']['ids']:
        sig = ' + '.join('%s(%s)' % (t, n) for n, t in fields) or '（无字段）'
        out.append('// %d %s（%s）: %s' % (num, nm, cn, sig))
        out.append('constexpr int HOST_LAYOUT_REQ_%s = %d;' % (nm, num))
        out.append('')
    out.append('constexpr int HOST_LAYOUT_REQ_RESERVED_FROM = %d;   // 这个号起预留（往尾部加，不插中间）'
               % LAYOUT['request']['reserved_from'])
    out.append('')
    for grp, items in LAYOUT['request']['actions'].items():
        for val, nm, cn in items:
            # 常量名左边对齐到 56 列（最长的 WINDOW_ACTION_FULLSCREEN 是 55 字符），注释成一列
            decl = 'constexpr int HOST_LAYOUT_%s_%s = %d;' % (grp, nm, val)
            out.append(decl.ljust(56) + '// ' + cn)
        out.append('')
    return '\n'.join(out).rstrip('\n') + '\n'


def gen_header():
    st = LAYOUT['state']
    cmd = LAYOUT['command']
    txt = HEADER_INTRO % dict(
        version=LAYOUT['version'], value_bytes=VALUE_BYTES, slot_align=SLOT_ALIGN)
    txt += HEADER_STATE % dict(
        state_slots=st['slots'], state_slot_consts=build_state_consts(),
        bitmap_words=st['bitmap']['words'], bits_per_word=st['bitmap']['bits_per_word'],
        vk_max=st['bitmap']['vk_max'])
    txt += HEADER_CMD % dict(
        max_records=cmd['max_records'], slots_per_record=cmd['slots_per_record'],
        count_slot=COMMAND_COUNT_SLOT, record_base=COMMAND_RECORD_BASE)
    txt += HEADER_REQ % dict(req_consts=build_request_consts())
    txt += HEADER_TAIL
    return txt


# ============================================================
# 产物 2：host_layout.json（同一份数据，给别的语言 / 引擎）
# ============================================================
def bitmap_base(name):
    """哪三张位图各从第几槽起（从真源里查，不手写 16/24/32）"""
    for rng, nm, _ty, _c in LAYOUT['state']['fields']:
        if nm == name and isinstance(rng, tuple):
            return rng[0]
    raise SystemExit('真源里找不到位图 ' + name)


def gen_json():
    st = LAYOUT['state']
    cmd = LAYOUT['command']
    req = LAYOUT['request']

    slots = []
    for rng, name, ty, cmt in st['fields']:
        if isinstance(rng, tuple):
            a, b = rng
            row = dict(range='%d-%d' % (a, b), slot=a, count=b - a + 1,
                       name=name, type=ty, comment=cmt)
            # 位图那三段：把每个 word 的绝对槽号也列出来
            #   （别的语言没有 C++ 的常量名可引用，只能拿数字，写清楚省得自己算）
            if name.startswith('KEY_'):
                row['word_slots'] = list(range(a, b + 1))
            slots.append(row)
        else:
            slots.append(dict(range=str(rng), slot=rng, count=1, name=name, type=ty, comment=cmt))

    doc = {
        '_note': ('VM ↔ 宿主引擎 共享内存布局契约。生成物，别手改——'
                  '真源是 tools/gen_host_layout.py，改布局要同时 +version 并两边重新对齐。'
                  '槽 = %d 字节（Value = {int32 类型, int32 值}），全部小端，跨语言按 %d 字节对齐。'
                  % (VALUE_BYTES, SLOT_ALIGN)),
        'version': LAYOUT['version'],
        'value_bytes': VALUE_BYTES,
        'slot_align': SLOT_ALIGN,
        'blocks': {
            'state': {
                'direction': 'engine -> vm',
                'kind': 'overwrite-per-frame',      # 覆盖式写，不需要清
                'slots': st['slots'],
                'bytes': st['slots'] * VALUE_BYTES,
                'producer': st['producer'], 'consumer': st['consumer'],
                'slots_map': slots,
                'keyboard_bitmap': {
                    'words_per_bitmap': st['bitmap']['words'],
                    'bits_per_word': st['bitmap']['bits_per_word'],
                    'vk_max': st['bitmap']['vk_max'],
                    'bitmaps': {'down': bitmap_base('KEY_DOWN_WORDS'),
                                'pressed': bitmap_base('KEY_PRESSED_WORDS'),
                                'released': bitmap_base('KEY_RELEASED_WORDS')},
                    'lookup': 'word_slot = w0 + (vk >> 5); bit = vk & 31; down = (word >> bit) & 1',
                    'out_of_range': 'vk 不在 0..255 → 当"没按"',
                    'rule': st['bitmap']['rule'],
                },
            },
            'command': {
                'direction': 'vm -> engine',
                'kind': 'count + fixed-records',
                'slots': command_slots(),
                'bytes': command_slots() * VALUE_BYTES,
                'producer': cmd['producer'], 'consumer': cmd['consumer'],
                'count_slot': COMMAND_COUNT_SLOT,
                'record_base_slot': COMMAND_RECORD_BASE,
                'max_records': cmd['max_records'],
                'slots_per_record': cmd['slots_per_record'],
                'record_bytes': record_bytes(),
                'record_fields': [dict(index=i, name=n, type=t, comment=c)
                                  for i, n, t, c in cmd['record_fields']],
                'rules': cmd['rules'],
            },
            'request': {
                'direction': 'vm -> engine',
                'kind': 'append-stream (host_shm_*)',
                'transport': req['transport'],
                'capacity_note': req['capacity_note'],
                'signature_note': req['signature_note'],
                'ids': [dict(id=i, name=n, comment=c,
                             signature=[dict(name=fn, type=ft) for fn, ft in fields])
                        for i, n, c, fields in req['ids']],
                'reserved_from': req['reserved_from'],
                'actions': {g: [dict(value=v, name=n, comment=c) for v, n, c in items]
                            for g, items in req['actions'].items()},
            },
        },
    }
    return json.dumps(doc, ensure_ascii=False, indent=2, sort_keys=False) + '\n'


# ============================================================
# 写文件（幂等：同样内容不重写，省得动 mtime）
# ============================================================
def write_if_changed(path, text):
    old = None
    if os.path.exists(path):
        old = io.open(path, encoding='utf-8', newline='').read()
    if old == text:
        return False
    d = os.path.dirname(path)
    if d and not os.path.isdir(d):
        os.makedirs(d)
    # 一律 LF：生成物的换行不该跟着平台漂（两边 diff 才稳定）
    io.open(path, 'w', encoding='utf-8', newline='\n').write(text)
    return True


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    h_path = os.path.join(root, 'DLL_VM', 'host', 'host_layout.h')
    j_path = os.path.join(root, 'DLL_VM', 'host', 'host_layout.json')

    h_txt = gen_header()
    j_txt = gen_json()
    h_new = write_if_changed(h_path, h_txt)
    j_new = write_if_changed(j_path, j_txt)

    summary(root, h_path, j_path, h_new, j_new)


def summary(root, h_path, j_path, h_new, j_new):
    """控制台摘要。⚠ 全部 ASCII：Windows 控制台是 GBK，打中文/箭头会 UnicodeEncodeError"""
    st = LAYOUT['state']
    cmd = LAYOUT['command']
    req = LAYOUT['request']

    print('=== VM <-> host shared-memory layout contract, v%d ===' % LAYOUT['version'])
    print('  slot = %d bytes (Value = {int32 t, int32 v}); little-endian; cross-language align %d'
          % (VALUE_BYTES, SLOT_ALIGN))
    print('  B1 state   engine->vm   : %d slots / %d bytes (overwrite every frame, no clear needed)'
          % (st['slots'], state_bytes()))
    print('     slot 0 frame no; 1-4 mouse xy(world/gui); 5-7 buttons mask(down/pressed/released);')
    print('     8 scroll; 9 focus; 10 ime; 11-12 window wxh; 13-14 frame ms / total time')
    print('     keyboard bitmaps x3: %d-%d down, %d-%d pressed, %d-%d released (bit = vk 0..255)'
          % (bitmap_base('KEY_DOWN_WORDS'), bitmap_base('KEY_DOWN_WORDS') + 7,
             bitmap_base('KEY_PRESSED_WORDS'), bitmap_base('KEY_PRESSED_WORDS') + 7,
             bitmap_base('KEY_RELEASED_WORDS'), bitmap_base('KEY_RELEASED_WORDS') + 7))
    print('     reserved: slot 15 + slots 40-63 (24 slots)')
    print('  B2 command vm->engine   : %d slots / %d bytes = count 1 slot + %d records x %d slots'
          % (command_slots(), command_bytes(), cmd['max_records'], cmd['slots_per_record']))
    print('     record = %d bytes: [0]cmd i32 [1][2]int i32 [3..7] 5x float f32' % record_bytes())
    print('  A  request vm->engine   : host_shm_* stream (capacity/cursor from host_shm_size()/host_shm_cursor())')
    for i, n, c, fields in req['ids']:
        sig = ' '.join('%s:%s' % (fn, ft) for fn, ft in fields) or '(none)'
        print('     %d %-13s %s' % (i, n, sig))
    print('     %d+ reserved (append at the tail, never insert in the middle)' % req['reserved_from'])
    print('  -> %s (%s)' % (os.path.relpath(h_path, root).replace('\\', '/'),
                            'updated' if h_new else 'unchanged (idempotent)'))
    print('  -> %s (%s)' % (os.path.relpath(j_path, root).replace('\\', '/'),
                            'updated' if j_new else 'unchanged (idempotent)'))
    print("  NOTE: any layout change => LAYOUT['version'] +1 and re-align both sides")
    print('        (engine compares host_layout_version() at startup)')


main()
