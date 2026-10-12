#include "MistakeTracker.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "Util.h"

MistakeTracker::MistakeTracker(std::string filePath) : path_(std::move(filePath)) { load(); }

void MistakeTracker::load() {
    if (!std::filesystem::exists(path_)) return;
    try {
        const json::Value root = json::parseFile(path_);
        for (const auto& v : root["mistakes"].asArray()) {
            MistakeRecord r;
            r.question = Question::fromJson(v["question"]);
            r.userAnswer = v.getString("user_answer");
            r.source = v.getString("source");
            r.count = static_cast<int>(v.getInt("count", 1));
            r.firstSeen = v.getInt("first_seen");
            r.lastSeen = v.getInt("last_seen");
            r.explanation = v.getString("explanation");
            if (!r.question.id.empty() && !r.question.options.empty()) records_.push_back(std::move(r));
        }
        sortByRecency();
    } catch (const std::exception& e) {
        std::cerr << "Warning: mistakes file unreadable (" << e.what() << "), starting fresh.\n";
    }
}

bool MistakeTracker::save() const {
    json::Value list = json::Value::array();
    for (const auto& r : records_) {
        json::Value v = json::Value::object();
        v.set("question", r.question.toJson());
        v.set("user_answer", r.userAnswer);
        v.set("source", r.source);
        v.set("count", r.count);
        v.set("first_seen", r.firstSeen);
        v.set("last_seen", r.lastSeen);
        if (!r.explanation.empty()) v.set("explanation", r.explanation);
        list.push_back(std::move(v));
    }
    json::Value root = json::Value::object();
    root.set("mistakes", std::move(list));
    return json::writeFileAtomic(path_, root.dump(2));
}

void MistakeTracker::sortByRecency() {
    std::stable_sort(records_.begin(), records_.end(),
                     [](const auto& a, const auto& b) { return a.lastSeen > b.lastSeen; });
}

void MistakeTracker::add(const Question& q, int chosenIndex, const std::string& source) {
    const std::int64_t now = util::nowSeconds();
    const std::string answer =
        chosenIndex >= 0 && chosenIndex < static_cast<int>(q.options.size()) ? q.options[static_cast<std::size_t>(chosenIndex)] : "";
    auto it = std::find_if(records_.begin(), records_.end(), [&](const auto& r) { return r.question.id == q.id; });
    if (it != records_.end()) {
        ++it->count;
        it->lastSeen = now;
        it->userAnswer = answer;
        it->question = q;  // refresh snapshot (option order may differ)
        it->explanation.clear();
    } else {
        records_.push_back({q, answer, source, 1, now, now, {}});
    }
    sortByRecency();
    save();
}

void MistakeTracker::attachExplanation(const std::string& questionId, const std::string& text) {
    for (auto& r : records_) {
        if (r.question.id == questionId) {
            r.explanation = text;
            save();
            return;
        }
    }
}

bool MistakeTracker::resolve(const std::string& questionId) {
    const auto before = records_.size();
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                  [&](const auto& r) { return r.question.id == questionId; }),
                   records_.end());
    if (records_.size() == before) return false;
    save();
    return true;
}

void MistakeTracker::clear() {
    records_.clear();
    save();
}

const MistakeRecord* MistakeTracker::find(const std::string& questionId) const {
    for (const auto& r : records_)
        if (r.question.id == questionId) return &r;
    return nullptr;
}
