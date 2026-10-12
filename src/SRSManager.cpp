#include "SRSManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

#include "Json.h"

namespace {
constexpr std::int64_t kDay = 86400;
constexpr std::int64_t kRelearnDelay = 10 * 60;
constexpr double kMinEase = 1.3;
constexpr int kMaxIntervalDays = 365;
}  // namespace

const char* toString(Grade g) {
    switch (g) {
        case Grade::Again: return "Again";
        case Grade::Hard: return "Hard";
        case Grade::Good: return "Good";
        case Grade::Easy: return "Easy";
    }
    return "?";
}

SRSManager::SRSManager(std::string filePath) : path_(std::move(filePath)) { load(); }

void SRSManager::load() {
    if (!std::filesystem::exists(path_)) return;
    try {
        const json::Value root = json::parseFile(path_);
        for (const auto& [id, v] : root["cards"].asObject()) {
            SrsCard c;
            c.id = id;
            c.ease = v.getNumber("ease", 2.5);
            c.intervalDays = static_cast<int>(v.getInt("interval"));
            c.repetitions = static_cast<int>(v.getInt("repetitions"));
            c.lapses = static_cast<int>(v.getInt("lapses"));
            c.due = v.getInt("due");
            c.lastReview = v.getInt("last_review");
            cards_[id] = c;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: SRS file unreadable (" << e.what() << "), starting fresh.\n";
    }
}

bool SRSManager::save() const {
    json::Value cards = json::Value::object();
    for (const auto& [id, c] : cards_) {
        json::Value v = json::Value::object();
        v.set("ease", std::round(c.ease * 100.0) / 100.0);
        v.set("interval", c.intervalDays);
        v.set("repetitions", c.repetitions);
        v.set("lapses", c.lapses);
        v.set("due", c.due);
        v.set("last_review", c.lastReview);
        cards.set(id, std::move(v));
    }
    json::Value root = json::Value::object();
    root.set("cards", std::move(cards));
    return json::writeFileAtomic(path_, root.dump(2));
}

SrsCard SRSManager::schedule(const SrsCard& card, Grade grade, std::int64_t now) {
    SrsCard c = card;
    const int q = static_cast<int>(grade);
    c.lastReview = now;

    if (grade == Grade::Again) {
        c.repetitions = 0;
        c.intervalDays = 0;
        if (!card.isNew()) ++c.lapses;
        c.ease = std::max(kMinEase, c.ease - 0.2);
        c.due = now + kRelearnDelay;
        return c;
    }

    // SM-2 ease update: Hard −0.14, Good ±0, Easy +0.10.
    c.ease = std::max(kMinEase, c.ease + (0.1 - (5 - q) * (0.08 + (5 - q) * 0.02)));
    ++c.repetitions;

    double next;
    if (c.repetitions == 1) next = grade == Grade::Easy ? 3 : 1;
    else if (c.repetitions == 2) next = grade == Grade::Hard ? 3 : grade == Grade::Easy ? 8 : 6;
    else {
        const double base = std::max(1, card.intervalDays);
        next = grade == Grade::Hard ? base * 1.2 : base * c.ease * (grade == Grade::Easy ? 1.3 : 1.0);
    }
    c.intervalDays = std::clamp(static_cast<int>(std::lround(next)), 1, kMaxIntervalDays);
    if (c.intervalDays <= card.intervalDays && c.repetitions > 2) c.intervalDays = card.intervalDays + 1;
    c.due = now + c.intervalDays * kDay;
    return c;
}

Grade SRSManager::gradeFromAnswer(bool correct, double seconds) {
    if (!correct) return Grade::Again;
    if (seconds < 4.0) return Grade::Easy;
    if (seconds < 12.0) return Grade::Good;
    return Grade::Hard;
}

std::vector<std::string> SRSManager::dueCards(const std::vector<std::string>& deck, std::int64_t now,
                                              std::size_t limit) const {
    std::vector<const SrsCard*> due;
    for (const auto& id : deck) {
        const SrsCard* c = find(id);
        if (c && !c->isNew() && c->due <= now) due.push_back(c);
    }
    std::sort(due.begin(), due.end(), [](auto* a, auto* b) { return a->due < b->due; });
    std::vector<std::string> out;
    for (std::size_t i = 0; i < due.size() && i < limit; ++i) out.push_back(due[i]->id);
    return out;
}

std::vector<std::string> SRSManager::newCards(const std::vector<std::string>& deck, std::size_t limit) const {
    std::vector<std::string> out;
    for (const auto& id : deck) {
        if (out.size() >= limit) break;
        const SrsCard* c = find(id);
        if (!c || c->isNew()) out.push_back(id);
    }
    return out;
}

const SrsCard* SRSManager::find(const std::string& id) const {
    const auto it = cards_.find(id);
    return it == cards_.end() ? nullptr : &it->second;
}

SrsCard SRSManager::card(const std::string& id) const {
    if (const SrsCard* c = find(id)) return *c;
    SrsCard fresh;
    fresh.id = id;
    return fresh;
}

SrsCard SRSManager::review(const std::string& id, Grade grade, std::int64_t now) {
    SrsCard updated = schedule(card(id), grade, now);
    cards_[id] = updated;
    save();
    return updated;
}

SrsStats SRSManager::stats(const std::vector<std::string>& deck, std::int64_t now) const {
    SrsStats s;
    s.total = static_cast<int>(deck.size());
    for (const auto& id : deck) {
        const SrsCard* c = find(id);
        if (!c || c->isNew()) { ++s.unseen; continue; }
        if (c->intervalDays == 0) ++s.learning;
        else if (c->isMature()) ++s.mature;
        else ++s.young;
        if (c->due <= now) ++s.dueNow;
        else if (s.nextDue == 0 || c->due < s.nextDue) s.nextDue = c->due;
    }
    return s;
}
