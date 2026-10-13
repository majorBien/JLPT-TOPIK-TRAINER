// "My Mistakes" log: every wrongly answered question is stored as a full
// snapshot so it can be replayed later. Mistakes are reviewed on a fixed
// spaced-repetition ladder (1 → 3 → 7 → 14 → 30 days): a correct answer when
// the item is due moves it one step up, a correct answer at the last step
// retires it, and a wrong answer anywhere sends it back to the start.
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
    int step = 0;             // 0 = just missed; 1…5 = passed reviews on the ladder
    std::int64_t due = 0;     // next review time (0 = now)

    bool isDue(std::int64_t now) const { return due <= now; }
};

enum class MistakeReview { NotTracked, NotDue, Advanced, Retired };

class MistakeTracker {
public:
    explicit MistakeTracker(std::string filePath);

    void add(const Question& q, int chosenIndex, const std::string& source);
    void attachExplanation(const std::string& questionId, const std::string& text);
    // A correct answer to a saved mistake: advances it if due, retires it after the last step.
    MistakeReview reviewCorrect(const std::string& questionId);
    std::size_t dueCount(std::int64_t now) const;
    std::int64_t nextDue() const;  // earliest due time (0 if empty)

    static constexpr int kSteps = 5;
    static std::int64_t intervalForStep(int step);  // seconds until the review after `step`
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
