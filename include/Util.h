// Small string / time / random helpers shared across modules.
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace util {

std::string trim(std::string_view s);
std::string toLower(std::string_view s);
bool startsWith(std::string_view s, std::string_view prefix);
std::string replaceAll(std::string s, std::string_view from, std::string_view to);
std::string expandHome(const std::string& path);
std::string shellQuote(const std::string& s);              // POSIX single-quote escaping
bool commandExists(const std::string& name);              // looks up $PATH
bool isKanji(std::uint32_t codepoint);                    // CJK ideographs and 々
bool hasKanji(std::string_view s);                        // any kanji in a UTF-8 string

std::int64_t nowSeconds();
std::string formatDate(std::int64_t timestamp);         // "2026-10-12 14:03"
std::string formatInterval(std::int64_t seconds);       // "10m", "3d", "2mo"
std::string formatPercent(double ratio);                // "83%"

// Weighted sampling without replacement. Returns up to k distinct indices.
std::vector<std::size_t> weightedSample(const std::vector<double>& weights, std::size_t k,
                                        std::mt19937& rng);

}  // namespace util
