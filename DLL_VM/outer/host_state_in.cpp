// ============================================================
// host_state_in.cpp — 读 **B1 状态区** 的那 6 条输入 native（名单见 outer_split.h 的 HOST_STATE_LIST）
//
//     keyboard_check_pressed(key)       ← B1 的"本帧刚按下"键盘位图
//     mouse_check_button(btn)           ← B1 的"按住"掩码
//     mouse_check_button_pressed(btn)   ← B1 的"本帧刚按下"掩码
//     mouse_check_button_released(btn)  ← B1 的"本帧刚松开"掩码
//     device_mouse_x_to_gui()           ← B1 的 gui 鼠标 x
//     device_mouse_y_to_gui()           ← B1 的 gui 鼠标 y
//
//   为什么这 6 条**不该**留在 host_fn.cpp 当空壳：
//     它们不需要引擎回调 / 函数指针 —— 引擎每帧已经把输入**覆盖着写进** B1 那块共享内存了
//     （契约 host/host_layout.h，真源 tools/gen_host_layout.py），这里只要按槽号读一眼。
//     空壳名单那批的语义是"等宿主把函数指针填进来"，这 6 条不是那个形状的活。
//
//   为什么**不**借道 mem 的临时槽去猜：
//     host_state_base() 给的就是那块内存本身，值就在槽里 —— 多绕一次临时槽只会多一次
//     拷贝和一次"类型被中间层改过"的机会（而且还得先借几槽，等于凭空多一份状态）。
//
//   ⚠ 槽号 / 位图规则**一个都不在这里手写**：全用契约（host_layout.h）的常量。
//     唯一"翻译"的是 GML 的 mb_* 编号 → 契约掩码位（两边编号确实不一样，见下面 mb_bit）。
//
//   ⚠ 本文件不 printf；参数不对 / 契约外的输入照 GML 语义回 0，不乱置 vm_set_err
//     （输入查询"这个键没按"不是错误，GML 里也一样）。
// ============================================================
#include "outer.h"
#include "../host/host.h"
#include "../host/host_layout.h"

// ============================================================
// 分层互检（编译期）—— 这几条 assert 是"两边抄岔了"的唯一防线：
//   B1 的内存是跨层共享的，错位读**不会**报错、只会读出垃圾，所以必须在这儿钉死。
// ============================================================
// ① 内层的 Value 和 host 层那个 POD 槽必须同宽 —— 否则整个槽数组索引全错位
static_assert(sizeof(Value) == sizeof(HostStateSlot),
              "B1 槽：内层 Value 和 host 的 HostStateSlot 必须同宽（都是 {int32,int32}）");
// ② host 层为了"清成 undefined"只抄了一个类型号，必须和内层的 MEM_UNDEFINED 一致
static_assert(HOST_STATE_T_UNDEFINED == MEM_UNDEFINED,
              "B1：host 层抄的 undefined 类型号（host.h 的 HOST_STATE_T_UNDEFINED）和内层漂了");
// ③ 契约里的槽数必须装得下"最后一张键盘位图的最后一槽"（写死 64 槽的契约自检也在那边）
static_assert(HOST_LAYOUT_STATE_KEY_RELEASED_WORDS_W0 + HOST_LAYOUT_STATE_KEY_WORDS
              <= HOST_LAYOUT_STATE_SLOTS,
              "B1：三张键盘位图必须都在状态区范围内");

namespace {

// ── GML 的 mb_* 编号 → 契约掩码位 ──
// GML 的 mb_* 编号（gm_builtin_consts.json：mb_left=1 / mb_middle=2 / mb_right=3）
// 和契约掩码（host_layout.h 的 MOUSE_BUTTONS："bit0 左 / bit1 右 / bit2 中"）
// **不是同一套编号**，所以绝不能拿 mb_ 的值当位号：
//     mb_left  = 1 → bit 0
//     mb_middle= 2 → bit 2      （契约里"中"是第三个位，不是第二个）
//     mb_right = 3 → bit 1
//   别的值（mb_any / mb_none / 乱传的数）契约里没有对应的位 → -1 = "这条不算数"。
int mb_bit(VM_INT btn) {
    switch (btn) {
        case 1:  return 0;      // mb_left
        case 2:  return 2;      // mb_middle
        case 3:  return 1;      // mb_right
        default: return -1;
    }
}

// ── 取槽的小工具 ──
//   ⚠ B1 首地址**每次现取**（和 host_shm_base() 一个规矩）：地址现在不会变，
//     但这样"以后可能变"不需要改任何调用方。
//
// 一个 int 槽（掩码 / 位图字）。类型不是 int（引擎没照契约写）→ 当 0：
//   掩码和位图是**位**，不能拿 float 去凑；宁可回"没按"，也不把 float 的位模式当按键位。
int32_t st_int(int slot) {
    const HostStateSlot* p = host_state_base();
    return (p[slot].t == MEM_INT) ? p[slot].v : 0;
}

// 一个数值槽（坐标）。契约写的是 float，但引擎真写成 int 也得认 —— 数就是数，
//   这里不是"宽容"，是"坐标的数值语义不依赖它被存成 int 还是 float"。
//   ⚠ 别的类型（undefined = 引擎还没写过 / 清过）→ 0：GML 里 device_mouse_*_to_gui
//     没有"没有值"这个返回，只能给 0（和 host_input.cpp 那套"没接宿主 → 0"一致）。
VM_FLOAT st_num(int slot) {
    const HostStateSlot* p = host_state_base();
    if (p[slot].t == MEM_FLOAT) return f_of(p[slot].v);
    if (p[slot].t == MEM_INT)   return (VM_FLOAT)p[slot].v;
    return (VM_FLOAT)0;
}

// vk 键码 → 位图里那一位。三张位图共用这一套取法（契约的 lookup）。
//   ⚠ **先裁边界再算槽号**：契约的 HOST_LAYOUT_STATE_VK_WORD(vk) 是故意不裁的
//     （它按契约假设调用方已经保证 0..255），拿 -1 / 999 直接去算会读到槽数组**外面**——
//     那不是"没按"，是别的内存，是这套共享内存最容易出的事故。
//     GML 里 keyboard_check_pressed(-1) 只是"没这个键" → 回 0，不越界读（这条有自检钉着）。
bool vk_bit(int bitmap_w0, VM_INT vk) {
    if (vk < 0 || vk > HOST_LAYOUT_STATE_VK_MAX) return false;
    int32_t word = st_int(bitmap_w0 + HOST_LAYOUT_STATE_VK_WORD((int)vk));
    return (((uint32_t)word >> HOST_LAYOUT_STATE_VK_BIT((int)vk)) & 1) != 0;
}

// 掩码查一位（三个 mouse_check_button* 共用）。btn 不是 mb_left/middle/right → 0。
bool mask_bit(int mask_slot, VM_INT btn) {
    int bit = mb_bit(btn);
    if (bit < 0) return false;
    return (((uint32_t)st_int(mask_slot) >> bit) & 1) != 0;
}

// ── 位图槽号 ──
//   ⚠ 契约生成器把"float 类型的字段"也写成了 constexpr float（见 host_layout.h 的
//     MOUSE_GUI_X），槽号是**整数**，所以取用时一律显式转 int：既避免 /W4 的
//     float→int 转换告警，也让"这是槽号、不是槽里的值"在代码里一眼看得出。
const int KEY_PRESSED_W0 = (int)HOST_LAYOUT_STATE_KEY_PRESSED_WORDS_W0;
const int MASK_DOWN      = (int)HOST_LAYOUT_STATE_MOUSE_BUTTONS;
const int MASK_PRESSED   = (int)HOST_LAYOUT_STATE_MOUSE_PRESSED;
const int MASK_RELEASED  = (int)HOST_LAYOUT_STATE_MOUSE_RELEASED;
const int GUI_X          = (int)HOST_LAYOUT_STATE_MOUSE_GUI_X;
const int GUI_Y          = (int)HOST_LAYOUT_STATE_MOUSE_GUI_Y;

}  // namespace

// ============================================================
// 键盘：keyboard_check_pressed(key) —— "本帧刚按下"
//   逐帧语义：B1 的 KEY_PRESSED 位图是引擎**每帧重写**的（只有这一帧新按下的键才置位），
//   所以查一位就够，不用（也不能）在这儿记"上一次查过没有"——那会把同一帧读两次
//   变成两次点击（契约头里专门点了这个坑）。
//   返回值照 GM：**数字 0/1**（不是 true/false 文本）。
// ============================================================
void keyboard_check_pressed(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT key = (argc > 0) ? arg_i(mem, args, 0) : 0;
    wr_i(mem, dst, vk_bit(KEY_PRESSED_W0, key) ? 1 : 0);
}

// ============================================================
// 鼠标三条：按住 / 本帧刚按下 / 本帧刚松开
//   三张掩码是**互相独立**的字段（契约里是三个槽）：引擎可以只置 pressed 不置 down
//   （比如一帧内按下又松开），所以这里各查各的，不许拿一张推另一张。
//   参数 btn = GML 的 mb_*（见 mb_bit 的映射表）。
// ============================================================
void mouse_check_button(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT btn = (argc > 0) ? arg_i(mem, args, 0) : 0;
    wr_i(mem, dst, mask_bit(MASK_DOWN, btn) ? 1 : 0);
}

void mouse_check_button_pressed(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT btn = (argc > 0) ? arg_i(mem, args, 0) : 0;
    wr_i(mem, dst, mask_bit(MASK_PRESSED, btn) ? 1 : 0);
}

void mouse_check_button_released(Value* mem, const uint8_t* args, int argc, int dst) {
    VM_INT btn = (argc > 0) ? arg_i(mem, args, 0) : 0;
    wr_i(mem, dst, mask_bit(MASK_RELEASED, btn) ? 1 : 0);
}

// ============================================================
// 鼠标坐标：device_mouse_x_to_gui() / y —— GUI 层坐标，**数值**返回（不是 0/1）
//   为什么是 wr_f（浮点槽）：GML 的 device_mouse_*_to_gui 回的是 real，
//   坐标做除法 / 对齐时整数化就是自己找死；这里不替调用方取整。
// ============================================================
void device_mouse_x_to_gui(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_f(mem, dst, st_num(GUI_X));
}

void device_mouse_y_to_gui(Value* mem, const uint8_t* args, int argc, int dst) {
    (void)args; (void)argc;
    wr_f(mem, dst, st_num(GUI_Y));
}
