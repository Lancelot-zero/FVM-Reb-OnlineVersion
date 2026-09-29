// ============================================================
// host_net.cpp — 网络协议（要看平台，走表）
//
//   这里**只给原语**：发一段字节、收一段字节、断开、广播。
//   "协议"本身（包头怎么排、什么时候重连、丢包怎么办）在宿主那边 ——
//   VM 只负责把脚本给的那几个参数摆好交出去。
//
//   默认那套 = "没接宿主"：发送全失败、收到 0 字节、断开是空操作。
// ============================================================
#include "host.h"

static bool d_send_raw(int, const void*, int) { return false; }
static int  d_recv_raw(int, void*, int)       { return 0; }
static void d_disconnect()                    {}
static void d_destroy()                       {}
static bool d_http_request(const std::string&, const std::string&, std::string*) { return false; }
static void d_url_open(const std::string&)    {}
static void d_broadcast(int, const void*, int) {}

HostNetOps g_host_net = {
    d_send_raw, d_recv_raw, d_disconnect, d_destroy, d_http_request, d_url_open, d_broadcast
};
