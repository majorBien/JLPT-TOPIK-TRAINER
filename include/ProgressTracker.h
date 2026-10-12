// Per-item answer statistics + session history, persisted to progress.json.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Question.h"

struct ItemStats {
    std::string group;   // group::Kanji | group::Grammar | group::Listening
    std::string label;
    int correct = 0;
    int wrong = 0;
    int streak = 0;      // consecutive correct answers
    std::int64_t lastSeen = 0;

    int attempts() const { return correct + wrong; }
    double accuracy() const { return attempts() ? static_cast<double>(correct) / attempts() : 0.0; }
};

struct SessionRecord {
    std::int64_t timestamp = 0;
    std::string module;
    int correct = 0;
    int total = 0;
};

enum class Mastery { NotStarted, NeedsReview, Mastered };

class ProgressTracker {
public:
    explicit ProgressTracker(std::string filePath);

    void record(const Question& q, bool correct);
    void record(const std::string& key, const std::string& group, const std::string& label, bool correct);
    void recordSession(const std::string& module, int correct, int total);

    const ItemStats* find(const std::string& key) const;
    const std::map<std::string, ItemStats>& items() const { return items_; }
    const std::vector<SessionRecord>& sessions() const { return sessions_; }

    // Mastered = answered at least 3 times, ≥80% accuracy and the last two were correct.
    static Mastery mastery(const ItemStats* stats);

    // Selection weight for adaptive sessions: unseen and weak items come up more often.
    double weightFor(const std::string& key) const;

    bool save() const;

private:
    void load();

    std::string path_;
    std::map<std::string, ItemStats> items_;
    std::vector<SessionRecord> sessions_;
};
