// Terminal UI toolkit: ANSI styling, UTF-8/CJK-aware width handling,
// boxes, tables, word-wrapping, input and a spinner for blocking work.
#pragma once

#include <chrono>
#include <cstdio>
#include <future>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

namespace style {
inline constexpr const char* Reset = "\033[0m";
inline constexpr const char* Bold = "\033[1m";
inline constexpr const char* Dim = "\033[2m";
inline constexpr const char* Italic = "\033[3m";
inline constexpr const char* Underline = "\033[4m";
// Muted, green-leaning 256-colour palette. The names describe each colour's
// role in the original ANSI palette, so call sites keep their meaning.
inline constexpr const char* Red = "\033[38;5;131m";           // brick
inline constexpr const char* Green = "\033[38;5;108m";         // sage
inline constexpr const char* Yellow = "\033[38;5;143m";        // olive
inline constexpr const char* Blue = "\033[38;5;67m";           // steel blue
inline constexpr const char* Magenta = "\033[38;5;65m";        // moss (box borders)
inline constexpr const char* Cyan = "\033[38;5;66m";           // slate teal
inline constexpr const char* White = "\033[38;5;250m";
inline constexpr const char* Gray = "\033[38;5;244m";
inline constexpr const char* BrightWhite = "\033[38;5;253m";
inline constexpr const char* BrightRed = "\033[38;5;167m";     // soft red (wrong answers)
inline constexpr const char* BrightGreen = "\033[38;5;114m";   // fresh green (correct answers)
inline constexpr const char* BrightYellow = "\033[38;5;150m";  // pale olive-green (titles)
inline constexpr const char* BrightBlue = "\033[38;5;110m";    // pale blue
inline constexpr const char* BrightMagenta = "\033[38;5;72m";  // sea green (accents, option labels)
inline constexpr const char* BrightCyan = "\033[38;5;109m";    // grey teal
}  // namespace style

bool colorEnabled();
std::string paint(std::string_view text, std::initializer_list<const char*> styles);
inline std::string paint(std::string_view text, const char* s) { return paint(text, {s}); }

// Width helpers (ANSI escapes count as 0 columns, CJK as 2).
std::size_t displayWidth(std::string_view s);
std::string padRight(std::string_view s, std::size_t width);
std::string padLeft(std::string_view s, std::size_t width);
std::string center(std::string_view s, std::size_t width);
std::string truncate(std::string_view s, std::size_t width);
std::vector<std::string> wrap(std::string_view text, std::size_t width);

int terminalWidth();
std::size_t frameWidth();    // total width of boxes/tables (capped for readability)
std::size_t contentWidth();  // text width available inside a box

void clearScreen();
void printBox(const std::vector<std::string>& lines, std::string_view title = {},
              const char* borderStyle = style::Cyan, bool doubleLine = false);
void printRule(std::string_view title = {}, const char* lineStyle = style::Gray);
void printHeader(std::string_view japanese, std::string_view english);
std::string progressBar(double ratio, std::size_t width, const char* fillStyle = style::Green);

// Inline markup used in question data: [[word]] => highlighted/underlined.
std::string renderMarkup(std::string_view s);
std::string plainMarkup(std::string_view s);  // [[word]] => 【word】 (for API prompts)

// Light markdown renderer for LLM output (headings, **bold**, bullets).
std::vector<std::string> renderMarkdown(std::string_view text, std::size_t width);

// Input. nullopt means EOF (Ctrl+D) — callers should treat it as "quit".
std::optional<std::string> readLine(std::string_view prompt);
bool waitForEnter(std::string_view message = "Press Enter to continue...");

void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);

class Table {
public:
    enum class Align { Left, Right, Center };

    explicit Table(std::vector<std::string> headers);
    void setAlign(std::size_t column, Align align);
    void setMaxWidth(std::size_t column, std::size_t width);
    void addRow(std::vector<std::string> row);
    bool empty() const { return rows_.empty(); }
    void print(const char* borderStyle = style::Gray) const;

private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<Align> align_;
    std::vector<std::size_t> maxWidth_;
};

// Runs `work` on a background thread while animating a spinner.
template <typename F>
auto withSpinner(std::string_view message, F&& work) -> decltype(work()) {
    auto future = std::async(std::launch::async, std::forward<F>(work));
    static const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    std::size_t i = 0;
    while (future.wait_for(std::chrono::milliseconds(90)) != std::future_status::ready) {
        std::cout << "\r  " << paint(frames[i++ % 10], style::BrightMagenta) << ' '
                  << paint(message, style::Dim) << std::flush;
    }
    std::cout << "\r\033[2K" << std::flush;
    return future.get();
}

}  // namespace ui
