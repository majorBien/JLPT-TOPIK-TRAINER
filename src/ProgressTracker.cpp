#include "ProgressTracker.h"

#include <filesystem>
#include <iostream>

#include "Util.h"

namespace {
constexpr std::size_t kMaxSessionHistory = 200;
}

ProgressTracker::ProgressTracker(std::string filePath) : path_(std::move(filePath)) { load(); }

void ProgressTracker::load() {
    if (!std::filesystem::exists(path_)) return;
    try {
        const json::Value root = json::parseFile(path_);
        for (const auto& [key, v] : root["items"].asObject()) {
            ItemStats s;
            s.group = v.getString("group");
            s.label = v.getString("label");
            s.correct = static_cast<int>(v.getInt("correct"));
            s.wrong = static_cast<int>(v.getInt("wrong"));
            s.streak = static_cast<int>(v.getInt("streak"));
            s.lastSeen = v.getInt("last_seen");
            items_[key] = s;
        }
        for (const auto& v : root["sessions"].asArray()) {
            sessions_.push_back({v.getInt("timestamp"), v.getString("module"),
                                 static_cast<int>(v.getInt("correct")), static_cast<int>(v.getInt("total"))});
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: progress file unreadable (" << e.what() << "), starting fresh.\n";
    }
}

bool ProgressTracker::save() const {
    json::Value root = json::Value::object();
    json::Value items = json::Value::object();
    for (const auto& [key, s] : items_) {
        json::Value v = json::Value::object();
        v.set("group", s.group);
        v.set("label", s.label);
        v.set("correct", s.correct);
        v.set("wrong", s.wrong);
        v.set("streak", s.streak);
        v.set("last_seen", s.lastSeen);
        items.set(key, std::move(v));
    }
    json::Value sessions = json::Value::array();
    for (const auto& s : sessions_) {
        json::Value v = json::Value::object();
        v.set("timestamp", s.timestamp);
        v.set("module", s.module);
        v.set("correct", s.correct);
        v.set("total", s.total);
        sessions.push_back(std::move(v));
    }
    root.set("items", std::move(items));
    root.set("sessions", std::move(sessions));
    return json::writeFileAtomic(path_, root.dump(2));
}

void ProgressTracker::record(const Question& q, bool correct) {
    if (!q.trackKey.empty()) record(q.trackKey, q.trackGroup, q.trackLabel, correct);
}

void ProgressTracker::record(const std::string& key, const std::string& group, const std::string& label,
                             bool correct) {
    ItemStats& s = items_[key];
    s.group = group;
    s.label = label;
    if (correct) {
        ++s.correct;
        ++s.streak;
    } else {
        ++s.wrong;
        s.streak = 0;
    }
    s.lastSeen = util::nowSeconds();
    save();
}

void ProgressTracker::recordSession(const std::string& module, int correct, int total) {
    if (total <= 0) return;
    sessions_.push_back({util::nowSeconds(), module, correct, total});
    if (sessions_.size() > kMaxSessionHistory)
        sessions_.erase(sessions_.begin(), sessions_.end() - static_cast<std::ptrdiff_t>(kMaxSessionHistory));
    save();
}

const ItemStats* ProgressTracker::find(const std::string& key) const {
    const auto it = items_.find(key);
    return it == items_.end() ? nullptr : &it->second;
}

Mastery ProgressTracker::mastery(const ItemStats* s) {
    if (!s || s->attempts() == 0) return Mastery::NotStarted;
    if (s->attempts() >= 3 && s->accuracy() >= 0.8 && s->streak >= 2) return Mastery::Mastered;
    return Mastery::NeedsReview;
}

double ProgressTracker::weightFor(const std::string& key) const {
    const ItemStats* s = find(key);
    switch (mastery(s)) {
        case Mastery::NotStarted: return 3.0;
        case Mastery::NeedsReview: return 2.0 + 2.0 * (1.0 - s->accuracy());
        case Mastery::Mastered: return 0.5;
    }
    return 1.0;
}
