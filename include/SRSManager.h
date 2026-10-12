// Spaced-repetition engine (SM-2 variant, Anki-like) persisted to srs.json.
//
// Grades: Again (wrong) resets the card to a 10-minute relearning step and
// lowers its ease; Hard/Good/Easy grow the interval by the ease factor.
// For multiple choice the grade is derived from correctness + answer time,
// and the user may override it (h/g/e) like Anki's answer buttons.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class Grade { Again = 1, Hard = 3, Good = 4, Easy = 5 };

const char* toString(Grade g);

struct SrsCard {
    std::string id;
    double ease = 2.5;
    int intervalDays = 0;
    int repetitions = 0;      // successful reviews in a row
    int lapses = 0;
    std::int64_t due = 0;
    std::int64_t lastReview = 0;

    bool isNew() const { return lastReview == 0; }
    bool isMature() const { return intervalDays >= 21; }
};

struct SrsStats {
    int total = 0;       // cards known to the deck (= vocabulary size)
    int unseen = 0;
    int learning = 0;    // interval < 1 day (relearning)
    int young = 0;       // 1..20 days
    int mature = 0;      // >= 21 days
    int dueNow = 0;
    std::int64_t nextDue = 0;
};

class SRSManager {
public:
    explicit SRSManager(std::string filePath);

    // Pure scheduling function: what the card would look like after `grade`.
    static SrsCard schedule(const SrsCard& card, Grade grade, std::int64_t now);
    static Grade gradeFromAnswer(bool correct, double seconds);

    std::vector<std::string> dueCards(const std::vector<std::string>& deck, std::int64_t now, std::size_t limit) const;
    std::vector<std::string> newCards(const std::vector<std::string>& deck, std::size_t limit) const;

    SrsCard card(const std::string& id) const;  // existing state or a fresh card
    const SrsCard* find(const std::string& id) const;
    SrsCard review(const std::string& id, Grade grade, std::int64_t now);

    SrsStats stats(const std::vector<std::string>& deck, std::int64_t now) const;
    bool save() const;

private:
    void load();

    std::string path_;
    std::map<std::string, SrsCard> cards_;
};
