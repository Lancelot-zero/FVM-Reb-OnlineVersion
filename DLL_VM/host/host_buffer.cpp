// ============================================================
// host_buffer.cpp — 缓冲区（默认实现）
//
//   外层 buffer_* 落到这儿。id 是池下标（0 起、可复用），每个 buf 一个游标。
//   这一层**只有字节**：buffer_u8 / buffer_s32 / buffer_f32 / buffer_string 那套
//   类型编解码在外层（它知道 Value 的类型，也该由它决定字节序）。
//
//   写越界 = 自动变长（GML 的 buffer 就是这个脾气）；读越界 = 读到多少算多少。
// ============================================================
#include "host.h"
#include <cstdio>
#include <filesystem>
#include <vector>

struct HostBuf {
    std::vector<uint8_t> data;
    size_t               pos;
    int                  type;      // GML 的 buffer 类型号（外层给的，我们只存着）
};

static std::vector<HostBuf> g_buf_pool;
static std::vector<int>     g_buf_free;      // 回收的 id，等着复用

static HostBuf* buf_of(int id) {
    if (id < 0 || id >= (int)g_buf_pool.size()) return NULL;
    if (g_buf_pool[(size_t)id].type < 0) return NULL;      // type < 0 = 这个槽是死的
    return &g_buf_pool[(size_t)id];
}

int host_buf_create(int size, int type) {
    if (size < 0) size = 0;
    int id;
    if (!g_buf_free.empty()) { id = g_buf_free.back(); g_buf_free.pop_back(); }
    else { g_buf_pool.push_back(HostBuf()); id = (int)g_buf_pool.size() - 1; }
    HostBuf& b = g_buf_pool[(size_t)id];
    b.data.assign((size_t)size, 0);            // GML：新建的 buf 是 0 填充
    b.pos  = 0;
    b.type = type;
    return id;
}

void host_buf_delete(int id) {
    HostBuf* b = buf_of(id);
    if (!b) return;
    b->data.clear();
    b->data.shrink_to_fit();
    b->pos  = 0;
    b->type = -1;                              // 标记成死的
    g_buf_free.push_back(id);
}

bool host_buf_alive(int id) { return buf_of(id) != NULL; }

int host_buf_size(int id) {
    HostBuf* b = buf_of(id);
    return b ? (int)b->data.size() : 0;
}

int host_buf_tell(int id) {
    HostBuf* b = buf_of(id);
    return b ? (int)b->pos : 0;
}

void host_buf_seek(int id, int pos) {
    HostBuf* b = buf_of(id);
    if (!b || pos < 0) return;
    if ((size_t)pos > b->data.size()) b->data.resize((size_t)pos, 0);   // 越界就补 0
    b->pos = (size_t)pos;
}

bool host_buf_write(int id, const void* p, int n) {
    HostBuf* b = buf_of(id);
    if (!b || n < 0 || (!p && n > 0)) return false;
    if (b->pos + (size_t)n > b->data.size()) b->data.resize(b->pos + (size_t)n, 0);
    if (n > 0) memcpy(&b->data[b->pos], p, (size_t)n);
    b->pos += (size_t)n;
    return true;
}

int host_buf_read(int id, void* out, int n) {
    HostBuf* b = buf_of(id);
    if (!b || n <= 0 || !out) return 0;
    size_t avail = b->data.size() - b->pos;
    size_t take  = ((size_t)n < avail) ? (size_t)n : avail;
    if (take > 0) memcpy(out, &b->data[b->pos], take);
    b->pos += take;
    return (int)take;
}

bool host_buf_poke(int id, int off, const void* p, int n) {
    HostBuf* b = buf_of(id);
    if (!b || off < 0 || n < 0) return false;
    if ((size_t)off + (size_t)n > b->data.size()) b->data.resize((size_t)off + (size_t)n, 0);
    if (n > 0) memcpy(&b->data[(size_t)off], p, (size_t)n);
    return true;
}

bool host_buf_peek(int id, int off, void* out, int n) {
    HostBuf* b = buf_of(id);
    if (!b || off < 0 || n < 0 || !out) return false;
    if ((size_t)off + (size_t)n > b->data.size()) return false;      // 读越界：不给你半个
    if (n > 0) memcpy(out, &b->data[(size_t)off], (size_t)n);
    return true;
}

bool host_buf_copy(int dst, int src, int src_off, int dst_off, int n) {
    HostBuf* d = buf_of(dst);
    HostBuf* s = buf_of(src);
    if (!d || !s || src_off < 0 || dst_off < 0 || n < 0) return false;
    if ((size_t)src_off + (size_t)n > s->data.size()) return false;
    if ((size_t)dst_off + (size_t)n > d->data.size()) d->data.resize((size_t)dst_off + (size_t)n, 0);
    if (n > 0) memmove(&d->data[(size_t)dst_off], &s->data[(size_t)src_off], (size_t)n);
    return true;
}

// 把文件内容填进 buf（GML 的 buffer_load：从当前位置覆盖，不够就变长）
bool host_buf_load(int id, const std::string& path) {
    HostBuf* b = buf_of(id);
    if (!b) return false;
    FILE* fp = _wfopen(std::filesystem::u8path(path).c_str(), L"rb");   // 中文路径：同 host_file_open
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) { fclose(fp); return false; }
    if (b->pos + (size_t)sz > b->data.size()) b->data.resize(b->pos + (size_t)sz, 0);
    size_t got = (sz > 0) ? fread(&b->data[b->pos], 1, (size_t)sz, fp) : 0;
    b->pos += got;
    fclose(fp);
    return true;
}
