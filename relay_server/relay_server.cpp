// relay_server.cpp — C++ 中继服务器（单线程事件循环版）
// 编译 (MinGW):  g++ -o relay_server.exe relay_server.cpp -lws2_32 -std=c++17 -O2
// 编译 (MSVC):   cl /EHsc /std:c++17 /O2 relay_server.cpp ws2_32.lib
// 用法: relay_server.exe [--port <端口>] [--max-members <人数>]
//
// 与旧版（一线程一连接 + 全局锁）相比：
//   · 单线程事件循环：poll + 非阻塞 I/O + 每连接收发缓冲，消除死锁/竞态/双重关闭
//   · 命令前缀统一为 "/"（旧版检测 "\" 与帮助文本不一致）；移除 \rename 命令
//   · 房间闲置按"距最后一条消息"判定（旧版按创建时间，会误杀活跃房间）
//   · 防护：包长上限、发送缓冲上限（超限断开不读数据的连接）、握手超时
//   · 首包支持 /listroom 查询：返回房间列表后断开（不入房）
//   · 其余协议行为与旧版一致：包格式、信号字符串、房间信息、文件中转、聊天前缀等

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    // Windows 下用 select 等事件（老版 MinGW 的 winsock2.h 不提供 WSAPoll）。
    // Windows 的 fd_set 是数组实现，扩大 FD_SETSIZE 即可支撑更多连接。
    #ifndef FD_SETSIZE
        #define FD_SETSIZE 4096
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socklen_t = int;
    #define MSG_NOSIGNAL 0
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <cerrno>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define closesocket    close
    using pollfd_t = pollfd;
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ================================================================
//  消息ID (与 GM 端一致)
// ================================================================
enum : int32_t {
    MSG_CHAT             = 3,
    MSG_START_BATTLE     = 12,
    MSG_ENTER_ROOM_READY = 13,
    MSG_SERVER_ACTION    = 19,
    MSG_PUB_INFO         = 23,
    MSG_REQUEST_FILE     = 26,
    MSG_TRANSFER_FILE    = 27,
};

// ================================================================
//  可调参数
// ================================================================
const size_t MAX_PACKET     = 32 * 1024 * 1024;              // 单包 body_len 上限
const size_t MAX_OUTBUF     = MAX_PACKET + 8 * 1024 * 1024;  // 每连接发送缓冲上限，超限断开
const double HANDSHAKE_TIMEOUT = 120.0;  // 连接后未完成握手则断开（秒）
const double FLUSH_GRACE       = 10.0;   // 优雅关闭最多等待缓冲发完（秒）
const double BATTLE_MAX_HOURS  = 2.0;    // 战斗超时强制关房
const double IDLE_MAX_HOURS    = 2.0;    // 非战斗房间闲置超时强制关房

// ================================================================
//  随机名字池
// ================================================================
const std::vector<std::string> NAMES = {
    "小笼包","烧麦","虾饺","春卷","汤圆","粽子","月饼",
    "火锅","麻辣烫","烤串","炸鸡","汉堡","薯条","可乐",
    "奶茶","布丁","果冻","蛋糕","饼干","冰淇淋","巧克力",
    "爆米花","甜甜圈","马卡龙","提拉米苏","芒果","草莓","西瓜",
    "菠萝","葡萄","柠檬","樱桃","蜜桃","椰子","榴莲",
    "年糕","麻薯","蛋挞","松饼","可颂","吐司","饭团",
};

// ================================================================
//  socket 工具
// ================================================================
bool set_nonblock(SOCKET s) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

void init_network() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
}

void cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool would_block() {
#ifdef _WIN32
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

double now_sec() {
    return std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void close_sock(SOCKET s) {
    shutdown(s, 2);   // SD_BOTH / SHUT_RDWR
    closesocket(s);
}

// ================================================================
//  数据结构
// ================================================================
struct Room {
    std::string id;
    std::string version;
    SOCKET host = INVALID_SOCKET;
    std::map<int, SOCKET> clients;   // cid → sock
    std::map<SOCKET, std::string> nicks;
    int next_cid = 1;
    std::string state = "lobby";
    std::string data;
    double created_at = 0;
    double battle_started_at = 0;
    double last_active = 0;
    std::unordered_map<std::string, std::vector<uint8_t>> file_cache;
    std::unordered_map<std::string, std::vector<std::pair<SOCKET, std::string>>> file_pending;
    std::vector<uint8_t> raw_payload;    // MSG_ENTER_ROOM_READY 原始二进制
    std::unordered_map<std::string, std::string> msgs;  // name → 最新消息

    int member_count() const {
        int n = (host != INVALID_SOCKET) ? 1 : 0;
        return n + (int)clients.size();
    }

    std::set<std::string> used_names() const {
        std::set<std::string> s;
        for (auto& kv : nicks) s.insert(kv.second);
        return s;
    }
};

struct Conn {
    SOCKET sock = INVALID_SOCKET;
    std::vector<uint8_t> in;   // 接收缓冲
    size_t in_pos = 0;         // 已消费偏移
    std::vector<uint8_t> out;  // 发送缓冲
    bool joined = false;            // 握手完成、已入房
    bool handshake_failed = false;  // 握手被拒，丢弃后续输入
    bool closing = false;           // 已在待关闭队列
    bool close_after_flush = false; // 发完 out 后关闭
    bool dead = false;              // 发送缓冲超限，忽略后续发送
    double connected_at = 0;
    double flush_deadline = 0;
    Room* room = nullptr;
    int role = -1;   // 0 = 房主, 1 = 客户端
    int cid = -1;
    std::string name;
};

// ================================================================
//  全局状态（单线程，无需锁）
// ================================================================
std::unordered_map<std::string, Room> rooms;
std::unordered_map<SOCKET, Conn> conns;
std::vector<SOCKET> g_to_close;        // 待关闭队列
std::mt19937 rng{ std::random_device{}() };
int g_max_members = 8;

// 前置声明
void queue_hard_close(SOCKET s);
void queue_graceful_close(SOCKET s);
void append_out(Conn& c, const void* data, size_t len);
void send_pkt(Conn& c, int32_t msg_id, const void* payload = nullptr, int payload_len = 0);
void send_str(Conn& c, int32_t msg_id, const std::string& text);
void sync_room_info(Room& room);
void broadcast_pub(Room& room, const std::string& text);
void sync_player_ids(Room& room);

// ================================================================
//  读/写工具 — 包格式: [u32 body_len][i32 msg_id][payload]
// ================================================================
void append_out(Conn& c, const void* data, size_t len) {
    if (c.dead || len == 0) return;
    if (c.out.size() + len > MAX_OUTBUF) {
        // 对端长时间不读数据，直接断开
        c.dead = true;
        queue_hard_close(c.sock);
        return;
    }
    c.out.insert(c.out.end(), (const uint8_t*)data, (const uint8_t*)data + len);
}

void send_pkt(Conn& c, int32_t msg_id, const void* payload, int payload_len) {
    if (c.dead) return;
    int body_len = 4 + payload_len;
    std::vector<uint8_t> pkt(4 + body_len);
    memcpy(pkt.data(), &body_len, 4);
    memcpy(pkt.data() + 4, &msg_id, 4);
    if (payload && payload_len > 0)
        memcpy(pkt.data() + 8, payload, payload_len);
    append_out(c, pkt.data(), pkt.size());
}

void send_str(Conn& c, int32_t msg_id, const std::string& text) {
    if (c.dead) return;
    std::vector<uint8_t> payload(text.size() + 1);
    memcpy(payload.data(), text.c_str(), text.size() + 1);
    send_pkt(c, msg_id, payload.data(), (int)payload.size());
}

// 读一个 NUL 结尾的字符串（带边界保护；坏包返回空串且不移动 off）
std::string read_str(const uint8_t* data, int& off, size_t len) {
    if ((size_t)off >= len) return "";
    const char* start = (const char*)data + off;
    const char* end   = (const char*)memchr(start, 0, len - off);
    if (!end) return "";
    off += (int)(end - start) + 1;
    return std::string(start, end - start);
}

// 构建 NUL 结尾字符串
std::vector<uint8_t> build_str(const std::string& s) {
    std::vector<uint8_t> out(s.size() + 1);
    memcpy(out.data(), s.c_str(), s.size() + 1);
    return out;
}

std::vector<uint8_t> build_str(const std::string& a, const std::string& b) {
    std::vector<uint8_t> out(a.size() + 1 + b.size() + 1);
    memcpy(out.data(), a.c_str(), a.size() + 1);
    memcpy(out.data() + a.size() + 1, b.c_str(), b.size() + 1);
    return out;
}

// ================================================================
//  关闭队列（统一在事件循环开头处理，避免重入）
// ================================================================
void queue_hard_close(SOCKET s) {
    auto it = conns.find(s);
    if (it == conns.end() || it->second.closing) return;
    it->second.closing = true;
    g_to_close.push_back(s);
}

void queue_graceful_close(SOCKET s) {
    auto it = conns.find(s);
    if (it == conns.end() || it->second.closing) return;
    it->second.close_after_flush = true;
    it->second.flush_deadline = now_sec() + FLUSH_GRACE;
    if (it->second.out.empty())
        queue_hard_close(s);
}

// 断开时的房间清理（房主离开 → 关房；客机离开 → 通知房主）
void detach_from_room(Conn& c) {
    if (!c.room) return;
    Room& r = *c.room;
    if (c.role == 0) {
        printf("[%s] 房主(%s) 离开，房间关闭\n", r.id.c_str(), c.name.c_str());
        std::vector<SOCKET> socks;
        for (auto& kv : r.clients) socks.push_back(kv.second);
        for (SOCKET s : socks) {
            auto cit = conns.find(s);
            if (cit == conns.end()) continue;
            cit->second.room = nullptr;   // 先解除关联，防止重复清理
            send_str(cit->second, MSG_PUB_INFO, "\\host_left");
            queue_graceful_close(s);
        }
        r.clients.clear();
        r.nicks.clear();
        r.host = INVALID_SOCKET;
        rooms.erase(r.id);
    } else {
        r.clients.erase(c.cid);
        r.nicks.erase(c.sock);
        if (r.host != INVALID_SOCKET) {
            auto hit = conns.find(r.host);
            if (hit != conns.end())
                send_str(hit->second, MSG_CHAT, "[系统] " + c.name + " 离开");
        }
        printf("[%s] %s 离开 (剩余 %d 人)\n", r.id.c_str(), c.name.c_str(), r.member_count());
        broadcast_pub(r, "\\left " + c.name);   // 人数变化 → 广播提示（客户端显示"XX 离开了房间"）
        sync_player_ids(r);                     // 人数变化 → 剩下的人各自再收一遍自己的 id
        sync_room_info(r);
    }
    c.room = nullptr;
}

void process_close(SOCKET s) {
    auto it = conns.find(s);
    if (it == conns.end()) return;
    Conn c = std::move(it->second);
    conns.erase(it);
    detach_from_room(c);
    close_sock(c.sock);
}

// ================================================================
//  名字
// ================================================================
std::string pick_name(Room& room, const std::string& preferred = "") {
    auto used = room.used_names();
    if (preferred.empty()) {
        std::vector<std::string> pool;
        for (auto& n : NAMES) if (!used.count(n)) pool.push_back(n);
        if (pool.empty()) {
            std::uniform_int_distribution<int> d(0, (int)NAMES.size()-1);
            for (int i = 0; i < 5; i++)
                pool.push_back(NAMES[d(rng)] + "_" + std::to_string(used.size()));
        }
        std::uniform_int_distribution<int> d(0, (int)pool.size()-1);
        std::string name = pool[d(rng)];
        int n = 2;
        while (used.count(name)) { name = pool[0] + std::to_string(n); n++; }
        return name;
    }
    std::string name = preferred;
    int n = 2;
    while (used.count(name)) { name = preferred + std::to_string(n); n++; }
    return name;
}

// ================================================================
//  room info
// ================================================================
std::string build_room_info(Room& room) {
    std::string members = "{";
    std::string ids = "{";
    if (room.host != INVALID_SOCKET) {
        auto it = room.nicks.find(room.host);
        std::string name = (it != room.nicks.end()) ? it->second : "???";
        name = name.substr(0, 20);
        auto mit = room.msgs.find(name);
        std::string msg = (mit != room.msgs.end()) ? mit->second : "";
        members += "\"" + name + "\":\"" + msg + "\"";
        ids += "\"" + name + "\":0";                      // 房主的房间内 id = 0
    }
    for (auto& kv : room.clients) {
        auto it = room.nicks.find(kv.second);
        std::string name = (it != room.nicks.end()) ? it->second : "???";
        name = name.substr(0, 20);
        members += (members.size() > 1 ? "," : "") + std::string("\"") + name + "\":\"";
        auto mit = room.msgs.find(name);
        if (mit != room.msgs.end()) members += mit->second;
        members += "\"";
        ids += (ids.size() > 1 ? "," : "") + std::string("\"") + name + "\":" + std::to_string(kv.first);
    }
    members += "}";
    ids += "}";
    // ids：名字 → 房间内 id（房主 0，客机从 1 起）；客户端只认 room/members，多出来的字段会被忽略
    std::string json = "{\"room\":\"" + room.id + "\",\"members\":" + members + ",\"ids\":" + ids + "}";
    return "\\roominfo " + json;
}

// 房间公告：给房主 + 所有客户端发一条 MSG_PUB_INFO 文本（\join / \left / \setglobal ...）
void broadcast_pub(Room& room, const std::string& text) {
    std::vector<SOCKET> socks;
    if (room.host != INVALID_SOCKET) socks.push_back(room.host);
    for (auto& kv : room.clients) socks.push_back(kv.second);
    for (SOCKET s : socks) {
        auto it = conns.find(s);
        if (it != conns.end()) send_str(it->second, MSG_PUB_INFO, text);
    }
}

// 给房间里**每个成员**各发一遍"你在房间里的 id" + 当前房间人数（人数变化时调用）
//   → 各端更新 global.mod_net_player_id / global.net_player_number
void sync_player_ids(Room& room) {
    const int cnt = room.member_count();
    if (room.host != INVALID_SOCKET) {
        auto it = conns.find(room.host);
        if (it != conns.end())
            send_str(it->second, MSG_PUB_INFO,
                     "\\setglobal {\"mod_net_player_id\":0,\"net_player_number\":" + std::to_string(cnt) + "}");
    }
    for (auto& kv : room.clients) {
        auto it = conns.find(kv.second);
        if (it != conns.end())
            send_str(it->second, MSG_PUB_INFO,
                     "\\setglobal {\"mod_net_player_id\":" + std::to_string(kv.first) +
                     ",\"net_player_number\":" + std::to_string(cnt) + "}");
    }
}

// 把最新成员表（含 ids）发给房主 + 所有客户端
void sync_room_info(Room& room) {
    std::string info = build_room_info(room);
    broadcast_pub(room, info);
}

// /listroom 的返回文本（首包查询与房内命令共用，保证一致）
std::string build_listroom() {
    if (rooms.empty()) return "[系统] 当前没有房间";
    std::string out = "=== 房间列表 ===";
    for (auto& kv : rooms)
        out += "\n  " + kv.first + " - " + std::to_string(kv.second.member_count()) + " 人";
    return out;
}

// ================================================================
//  文件缓存
// ================================================================
void handle_request_file(Room& room, SOCKET sock, const std::vector<uint8_t>& payload) {
    int off = 0;
    std::string filename = read_str(payload.data(), off, payload.size());
    std::string purpose  = read_str(payload.data(), off, payload.size());
    auto it = room.nicks.find(sock);
    std::string name = (it != room.nicks.end()) ? it->second : "???";

    auto cit = room.file_cache.find(filename);
    if (cit != room.file_cache.end()) {
        auto& data = cit->second;
        printf("[文件] 缓存命中 → %s: [%s] purpose=[%s] (%zu bytes)\n", name.c_str(), filename.c_str(), purpose.c_str(), data.size());
        auto str = build_str(filename, purpose);
        std::vector<uint8_t> body(4 + str.size() + 4 + data.size());
        int32_t msg = MSG_TRANSFER_FILE;
        memcpy(body.data(), &msg, 4);
        memcpy(body.data() + 4, str.data(), str.size());
        int32_t sz = (int32_t)data.size();
        memcpy(body.data() + 4 + str.size(), &sz, 4);
        memcpy(body.data() + 4 + str.size() + 4, data.data(), data.size());
        auto s_it = conns.find(sock);
        if (s_it != conns.end())
            send_pkt(s_it->second, MSG_TRANSFER_FILE, body.data() + 4, (int)body.size() - 4);
        return;
    }

    auto pit = room.file_pending.find(filename);
    if (pit != room.file_pending.end()) {
        printf("[文件] 排队等待 → %s: [%s] (已有 %zu 人在等)\n", name.c_str(), filename.c_str(), pit->second.size());
        pit->second.push_back({sock, purpose});
        return;
    }
    printf("[文件] 向房主请求 → %s: [%s]\n", name.c_str(), filename.c_str());
    room.file_pending[filename] = {{sock, purpose}};
    if (room.host != INVALID_SOCKET) {
        auto h_it = conns.find(room.host);
        if (h_it != conns.end())
            send_pkt(h_it->second, MSG_REQUEST_FILE, payload.data(), (int)payload.size());
    }
}

void handle_transfer_file(Room& room, const std::vector<uint8_t>& payload) {
    int off = 0;
    std::string filename = read_str(payload.data(), off, payload.size());
    std::string purpose  = read_str(payload.data(), off, payload.size());
    if (payload.size() - off < 4) { printf("[文件] 房主包不完整，忽略\n"); return; }
    int32_t size = 0;
    memcpy(&size, payload.data() + off, 4);
    off += 4;
    if (size < 0 || (size_t)size > payload.size() - off) { printf("[文件] 房主包大小越界，忽略\n"); return; }
    std::vector<uint8_t> data(payload.begin() + off, payload.begin() + off + size);
    room.file_cache[filename] = data;

    auto waiters = std::move(room.file_pending[filename]);
    room.file_pending.erase(filename);
    printf("[文件] 房主返回 → [%s] (%d bytes) → 发给 %zu 人\n", filename.c_str(), size, waiters.size());

    for (auto& w : waiters) {
        SOCKET ws = w.first;
        auto str = build_str(filename, w.second);
        std::vector<uint8_t> body(4 + str.size() + 4 + data.size());
        int32_t msg = MSG_TRANSFER_FILE;
        memcpy(body.data(), &msg, 4);
        memcpy(body.data() + 4, str.data(), str.size());
        int32_t sz = (int32_t)data.size();
        memcpy(body.data() + 4 + str.size(), &sz, 4);
        memcpy(body.data() + 4 + str.size() + 4, data.data(), data.size());
        auto w_it = conns.find(ws);
        if (w_it != conns.end())
            send_pkt(w_it->second, MSG_TRANSFER_FILE, body.data() + 4, (int)body.size() - 4);
    }
}

// ================================================================
//  CHAT 命令处理
// ================================================================
bool handle_cmd(Conn& c, Room& room, const std::string& text) {
    if (text.empty() || text[0] != '/') return false;

    auto pos = text.find(' ');
    std::string cmd = text.substr(0, pos);
    std::string rest = (pos != std::string::npos) ? text.substr(pos + 1) : "";

    if (cmd == "/connectroom") return false;

    // /syncroom
    if (cmd == "/syncroom" && c.role == 0) {
        room.data = rest;
        room.state = "lobby";
        return true;
    }
    // /list
    if (cmd == "/list") {
        std::string out = "=== 房间 " + room.id + " ===\n";
        auto hit = room.nicks.find(room.host);
        out += "  [房主] " + ((hit != room.nicks.end()) ? hit->second : "(无)") + "\n";
        for (auto& kv : room.clients) {
            auto cit = room.nicks.find(kv.second);
            out += "  " + ((cit != room.nicks.end()) ? cit->second : "???") + "\n";
        }
        send_str(c, MSG_CHAT, out);
        return true;
    }
    // /who
    if (cmd == "/who") {
        std::string rn = (c.role == 0) ? "房主" : "客户端";
        send_str(c, MSG_CHAT, "[系统] 你是 " + c.name + " (" + rn + ")，房间 " + room.id);
        return true;
    }
    // /kick
    if (cmd == "/kick") {
        if (c.role != 0) { send_str(c, MSG_CHAT, "[系统] 只有房主可以踢人"); return true; }
        if (rest.empty()) { send_str(c, MSG_CHAT, "[系统] 用法: /kick <玩家名>"); return true; }
        SOCKET target = INVALID_SOCKET;
        int target_cid = -1;
        for (auto& kv : room.clients) {
            auto nit = room.nicks.find(kv.second);
            if (nit != room.nicks.end() && nit->second == rest) {
                target = kv.second; target_cid = kv.first; break;
            }
        }
        if (target == INVALID_SOCKET) {
            send_str(c, MSG_CHAT, "[系统] 没有叫 " + rest + " 的玩家");
            return true;
        }
        room.clients.erase(target_cid);
        room.nicks.erase(target);
        auto tit = conns.find(target);
        if (tit != conns.end()) {
            tit->second.room = nullptr;
            send_str(tit->second, MSG_PUB_INFO, "\\kicked");
            queue_graceful_close(target);
        }
        send_str(c, MSG_CHAT, "[系统] 你踢出了 " + rest);
        std::string notice = "[系统] " + rest + " 被房主踢出";
        for (auto& kv : room.clients) {
            auto cit = conns.find(kv.second);
            if (cit != conns.end()) send_str(cit->second, MSG_CHAT, notice);
        }
        sync_room_info(room);
        return true;
    }
    // /listroom
    if (cmd == "/listroom") {
        send_str(c, MSG_CHAT, build_listroom());
        return true;
    }
    // /listcommand
    if (cmd == "/listcommand") {
        send_str(c, MSG_CHAT,
            "=== 可用命令 ===\n"
            "  /list         - 列出房间成员\n"
            "  /who          - 显示自己是谁\n"
            "  /kick <名>    - 房主踢人\n"
            "  /listroom     - 列出所有房间\n"
            "  /listcommand  - 列出所有命令");
        return true;
    }

    send_str(c, MSG_CHAT, "[系统] 未知命令: " + cmd);
    return true;
}

// ================================================================
//  广播/转发
// ================================================================
void broadcast(Room& room, const std::vector<uint8_t>& body) {
    std::vector<SOCKET> socks;
    for (auto& kv : room.clients) socks.push_back(kv.second);
    for (SOCKET s : socks) {
        auto it = conns.find(s);
        if (it != conns.end())
            send_pkt(it->second, *(int32_t*)body.data(), body.data() + 4, (int)body.size() - 4);
    }
}

void to_host(Room& room, const std::vector<uint8_t>& body) {
    if (room.host == INVALID_SOCKET) return;
    auto it = conns.find(room.host);
    if (it != conns.end())
        send_pkt(it->second, *(int32_t*)body.data(), body.data() + 4, (int)body.size() - 4);
}

// ================================================================
//  握手与消息分发
// ================================================================
bool handle_handshake(Conn& c, const std::vector<uint8_t>& body) {
    int32_t msg_id;
    memcpy(&msg_id, body.data(), 4);
    if (msg_id != MSG_CHAT) {
        printf("  首包不是 MSG_CHAT\n");
        c.handshake_failed = true;
        queue_hard_close(c.sock);
        return false;
    }

    std::string text((char*)body.data() + 4, body.size() - 4);
    if (!text.empty() && text.back() == 0) text.pop_back();

    // 首包 /listroom：返回房间列表后断开（查询房间用，不入房）
    auto sp = text.find(' ');
    if (text.substr(0, sp) == "/listroom") {
        send_str(c, MSG_CHAT, build_listroom());
        printf("  首包 /listroom 查询，返回后断开\n");
        c.handshake_failed = true;
        queue_graceful_close(c.sock);
        return false;
    }

    if (text.find("/connectroom ") != 0) {
        printf("  首包不是 /connectroom: %s\n", text.c_str());
        c.handshake_failed = true;
        queue_hard_close(c.sock);
        return false;
    }

    // 解析 /connectroom [version] <room_id> [name]
    // 兼容旧格式 /connectroom <房间ID>
    // 新格式 /connectroom <版本> <房间ID> [名字]
    std::string raw = text.substr(13);
    while (!raw.empty() && raw[0] == ' ') raw.erase(0, 1);
    std::vector<std::string> parts;
    std::istringstream iss(raw);
    std::string tok;
    while (iss >> tok) parts.push_back(tok);
    // 名字可能有空格，取剩余部分
    std::string client_version, room_id, preferred_name;
    if (parts.size() >= 2 && parts[0].find('.') != std::string::npos) {
        client_version = parts[0];
        room_id        = parts[1];
        if (parts.size() > 2) {
            auto pos = raw.find(parts[0]);
            pos = raw.find(parts[1], pos + parts[0].size());
            preferred_name = raw.substr(pos + parts[1].size());
            while (!preferred_name.empty() && preferred_name[0] == ' ') preferred_name.erase(0, 1);
        }
    } else {
        room_id = parts.size() > 0 ? parts[0] : "";
    }

    if (room_id.empty()) {
        printf("  缺少房间ID\n");
        c.handshake_failed = true;
        queue_hard_close(c.sock);
        return false;
    }

    auto rit = rooms.find(room_id);
    if (rit == rooms.end()) {
        rooms[room_id] = Room{};
        rit = rooms.find(room_id);
        rit->second.id = room_id;
        rit->second.version = client_version;
        rit->second.created_at = now_sec();
        rit->second.last_active = now_sec();
    }
    Room& room = rit->second;

    // 版本校验
    if (room.host != INVALID_SOCKET) {
        if (!room.version.empty() && !client_version.empty() && client_version != room.version) {
            printf("  [%s] 版本不匹配: 房主=%s 客机=%s\n", room.id.c_str(), room.version.c_str(), client_version.c_str());
            send_str(c, MSG_CHAT,
                "[系统] 版本不匹配！房主版本: " + room.version + "，你的版本: " + client_version);
            send_str(c, MSG_PUB_INFO, "\\kicked");
            c.handshake_failed = true;
            queue_graceful_close(c.sock);
            return false;
        }
    }

    // 战斗中不能加入
    if (room.host != INVALID_SOCKET && room.state == "battle") {
        send_str(c, MSG_CHAT, "[系统] 房间战斗中，无法加入");
        send_str(c, MSG_PUB_INFO, "\\kicked");
        c.handshake_failed = true;
        queue_graceful_close(c.sock);
        return false;
    }

    // 房间已满
    if (room.host != INVALID_SOCKET && room.member_count() >= g_max_members) {
        printf("  [%s] 房间已满 (%d/%d)，拒绝加入\n", room.id.c_str(), room.member_count(), g_max_members);
        send_str(c, MSG_CHAT, "[系统] 房间已满 (" + std::to_string(g_max_members) + "人)，无法加入");
        send_str(c, MSG_PUB_INFO, "\\kicked");
        c.handshake_failed = true;
        queue_graceful_close(c.sock);
        return false;
    }

    // 入房
    if (room.host == INVALID_SOCKET) {
        room.host = c.sock;
        c.role = 0;
        c.cid  = 0;
    } else {
        c.cid  = room.next_cid++;
        c.role = 1;
        room.clients[c.cid] = c.sock;
    }
    c.name = pick_name(room, preferred_name);
    room.nicks[c.sock] = c.name;
    c.room = &room;
    c.joined = true;
    room.last_active = now_sec();

    // 告诉客户端身份
    send_str(c, MSG_PUB_INFO, (c.role == 0) ? "\\modserver" : "\\modclient");
    // 人数变化 → 给房间里每个人都发一遍自己的房间内 id（本人也在这轮里拿到）
    sync_player_ids(room);
    // 人数变化 → 广播一条提示（客户端显示"XX 加入了房间"）
    broadcast_pub(room, "\\join " + c.name);

    if (c.role == 0) {
        send_str(c, MSG_CHAT,
            "[系统] 你已创建房间 " + room.id + "\n"
            "[系统] 你的名字是 " + c.name + "，可使用 /listcommand 查看命令");
    } else {
        send_str(c, MSG_CHAT,
            "[系统] 你已加入房间 " + room.id + "\n"
            "[系统] 你的名字是 " + c.name + "，可使用 /listcommand 查看命令，或等待房主操作");
        if (!room.raw_payload.empty())
            send_pkt(c, MSG_ENTER_ROOM_READY, room.raw_payload.data(), (int)room.raw_payload.size());
        else if (!room.data.empty())
            send_pkt(c, MSG_ENTER_ROOM_READY, room.data.c_str(), (int)room.data.size() + 1);
    }
    send_str(c, MSG_CHAT, "[系统] 输入文字即可聊天");

    printf("  [%s] %s 加入 (%d 人)\n", room.id.c_str(), c.name.c_str(), room.member_count());

    if (c.role == 1 && room.host != INVALID_SOCKET) {
        auto hit = conns.find(room.host);
        if (hit != conns.end())
            send_str(hit->second, MSG_CHAT, "[系统] " + c.name + " 加入");
    }

    sync_room_info(room);
    return true;
}

void dispatch(Conn& c, std::vector<uint8_t> pkt_body, uint32_t pkt_len) {
    Room& room = *c.room;
    room.last_active = now_sec();

    int32_t mid;
    memcpy(&mid, pkt_body.data(), 4);

    if (mid == MSG_CHAT) {
        std::string txt((char*)pkt_body.data() + 4, pkt_len - 4);
        if (!txt.empty() && txt.back() == 0) txt.pop_back();
        // 去掉 "say " 前缀
        if (txt.find("say ") == 0) txt = txt.substr(4);

        if (handle_cmd(c, room, txt)) return;

        if (txt.find("[系统]") != 0) {
            bool already = false;
            auto names = room.used_names();
            for (auto& n : names) {
                if (txt.find(n + ": ") == 0) { already = true; break; }
            }
            if (!already) {
                room.msgs[c.name] = txt.substr(0, 20);
                sync_room_info(room);
                txt = c.name + ": " + txt;
            }
        }
        // 重新打包
        pkt_body.clear();
        pkt_body.resize(4 + txt.size() + 1);
        memcpy(pkt_body.data(), &mid, 4);
        memcpy(pkt_body.data() + 4, txt.c_str(), txt.size() + 1);
    }

    // 监控房间状态
    if (mid == MSG_ENTER_ROOM_READY && c.role == 0) {
        room.raw_payload.assign(pkt_body.begin() + 4, pkt_body.begin() + pkt_len);
        room.state = "lobby";
        room.file_cache.clear();
        room.file_pending.clear();
    } else if (mid == MSG_START_BATTLE) {
        room.state = "battle";
        room.battle_started_at = now_sec();
    } else if (mid == MSG_SERVER_ACTION) {
        if (pkt_len > 4 && pkt_body[4] == 4) room.state = "lobby";
    }

    // 文件请求/传输
    if (mid == MSG_REQUEST_FILE && c.role == 1) {
        std::vector<uint8_t> pl(pkt_body.begin() + 4, pkt_body.end());
        handle_request_file(room, c.sock, pl);
        return;
    }
    if (mid == MSG_TRANSFER_FILE && c.role == 0) {
        std::vector<uint8_t> pl(pkt_body.begin() + 4, pkt_body.end());
        handle_transfer_file(room, pl);
        return;
    }

    if (c.role == 0)
        broadcast(room, pkt_body);
    else
        to_host(room, pkt_body);
}

// ================================================================
//  事件处理
// ================================================================
void read_available(Conn& c) {
    char buf[65536];
    size_t budget = 4 * 1024 * 1024;   // 单轮最多读 4MB，防止一个连接饿死其他连接
    while (budget > 0) {
        int n = recv(c.sock, buf, sizeof(buf), 0);
        if (n > 0) {
            budget -= (size_t)n;
            c.in.insert(c.in.end(), buf, buf + n);
            if (c.in.size() - c.in_pos > MAX_PACKET + 4) {
                queue_hard_close(c.sock);
                return;
            }
            continue;
        }
        if (n == 0) { queue_hard_close(c.sock); return; }
        if (would_block()) return;
        queue_hard_close(c.sock);
        return;
    }
}

void flush_out(Conn& c) {
    if (c.out.empty()) {
        if (c.close_after_flush) queue_hard_close(c.sock);
        return;
    }
    size_t chunk = std::min<size_t>(c.out.size(), 262144);
    int n = send(c.sock, (const char*)c.out.data(), (int)chunk, MSG_NOSIGNAL);
    if (n > 0) {
        c.out.erase(c.out.begin(), c.out.begin() + n);
        if (c.out.empty() && c.close_after_flush)
            queue_hard_close(c.sock);
    } else if (!would_block()) {
        queue_hard_close(c.sock);
    }
}

void handle_data(Conn& c) {
    while (true) {
        if (c.closing) return;
        size_t avail = c.in.size() - c.in_pos;

        if (!c.joined) {
            // 握手：首包 [u32 len][i32 MSG_CHAT][payload]
            if (c.handshake_failed) { c.in_pos = c.in.size(); return; }   // 已被拒，丢弃输入
            if (avail < 4) return;
            uint32_t len;
            memcpy(&len, c.in.data() + c.in_pos, 4);
            if (len < 4 || len > MAX_PACKET) { queue_hard_close(c.sock); return; }
            if (avail < (size_t)4 + len) return;
            std::vector<uint8_t> body(c.in.begin() + c.in_pos + 4, c.in.begin() + c.in_pos + 4 + len);
            c.in_pos += (size_t)4 + len;
            if (!handle_handshake(c, body)) return;
        } else {
            if (c.room == nullptr) {
                // 房间已关闭（正在优雅断开），丢弃剩余输入
                c.in_pos = c.in.size();
                return;
            }
            if (avail < 4) return;
            uint32_t len;
            memcpy(&len, c.in.data() + c.in_pos, 4);
            if (len < 4 || len > MAX_PACKET) { queue_hard_close(c.sock); return; }
            if (avail < (size_t)4 + len) return;
            std::vector<uint8_t> body(c.in.begin() + c.in_pos + 4, c.in.begin() + c.in_pos + 4 + len);
            c.in_pos += (size_t)4 + len;
            dispatch(c, std::move(body), len);
        }

        if (c.in_pos == c.in.size()) { c.in.clear(); c.in_pos = 0; }
    }
}

// ================================================================
//  定时清理
// ================================================================
void sweep_rooms() {
    double now = now_sec();
    std::vector<std::string> to_remove;
    for (auto& kv : rooms) {
        auto& room = kv.second;
        double idle_hours = (now - room.last_active) / 3600.0;
        if (room.state == "battle") {
            if (room.battle_started_at > 0) {
                double battle_hours = (now - room.battle_started_at) / 3600.0;
                if (battle_hours > BATTLE_MAX_HOURS) {
                    printf("[清理] 房间 %s 战斗持续 %.1fh，强制关闭\n", kv.first.c_str(), battle_hours);
                    to_remove.push_back(kv.first);
                }
            } else if (idle_hours > IDLE_MAX_HOURS) {
                // 理论上不可达（state=battle 时 battle_started_at 必已设置），兜底按闲置关闭
                printf("[清理] 房间 %s 战斗状态异常，按闲置 %.1fh 强制关闭\n", kv.first.c_str(), idle_hours);
                to_remove.push_back(kv.first);
            }
        } else {
            if (idle_hours > IDLE_MAX_HOURS) {
                printf("[清理] 房间 %s 闲置 %.1fh 未开战，强制关闭\n", kv.first.c_str(), idle_hours);
                to_remove.push_back(kv.first);
            }
        }
    }
    for (auto& rid : to_remove) {
        auto it = rooms.find(rid);
        if (it == rooms.end()) continue;
        Room& room = it->second;
        std::vector<SOCKET> socks;
        if (room.host != INVALID_SOCKET) socks.push_back(room.host);
        for (auto& kv : room.clients) socks.push_back(kv.second);
        for (SOCKET s : socks) {
            auto cit = conns.find(s);
            if (cit == conns.end()) continue;
            cit->second.room = nullptr;
            send_str(cit->second, MSG_PUB_INFO, "\\kicked");
            queue_graceful_close(s);
        }
        room.clients.clear();
        room.nicks.clear();
        room.host = INVALID_SOCKET;
        rooms.erase(rid);
    }
    if (!to_remove.empty())
        printf("[清理] 本轮清理了 %zu 个房间\n", to_remove.size());
}

void sweep_conns() {
    double now = now_sec();
    for (auto& kv : conns) {
        Conn& c = kv.second;
        if (c.closing) continue;
        if (!c.joined && now - c.connected_at > HANDSHAKE_TIMEOUT) {
            printf("  握手超时，断开\n");
            queue_hard_close(c.sock);
        } else if (c.close_after_flush && now > c.flush_deadline) {
            printf("  优雅关闭超时，强制断开\n");
            queue_hard_close(c.sock);
        }
    }
}

// ================================================================
//  main
// ================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // UTF-8 控制台输出
    SetConsoleCP(65001);        // UTF-8 控制台输入
#endif
    init_network();

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("FVM 中继服务器\n");
        printf("用法: relay_server.exe [--port <端口>] [--max-members <人数>]\n");
        printf("  --port, -p       监听端口，默认 27085，被占用则递增尝试\n");
        printf("  --max-members, -m 房间最大人数，默认 8，最少 2\n");
        printf("  --help, -h       显示帮助\n");
        printf("示例: relay_server.exe --port 27085 --max-members 4\n");
        return 0;
    }

    int start_port = 27085;
    int max_members = 8;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            start_port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--max-members") == 0 || strcmp(argv[i], "-m") == 0) && i + 1 < argc) {
            max_members = atoi(argv[++i]);
        }
    }
    if (start_port <= 0 || start_port > 65535) { printf("端口无效\n"); return 1; }
    if (max_members < 2) { printf("人数上限至少为2\n"); return 1; }
    g_max_members = max_members;

    SOCKET srv = INVALID_SOCKET;
    int port = start_port;
    for (int tries = 0; tries < 100; tries++, port++) {
        if (port > 65535) port = 1024;
        srv = socket(AF_INET, SOCK_STREAM, 0);
        if (srv == INVALID_SOCKET) { printf("socket 失败\n"); return 1; }

        int reuse = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((uint16_t)port);

        if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == 0) {
            if (listen(srv, 32) == 0) break;  // 成功
        }
        printf("[%d] bind/listen 失败，尝试下一个...\n", port);
        closesocket(srv);
        srv = INVALID_SOCKET;
    }

    if (srv == INVALID_SOCKET) {
        printf("尝试了 100 个端口（%d ~ %d），全部失败，退出\n", start_port, port - 1);
        return 1;
    }
    set_nonblock(srv);

    printf("中继启动: 0.0.0.0:%d  最大人数: %d\n", port, max_members);

    while (true) {
        // ---- 处理待关闭连接（统一在此收尾，避免函数重入）----
        while (!g_to_close.empty()) {
            SOCKET s = g_to_close.back();
            g_to_close.pop_back();
            process_close(s);
        }

        // ---- 定时清理 ----
        sweep_rooms();
        sweep_conns();

#ifdef _WIN32
        // ---- 等待事件 (select；Windows 忽略第一个参数) ----
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(srv, &readfds);
        for (auto& kv : conns) {
            FD_SET(kv.first, &readfds);
            if (!kv.second.out.empty() || kv.second.close_after_flush)
                FD_SET(kv.first, &writefds);
        }
        timeval tv{ 1, 0 };
        int ret = select(0, &readfds, &writefds, nullptr, &tv);
        if (ret < 0) {
            if (would_block()) continue;   // EINTR，重试
            printf("select 失败\n");
            continue;
        }

        // ---- 处理事件 ----
        if (FD_ISSET(srv, &readfds)) {
            for (;;) {
                sockaddr_in cli_addr{};
                socklen_t cli_len = sizeof(cli_addr);
                SOCKET cli = accept(srv, (sockaddr*)&cli_addr, &cli_len);
                if (cli == INVALID_SOCKET) break;
                set_nonblock(cli);
                Conn nc;
                nc.sock = cli;
                nc.connected_at = now_sec();
                conns[cli] = std::move(nc);
                printf("[连接] %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            }
        }
        for (auto& kv : conns) {
            Conn& c = kv.second;
            if (c.closing) continue;
            bool rd = FD_ISSET(c.sock, &readfds) != 0;
            bool wr = FD_ISSET(c.sock, &writefds) != 0;
            if (rd) read_available(c);
            if (c.closing) continue;
            if (rd) handle_data(c);
            if (c.closing) continue;
            if (wr) flush_out(c);
        }
#else
        // ---- 等待事件 (poll) ----
        std::vector<pollfd_t> fds;
        fds.reserve(1 + conns.size());
        fds.push_back(pollfd_t{ srv, POLLIN, 0 });
        for (auto& kv : conns) {
            short ev = POLLIN;
            if (!kv.second.out.empty() || kv.second.close_after_flush) ev |= POLLOUT;
            fds.push_back(pollfd_t{ kv.second.sock, ev, 0 });
        }
        int ret = poll(fds.data(), (nfds_t)fds.size(), 1000);
        if (ret < 0) {
            if (would_block()) continue;   // EINTR，重试
            printf("poll 失败\n");
            continue;
        }

        // ---- 处理事件 ----
        for (auto& p : fds) {
            if (p.revents == 0) continue;
            if (p.fd == srv) {
                if (p.revents & POLLIN) {
                    for (;;) {
                        sockaddr_in cli_addr{};
                        socklen_t cli_len = sizeof(cli_addr);
                        SOCKET cli = accept(srv, (sockaddr*)&cli_addr, &cli_len);
                        if (cli == INVALID_SOCKET) break;
                        set_nonblock(cli);
                        Conn nc;
                        nc.sock = cli;
                        nc.connected_at = now_sec();
                        conns[cli] = std::move(nc);
                        printf("[连接] %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    }
                }
                continue;
            }
            auto it = conns.find(p.fd);
            if (it == conns.end()) continue;   // 本轮已关闭
            Conn& c = it->second;
            if (c.closing) continue;
            if (p.revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
                read_available(c);
            if (c.closing) continue;
            handle_data(c);
            if (c.closing) continue;
            if (p.revents & POLLOUT)
                flush_out(c);
        }
#endif
    }
}
