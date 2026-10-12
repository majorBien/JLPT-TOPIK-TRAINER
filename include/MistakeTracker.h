// "My Mistakes" log: every wrongly answered question is stored as a full
// snapshot so it can be replayed later; a correct replay resolves it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Question.h"

struct MistakeRecord {
    Question question;
    std::string userAnswer;   // text of the (last) wrong option chosen
    std::string source;       // module that produced it
    int count = 1;            // how many times it was answered wrongly
    std::int64_t firstSeen = 0;
    std::int64_t lastSeen = 0;
    std::string explanation;  // cached DeepSeek explanation (if requested)
};

class MistakeTracker {
public:
    explicit MistakeTracker(std::string filePath);

    void add(const Question& q, int chosenIndex, const std::string& source);
    void attachExplanation(const std::string& questionId, const std::string& text);
    bool resolve(const std::string& questionId);
    void clear();

    const MistakeRecord* find(const std::string& questionId) const;
    const std::vector<MistakeRecord>& all() const { return records_; }  // most recent first
    std::size_t size() const { return records_.size(); }

    bool save() const;

private:
    void load();
    void sortByRecency();

    std::string path_;
    std::vector<MistakeRecord> records_;
};
