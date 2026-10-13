#include "Util.h"

#include "Platform.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <numeric>


namespace util {

std::string trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(first, last - first + 1));
}

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.substr(0, prefix.size()) == prefix;
}

std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string expandHome(const std::string& path) {
    if (!startsWith(path, "~/") && !startsWith(path, "~\\")) return path;
    const std::string home = platform::homeDir();
    return home.empty() ? path : home + path.substr(1);
}

std::string shellQuote(const std::string& s) { return platform::quote(s); }

bool commandExists(const std::string& name) { return platform::commandExists(name); }

bool isKanji(std::uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || cp == 0x3005;  // 々
}

bool hasKanji(std::string_view s) {
    // Every kanji in the ranges above is a 3-byte UTF-8 sequence.
    for (std::size_t i = 0; i + 2 < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if ((c & 0xF0) != 0xE0) continue;
        const std::uint32_t cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                                 (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
        if (isKanji(cp)) return true;
        i += 2;
    }
    return false;
}

std::int64_t nowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string formatDate(std::int64_t timestamp) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string formatInterval(std::int64_t seconds) {
    if (seconds <= 0) return "now";
    if (seconds < 3600) return std::to_string(std::max<std::int64_t>(1, seconds / 60)) + "m";
    if (seconds < 86400) return std::to_string(seconds / 3600) + "h";
    const std::int64_t days = seconds / 86400;
    if (days < 60) return std::to_string(days) + "d";
    if (days < 365) return std::to_string(days / 30) + "mo";
    return std::to_string(days / 365) + "y";
}

std::string formatPercent(double ratio) {
    return std::to_string(static_cast<int>(ratio * 100.0 + 0.5)) + "%";
}

std::vector<std::size_t> weightedSample(const std::vector<double>& weights, std::size_t k,
                                        std::mt19937& rng) {
    // Efraimidis–Spirakis: key = u^(1/w), take the k largest keys.
    std::uniform_real_distribution<double> uni(1e-12, 1.0);
    std::vector<std::pair<double, std::size_t>> keyed;
    keyed.reserve(weights.size());
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const double w = std::max(weights[i], 1e-6);
        keyed.emplace_back(std::pow(uni(rng), 1.0 / w), i);
    }
    k = std::min(k, keyed.size());
    std::partial_sort(keyed.begin(), keyed.begin() + static_cast<std::ptrdiff_t>(k), keyed.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < k; ++i) out.push_back(keyed[i].second);
    return out;
}

}  // namespace util
