// Loads one level's data files and turns vocabulary entries into exam-style
// multiple-choice questions (with plausible distractors).
//
// Level directory layout (add a new level by dropping in another folder):
//   data/<lang>/levels/<LEVEL>/vocabulary.json   { "entries":   [VocabEntry...],
//                                                  "questions": [Question...] }  (optional hand-written items)
//   data/<lang>/levels/<LEVEL>/grammar.json      { "questions": [Question...]   }
//   data/<lang>/levels/<LEVEL>/listening.json    { "items":     [ListeningItem...] }
// Static questions may carry their own "section", "instruction" and "instruction_en".
#pragma once

#include <random>
#include <string>
#include <vector>

#include "Language.h"
#include "Question.h"

enum class VocabQuestionType {
    KanjiReading,    // 漢字読み — how is the underlined word read? (Japanese only)
    Orthography,     // 表記     — how is the underlined (kana) word written? (Japanese only)
    Context,         // 文脈規定 / 빈칸 — which word fits the blank?
    Meaning,         // word → English meaning (trainer style)
    ReverseMeaning,  // English meaning → word (trainer style)
};

const char* toString(VocabQuestionType t);

class QuestionBank {
public:
    static QuestionBank load(const std::string& levelDir, const std::string& level,
                             const LanguageProfile& lang);  // throws

    const std::string& level() const { return level_; }
    const std::vector<VocabEntry>& vocabulary() const { return vocab_; }
    // Hand-written vocabulary items, e.g. JLPT 言い換え／用法 or TOPIK 화제 (31–33).
    const std::vector<Question>& vocabQuestions() const { return vocabQuestions_; }
    const std::vector<Question>& grammarQuestions() const { return grammar_; }
    const std::vector<ListeningItem>& listeningItems() const { return listening_; }
    const VocabEntry* findVocab(const std::string& id) const;

    // Unique grammar points (progress labels) in file order.
    std::vector<std::string> grammarPoints() const;

    Question makeVocabQuestion(const VocabEntry& entry, VocabQuestionType type, std::mt19937& rng) const;

    static std::string vocabTrackKey(const VocabEntry& e) { return "vocab:" + e.id; }
    static std::string grammarTrackKey(const std::string& point) { return "grammar:" + point; }

private:
    std::vector<std::string> pickDistractors(const VocabEntry& entry, VocabQuestionType type,
                                             std::mt19937& rng) const;

    std::string level_;
    const LanguageProfile* lang_ = nullptr;
    std::vector<VocabEntry> vocab_;
    std::vector<Question> vocabQuestions_;
    std::vector<Question> grammar_;
    std::vector<ListeningItem> listening_;
};
