// Furigana rendering on top of the text analyser's word readings.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "TtsEngine.h"

namespace furigana {

std::string toHiragana(std::string_view katakana);

// "子ども" + "コドモ" → "子(こ)ども": readings only over the kanji part, okurigana stays outside.
// One chunk per word, so a word and its reading are never split across lines.
std::vector<std::string> inlineChunks(const std::vector<Morpheme>& words, const char* readingStyle);
// Packs chunks into lines of at most `width` columns, each line prefixed with `lead`.
std::vector<std::string> wrapChunks(const std::vector<std::string>& chunks, std::size_t width, const std::string& lead = {});
// The whole line written in hiragana.
std::string hiraganaText(const std::vector<Morpheme>& words);

}  // namespace furigana
