#include "Terminal.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <numeric>

#include "Util.h"

namespace ui {

namespace {

// ---- UTF-8 / ANSI scanning -------------------------------------------------

struct Unit {
    std::string_view bytes;
    std::size_t width = 0;
    bool escape = false;
    std::uint32_t cp = 0;
};

std::size_t codepointWidth(std::uint32_t cp) {
    if (cp == 0) return 0;
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) ||
        (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0x3099 || cp == 0x309A)
        return 0;
    const bool wide =
        (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3041 && cp <= 0x33FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xA000 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1F64F) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x20000 && cp <= 0x3FFFD);
    return wide ? 2 : 1;
}

std::vector<Unit> scan(std::string_view s) {
    std::vector<Unit> units;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == 0x1B && i + 1 < s.size() && s[i + 1] == '[') {
            std::size_t j = i + 2;
            while (j < s.size() && !(s[j] >= 0x40 && s[j] <= 0x7E)) ++j;
            j = std::min(j + 1, s.size());
            units.push_back({s.substr(i, j - i), 0, true, 0});
            i = j;
            continue;
        }
        std::size_t len = 1;
        std::uint32_t cp = c;
        if (c >= 0xF0) { len = 4; cp = c & 0x07; }
        else if (c >= 0xE0) { len = 3; cp = c & 0x0F; }
        else if (c >= 0xC0) { len = 2; cp = c & 0x1F; }
        len = std::min(len, s.size() - i);
        for (std::size_t k = 1; k < len; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        units.push_back({s.substr(i, len), codepointWidth(cp), false, cp});
        i += len;
    }
    return units;
}

bool isClosingPunct(std::uint32_t cp) {
    switch (cp) {
        case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0x300D: case 0x300F:
        case 0xFF09: case 0xFF01: case 0xFF1F: case 0x30FC: case 0x3011:
            return true;
        default:
            return false;
    }
}

void applyEscape(std::string& active, std::string_view esc) {
    if (esc == style::Reset) active.clear();
    else active += esc;
}

// ---- Inline markdown -------------------------------------------------------

std::string renderInline(std::string_view line) {
    std::string out;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line.substr(i, 2) == "**") {
            const auto end = line.find("**", i + 2);
            if (end != std::string_view::npos) {
                out += paint(line.substr(i + 2, end - i - 2), {style::Bold, style::BrightYellow});
                i = end + 2;
                continue;
            }
        }
        if (line[i] == '`') {
            const auto end = line.find('`', i + 1);
            if (end != std::string_view::npos) {
                out += paint(line.substr(i + 1, end - i - 1), style::BrightCyan);
                i = end + 1;
                continue;
            }
        }
        out += line[i++];
    }
    return out;
}

void appendHanging(std::vector<std::string>& out, std::string_view prefix, std::string_view body,
                   std::size_t width) {
    const std::size_t indent = displayWidth(prefix);
    const auto wrapped = wrap(renderInline(body), width > indent + 10 ? width - indent : 10);
    for (std::size_t i = 0; i < wrapped.size(); ++i)
        out.push_back((i == 0 ? std::string(prefix) : std::string(indent, ' ')) + wrapped[i]);
}

}  // namespace

// ---- Styling ---------------------------------------------------------------

bool colorEnabled() {
    static const bool enabled = [] {
        if (std::getenv("NO_COLOR")) return false;
        if (std::getenv("JLPT_FORCE_COLOR")) return true;
        return isatty(STDOUT_FILENO) != 0;
    }();
    return enabled;
}

std::string paint(std::string_view text, std::initializer_list<const char*> styles) {
    if (!colorEnabled()) return std::string(text);
    std::string out;
    for (const char* s : styles) out += s;
    out += text;
    out += style::Reset;
    return out;
}

// ---- Width -----------------------------------------------------------------

std::size_t displayWidth(std::string_view s) {
    std::size_t w = 0;
    for (const auto& u : scan(s)) w += u.width;
    return w;
}

std::string padRight(std::string_view s, std::size_t width) {
    const std::size_t w = displayWidth(s);
    return std::string(s) + std::string(w < width ? width - w : 0, ' ');
}

std::string padLeft(std::string_view s, std::size_t width) {
    const std::size_t w = displayWidth(s);
    return std::string(w < width ? width - w : 0, ' ') + std::string(s);
}

std::string center(std::string_view s, std::size_t width) {
    const std::size_t w = displayWidth(s);
    if (w >= width) return std::string(s);
    const std::size_t left = (width - w) / 2;
    return std::string(left, ' ') + std::string(s) + std::string(width - w - left, ' ');
}

std::string truncate(std::string_view s, std::size_t width) {
    if (displayWidth(s) <= width) return std::string(s);
    std::string out;
    std::size_t w = 0;
    bool styled = false;
    for (const auto& u : scan(s)) {
        if (u.escape) { out += u.bytes; styled = true; continue; }
        if (w + u.width + 1 > width) break;
        out += u.bytes;
        w += u.width;
    }
    out += "…";
    if (styled && colorEnabled()) out += style::Reset;
    return out;
}

std::vector<std::string> wrap(std::string_view text, std::size_t width) {
    width = std::max<std::size_t>(width, 4);
    std::vector<std::string> lines;
    std::string active;  // SGR styles currently in effect, re-applied after breaks

    enum class Kind { Word, Space, Escape };
    struct Token { std::string text; std::size_t width; Kind kind; bool narrow; };

    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find('\n', start);
        const auto paragraph = text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);

        // Tokenize: ASCII runs are words, every wide char is its own token,
        // closing Japanese punctuation sticks to the previous token (kinsoku).
        std::vector<Token> tokens;
        for (const auto& u : scan(paragraph)) {
            if (u.escape) { tokens.push_back({std::string(u.bytes), 0, Kind::Escape, false}); continue; }
            if (u.cp == ' ' || u.cp == '\t') { tokens.push_back({" ", 1, Kind::Space, false}); continue; }
            if (u.width == 2 && isClosingPunct(u.cp) && !tokens.empty() && tokens.back().kind == Kind::Word) {
                tokens.back().text += u.bytes;
                tokens.back().width += u.width;
                tokens.back().narrow = false;
                continue;
            }
            const bool narrow = u.width < 2;
            if (narrow && !tokens.empty() && tokens.back().kind == Kind::Word && tokens.back().narrow) {
                tokens.back().text += u.bytes;
                tokens.back().width += u.width;
            } else {
                tokens.push_back({std::string(u.bytes), u.width, Kind::Word, narrow});
            }
        }

        std::string line = active;
        std::size_t lineWidth = 0;
        bool pendingSpace = false;
        auto flush = [&] {
            lines.push_back(line + (active.empty() || !colorEnabled() ? "" : style::Reset));
            line = active;
            lineWidth = 0;
            pendingSpace = false;
        };

        for (const auto& tok : tokens) {
            if (tok.kind == Kind::Escape) { line += tok.text; applyEscape(active, tok.text); continue; }
            if (tok.kind == Kind::Space) { if (lineWidth > 0) pendingSpace = true; continue; }

            const std::size_t need = tok.width + (pendingSpace ? 1 : 0);
            if (lineWidth + need <= width) {
                if (pendingSpace) { line += ' '; ++lineWidth; }
                line += tok.text;
                lineWidth += tok.width;
                pendingSpace = false;
                continue;
            }
            if (lineWidth > 0) flush();
            if (tok.width <= width) {
                line += tok.text;
                lineWidth += tok.width;
                continue;
            }
            for (const auto& u : scan(tok.text)) {  // hard split of an over-long token
                if (lineWidth + u.width > width) flush();
                line += u.bytes;
                lineWidth += u.width;
            }
        }
        flush();

        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return lines;
}

// ---- Layout ----------------------------------------------------------------

int terminalWidth() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    if (const char* cols = std::getenv("COLUMNS")) return std::max(40, std::atoi(cols));
    return 80;
}

std::size_t frameWidth() {
    return static_cast<std::size_t>(std::clamp(terminalWidth() - 2, 40, 80));
}

std::size_t contentWidth() { return frameWidth() - 4; }

void clearScreen() {
    if (isatty(STDOUT_FILENO)) std::cout << "\033[2J\033[H";
    else std::cout << '\n';
}

void printBox(const std::vector<std::string>& lines, std::string_view title, const char* borderStyle,
              bool doubleLine) {
    const char* tl = doubleLine ? "╔" : "╭";
    const char* tr = doubleLine ? "╗" : "╮";
    const char* bl = doubleLine ? "╚" : "╰";
    const char* br = doubleLine ? "╝" : "╯";
    const char* h = doubleLine ? "═" : "─";
    const char* v = doubleLine ? "║" : "│";

    const std::size_t total = frameWidth();
    const std::size_t inner = total - 2;
    auto repeat = [](const char* s, std::size_t n) {
        std::string out;
        for (std::size_t i = 0; i < n; ++i) out += s;
        return out;
    };

    std::string top = std::string(tl) + repeat(h, 1);
    std::size_t used = 1;
    if (!title.empty()) {
        const std::string t = " " + std::string(title) + " ";
        top = paint(top, borderStyle) + paint(t, {style::Bold, style::BrightWhite});
        used += displayWidth(t);
        top += paint(repeat(h, inner > used ? inner - used : 0) + tr, borderStyle);
    } else {
        top = paint(top + repeat(h, inner - used) + tr, borderStyle);
    }
    std::cout << ' ' << top << '\n';

    const std::size_t textWidth = inner - 2;
    for (const auto& raw : lines) {
        // Lines that already fit are printed as-is: wrapping would collapse runs of spaces (ASCII art, tables).
        const bool fits = displayWidth(raw) <= textWidth && raw.find('\n') == std::string::npos;
        for (const auto& line : fits ? std::vector<std::string>{raw} : wrap(raw, textWidth)) {
            std::cout << ' ' << paint(v, borderStyle) << ' ' << padRight(line, textWidth) << ' '
                      << paint(v, borderStyle) << '\n';
        }
    }
    std::cout << ' ' << paint(std::string(bl) + repeat(h, inner) + br, borderStyle) << '\n';
}

void printRule(std::string_view title, const char* lineStyle) {
    const std::size_t total = frameWidth();
    std::string out = " ";
    std::size_t used = 0;
    if (!title.empty()) {
        out += paint("── ", lineStyle) + paint(title, {style::Bold, style::BrightWhite}) + " ";
        used = 4 + displayWidth(title);
    }
    std::string rest;
    for (std::size_t i = used; i < total; ++i) rest += "─";
    std::cout << out << paint(rest, lineStyle) << '\n';
}

void printHeader(std::string_view japanese, std::string_view english) {
    clearScreen();
    const std::size_t w = frameWidth() - 4;
    printBox({center(paint(japanese, {style::Bold, style::BrightYellow}), w),
              center(paint(english, style::Gray), w)},
             {}, style::Magenta, true);
}

std::string progressBar(double ratio, std::size_t width, const char* fillStyle) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    const auto filled = static_cast<std::size_t>(ratio * static_cast<double>(width) + 0.5);
    std::string a, b;
    for (std::size_t i = 0; i < filled; ++i) a += "█";
    for (std::size_t i = filled; i < width; ++i) b += "░";
    return paint(a, fillStyle) + paint(b, style::Gray);
}

// ---- Markup ----------------------------------------------------------------

namespace {
template <typename F>
std::string transformMarkup(std::string_view s, F&& f) {
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto open = s.find("[[", i);
        if (open == std::string_view::npos) break;
        const auto close = s.find("]]", open + 2);
        if (close == std::string_view::npos) break;
        out += s.substr(i, open - i);
        out += f(s.substr(open + 2, close - open - 2));
        i = close + 2;
    }
    out += s.substr(i);
    return out;
}
}  // namespace

std::string renderMarkup(std::string_view s) {
    return transformMarkup(s, [](std::string_view w) {
        if (!colorEnabled()) return "【" + std::string(w) + "】";
        return paint(w, {style::Underline, style::Bold, style::BrightYellow});
    });
}

std::string plainMarkup(std::string_view s) {
    return transformMarkup(s, [](std::string_view w) { return "【" + std::string(w) + "】"; });
}

std::vector<std::string> renderMarkdown(std::string_view text, std::size_t width) {
    std::vector<std::string> out;
    auto blank = [&] { if (!out.empty() && !out.back().empty()) out.emplace_back(); };

    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find('\n', start);
        const std::string line = util::trim(
            text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start));
        start = nl == std::string_view::npos ? text.size() + 1 : nl + 1;

        if (line.empty()) { blank(); continue; }
        if (line == "---" || line == "***") { blank(); continue; }
        if (line[0] == '#') {
            const auto body = util::trim(std::string_view(line).substr(line.find_first_not_of('#')));
            blank();
            out.push_back(paint("▍" + util::replaceAll(body, "**", ""), {style::Bold, style::BrightCyan}));
            continue;
        }
        if (util::startsWith(line, "- ") || util::startsWith(line, "* ") || util::startsWith(line, "• ")) {
            const auto bodyStart = line.find(' ') + 1;
            appendHanging(out, "  " + paint("•", style::BrightMagenta) + " ", std::string_view(line).substr(bodyStart), width);
            continue;
        }
        const auto digits = line.find_first_not_of("0123456789");
        if (digits != std::string::npos && digits > 0 && digits + 1 < line.size() &&
            (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ') {
            appendHanging(out, " " + paint(line.substr(0, digits + 1), style::BrightMagenta) + " ",
                          std::string_view(line).substr(digits + 2), width);
            continue;
        }
        for (auto& l : wrap(renderInline(line), width)) out.push_back(std::move(l));
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

// ---- Input / messages ------------------------------------------------------

std::optional<std::string> readLine(std::string_view prompt) {
    std::cout << "  " << paint("❯ ", {style::Bold, style::BrightMagenta}) << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << '\n';
        return std::nullopt;
    }
    return util::trim(line);
}

bool waitForEnter(std::string_view message) {
    return readLine(paint(message, style::Gray)).has_value();
}

void info(std::string_view msg) { std::cout << "  " << paint("ℹ ", style::BrightCyan) << msg << '\n'; }
void warn(std::string_view msg) { std::cout << "  " << paint("⚠ " + std::string(msg), style::BrightYellow) << '\n'; }
void error(std::string_view msg) { std::cout << "  " << paint("✘ " + std::string(msg), style::BrightRed) << '\n'; }

// ---- Table -----------------------------------------------------------------

Table::Table(std::vector<std::string> headers)
    : headers_(std::move(headers)), align_(headers_.size(), Align::Left), maxWidth_(headers_.size(), 0) {}

void Table::setAlign(std::size_t column, Align align) {
    if (column < align_.size()) align_[column] = align;
}

void Table::setMaxWidth(std::size_t column, std::size_t width) {
    if (column < maxWidth_.size()) maxWidth_[column] = width;
}

void Table::addRow(std::vector<std::string> row) {
    row.resize(headers_.size());
    rows_.push_back(std::move(row));
}

void Table::print(const char* borderStyle) const {
    const std::size_t n = headers_.size();
    std::vector<std::size_t> widths(n);
    for (std::size_t c = 0; c < n; ++c) {
        widths[c] = displayWidth(headers_[c]);
        for (const auto& r : rows_) widths[c] = std::max(widths[c], displayWidth(r[c]));
        if (maxWidth_[c]) widths[c] = std::min(widths[c], maxWidth_[c]);
    }
    // Shrink the widest column until the table fits the frame.
    const std::size_t budget = frameWidth() - (3 * n + 1);
    while (std::accumulate(widths.begin(), widths.end(), std::size_t{0}) > budget) {
        auto widest = std::max_element(widths.begin(), widths.end());
        if (*widest <= 4) break;
        --*widest;
    }

    auto border = [&](const char* l, const char* m, const char* r) {
        std::string s = l;
        for (std::size_t c = 0; c < n; ++c) {
            for (std::size_t i = 0; i < widths[c] + 2; ++i) s += "─";
            s += c + 1 < n ? m : r;
        }
        std::cout << ' ' << paint(s, borderStyle) << '\n';
    };
    auto cell = [&](const std::string& text, std::size_t c) {
        const std::string t = truncate(text, widths[c]);
        switch (align_[c]) {
            case Align::Right: return padLeft(t, widths[c]);
            case Align::Center: return center(t, widths[c]);
            default: return padRight(t, widths[c]);
        }
    };
    auto row = [&](const std::vector<std::string>& r, bool header) {
        std::cout << ' ' << paint("│", borderStyle);
        for (std::size_t c = 0; c < n; ++c) {
            const std::string text = header ? paint(r[c], {style::Bold, style::BrightWhite}) : r[c];
            std::cout << ' ' << cell(text, c) << ' ' << paint("│", borderStyle);
        }
        std::cout << '\n';
    };

    border("┌", "┬", "┐");
    row(headers_, true);
    border("├", "┼", "┤");
    for (const auto& r : rows_) row(r, false);
    border("└", "┴", "┘");
}

}  // namespace ui
