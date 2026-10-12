// Everything that differs between study languages: exam name, menu titles,
// instructions, listening part names, UI phrases. Data lives in
// data/<language id>/levels/<LEVEL>/; code paths look strings up here.
#pragma once

#include <map>
#include <string>
#include <vector>

struct LanguageProfile {
    std::string id;          // "ja" | "ko"
    std::string name;        // English name, also used in DeepSeek prompts
    std::string nativeName;  // 日本語 / 한국어
    std::string exam;        // JLPT / TOPIK I

    // Main screen.
    std::vector<std::string> banner;
    std::string tagline;     // "{level}" is replaced with levelLabel(level)
    std::string subtitle;
    std::string goodbye;
    std::string menuTitle;
    std::string levelTitle, settingsTitle, exitTitle;

    // Module titles: key → {native, English}.
    std::map<std::string, std::pair<std::string, std::string>> modules;

    // Levels.
    bool levelsDescending = false;                    // JLPT: N5 (easiest) … N1
    std::map<std::string, std::string> levelNames;    // A1 → "TOPIK I · 1급 (A1)"

    // Quiz screens.
    std::string questionNo;  // 問 / 문제
    std::string correct, incorrect;
    std::string transcriptTitle, passageTitle, listenToAudio;
    std::string whyTitle, translationTitle, resultsTitle;
    std::string cheerPerfect, cheerGood, cheerKeepGoing;
    std::string exampleLabel;  // 例 / 예
    std::string blank;         // generated fill-in blank
    bool furigana = false;
    bool speakListeningQuestion = true;  // JLPT reads the question aloud; TOPIK prints it
    int readingPerSession = 1;

    // Section names used in lists (vocabulary/grammar/reading/listening).
    std::map<std::string, std::string> sections;

    // Vocabulary question instructions: type → {native, English}.
    std::map<std::string, std::pair<std::string, std::string>> vocabInstructions;
    std::pair<std::string, std::string> defaultInstruction;  // static items without their own

    // Listening.
    std::vector<std::string> listeningOrder;                 // exam order of the parts
    std::map<std::string, std::string> listeningTypes;       // type → label
    std::map<std::string, std::string> listeningInstructions;  // type → instruction (fallback: "")
    std::string listeningInstruction;                        // fallback instruction
    std::string listeningPrompt;                             // when an item has no question
    std::string listeningHint;
    std::map<std::string, std::string> speakers;             // M/F/N → label
    std::string speakerSeparator;
    std::vector<std::string> spokenNumbers;
    std::string preparingAudio, playing, finished;

    // Misc phrases.
    std::string wellDone;      // nothing left to review
    std::string reviewDone;
    std::string perfectNoMistakes;

    // DeepSeek.
    std::string exampleStyle;  // how examples in the target language should be written

    std::string levelLabel(const std::string& level) const;
    std::string moduleNative(const std::string& key) const;
    std::string moduleEnglish(const std::string& key) const;
    std::string section(const std::string& key) const;
    std::string listeningTypeLabel(const std::string& type) const;
};

const LanguageProfile& languageProfile(const std::string& id);  // unknown ids → Japanese
const std::vector<std::string>& languageIds();                   // {"ja", "ko"}
