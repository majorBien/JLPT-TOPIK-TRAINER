// Loads one JLPT level's data files and turns vocabulary entries into
// JLPT-format multiple-choice questions (with plausible distractors).
//
// Level directory layout (add a new level by dropping in another folder):
//   data/levels/<LEVEL>/vocabulary.json   { "entries":   [VocabEntry...],
//                                           "questions": [Question...] }   (optional 言い換え / 用法 items)
//   data/levels/<LEVEL>/grammar.json      { "questions": [Question...]   }
//   data/levels/<LEVEL>/listening.json    { "items":     [ListeningItem...] }
#pragma once

#include <random>
#include <string>
#include <vector>

#include "Question.h"

enum class VocabQuestionType {
    KanjiReading,    // 漢字読み — how is the underlined word read?
    Orthography,     // 表記     — how is the underlined (kana) word written?
    Context,         // 文脈規定 — which word fits the blank?
    Meaning,         // word → English meaning (trainer style)
    ReverseMeaning,  // English meaning → word (trainer style)
};

const char* toString(VocabQuestionType t);

class QuestionBank {
public:
    static QuestionBank load(const std::string& levelDir, const std::string& level);  // throws

    const std::string& level() const { return level_; }
    const std::vector<VocabEntry>& vocabulary() const { return vocab_; }
    // Hand-written vocabulary items: paraphrase (言い換え類義) and usage (用法).
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
    std::vector<VocabEntry> vocab_;
    std::vector<Question> vocabQuestions_;
    std::vector<Question> grammar_;
    std::vector<ListeningItem> listening_;
};
