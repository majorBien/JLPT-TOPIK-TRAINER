// Core data model shared by every module. A Question is a fully-resolved,
// self-contained multiple-choice item (it can be stored in the mistakes log
// and replayed later without the generator that produced it).
#pragma once

#include <string>
#include <vector>

#include "Json.h"

// Progress groups shown in the Progress screen.
namespace group {
inline constexpr const char* Kanji = "kanji";
inline constexpr const char* Grammar = "grammar";
inline constexpr const char* Listening = "listening";
}  // namespace group

struct Question {
    std::string id;           // stable id, e.g. "vocab:n4v001:reading" or "g012"
    std::string section;      // "vocabulary" | "grammar" | "reading"
    std::string type;         // kanji_reading, orthography, meaning, context, grammar_fill, ...
    std::string instruction;  // JLPT-style instruction line (Japanese)
    std::string instructionEn;
    std::string passage;      // optional reading passage
    std::string prompt;       // may contain [[underlined]] markup
    std::vector<std::string> options;
    int answer = 0;
    std::string note;         // offline hint / translation shown after answering

    // Progress tracking.
    std::string trackKey;     // "vocab:<id>" or "grammar:<point>"
    std::string trackLabel;   // human readable label
    std::string trackGroup;   // group::Kanji | group::Grammar

    const std::string& correctOption() const { return options.at(static_cast<std::size_t>(answer)); }

    json::Value toJson() const;
    static Question fromJson(const json::Value& v);
};

struct VocabEntry {
    std::string id;
    std::string word;     // written form (kanji)
    std::string reading;  // hiragana
    std::string meaning;  // English gloss
    std::string pos;      // noun, verb, i-adj, na-adj, adverb
    std::string example;  // example sentence containing `word` verbatim
    std::vector<std::string> kanjiDistractors;  // optional hand-made look-alikes

    bool exampleContainsWord() const { return !example.empty() && example.find(word) != std::string::npos; }
    static VocabEntry fromJson(const json::Value& v);
};

// One spoken line of a listening script. Speaker: "M" man, "F" woman, "N" narrator.
struct ScriptLine {
    std::string speaker;
    std::string text;
};

// A JLPT listening item (聴解). Types follow the N4 exam parts:
//   task (課題理解), point (ポイント理解), utterance (発話表現), response (即時応答).
// For utterance/response the options are *spoken* (as in the real exam) and
// only revealed in text after answering.
struct ListeningItem {
    std::string id;
    std::string type;
    std::string situation;            // narrator intro (spoken)
    std::string question;             // narrator question (spoken, repeated at the end)
    std::vector<ScriptLine> lines;    // dialogue / monologue
    std::vector<std::string> options;
    int answer = 0;
    std::string optionSpeaker = "M";  // voice used for spoken options
    std::string note;

    bool spokenOptions() const { return type == "utterance" || type == "response"; }
    std::string typeLabel() const;    // "課題理解 (Task)", ...
    std::string transcript() const;   // human readable script (女：… / 男：…)
    Question toQuestion() const;      // snapshot for mistakes / DeepSeek
    static ListeningItem fromJson(const json::Value& v);
};
