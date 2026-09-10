#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <vector>
#include <cwchar>
#include <cwctype>
#include <string.h>

#include "file_system.h"
#include "json.hpp"
#include "typedef.h"

#include <imm.h>
#include <commctrl.h>
#include <msctf.h>
#include <msctf.h>

#pragma comment(lib, "comctl32.lib")

// IME 屏蔽心跳：用窗口定时器自持 1 秒重压。
// v5 的心跳挂在 obj_game_init 上，而该对象只在 room_init 存在且非持久 → 进游戏后循环就死了
static const UINT_PTR kImeTimerId = 0x494D;  // 'MI'

static void ApplyImeBlock(HWND hwnd, bool from_timer);

// 按窗口子类化：拦截 IME 相关消息 + 处理 IME 屏蔽心跳
LRESULT CALLBACK ImeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                            UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  switch (msg) {
    case WM_IME_SETCONTEXT:
      // 文档化做法：清掉「显示合成窗/候选窗/引导线」标志，其余交给 DefWindowProc
      lParam &= ~static_cast<LPARAM>(ISC_SHOWUIALL);
      break;
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_CONTROL:
    case WM_IME_NOTIFY:
    case WM_IME_REQUEST:
    case WM_IME_SELECT:
      // 丢弃合成/候选消息；不碰 WM_IME_CHAR / WM_CHAR / WM_KEYDOWN，避免影响打字
      return 0;
    case WM_SETFOCUS:
    case WM_ACTIVATE:
      // 窗口重新获得焦点时系统可能把 IME 上下文挂回来 → 立刻再断开（只影响本窗口）
      if (msg != WM_ACTIVATE || wParam != WA_INACTIVE) {
        ImmAssociateContext(hwnd, nullptr);
      }
      break;
    case WM_TIMER:
      if (wParam == kImeTimerId) {
        ApplyImeBlock(hwnd, true);
        return 0;
      }
      break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}
#pragma comment(lib, "imm32.lib")

using json = nlohmann::json;

inline void LogNativeError(int error_code, const char* message) {
  const auto& path = NativeLogFilePath();
  if (path.empty()) {
    return;
  }

  const std::filesystem::path log_path(FileSystem::Utf8ToUtf16(path.c_str()));
  // 确保日志目录存在：否则 ofstream 打开失败会静默丢日志（v5 的潜在坑）
  std::error_code ec;
  std::filesystem::create_directories(log_path.parent_path(), ec);

  std::ofstream ofs(log_path, std::ios::app);
  if (!ofs.is_open()) {
    return;
  }

  ofs << "[error_code=" << error_code << "] "
      << (message ? message : "") << '\n';
}

inline auto FailWith(int error_code, const char* message) -> double {
  LogNativeError(error_code, message);
  return static_cast<double>(error_code);
}

/**
 * @brief 设置 native 错误日志文件路径。成功返回 0。
 */
GmlCallable auto SetNativeLogFilePath(const char* log_file_path) -> double {
  if (!log_file_path || !*log_file_path) {
    return FailWith(NativeError::InvalidArgument,
                    "SetNativeLogFilePath: empty log_file_path");
  }
  NativeLogFilePath() = log_file_path;
  // 提前建目录，保证后续 LogNativeError 一定写得进去
  std::error_code ec;
  std::filesystem::create_directories(
      std::filesystem::path(FileSystem::Utf8ToUtf16(log_file_path))
          .parent_path(),
      ec);
  return static_cast<double>(NativeError::Ok);
}

/**
 * @brief 打开文件夹。成功返回 0，失败返回 ShellExecute 错误码。
 */
GmlCallable auto OpenFolder(const char* path) -> double {
  if (!path || !*path) {
    return FailWith(NativeError::InvalidArgument, "OpenFolder: empty path");
  }
  int code = FileSystem::OpenFolder(path);
  if (code != 0) {
    return FailWith(code, "OpenFolder failed");
  }
  return static_cast<double>(NativeError::Ok);
}

/**
 * @brief 检查文件夹是否存在 (1为真, 0为假)
 */
GmlCallable auto FolderExists(const char* path) -> double {
  return FileSystem::FolderExists(path) ? 1.0 : 0.0;
}

/**
 * @brief 检查文件是否存在 (1为真, 0为假)
 */
GmlCallable auto FileExists(const char* path) -> double {
  return FileSystem::FileExists(path) ? 1.0 : 0.0;
}

/**
 * @brief 复制并合并文件夹。成功返回 0，失败返回平台错误码。
 */
GmlCallable auto CopyFolder(const char* source, const char* destination)
    -> double {
  if (!source || !*source || !destination || !*destination) {
    return FailWith(NativeError::InvalidArgument,
                    "CopyFolder: empty source or destination");
  }
  int code = FileSystem::CopyAndMergeDirectory(source, destination);
  if (code != 0) {
    return FailWith(code, "CopyFolder failed");
  }
  return static_cast<double>(NativeError::Ok);
}

/**
 * @brief 删除指定文件夹及其内容。成功返回 0，失败返回 SHFileOperation 错误码。
 */
GmlCallable auto DeleteFolder(const char* path) -> double {
  if (!path || !*path) {
    return FailWith(NativeError::InvalidArgument, "DeleteFolder: empty path");
  }
  int code = FileSystem::DeleteFolder(path);
  if (code != 0) {
    return FailWith(code, "DeleteFolder failed");
  }
  return static_cast<double>(NativeError::Ok);
}

GmlCallable auto StartBackupWithTargetFile(const char* saves_dir,
                                           const char* target_file) -> double {
  if (!saves_dir || !*saves_dir || !target_file || !*target_file) {
    return FailWith(NativeError::InvalidArgument,
                    "StartBackupWithTargetFile: empty saves_dir or target_file");
  }

  namespace fs = std::filesystem;
  std::wstring w_saves_dir = FileSystem::Utf8ToUtf16(saves_dir);

  std::error_code ec;
  if (!fs::exists(w_saves_dir, ec)) {
    if (ec) {
      return FailWith(ec.value(),
                      "StartBackupWithTargetFile: exists check failed");
    }
    fs::create_directories(w_saves_dir, ec);
    if (ec) {
      return FailWith(ec.value(),
                      "StartBackupWithTargetFile: create_directories failed");
    }
  }

  json backup_json;
  backup_json["files"] = json::array();

  for (const auto& entry : fs::directory_iterator(w_saves_dir, ec)) {
    if (entry.path().extension() == L".json") {
      std::ifstream ifs(entry.path());
      if (ifs.is_open()) {
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        ifs.close();

        json file_entry;
        file_entry["name"] = entry.path().filename().string();
        file_entry["content"] = buffer.str();
        backup_json["files"].push_back(file_entry);
      }
    }
  }

  if (ec) {
    return FailWith(ec.value(),
                    "StartBackupWithTargetFile: directory iteration error");
  }

  std::string json_str = backup_json.dump(4);
  std::vector<uint8_t> data(json_str.begin(), json_str.end());

  int write_code = FileSystem::WriteNativeFile(target_file, data);
  if (write_code != 0) {
    return FailWith(write_code, "StartBackupWithTargetFile: write failed");
  }
  return static_cast<double>(NativeError::Ok);
}

GmlCallable auto StartBackup(const char* saves_dir) -> double {
  if (!saves_dir || !*saves_dir) {
    return FailWith(NativeError::InvalidArgument, "StartBackup: empty saves_dir");
  }
  std::string saves_dir_copy(saves_dir);

  std::string chosen_folder = FileSystem::ChooseFolder();
  if (chosen_folder.empty()) {
    return FailWith(NativeError::OperationCancelled,
                    "StartBackup: folder dialog cancelled");
  }

  namespace fs = std::filesystem;
  std::wstring w_chosen = FileSystem::Utf8ToUtf16(chosen_folder.c_str());
  fs::path backup_path = fs::path(w_chosen) / L"backup.json";
  std::string backup_path_str =
      FileSystem::Utf16ToUtf8(backup_path.wstring().c_str());
  if (backup_path_str.empty()) {
    return FailWith(NativeError::EncodingFailed,
                    "StartBackup: path encoding failed");
  }

  return StartBackupWithTargetFile(saves_dir_copy.c_str(),
                                   backup_path_str.c_str());
}

GmlCallable auto RestoreBackupWithTargetFile(const char* saves_dir,
                                             const char* target_file)
    -> double {
  if (!saves_dir || !*saves_dir || !target_file || !*target_file) {
    return FailWith(
        NativeError::InvalidArgument,
        "RestoreBackupWithTargetFile: empty saves_dir or target_file");
  }

  namespace fs = std::filesystem;
  std::wstring w_target = FileSystem::Utf8ToUtf16(target_file);
  std::error_code ec;
  if (!fs::exists(w_target, ec)) {
    return FailWith(ec ? ec.value() : ERROR_FILE_NOT_FOUND,
                    "RestoreBackupWithTargetFile: target file not found");
  }

  std::ifstream ifs(w_target);
  if (!ifs.is_open()) {
    return FailWith(ERROR_FILE_NOT_FOUND,
                    "RestoreBackupWithTargetFile: failed to open target file");
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  ifs.close();

  json backup_json;
  try {
    backup_json = json::parse(buffer.str());
  } catch (...) {
    return FailWith(NativeError::JsonParseFailed,
                    "RestoreBackupWithTargetFile: json parse failed");
  }

  std::wstring w_saves_dir = FileSystem::Utf8ToUtf16(saves_dir);
  fs::create_directories(w_saves_dir, ec);
  if (ec) {
    return FailWith(ec.value(),
                    "RestoreBackupWithTargetFile: create_directories failed");
  }

  if (!backup_json.contains("files") || !backup_json["files"].is_array()) {
    return FailWith(NativeError::JsonParseFailed,
                    "RestoreBackupWithTargetFile: missing files array");
  }

  for (const auto& file_entry : backup_json["files"]) {
    if (!file_entry.contains("name") || !file_entry.contains("content")) {
      return FailWith(NativeError::JsonParseFailed,
                      "RestoreBackupWithTargetFile: invalid file entry");
    }
    std::string name = file_entry["name"];
    std::string content = file_entry["content"];

    fs::path file_path =
        fs::path(w_saves_dir) / FileSystem::Utf8ToUtf16(name.c_str());

    std::vector<uint8_t> data(content.begin(), content.end());
    std::string file_path_utf8 =
        FileSystem::Utf16ToUtf8(file_path.wstring().c_str());
    if (file_path_utf8.empty()) {
      return FailWith(NativeError::EncodingFailed,
                      "RestoreBackupWithTargetFile: path encoding failed");
    }
    int write_code = FileSystem::WriteNativeFile(file_path_utf8, data);
    if (write_code != 0) {
      return FailWith(write_code,
                      "RestoreBackupWithTargetFile: write file failed");
    }
  }

  return static_cast<double>(NativeError::Ok);
}

GmlCallable auto RestoreBackup(const char* saves_dir,
                               const char* default_backup_dir) -> double {
  std::string saves_dir_copy = (saves_dir && *saves_dir) ? saves_dir : "";
  std::string default_dir_copy =
      (default_backup_dir && *default_backup_dir) ? default_backup_dir : "";

  if (saves_dir_copy.empty()) {
    return FailWith(NativeError::InvalidArgument,
                    "RestoreBackup: empty saves_dir");
  }

  std::string chosen_file =
      FileSystem::ChooseFileToOpen(default_dir_copy.empty()
                                       ? nullptr
                                       : default_dir_copy.c_str());
  if (chosen_file.empty()) {
    return FailWith(NativeError::OperationCancelled,
                    "RestoreBackup: file dialog cancelled");
  }

  return RestoreBackupWithTargetFile(saves_dir_copy.c_str(),
                                     chosen_file.c_str());
}

// 已知中文/第三方输入法候选窗口类名关键字（微信/搜狗等 TSF 输入法用独立候选窗）。
// 注意：Windows 默认 IME 窗口类名就是 "IME"；微软拼音候选窗宿主是 TextInputHost.exe。
static const wchar_t* kImeCandidateClassKeys[] = {
    L"WeType",    L"WeChat", L"Weixin",     L"Candidate", L"Sogou",
    L"QQPinyin",  L"ChsIME", L"InputMethod", L"InputCand", L"BaiduIME",
    L"HuaweiIME", L"MSIME",  L"OldMSIME",   L"Cand",      L"IME"};

// 候选窗宿主进程名关键字（TSF 输入法的候选窗属于独立进程，单靠类名会漏）
static const wchar_t* kImeCandidateProcKeys[] = {
    L"WeType",   L"WeChat", L"Weixin",      L"Sogou",
    L"QQPinyin", L"BaiduIME", L"HuaweiIME", L"TextInputHost"};

static bool EqualsNoCaseN(const wchar_t* a, const wchar_t* b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    const wchar_t ca = a[i];
    const wchar_t cb = b[i];
    if (ca == 0 || cb == 0) return ca == cb;
    if (std::towlower(ca) != std::towlower(cb)) return false;
  }
  return true;
}

static bool ContainsNoCase(const wchar_t* hay, const wchar_t* needle) {
  if (!hay || !needle || !*hay || !*needle) return false;
  const size_t n = wcslen(needle);
  for (const wchar_t* p = hay; *p; ++p) {
    if (EqualsNoCaseN(p, needle, n)) return true;
  }
  return false;
}

static void GetProcessBaseNameW(DWORD pid, wchar_t* out, size_t cch) {
  if (!out || cch == 0) return;
  out[0] = L'\0';
  if (!pid) return;
  HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!handle) return;
  wchar_t full[MAX_PATH] = {0};
  DWORD len = MAX_PATH;
  if (QueryFullProcessImageNameW(handle, 0, full, &len) && len > 0) {
    const wchar_t* base = wcsrchr(full, L'\\');
    wcsncpy_s(out, cch, base ? base + 1 : full, _TRUNCATE);
  }
  CloseHandle(handle);
}

// 候选窗的特征：无激活的弹出/置顶小窗。进程名匹配必须额外满足它，
// 否则会把输入法进程的其它窗口也算进来（微信输入法 wetype_renderer.exe 有上百个窗口）
static bool IsCandidateLikeWindow(HWND wnd) {
  if (!wnd) return false;
  const LONG style = GetWindowLongW(wnd, GWL_STYLE);
  const LONG_PTR ex = GetWindowLongPtrW(wnd, GWL_EXSTYLE);
  return (style & WS_POPUP) != 0 || (ex & WS_EX_TOPMOST) != 0 ||
         (ex & WS_EX_NOACTIVATE) != 0 || (ex & WS_EX_TOOLWINDOW) != 0;
}

static bool MatchesImeKeys(HWND wnd, const wchar_t* cls, const wchar_t* proc) {
  for (const wchar_t* key : kImeCandidateClassKeys) {
    if (ContainsNoCase(cls, key)) return true;
  }
  if (!IsCandidateLikeWindow(wnd)) return false;
  for (const wchar_t* key : kImeCandidateProcKeys) {
    if (ContainsNoCase(proc, key)) return true;
  }
  return false;
}

static std::string WideToUtf8(const wchar_t* w) {
  if (!w || !*w) return std::string();
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return std::string();
  std::string s(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  return s;
}

struct ImeHideCtx {
  HWND game;
  int hidden;
  int matched;
};

static BOOL CALLBACK HideImeCandidateProc(HWND wnd, LPARAM lParam) {
  ImeHideCtx* ctx = reinterpret_cast<ImeHideCtx*>(lParam);
  if (!wnd || wnd == ctx->game) return TRUE;
  wchar_t cls[256] = {0};
  if (!GetClassNameW(wnd, cls, 255)) return TRUE;
  DWORD pid = 0;
  GetWindowThreadProcessId(wnd, &pid);
  wchar_t proc[MAX_PATH] = {0};
  GetProcessBaseNameW(pid, proc, MAX_PATH);
  if (!MatchesImeKeys(wnd, cls, proc)) return TRUE;
  ctx->matched++;
  if (IsWindowVisible(wnd)) {
    ShowWindow(wnd, SW_HIDE);
    ctx->hidden++;
  }
  return TRUE;
}

// 诊断：把「可见顶层窗口」的增量写进 latest.log，用来拿真实候选窗类名与宿主进程
struct ImeSnapCtx {
  HWND game;
  HWND fg;
  std::wstring sig;
  std::vector<std::wstring> lines;
};

static BOOL CALLBACK SnapshotProc(HWND wnd, LPARAM lParam) {
  ImeSnapCtx* ctx = reinterpret_cast<ImeSnapCtx*>(lParam);
  if (!wnd || wnd == ctx->game) return TRUE;
  if (!IsWindowVisible(wnd)) return TRUE;
  wchar_t cls[256] = {0};
  if (!GetClassNameW(wnd, cls, 255)) return TRUE;
  wchar_t title[128] = {0};
  GetWindowTextW(wnd, title, 127);
  DWORD pid = 0;
  GetWindowThreadProcessId(wnd, &pid);
  wchar_t proc[MAX_PATH] = {0};
  GetProcessBaseNameW(pid, proc, MAX_PATH);
  ctx->sig += cls;
  ctx->sig += L'|';
  ctx->sig += std::to_wstring(pid);
  ctx->sig += L';';
  std::wstring line = L"ImeDbg: cls=";
  line += cls;
  line += L" proc=";
  line += proc;
  line += L" pid=";
  line += std::to_wstring(pid);
  if (wnd == ctx->fg) line += L" fg=1";
  if (MatchesImeKeys(wnd, cls, proc)) line += L" ime=1";
  if (title[0]) {
    line += L" title=";
    line += title;
  }
  ctx->lines.push_back(line);
  return TRUE;
}

// 窗口快照：只在 FVM_IME_DEBUG=1 时输出，平时不写（避免 latest.log 无限增长）
static void LogImeWindowSnapshot(HWND game, bool full) {
  if (!full) return;
  ImeSnapCtx ctx{};
  ctx.game = game;
  ctx.fg = GetForegroundWindow();
  EnumWindows(SnapshotProc, reinterpret_cast<LPARAM>(&ctx));
  char head[160] = {0};
  sprintf_s(head, sizeof(head), "ImeDbg: snapshot n=%d full=1",
            static_cast<int>(ctx.lines.size()));
  LogNativeError(0, head);
  int shown = 0;
  for (const std::wstring& line : ctx.lines) {
    if (++shown > 60) {
      LogNativeError(0, "ImeDbg: ... (truncated)");
      break;
    }
    LogNativeError(0, WideToUtf8(line.c_str()).c_str());
  }
}

// 全量模式：设环境变量 FVM_IME_DEBUG 为非空值（如 "1"）才输出窗口快照，不依赖 GML。
// 注意：不能只判断长度>0——空字符串会导致误判为真，必须要求"存在且非空"。
static bool ImeDebugEnvSet() {
  wchar_t buf[8] = {0};
  const DWORD len =
      GetEnvironmentVariableW(L"FVM_IME_DEBUG", buf, static_cast<DWORD>(std::size(buf)));
  if (len == 0) return false;                        // 不存在
  if (len >= static_cast<DWORD>(std::size(buf)))
    return *buf != L'\0';                            // 值太长：看首字符
  return buf[0] != L'\0';                            // 存在且非空
}

// 只在「本进程在前台」或「前台窗口本身就是输入法 UI」时压制，
// 避免玩家 Alt-Tab 到别的程序打字时，把那个程序的候选窗也隐藏掉
static bool ShouldSuppressNow() {
  HWND fg = GetForegroundWindow();
  if (!fg) return false;
  DWORD fg_pid = 0;
  GetWindowThreadProcessId(fg, &fg_pid);
  if (fg_pid == GetCurrentProcessId()) return true;
  wchar_t cls[256] = {0};
  if (!GetClassNameW(fg, cls, 255)) return false;
  wchar_t proc[MAX_PATH] = {0};
  GetProcessBaseNameW(fg_pid, proc, MAX_PATH);
  return MatchesImeKeys(fg, cls, proc);
}

// 核心压制逻辑：DisableIme（GML 调用）与 WM_TIMER 心跳共用
// v7：彻底不碰键盘布局/输入语言。v6 的 LoadKeyboardLayoutW + WM_INPUTLANGCHANGEREQUEST
//     会往系统里加一个英文输入法并一直切过去，污染全局（其它程序中文都用不了），已删除。
//     改成 per-window 断开 IME 上下文（只影响游戏窗口本身）。
static void ApplyImeBlock(HWND hwnd, bool from_timer) {
  if (!hwnd) return;

  // 1) 子类化（处理 WM_IME_SETCONTEXT 屏蔽候选窗 + 焦点/心跳）
  if (!GetWindowSubclass(hwnd, ImeWndProc, 1, 0)) {
    SetWindowSubclass(hwnd, ImeWndProc, 1, 0);
  }
  // 2) 断开本窗口的 IME 上下文：不动系统输入法/语言列表
  HIMC prev = ImmAssociateContext(hwnd, nullptr);
  HIMC now = ImmGetContext(hwnd);  // 同线程查询：断开后应为 NULL
  if (now) ImmReleaseContext(hwnd, now);
  // 3) 窗口级心跳：不依赖 GML 对象存活
  static HWND timer_hwnd = nullptr;
  if (timer_hwnd != hwnd) {
    SetTimer(hwnd, kImeTimerId, 1000, nullptr);
    timer_hwnd = hwnd;
  }
  // 4) 兜底：隐藏第三方输入法的独立候选窗（TSF 输入法可能绕过 HIMC）
  int hidden = 0;
  int matched = 0;
  if (ShouldSuppressNow()) {
    ImeHideCtx ctx{};
    ctx.game = hwnd;
    ctx.hidden = 0;
    ctx.matched = 0;
    EnumWindows(HideImeCandidateProc, reinterpret_cast<LPARAM>(&ctx));
    hidden = ctx.hidden;
    matched = ctx.matched;
  }
  // 5) 日志：默认完全关闭（IME 不写任何行，不占玩家空间）。
  //    说明：latest.log 是游戏原有的 native 错误日志（存档/备份等也写它），文件本身不能删；
  //    这里只保证 IME 相关行默认 0 条，设 FVM_IME_DEBUG=1 才输出（排查用）。
  const bool debug = ImeDebugEnvSet();
  if (debug) {
    static int ticks = 0;
    static int ticks_timer = 0;
    static int ticks_gml = 0;
    static int hidden_total = 0;
    ++ticks;
    if (from_timer) ++ticks_timer; else ++ticks_gml;
    hidden_total += hidden;
    static HWND last_hwnd = nullptr;
    static int last_hidden = -1;
    static DWORD last_debug_tick = 0;
    const DWORD now_tick = GetTickCount();
    const bool hwnd_changed = (hwnd != last_hwnd);
    const bool himc_reattached = (prev != nullptr);
    const bool hid_something = (hidden > 0 && hidden != last_hidden);
    const bool debug_due = (now_tick - last_debug_tick >= 5000);
    const bool alive_due = ((ticks % 300) == 0);
    if (hwnd_changed || himc_reattached || hid_something || debug_due ||
        alive_due) {
      last_hwnd = hwnd;
      last_hidden = hidden;
      last_debug_tick = now_tick;
      HWND fg = GetForegroundWindow();
      wchar_t fg_cls[256] = {0};
      if (fg) GetClassNameW(fg, fg_cls, 255);
      char msg[512] = {0};
      sprintf_s(msg, sizeof(msg),
                "DisableIme[v7.1]: src=%s ticks=%d(timer=%d,gml=%d) hwnd=%p "
                "himc_prev=%p himc_now=%p hidden=%d/%d matched=%d fg=%p "
                "fgcls=%s debug=%d",
                from_timer ? "T" : "G", ticks, ticks_timer, ticks_gml, hwnd,
                reinterpret_cast<void*>(prev), reinterpret_cast<void*>(now),
                hidden, hidden_total, matched, fg, WideToUtf8(fg_cls).c_str(),
                1);
      LogNativeError(0, msg);
    }
    LogImeWindowSnapshot(hwnd, true);
  }
}

/**
 * @brief 屏蔽输入法（IME）候选框。返回 0 表示调用成功。
 *        v7：per-window 断开 IME 上下文（ImmAssociateContext(hwnd,nullptr)）
 *            + WM_IME_SETCONTEXT 清候选窗标志 + 窗口级 1 秒心跳 + 诊断日志。
 *            **不再触碰键盘布局/输入语言**（v6 会污染系统输入法，已废弃）。
 */
GmlCallable auto DisableIme(double hwnd_value) -> double {
  HWND hwnd = static_cast<HWND>(
      reinterpret_cast<void*>(static_cast<INT_PTR>(hwnd_value)));
  if (!hwnd) hwnd = GetForegroundWindow();
  ApplyImeBlock(hwnd, false);
  return static_cast<double>(NativeError::Ok);
}
