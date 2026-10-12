#include "QuestionBank.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <stdexcept>

#include "Util.h"

namespace fs = std::filesystem;

const char* toString(VocabQuestionType t) {
    switch (t) {
        case VocabQuestionType::KanjiReading: return "kanji_reading";
        case VocabQuestionType::Orthography: return "orthography";
        case VocabQuestionType::Context: return "context";
        case VocabQuestionType::Meaning: return "meaning";
        case VocabQuestionType::ReverseMeaning: return "reverse_meaning";
    }
    return "unknown";
}

namespace {

// ---- Kana helpers for realistic reading distractors ------------------------

std::vector<std::string> splitChars(const std::string& s) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        out.push_back(s.substr(i, len));
        i += len;
    }
    return out;
}

std::string joinChars(const std::vector<std::string>& chars) {
    std::string out;
    for (const auto& c : chars) out += c;
    return out;
}

bool isOneOf(const std::string& c, std::initializer_list<const char*> set) {
    return std::any_of(set.begin(), set.end(), [&](const char* s) { return c == s; });
}

const std::vector<std::pair<std::string, std::string>>& dakutenPairs() {
    static const std::vector<std::pair<std::string, std::string>> pairs = {
        {"か", "が"}, {"き", "ぎ"}, {"く", "ぐ"}, {"け", "げ"}, {"こ", "ご"}, {"さ", "ざ"}, {"し", "じ"},
        {"す", "ず"}, {"せ", "ぜ"}, {"そ", "ぞ"}, {"た", "だ"}, {"て", "で"}, {"と", "ど"}, {"は", "ば"},
        {"ひ", "び"}, {"ふ", "ぶ"}, {"へ", "べ"}, {"ほ", "ぼ"}, {"は", "ぱ"}, {"ひ", "ぴ"}, {"ふ", "ぷ"},
        {"へ", "ぺ"}, {"ほ", "ぽ"}};
    return pairs;
}

// Single-step "near miss" mutations, mimicking typical JLPT reading traps:
// voicing (か/が), vowel length (こ/こう), gemination (っ) and small kana (ょ/よ).
std::vector<std::string> readingMutations(const std::string& reading) {
    const auto chars = splitChars(reading);
    std::set<std::string> out;
    auto emit = [&](std::vector<std::string> v) { out.insert(joinChars(v)); };

    for (std::size_t i = 0; i < chars.size(); ++i) {
        const std::string& c = chars[i];
        const std::string next = i + 1 < chars.size() ? chars[i + 1] : "";

        for (const auto& [plain, voiced] : dakutenPairs()) {
            if (c == plain || c == voiced) {
                auto v = chars;
                v[i] = c == plain ? voiced : plain;
                emit(v);
            }
        }

        const bool longCapable = isOneOf(c, {"こ", "そ", "と", "の", "ほ", "も", "よ", "ろ", "ご", "ぞ", "ど", "ぼ",
                                             "ぽ", "ょ", "ゅ", "く", "す", "つ", "ふ", "ゆ", "る", "ぐ", "ず", "ぶ"});
        if (longCapable && next == "う") {
            auto v = chars;
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(i) + 1);
            emit(v);
        } else if (longCapable) {
            auto v = chars;
            v.insert(v.begin() + static_cast<std::ptrdiff_t>(i) + 1, "う");
            emit(v);
        }
        if (next == "い" && isOneOf(c, {"け", "せ", "て", "ね", "へ", "め", "れ", "げ", "ぜ", "で", "べ"})) {
            auto v = chars;
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(i) + 1);
            emit(v);
        }

        if (c == "っ") {
            auto v = chars;
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(i));
            emit(v);
        } else if (i > 0 && chars[i - 1] != "っ" && chars[i - 1] != "ん" &&
                   isOneOf(c, {"か", "き", "く", "け", "こ", "さ", "し", "す", "せ", "そ", "た", "ち", "つ", "て",
                               "と", "ぱ", "ぴ", "ぷ", "ぺ", "ぽ"})) {
            auto v = chars;
            v.insert(v.begin() + static_cast<std::ptrdiff_t>(i), "っ");
            emit(v);
        }

        static const std::vector<std::pair<std::string, std::string>> small = {
            {"ゃ", "や"}, {"ゅ", "ゆ"}, {"ょ", "よ"}};
        for (const auto& [s, big] : small) {
            if (c == s || c == big) {
                if (c == big && i == 0) continue;
                auto v = chars;
                v[i] = c == s ? big : s;
                emit(v);
            }
        }
    }
    out.erase(reading);
    return {out.begin(), out.end()};
}

bool sharesCharacter(const std::string& a, const std::string& b) {
    const auto ca = splitChars(a);
    const auto cb = splitChars(b);
    return std::any_of(ca.begin(), ca.end(), [&](const std::string& c) {
        return c.size() > 1 && std::find(cb.begin(), cb.end(), c) != cb.end();
    });
}

Question baseGrammarQuestion(const json::Value& v) {
    Question q;
    q.id = v.getString("id");
    q.type = v.getString("type", "grammar_fill");
    q.section = q.type == "reading" ? "reading" : "grammar";
    q.passage = v.getString("passage");
    q.prompt = v.getString("prompt");
    q.options = v.getStringArray("options");
    q.answer = static_cast<int>(v.getInt("answer"));
    q.note = v.getString("note");

    if (q.type == "paraphrase") {
        q.section = "vocabulary";
        q.instruction = "＿＿＿の 文と だいたい おなじ いみの 文が あります。いちばん いい ものを えらんで ください。";
        q.instructionEn = "Which sentence has roughly the same meaning as the underlined part?";
    } else if (q.type == "usage") {
        q.section = "vocabulary";
        q.instruction = "つぎの ことばの つかいかたで いちばん いい ものを えらんで ください。";
        q.instructionEn = "Which sentence uses the word correctly?";
    } else if (q.type == "sentence_order") {
        q.instruction = "★に 入る ものは どれですか。";
        q.instructionEn = "Put the pieces in order. Which one goes in the ★ slot?";
    } else if (q.type == "reading") {
        q.instruction = "つぎの 文章を 読んで、質問に 答えて ください。";
        q.instructionEn = "Read the passage and answer the question.";
    } else {
        q.instruction = "つぎの 文の （　　）に 何を 入れますか。";
        q.instructionEn = "Choose the best option for the blank.";
    }
    q.instruction = v.getString("instruction", q.instruction);
    q.instructionEn = v.getString("instruction_en", q.instructionEn);

    if (q.section == "vocabulary") {
        // Counted towards the vocabulary entry the item is about.
        q.trackKey = "vocab:" + v.getString("word_id", q.id);
        q.trackLabel = v.getString("word", q.id);
        q.trackGroup = group::Kanji;
        return q;
    }
    const std::string point = v.getString("grammar_point", q.type);
    q.trackKey = QuestionBank::grammarTrackKey(point);
    q.trackLabel = point;
    q.trackGroup = group::Grammar;
    return q;
}

}  // namespace

QuestionBank QuestionBank::load(const std::string& levelDir, const std::string& level) {
    QuestionBank bank;
    bank.level_ = level;

    const fs::path dir(levelDir);
    const fs::path vocabPath = dir / "vocabulary.json";
    const fs::path grammarPath = dir / "grammar.json";
    const fs::path listeningPath = dir / "listening.json";

    if (fs::exists(vocabPath)) {
        const json::Value root = json::parseFile(vocabPath.string());
        for (const auto& item : root["entries"].asArray()) {
            VocabEntry e = VocabEntry::fromJson(item);
            if (!e.id.empty() && !e.word.empty() && !e.reading.empty()) bank.vocab_.push_back(std::move(e));
        }
        for (const auto& item : root["questions"].asArray()) {
            Question q = baseGrammarQuestion(item);
            if (q.options.size() >= 2 && q.answer >= 0 && q.answer < static_cast<int>(q.options.size()))
                bank.vocabQuestions_.push_back(std::move(q));
        }
    }
    if (fs::exists(grammarPath)) {
        const json::Value root = json::parseFile(grammarPath.string());
        for (const auto& item : root["questions"].asArray()) {
            Question q = baseGrammarQuestion(item);
            if (q.options.size() >= 2 && q.answer >= 0 && q.answer < static_cast<int>(q.options.size()))
                bank.grammar_.push_back(std::move(q));
        }
    }
    if (fs::exists(listeningPath)) {
        const json::Value root = json::parseFile(listeningPath.string());
        for (const auto& item : root["items"].asArray()) {
            ListeningItem li = ListeningItem::fromJson(item);
            if (li.options.size() >= 2 && li.answer >= 0 && li.answer < static_cast<int>(li.options.size()))
                bank.listening_.push_back(std::move(li));
        }
    }
    if (bank.vocab_.size() < 4 && bank.grammar_.empty() && bank.listening_.empty())
        throw std::runtime_error("level " + level + " has no usable data in " + levelDir);
    return bank;
}

const VocabEntry* QuestionBank::findVocab(const std::string& id) const {
    for (const auto& e : vocab_)
        if (e.id == id) return &e;
    return nullptr;
}

std::vector<std::string> QuestionBank::grammarPoints() const {
    std::vector<std::string> points;
    for (const auto& q : grammar_)
        if (std::find(points.begin(), points.end(), q.trackLabel) == points.end()) points.push_back(q.trackLabel);
    return points;
}

std::vector<std::string> QuestionBank::pickDistractors(const VocabEntry& entry, VocabQuestionType type,
                                                       std::mt19937& rng) const {
    auto field = [type](const VocabEntry& e) -> const std::string& {
        switch (type) {
            case VocabQuestionType::KanjiReading: return e.reading;
            case VocabQuestionType::Meaning: return e.meaning;
            default: return e.word;
        }
    };
    const std::string& correct = field(entry);
    std::vector<std::string> picked;
    auto take = [&](const std::string& candidate) {
        if (picked.size() >= 3 || candidate.empty() || candidate == correct) return;
        if (std::find(picked.begin(), picked.end(), candidate) != picked.end()) return;
        picked.push_back(candidate);
    };

    // 1) Hand-made / algorithmic near misses.
    if (type == VocabQuestionType::KanjiReading) {
        auto mutations = readingMutations(entry.reading);
        std::shuffle(mutations.begin(), mutations.end(), rng);
        for (const auto& m : mutations) take(m);
    } else if (type == VocabQuestionType::Orthography) {
        auto handMade = entry.kanjiDistractors;
        std::shuffle(handMade.begin(), handMade.end(), rng);
        for (const auto& d : handMade) take(d);
    }

    // 2) Other entries: same part of speech (and shared kanji for orthography) first.
    std::vector<const VocabEntry*> pool;
    for (const auto& e : vocab_)
        if (e.id != entry.id) pool.push_back(&e);
    std::shuffle(pool.begin(), pool.end(), rng);
    auto score = [&](const VocabEntry* e) {
        int s = 0;
        if (e->pos == entry.pos) s += 2;
        if (type == VocabQuestionType::Orthography && sharesCharacter(e->word, entry.word)) s += 3;
        return s;
    };
    std::stable_sort(pool.begin(), pool.end(), [&](auto* a, auto* b) { return score(a) > score(b); });
    for (const auto* e : pool) take(field(*e));
    return picked;
}

Question QuestionBank::makeVocabQuestion(const VocabEntry& entry, VocabQuestionType type,
                                         std::mt19937& rng) const {
    // Words written in kana only (なかなか, アルバイト) have nothing to read or write in kanji.
    if (!util::hasKanji(entry.word) &&
        (type == VocabQuestionType::KanjiReading || type == VocabQuestionType::Orthography))
        type = entry.exampleContainsWord() ? VocabQuestionType::Context : VocabQuestionType::Meaning;
    // Context/orthography need the word inside the example sentence.
    if ((type == VocabQuestionType::Context || type == VocabQuestionType::Orthography) && !entry.exampleContainsWord())
        type = util::hasKanji(entry.word) ? VocabQuestionType::KanjiReading : VocabQuestionType::Meaning;

    Question q;
    q.id = "vocab:" + entry.id + ":" + toString(type);
    q.section = "vocabulary";
    q.type = toString(type);
    q.trackKey = vocabTrackKey(entry);
    q.trackLabel = entry.word;
    q.trackGroup = group::Kanji;
    q.note = entry.word + "（" + entry.reading + "） — " + entry.meaning;
    if (!entry.example.empty()) q.note += "\n例: " + entry.example;

    const std::string underlined =
        entry.exampleContainsWord() ? util::replaceAll(entry.example, entry.word, "[[" + entry.word + "]]")
                                    : "[[" + entry.word + "]]";
    std::string correct;
    switch (type) {
        case VocabQuestionType::KanjiReading:
            q.instruction = "＿＿＿の ことばは ひらがなで どう かきますか。";
            q.instructionEn = "How is the underlined word read?";
            q.prompt = underlined;
            correct = entry.reading;
            break;
        case VocabQuestionType::Orthography:
            q.instruction = "＿＿＿の ことばは どう かきますか。";
            q.instructionEn = "How is the underlined word written?";
            q.prompt = util::replaceAll(entry.example, entry.word, "[[" + entry.reading + "]]");
            correct = entry.word;
            break;
        case VocabQuestionType::Context:
            q.instruction = "（　　）に なにを いれますか。いちばん いい ものを えらんで ください。";
            q.instructionEn = "Which word best fills the blank?";
            q.prompt = util::replaceAll(entry.example, entry.word, "（　　　）");
            correct = entry.word;
            break;
        case VocabQuestionType::Meaning:
            q.instruction = "＿＿＿の ことばの いみは なんですか。";
            q.instructionEn = "What does the underlined word mean?";
            q.prompt = underlined;
            correct = entry.meaning;
            break;
        case VocabQuestionType::ReverseMeaning:
            q.instruction = "つぎの いみの ことばは どれですか。";
            q.instructionEn = "Which word has this meaning?";
            q.prompt = "“" + entry.meaning + "”";
            correct = entry.word;
            break;
    }

    q.options = pickDistractors(entry, type, rng);
    q.options.push_back(correct);
    std::shuffle(q.options.begin(), q.options.end(), rng);
    q.answer = static_cast<int>(std::find(q.options.begin(), q.options.end(), correct) - q.options.begin());
    return q;
}
