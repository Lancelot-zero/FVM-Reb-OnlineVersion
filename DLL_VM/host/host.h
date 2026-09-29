// ============================================================
// host.h — 宿主层：**外层依赖的平台原语**
//
//   分层：
//     外层  outer/     VM 语义：把 Value 从槽里取出来、把结果写回槽
//       ↓ 调这里
//     宿主  host/      平台原语：文件 / 缓冲区 / 输入 / 绘制 / 网络
//       ↓
//     游戏引擎 / 操作系统
//
//   两类东西，分得很清楚：
//     【自己能干的】本地文件读写、缓冲区、时间
//         → 这里就有**默认实现**（标准库），不接宿主也能跑
//     【要看引擎的】输入、绘制、网络
//         → 一张**函数表**（g_host_in / g_host_draw / g_host_net）
//           宿主在启动时把自己的实现填进去；不填就是"没接"，返回 0 / 空操作，
//           外层那边会看到"没反应"，而不是崩
//
//   约定：
//     · 这里只有**原语**（取一个数、画一个精灵、写一段字节）——
//       GML 那套类型语义（Value 的类型、返回值是 0/1 还是 undefined）留在外层
//     · 字符串用 std::string（UTF-8 字节）；运行时"池下标 → 文本"的转换在外层做
//     · 这一层不认识 Value / 槽 / VM，只认平台类型
// ============================================================
#pragma once
#include <cstdint>
#include <string>

// ============================================================
// 1 输入控制（host_input.cpp）—— 要看引擎，走表
// ============================================================
struct HostInputOps {
    // 键盘：key = 键码（GML 的 vk_* / 字符码，外层直接透传）
    bool (*key_down)   (int key);          // 一直按着
    bool (*key_pressed)(int key);          // 这一帧刚按下
    std::string (*key_string)();           // 这一帧输入的文本（UTF-8，没输入 → 空）
    // 鼠标：btn = 0 左 / 1 右 / 2 中
    bool (*mouse_down)   (int btn);
    bool (*mouse_pressed)(int btn);
    bool (*mouse_released)(int btn);
    double (*mouse_x)();                   // 房间坐标
    double (*mouse_y)();
    double (*mouse_x_to_gui)();            // GUI 层坐标
    double (*mouse_y_to_gui)();
};
extern HostInputOps g_host_in;             // 宿主填；没填就是下面那套默认（全假 / 全 0）

// ============================================================
// 2 绘制控制（host_draw.cpp）—— 要看引擎，走表
// ============================================================
struct HostDrawOps {
    // 画图：spr = 精灵句柄（宿主自己那套编号），坐标是房间坐标
    void (*sprite)(int spr, double x, double y);
    void (*sprite_ext)(int spr, double x, double y, double xs, double ys, double rot,
                       double alpha, int blend, int frame);
    void (*text)(double x, double y, const std::string& s);        // 用当前的字体/颜色/对齐
    void (*text_ext)(double x, double y, const std::string& s, double w);
    void (*rectangle)(double x1, double y1, double x2, double y2, bool outline);
    void (*rectangle_colour)(double x1, double y1, double x2, double y2,
                             int c1, int c2, int c3, int c4, bool outline);
    void (*roundrect)(double x1, double y1, double x2, double y2, double rx, double ry, bool outline);
    void (*line)(double x1, double y1, double x2, double y2);
    void (*clear_alpha)(int colour, double alpha);
    // 状态：颜色 / 透明度 / 字体 / 对齐
    void (*set_colour)(int colour);
    void (*set_alpha)(double alpha);
    void (*set_font)(int font);
    void (*set_halign)(int align);         // 0 左 / 1 中 / 2 右
    void (*set_valign)(int align);         // 0 上 / 1 中 / 2 下
    // 表面（离屏画布）与纹理
    int  (*surface_create)(int w, int h);
    void (*surface_free)(int surf);
    bool (*surface_exists)(int surf);
    void (*surface_set_target)(int surf);
    void (*surface_reset_target)();
    void (*draw_surface)(int surf, double x, double y);
    void (*gpu_set_tex_filter)(bool on);
    void (*texture_prefetch)(int tex);
    // 绘制开关 / 尺寸查询
    void (*enable_drawevent)(bool on);
    int  (*sprite_width)(int spr);
    int  (*sprite_height)(int spr);
    std::string (*sprite_name)(int spr);
    int  (*string_width)(const std::string& s);      // 文字量宽：要字体度量，所以也在宿主这边
    int  (*string_height)(const std::string& s);
    int  (*string_width_ext)(const std::string& s, double sep, double w);
    int  (*string_height_ext)(const std::string& s, double sep, double w);
};
extern HostDrawOps g_host_draw;

// 画布尺寸（VM_DrawSpriteExt / 屏幕坐标换算要用）
int  host_draw_width();
int  host_draw_height();
void host_draw_set_size(int w, int h);          // 宿主启动时填一次（不填就是 0）

// ============================================================
// 3 本地文件读写（host_file.cpp）—— 默认实现就是标准库，直接能用
// ============================================================
enum HostFileMode { HOST_FILE_READ = 0, HOST_FILE_WRITE = 1, HOST_FILE_APPEND = 2, HOST_FILE_READ_TEXT = 3 };

bool host_file_exists(const std::string& path);
bool host_dir_exists (const std::string& path);
bool host_dir_create (const std::string& path);

void* host_file_open (const std::string& path, int mode);   // 失败 → NULL
bool  host_file_write(void* h, const std::string& s);       // 不加换行
bool  host_file_writeln(void* h, const std::string& s);     // 加换行
std::string host_file_read_string(void* h);                 // 读一个"非空白"词（照 GML）
std::string host_file_readln(void* h);                      // 读一行（不含换行）
bool  host_file_eof(void* h);
void  host_file_close(void* h);

// 时间（存档 / 日志要用；本地时间，格式自己拼）
std::string host_date_current();                            // "YYYY-MM-DD hh:mm:ss"
std::string host_date_format(const std::string& fmt);       // 只认 %Y %m %d %H %M %S

// ── date_* 用：GML 的 datetime 实数（1899-12-30 起的天数，本地时间）──
//   为什么放在宿主层：<ctime> 才是"当前时间 / 纪元换算"的真源，外层不该自己算日历
//   （闰年、时区那套）。外层只负责把 double 塞进槽。
//   ⚠ GML 的纪元是 1899-12-30，和 Unix(1970-01-01) 差 **25569** 天。
//   ⚠ 本地时间语义：先取本地 tm，再按"假装它是 UTC"换算 → 得到的日期数就是挂钟时间，
//     这样 host_date_now / host_date_to_string 一来一回不会漂。
double      host_date_now();                                // 现在（本地时间）→ datetime
std::string host_date_to_string(double dt);                 // datetime → "YYYY-MM-DD hh:mm:ss"

// ============================================================
// 4 buf 处理（host_buffer.cpp）—— 默认实现，直接能用
//   id 是池下标（0 起，可复用）；读写位置在句柄上，和 GML 的 buffer 一致
//   类型化读写（buffer_u8 / buffer_s32 / buffer_f32 …）留在外层：这里只有字节
// ============================================================
int  host_buf_create(int size, int type);                   // 全 0 填充
void host_buf_delete(int id);
bool host_buf_alive (int id);
int  host_buf_size  (int id);
int  host_buf_tell  (int id);
void host_buf_seek  (int id, int pos);
bool host_buf_write (int id, const void* p, int n);         // 当前位置写，超了就变长
int  host_buf_read  (int id, void* out, int n);             // 当前位置读，返回真读到的字节数
bool host_buf_poke  (int id, int off, const void* p, int n);
bool host_buf_peek  (int id, int off, void* out, int n);
bool host_buf_copy  (int dst, int src, int src_off, int dst_off, int n);
bool host_buf_load  (int id, const std::string& path);      // 把文件内容填进 buf（覆盖前面）

// ============================================================
// 5 网络协议（host_net.cpp）—— 要看平台，走表
//   ⚠ "协议"两个字在宿主手里：这里只给"发一段字节 / 收到一段字节"的原语，
//     怎么编解码、连谁、什么时候重连，都由宿主实现决定
// ============================================================
struct HostNetOps {
    bool (*send_raw)(int sock, const void* p, int n);
    int  (*recv_raw)(int sock, void* out, int n);            // 返回真读到的字节数，0 = 没有
    void (*disconnect)();                                    // 断连接（GML 的 disconnect）
    void (*destroy)();                                       // 关掉整条网络（network_destroy）
    bool (*http_request)(const std::string& url, const std::string& body, std::string* out);
    void (*url_open)(const std::string& url);                // 拿系统浏览器打开
    void (*broadcast)(int kind, const void* p, int n);       // 主动广播（技能 / 铲子那种）
};
extern HostNetOps g_host_net;

// ============================================================
// 6 出站共享内存（host_shared.cpp）—— VM → 宿主引擎的**单向**数据通道
//
//   一块**连续内存** + 一个**写游标**：
//     VM 侧只有 host_push.cpp 的 VM_HostPush 往游标后面**追加**字节；
//     宿主引擎拿着 host_shm_base() 读 [0, host_shm_cursor()) 那一段。
//
//   ⚠ 方向是单向的：**谁读、读多少、读完什么时候清，全由引擎说了算** ——
//     所以"复位"（host_shm_reset）是给引擎消费完之后调的，**不给 VM 暴露**，
//     读游标 / 取首地址也一样（那是宿主 / 引擎的事，VM 只管往后写）。
//   ⚠ 游标**只增不减**（只有 host_shm_reset 归零），这样 VM 侧永远不需要知道"读到哪了"，
//     也就不会出现"VM 把引擎没读走的数据覆盖掉"这种事。
//   ⚠ 容量策略见 host_shared.cpp：初始 64KB / 不够翻倍 / 上限 16MB，
//     写不下时 host_shm_write 返回 false（**不静默截断**），由调用方去报错。
//
//   ── "按类型追加"的那三个（宿主/引擎侧**直接可用**，不必经过 VM）──
//     通道的**字节长什么样**只在这儿定（见 host_shared.cpp 的 ★编码格式★）：
//       int   → s32（4 字节小端）
//       float → f32（4 字节小端）
//       string / buf → u32 长度（字节数，小端）+ 那段裸字节（**两个共用同一支**）
//       undefined → 什么都不写（0 字节）
//     三个都满足"这一笔要么整段进去、要么一个字节都不动"（失败 → false）。
//     VM 侧的 VM_HostPush 只是薄薄一层：照实参的 Value 类型挑其中一支调。
//   ⚠ 包里**没有类型字节**：类型靠两端事先约定的布局；传错类型不会被发现（设计前提）。
// ============================================================
void      host_shm_reset();                       // 游标归零（**引擎消费完自己调**）
uint8_t*  host_shm_base();                        // 首地址（就是要传给引擎的那个）
int       host_shm_size();                        // 当前容量（字节）
int       host_shm_cursor();                      // 写游标：已经写到第几字节
bool      host_shm_write(const void* p, int n);   // 往后追加 n 字节；写不下 → false（一个字节都不动）

bool      host_shm_push_i32(int32_t v);           // 追加一个 int：4 字节小端
bool      host_shm_push_f32(float v);             // 追加一个 float：4 字节小端
bool      host_shm_push_bytes(const void* p, uint32_t n);   // 追加 u32 长度 + n 字节（string / buf 共用）

// ============================================================
// 7 入站包（host_packet.cpp）—— 宿主引擎 → VM 的**单向**数据通道
//
//   和上面第 6 节正好是反方向：
//     出站：VM 往后写一整块，引擎拿 base + cursor **自己读**（不走回调）
//     入站：引擎把**一包**字节的首地址和长度递进来，调 host_packet_apply() ——
//           这一下是**回调**：控制权当场进 VM，返回时这一包已经处理完
//
//   ⚠ 这一层照旧**不认识 Value / 槽 / VM**：包里是什么、怎么解包，全是 VM 侧的事
//     （规则见 outer/packet.cpp）。这里只有"引擎该调哪个函数"这**一个函数指针**。
//   ⚠ 所以 host_packet_apply 只判**注册**：没注册 handler → 返回负数（**不崩**）。
//     buf / size 的合法性**故意留给 handler**：只有 VM 侧才能 vm_set_err 响亮报错，
//     在这一层先拦掉会让错误码永远置不上 —— "坏包静默"是明确否掉的。
//   ⚠ 跨 DLL 只过 POD：void* + int，没有对象、没有 std::string；调用是同步的，
//     引擎不持有任何 VM 侧东西的生命周期。
// ============================================================
typedef int (*HostPacketFn)(void* buf, int size);   // 包内存 + 字节数 → 处理结果（<0 = 失败）
void host_packet_set_handler(HostPacketFn fn);      // VM 启动时注册进来（幂等，可重复调；传 NULL = 撤销注册）
int  host_packet_apply(void* buf, int size);        // 引擎调这个；没注册 handler → 返回负数，别崩

// ============================================================
// 8 布局契约版本（host/host_layout.h + tools/gen_host_layout.py）—— 给引擎**对暗号**用的
//
//   共享内存那套约定（B1 状态区 / B2 命令区 / A 请求流）是"不链接、只约定"的：
//   两边的偏移要是错开了，**不会报错**，只会安静地读出垃圾 —— 这是这套接口唯一
//   真正危险的地方，所以必须有一句话能让引擎在**开工前**问清楚"你这份约定是第几版"。
//
//   返回值 = HOST_LAYOUT_VERSION（host_layout.h 里的编译期常量，由生成器写死）。
//   引擎启动时比一下：不一样就别开工（不要"凑合读"），去重新生成 / 换一份对齐的 DLL。
//
//   ⚠ 符号名固定是 host_layout_version（extern "C" + dllexport）：宿主可能是 C#/Rust/
//     别的 C++ 编译器，都要能按这个名字直接 GetProcAddress / dlsym 上来，
//     所以**不能**让它进 C++ 的名字修饰（mangling），也不改这个名字。
//   ⚠ 这里只**读一个编译期常量**，不碰任何运行期状态：VM 还没初始化也照样能问。
// ============================================================
#if defined(_WIN32)
#define HOST_LAYOUT_API extern "C" __declspec(dllexport)
#else
#define HOST_LAYOUT_API extern "C"
#endif

HOST_LAYOUT_API int host_layout_version();          // → HOST_LAYOUT_VERSION

// ============================================================
// 9 B1 状态区（host_state.cpp）—— 宿主引擎 **写** → VM **读**（覆盖式每帧写）
//
//   和上面第 6 / 7 节并列的第三块共享内存：
//     6 出站 host_shm_*    VM 往后写一整块，引擎拿 base + cursor 自己读
//     7 入站 host_packet_* 引擎递一包，VM 当场回调解掉
//     9 B1   host_state_*  **引擎每帧把整块重写**，VM 按槽号读（没有回调、没有游标）
//
//   槽号 / 字段类型 / 键盘位图规则**不在这里**：全在契约 host/host_layout.h
//   （生成物，真源 tools/gen_host_layout.py）。这里只给"把那块内存拿出来"的口 ——
//   约定只有一份真源，在这层再抄一遍槽号就等于给以后埋一个静默读错。
//
//   ⚠ 分层：host 层**不许** include 内层头（core/vm_def.h），所以这里用一个
//     **同布局的 POD** 把"一个槽长什么样"重述一遍，而不是去引 Value：
//         struct Value         { int32_t t; int32_t v; };   // 内层，8 字节
//         struct HostStateSlot { int32_t t; int32_t v; };   // 这里，同样 8 字节
//     下面 static_assert 把 8 字节钉死；外层（outer/host_state_in.cpp，它能看见 Value）
//     再拿 sizeof(Value) 和它比一次 —— 这样"两边抄岔了"是**编译期**报错，
//     而不是运行时按错位读出垃圾（那是这套共享内存唯一真正危险的失败模式）。
// ============================================================
struct HostStateSlot { int32_t t; int32_t v; };
static_assert(sizeof(HostStateSlot) == 8,
              "B1 槽 = {int32 类型, int32 值} = 8 字节（契约 HOST_LAYOUT_VALUE_BYTES）");

// 类型字节：HostStateSlot::t 的取值。host 层不认识 Value，但"清成 undefined"总得有个数。
//   ⚠ 只留**这一个**：能少抄一个就少一个抄岔的机会。它还由外层
//     static_assert(HOST_STATE_T_UNDEFINED == MEM_UNDEFINED) 钉住。
constexpr int32_t HOST_STATE_T_UNDEFINED = 6;

HostStateSlot* host_state_base();     // B1 首地址（槽数组）；引擎按 host_layout.h 的槽号直接写
int            host_state_slots();    // 槽数（== 契约里的 HOST_LAYOUT_STATE_SLOTS）
void           host_state_clear();    // 全部槽清成 undefined（可选：引擎想用就用；覆盖式写不需要）
