#include "modules/ProgressModule.h"

#include <algorithm>
#include <iostream>

#include "Terminal.h"
#include "Util.h"

namespace {

struct Row {
    std::string label;
    std::string extra;      // reading / type
    const ItemStats* stats;
    const SrsCard* card;
    Mastery mastery;
};

std::string accuracyCell(const ItemStats* s) {
    if (!s || s->attempts() == 0) return ui::paint("—", ui::style::Gray);
    const double a = s->accuracy();
    const char* color = a >= 0.8 ? ui::style::Green : a >= 0.5 ? ui::style::Yellow : ui::style::Red;
    return ui::paint(util::formatPercent(a), color) + ui::paint(" (" + std::to_string(s->correct) + "/" + std::to_string(s->attempts()) + ")", ui::style::Gray);
}

void printSplitBar(const std::string& title, const std::vector<Row>& rows) {
    int mastered = 0, review = 0, fresh = 0;
    for (const auto& r : rows) {
        if (r.mastery == Mastery::Mastered) ++mastered;
        else if (r.mastery == Mastery::NeedsReview) ++review;
        else ++fresh;
    }
    const double total = rows.empty() ? 1.0 : static_cast<double>(rows.size());
    const std::size_t w = 34;
    std::vector<std::string> lines = {
        ui::padRight("✔ Mastered", 16) + ui::progressBar(mastered / total, w, ui::style::BrightGreen) + ui::padLeft(std::to_string(mastered), 5),
        ui::padRight("↻ Needs review", 16) + ui::progressBar(review / total, w, ui::style::BrightYellow) + ui::padLeft(std::to_string(review), 5),
        ui::padRight("· Not started", 16) + ui::progressBar(fresh / total, w, ui::style::Blue) + ui::padLeft(std::to_string(fresh), 5),
    };
    ui::printBox(lines, title + "  (" + std::to_string(rows.size()) + ")", ui::style::Cyan);
}

std::vector<Row> kanjiRows(const AppContext& ctx) {
    std::vector<Row> rows;
    for (const auto& e : ctx.bank.vocabulary()) {
        const ItemStats* s = ctx.progress.find(QuestionBank::vocabTrackKey(e));
        const SrsCard* c = ctx.srs.find(e.id);
        Mastery m = ProgressTracker::mastery(s);
        if (c && !c->isNew()) {
            if (c->isMature()) m = Mastery::Mastered;
            else if (m == Mastery::NotStarted) m = Mastery::NeedsReview;
        }
        rows.push_back({e.word, e.reading, s, c, m});
    }
    return rows;
}

std::vector<Row> grammarRows(const AppContext& ctx) {
    std::vector<Row> rows;
    for (const auto& point : ctx.bank.grammarPoints()) {
        const ItemStats* s = ctx.progress.find(QuestionBank::grammarTrackKey(point));
        rows.push_back({point, {}, s, nullptr, ProgressTracker::mastery(s)});
    }
    return rows;
}

void sortWeakestFirst(std::vector<const Row*>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row* a, const Row* b) {
        const double aa = a->stats ? a->stats->accuracy() : 1.0;
        const double bb = b->stats ? b->stats->accuracy() : 1.0;
        return aa < bb;
    });
}

std::string srsCell(const SrsCard* c) {
    if (!c || c->isNew()) return ui::paint("—", ui::style::Gray);
    const auto now = util::nowSeconds();
    const std::string interval = c->intervalDays ? std::to_string(c->intervalDays) + "d" : "learning";
    const std::string due = c->due <= now ? ui::paint(" due", ui::style::BrightMagenta) : ui::paint(" in " + util::formatInterval(c->due - now), ui::style::Gray);
    return interval + due;
}

}  // namespace

void ProgressModule::run(AppContext& ctx) {
    for (;;) {
        showOverview(ctx);
        std::cout << "   " << ui::paint("[k]", ui::style::BrightMagenta) << " all vocabulary   "
                  << ui::paint("[g]", ui::style::BrightMagenta) << " all grammar   "
                  << ui::paint("[b]", ui::style::BrightMagenta) << " back\n\n";
        const auto in = ui::readLine("");
        if (!in) return;
        const std::string cmd = util::toLower(*in);
        if (cmd == "k") {
            ui::printHeader(ctx.lang.moduleNative("vocabulary") + "  Vocabulary", ctx.levelName());
            showKanji(ctx, true);
            ui::waitForEnter();
        } else if (cmd == "g") {
            ui::printHeader(ctx.lang.moduleNative("grammar") + "  Grammar & Reading", ctx.levelName());
            showGrammar(ctx, true);
            ui::waitForEnter();
        } else {
            return;
        }
    }
}

void ProgressModule::showOverview(AppContext& ctx) const {
    ui::printHeader(nativeTitle(ctx) + "  Progress", ctx.levelName() + " · statistics");
    std::cout << '\n';

    int answers = 0, correct = 0;
    for (const auto& [key, s] : ctx.progress.items()) {
        answers += s.attempts();
        correct += s.correct;
    }
    const auto& sessions = ctx.progress.sessions();
    const auto srs = ctx.srs.stats(ctx.vocabDeck(), util::nowSeconds());
    const double acc = answers ? static_cast<double>(correct) / answers : 0.0;
    ui::printBox({
        "Sessions: " + ui::paint(std::to_string(sessions.size()), ui::style::BrightWhite) +
            "   Answers: " + ui::paint(std::to_string(answers), ui::style::BrightWhite) +
            "   Accuracy: " + ui::paint(util::formatPercent(acc), ui::style::BrightWhite) +
            "   Mistakes saved: " + ui::paint(std::to_string(ctx.mistakes.size()), ui::style::BrightWhite),
        "SRS: " + std::to_string(srs.total - srs.unseen) + "/" + std::to_string(srs.total) + " studied · " +
            std::to_string(srs.mature) + " mature · " + std::to_string(srs.dueNow) + " due now" +
            (sessions.empty() ? "" : "   ·   last session " + util::formatDate(sessions.back().timestamp)),
    }, "Overview", ui::style::Magenta);
    std::cout << '\n';

    showKanji(ctx, false);
    showGrammar(ctx, false);
    showListening(ctx);

    if (!sessions.empty()) {
        ui::printRule("Recent sessions");
        ui::Table t({"Date", "Module", "Score", ""});
        for (std::size_t i = sessions.size(); i > 0 && i + 5 > sessions.size(); --i) {
            const auto& s = sessions[i - 1];
            const double r = s.total ? static_cast<double>(s.correct) / s.total : 0.0;
            t.addRow({util::formatDate(s.timestamp), s.module, std::to_string(s.correct) + "/" + std::to_string(s.total),
                      ui::progressBar(r, 12)});
        }
        t.print();
        std::cout << '\n';
    }
}

void ProgressModule::showKanji(AppContext& ctx, bool full) const {
    const auto rows = kanjiRows(ctx);
    printSplitBar(ctx.lang.moduleNative("vocabulary") + " Vocabulary", rows);

    std::vector<const Row*> review, mastered;
    for (const auto& r : rows) {
        if (r.mastery == Mastery::NeedsReview) review.push_back(&r);
        else if (r.mastery == Mastery::Mastered) mastered.push_back(&r);
    }
    sortWeakestFirst(review);

    const std::size_t limit = full ? review.size() : 5;
    if (!review.empty()) {
        std::cout << "  " << ui::paint("↻ Needs review", {ui::style::Bold, ui::style::BrightYellow})
                  << (full ? "" : ui::paint("  (weakest first)", ui::style::Gray)) << '\n';
        ui::Table t({"Word", "Reading", "Accuracy", "SRS"});
        for (std::size_t i = 0; i < review.size() && i < limit; ++i)
            t.addRow({review[i]->label, review[i]->extra, accuracyCell(review[i]->stats), srsCell(review[i]->card)});
        t.print();
        if (review.size() > limit) std::cout << "  " << ui::paint("… " + std::to_string(review.size() - limit) + " more — press [k]", ui::style::Gray) << '\n';
    }
    if (full && !mastered.empty()) {
        std::cout << '\n' << "  " << ui::paint("✔ Mastered", {ui::style::Bold, ui::style::BrightGreen}) << '\n';
        ui::Table t({"Word", "Reading", "Accuracy", "SRS"});
        for (const auto* r : mastered) t.addRow({r->label, r->extra, accuracyCell(r->stats), srsCell(r->card)});
        t.print();
    }
    std::cout << '\n';
}

void ProgressModule::showGrammar(AppContext& ctx, bool full) const {
    const auto rows = grammarRows(ctx);
    printSplitBar(ctx.lang.moduleNative("grammar") + " Grammar", rows);

    std::vector<const Row*> review, mastered;
    for (const auto& r : rows) {
        if (r.mastery == Mastery::NeedsReview) review.push_back(&r);
        else if (r.mastery == Mastery::Mastered) mastered.push_back(&r);
    }
    sortWeakestFirst(review);

    const std::size_t limit = full ? review.size() : 5;
    if (!review.empty()) {
        std::cout << "  " << ui::paint("↻ Needs review", {ui::style::Bold, ui::style::BrightYellow}) << '\n';
        ui::Table t({"Grammar point", "Accuracy", "Streak"});
        t.setAlign(2, ui::Table::Align::Right);
        for (std::size_t i = 0; i < review.size() && i < limit; ++i)
            t.addRow({review[i]->label, accuracyCell(review[i]->stats), std::to_string(review[i]->stats->streak)});
        t.print();
        if (review.size() > limit) std::cout << "  " << ui::paint("… " + std::to_string(review.size() - limit) + " more — press [g]", ui::style::Gray) << '\n';
    }
    if (full && !mastered.empty()) {
        std::cout << '\n' << "  " << ui::paint("✔ Mastered", {ui::style::Bold, ui::style::BrightGreen}) << '\n';
        ui::Table t({"Grammar point", "Accuracy", "Streak"});
        t.setAlign(2, ui::Table::Align::Right);
        for (const auto* r : mastered) t.addRow({r->label, accuracyCell(r->stats), std::to_string(r->stats->streak)});
        t.print();
    }
    std::cout << '\n';
}

void ProgressModule::showListening(AppContext& ctx) const {
    std::vector<const ItemStats*> stats;
    for (const auto& [key, s] : ctx.progress.items())
        if (s.group == group::Listening) stats.push_back(&s);
    if (stats.empty()) return;

    ui::Table t({ctx.lang.moduleNative("listening") + " Listening part", "Accuracy", "Status"});
    for (const auto* s : stats) {
        const Mastery m = ProgressTracker::mastery(s);
        t.addRow({s->label, accuracyCell(s),
                  m == Mastery::Mastered ? ui::paint("✔ mastered", ui::style::Green) : ui::paint("↻ needs review", ui::style::Yellow)});
    }
    t.print();
    std::cout << '\n';
}
