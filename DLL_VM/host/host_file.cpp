// ============================================================
// host_file.cpp — 本地文件读写（默认实现：标准库）
//
//   外层的 file_* / directory_* / date_* 落到这儿。
//   这一层不认 Value，只认"路径 → 东西"；GML 的返回值语义（0/1、空串还是 undefined）
//   由外层决定。
//
//   句柄就是 FILE*（外面当 void* 用）：读写位置、EOF 都归标准库管，
//   我们不自己维护第二套游标 —— 少一处能对不上的状态。
// ============================================================
#include "host.h"
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

bool host_file_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::u8path(path), ec);
}

bool host_dir_exists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(fs::u8path(path), ec);
}

bool host_dir_create(const std::string& path) {
    std::error_code ec;
    fs::create_directories(fs::u8path(path), ec);
    return !ec;
}

void* host_file_open(const std::string& path, int mode) {
    // 路径按 UTF-8 传进来（VM 里的字符串就是 UTF-8）；Windows 上 fopen 会按 ANSI 代码页解释，
    //   中文路径直接开不了（本仓库路径就带中文）→ 一律 _wfopen + fs::u8path
    const wchar_t* m = L"rb";
    if (mode == HOST_FILE_WRITE)     m = L"wb";
    if (mode == HOST_FILE_APPEND)    m = L"ab";
    if (mode == HOST_FILE_READ)      m = L"rb";
    if (mode == HOST_FILE_READ_TEXT) m = L"rt";
    FILE* fp = _wfopen(fs::u8path(path).c_str(), m);
    return (void*)fp;
}

bool host_file_write(void* h, const std::string& s) {
    if (!h) return false;
    return fwrite(s.data(), 1, s.size(), (FILE*)h) == s.size();
}

bool host_file_writeln(void* h, const std::string& s) {
    if (!h) return false;
    if (!host_file_write(h, s)) return false;
    return fputc('\n', (FILE*)h) != EOF;
}

// 读一个"非空白词"（GML 的 file_text_read_string 就是这个规矩）
std::string host_file_read_string(void* h) {
    if (!h) return std::string();
    FILE* fp = (FILE*)h;
    int c;
    std::string out;
    while ((c = fgetc(fp)) != EOF && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {}
    while (c != EOF && c != ' ' && c != '\t' && c != '\r' && c != '\n') {
        out.push_back((char)c);
        c = fgetc(fp);
    }
    if (c == '\r' || c == '\n') ungetc(c, fp);      // 行尾留给 readln 吃
    return out;
}

// 读一行（不含换行）
std::string host_file_readln(void* h) {
    if (!h) return std::string();
    FILE* fp = (FILE*)h;
    std::string out;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\r') { int n = fgetc(fp); if (n != '\n' && n != EOF) ungetc(n, fp); break; }
        if (c == '\n') break;
        out.push_back((char)c);
    }
    return out;
}

bool host_file_eof(void* h) {
    if (!h) return true;
    FILE* fp = (FILE*)h;
    int c = fgetc(fp);
    if (c == EOF) return true;
    ungetc(c, fp);
    return false;
}

void host_file_close(void* h) {
    if (h) fclose((FILE*)h);
}

// ── 时间 ──
static std::string fmt_time(const char* fmt) {
    std::time_t t = std::time(NULL);
    std::tm lt;
    localtime_s(&lt, &t);
    char buf[128];
    size_t n = strftime(buf, sizeof(buf), fmt, &lt);
    return std::string(buf, n);
}

std::string host_date_current() { return fmt_time("%Y-%m-%d %H:%M:%S"); }

std::string host_date_format(const std::string& fmt) {
    // strftime 认的格式直接透传（%Y %m %d %H %M %S …）
    return fmt_time(fmt.c_str());
}

// ── GML 的 date_* 用：datetime = 1899-12-30 起的天数（实数），本地时间 ──
//   GML 的 date_current_datetime() 是**本地挂钟**，而 <ctime> 的 time() 是 UTC 秒。
//   所以先把"本地 tm"用 _mkgmtime 当成 UTC 折回秒 —— 这串秒数就表示挂钟时间，
//   再 +25569 天换成 GML 纪元。host_date_to_string 反向走 gmtime，正好对上。
static const double GML_DATE_EPOCH_DAYS = 25569.0;      // 1899-12-30 → 1970-01-01

double host_date_now() {
    std::time_t t = std::time(NULL);
    std::tm lt;
    localtime_s(&lt, &t);
    std::time_t wall = _mkgmtime(&lt);                  // 挂钟时间当 UTC 看
    return GML_DATE_EPOCH_DAYS + (double)wall / 86400.0;
}

std::string host_date_to_string(double dt) {
    // 先换算成"挂钟秒"（可能为负 → 四舍五入也要往负方向靠，不然 -0.4 会进到前一天）
    double secs = (dt - GML_DATE_EPOCH_DAYS) * 86400.0;
    std::time_t t = (std::time_t)(secs < 0 ? secs - 0.5 : secs + 0.5);
    std::tm lt;
    gmtime_s(&lt, &t);                                  // 对上 host_date_now 的"假 UTC"
    char buf[64];
    size_t n = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    // ⚠ 简化：GML 的 date_datetime_string 是**系统区域格式**（乱），
    //    这里统一用项目自己的 "YYYY-MM-DD hh:mm:ss"（和 host_date_current 一致）。
    return std::string(buf, n);
}
