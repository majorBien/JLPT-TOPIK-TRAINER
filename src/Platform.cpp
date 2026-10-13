#include "Platform.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace fs = std::filesystem;

namespace platform {

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto end = s.find(sep, start);
        out.push_back(s.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

#if defined(_WIN32)
// cmd.exe /c strips the outer pair of quotes, so a command that starts with a
// quoted program path needs one more pair around everything.
std::string forCmd(const std::string& command) { return "\"" + command + "\""; }
#endif

}  // namespace

void initConsole() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
#endif
}

bool stdoutIsTerminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

int terminalColumns() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 0;
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 0;
#endif
}

std::string executableDir() {
    std::error_code ec;
#if defined(_WIN32)
    char buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameA(nullptr, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf) return {};
    return fs::path(std::string(buf, n)).parent_path().string();
#elif defined(__APPLE__)
    char buf[4096];
    std::uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    return fs::weakly_canonical(fs::path(buf), ec).parent_path().string();
#else
    return fs::read_symlink("/proc/self/exe", ec).parent_path().string();
#endif
}

std::string homeDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("USERPROFILE"); p && *p) return p;
#endif
    if (const char* p = std::getenv("HOME"); p && *p) return p;
    return {};
}

std::string writeTempFile(const std::string& content, const std::string& suffix) {
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec);
    if (ec) return {};
#if defined(_WIN32)
    static std::mt19937_64 rng{std::random_device{}()};
    for (int attempt = 0; attempt < 16; ++attempt) {
        char name[40];
        std::snprintf(name, sizeof name, "jkt-%016llx", static_cast<unsigned long long>(rng()));
        const fs::path p = dir / (std::string(name) + suffix);
        if (fs::exists(p, ec)) continue;
        std::ofstream out(p, std::ios::binary);
        out << content;
        if (out) return p.string();
    }
    return {};
#else
    std::string tmpl = (dir / ("jkt-XXXXXX" + suffix)).string();
    const int fd = mkstemps(tmpl.data(), static_cast<int>(suffix.size()));  // mode 0600
    if (fd < 0) return {};
    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t n = ::write(fd, content.data() + written, content.size() - written);
        if (n <= 0) break;
        written += static_cast<std::size_t>(n);
    }
    ::close(fd);
    if (written != content.size()) {
        std::remove(tmpl.c_str());
        return {};
    }
    return tmpl;
#endif
}

CommandResult runCapture(const std::string& command) {
    CommandResult result;
#if defined(_WIN32)
    FILE* pipe = _popen(forCmd(command).c_str(), "rb");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return result;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) result.output.append(buf, n);
#if defined(_WIN32)
    result.exitCode = _pclose(pipe);
#else
    const int status = pclose(pipe);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    return result;
}

int runQuiet(const std::string& command) {
#if defined(_WIN32)
    return std::system(forCmd(command + " <NUL >NUL 2>&1").c_str());
#else
    const int status = std::system((command + " </dev/null >/dev/null 2>&1").c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

std::string quote(const std::string& arg) {
#if defined(_WIN32)
    return "\"" + arg + "\"";  // paths and voice names never contain double quotes
#else
    std::string out = "'";
    for (char c : arg) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
#endif
}

std::string findCommand(const std::string& name) {
    std::error_code ec;
    if (name.empty()) return {};
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
        return fs::is_regular_file(name, ec) ? name : std::string{};
    const char* path = std::getenv("PATH");
    if (!path) return {};
#if defined(_WIN32)
    const char* pathext = std::getenv("PATHEXT");
    std::vector<std::string> exts = {""};
    for (auto& e : split(pathext ? pathext : ".COM;.EXE;.BAT;.CMD", ';'))
        if (!e.empty()) exts.push_back(e);
    for (const auto& dir : split(path, ';')) {
        if (dir.empty()) continue;
        for (const auto& ext : exts) {
            const fs::path p = fs::path(dir) / (name + ext);
            if (fs::is_regular_file(p, ec)) return p.string();
        }
    }
#else
    for (const auto& dir : split(path, ':')) {
        if (dir.empty()) continue;
        const std::string p = dir + "/" + name;
        if (access(p.c_str(), X_OK) == 0) return p;
    }
#endif
    return {};
}

bool commandExists(const std::string& name) { return !findCommand(name).empty(); }

}  // namespace platform
