// The only place that knows about the operating system (Linux, macOS, Windows):
// console setup, temp files, running external programs, finding executables.
#pragma once

#include <string>

namespace platform {

#if defined(_WIN32)
inline constexpr bool kWindows = true;
inline constexpr bool kMac = false;
#elif defined(__APPLE__)
inline constexpr bool kWindows = false;
inline constexpr bool kMac = true;
#else
inline constexpr bool kWindows = false;
inline constexpr bool kMac = false;
#endif

// Windows: UTF-8 code page and ANSI colours for the console. No-op elsewhere.
void initConsole();

bool stdoutIsTerminal();
int terminalColumns();          // 0 when unknown

std::string executableDir();    // folder of the running binary ("" if unknown)
std::string homeDir();          // $HOME / %USERPROFILE%

// Creates a new private temp file holding `content`; returns its path ("" on failure).
std::string writeTempFile(const std::string& content, const std::string& suffix = "");

struct CommandResult {
    int exitCode = -1;
    std::string output;  // stdout (add "2>&1" to the command for stderr)
};
CommandResult runCapture(const std::string& command);
// Runs a command with stdin/stdout/stderr detached; returns its exit code.
int runQuiet(const std::string& command);

std::string quote(const std::string& arg);       // shell-quote one argument
bool commandExists(const std::string& name);     // in $PATH (Windows: also tries %PATHEXT%), or an existing path
std::string findCommand(const std::string& name);  // full path, or "" if not found

}  // namespace platform
