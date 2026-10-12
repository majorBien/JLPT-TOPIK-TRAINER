#include "Furigana.h"

#include <algorithm>
#include <cstdint>

#include "Terminal.h"
#include "Util.h"

namespace furigana {

namespace {

std::vector<std::uint32_t> decode(std::string_view s) {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        std::uint32_t cp = len == 4 ? c & 0x07 : len == 3 ? c & 0x0F : len == 2 ? c & 0x1F : c;
        len = std::min(len, s.size() - i);
        for (std::size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        out.push_back(cp);
        i += len;
    }
    return out;
}

std::string encode(const std::vector<std::uint32_t>& cps, std::size_t from = 0, std::size_t to = std::string::npos) {
    std::string out;
    for (std::size_t i = from; i < std::min(to, cps.size()); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

std::uint32_t toHiraganaCp(std::uint32_t cp) { return cp >= 0x30A1 && cp <= 0x30F6 ? cp - 0x60 : cp; }

std::vector<std::uint32_t> toHiraganaCps(std::vector<std::uint32_t> cps) {
    for (auto& cp : cps) cp = toHiraganaCp(cp);
    return cps;
}

// The analyser normalises JLPT-style spacing to full-width spaces; show them as regular ones.
std::string displaySurface(const std::string& surface) { return surface == "　" ? " " : surface; }

}  // namespace

std::string toHiragana(std::string_view katakana) { return encode(toHiraganaCps(decode(katakana))); }

std::vector<std::string> inlineChunks(const std::vector<Morpheme>& words, const char* readingStyle) {
    std::vector<std::string> chunks;
    for (const auto& w : words) {
        if (w.reading.empty() || !util::hasKanji(w.surface)) {
            chunks.push_back(displaySurface(w.surface));
            continue;
        }
        const auto surface = decode(w.surface);
        const auto surfaceKana = toHiraganaCps(surface);
        const auto reading = toHiraganaCps(decode(w.reading));

        // Peel kana shared by surface and reading off both ends (お茶 → お + 茶(ちゃ), 食べ → 食(た) + べ).
        std::size_t head = 0, tail = 0;
        while (head < surface.size() && head < reading.size() && !util::isKanji(surface[head]) &&
               surfaceKana[head] == reading[head])
            ++head;
        while (tail < surface.size() - head && tail < reading.size() - head &&
               !util::isKanji(surface[surface.size() - 1 - tail]) &&
               surfaceKana[surface.size() - 1 - tail] == reading[reading.size() - 1 - tail])
            ++tail;

        chunks.push_back(encode(surface, 0, head) + encode(surface, head, surface.size() - tail) +
                         ui::paint("(" + encode(reading, head, reading.size() - tail) + ")", readingStyle) +
                         encode(surface, surface.size() - tail));
    }
    return chunks;
}

std::vector<std::string> wrapChunks(const std::vector<std::string>& chunks, std::size_t width, const std::string& lead) {
    std::vector<std::string> lines;
    std::string line = lead;
    const std::size_t leadWidth = ui::displayWidth(lead);
    std::size_t used = leadWidth;
    for (const auto& chunk : chunks) {
        const std::size_t w = ui::displayWidth(chunk);
        if (used + w > width && used > leadWidth) {
            lines.push_back(line);
            line = std::string(leadWidth, ' ');
            used = leadWidth;
        }
        if (used == leadWidth && chunk == " ") continue;  // no leading space after a break
        line += chunk;
        used += w;
    }
    if (used > leadWidth || lines.empty()) lines.push_back(line);
    return lines;
}

std::string hiraganaText(const std::vector<Morpheme>& words) {
    std::string out;
    for (const auto& w : words)
        out += w.reading.empty() || !util::hasKanji(w.surface) ? displaySurface(w.surface) : toHiragana(w.reading);
    return out;
}

}  // namespace furigana
