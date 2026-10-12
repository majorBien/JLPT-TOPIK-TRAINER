#include "modules/SRSModule.h"

#include <deque>
#include <iostream>
#include <map>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

namespace {
constexpr int kMaxRequeue = 2;

std::string gradeChip(Grade g, const SrsCard& preview, bool selected) {
    const std::string text = std::string(toString(g)) + " " +
                             (preview.intervalDays == 0 ? "10m" : util::formatInterval(preview.intervalDays * 86400LL));
    if (!selected) return ui::paint(text, ui::style::Gray);
    const char* color = g == Grade::Again ? ui::style::BrightRed : g == Grade::Hard ? ui::style::BrightYellow
                        : g == Grade::Good ? ui::style::BrightGreen : ui::style::BrightCyan;
    return ui::paint("[" + text + "]", {ui::style::Bold, color});
}
}  // namespace

std::string SRSModule::badge(const AppContext& ctx) const {
    const auto s = ctx.srs.stats(ctx.vocabDeck(), util::nowSeconds());
    return s.dueNow > 0 ? std::to_string(s.dueNow) + " due" : "";
}

void SRSModule::showDeckStatus(AppContext& ctx) const {
    const auto now = util::nowSeconds();
    const auto s = ctx.srs.stats(ctx.vocabDeck(), now);
    ui::printHeader("復習  SRS Vocabulary Trainer", "Spaced repetition · SM-2 · JLPT " + ctx.level);
    std::cout << '\n';

    const int seen = s.total - s.unseen;
    const std::size_t barW = 30;
    auto row = [&](const std::string& label, int n, const char* color) {
        const double ratio = s.total ? static_cast<double>(n) / s.total : 0.0;
        return ui::padRight(label, 16) + ui::progressBar(ratio, barW, color) + " " + ui::padLeft(std::to_string(n), 4);
    };
    std::vector<std::string> lines = {
        row("New (unseen)", s.unseen, ui::style::Blue),
        row("Learning", s.learning, ui::style::BrightRed),
        row("Young (<21d)", s.young, ui::style::BrightYellow),
        row("Mature (≥21d)", s.mature, ui::style::BrightGreen),
        "",
        "Due now: " + ui::paint(std::to_string(s.dueNow), {ui::style::Bold, ui::style::BrightMagenta}) +
            "   ·   studied: " + std::to_string(seen) + "/" + std::to_string(s.total) +
            (s.nextDue ? "   ·   next due in " + util::formatInterval(s.nextDue - now) : ""),
        ui::paint("Session: up to " + std::to_string(ctx.config.srsMaxReviews) + " reviews + " +
                      std::to_string(ctx.config.srsNewPerSession) + " new cards",
                  ui::style::Gray),
    };
    ui::printBox(lines, "Deck " + ctx.level, ui::style::Cyan);
    std::cout << '\n';
    std::cout << "  " << ui::paint("How grading works:", ui::style::Bold) << ui::paint(
        " wrong → Again (10 min). Correct → Easy (<4s), Good (<12s) or Hard (slower);\n"
        "  you can override with h/g/e before moving on — intervals grow with each card's ease.", ui::style::Gray) << "\n\n";
}

void SRSModule::run(AppContext& ctx) {
    if (ctx.bank.vocabulary().size() < 4) {
        ui::printHeader("復習 SRS", title());
        ui::warn("Not enough vocabulary in this level for ABCD reviews.");
        ui::waitForEnter();
        return;
    }
    for (;;) {
        showDeckStatus(ctx);
        std::cout << "   " << ui::paint("[Enter]", ui::style::BrightMagenta) << " start review   "
                  << ui::paint("[b]", ui::style::BrightMagenta) << " back\n\n";
        const auto in = ui::readLine("");
        if (!in || util::toLower(*in) == "b" || util::toLower(*in) == "q") return;
        if (in->empty()) runReviews(ctx);
    }
}

void SRSModule::runReviews(AppContext& ctx) const {
    const auto deck = ctx.vocabDeck();
    const auto now = util::nowSeconds();
    std::deque<std::string> queue;
    for (const auto& id : ctx.srs.dueCards(deck, now, static_cast<std::size_t>(ctx.config.srsMaxReviews))) queue.push_back(id);
    for (const auto& id : ctx.srs.newCards(deck, static_cast<std::size_t>(ctx.config.srsNewPerSession))) queue.push_back(id);

    if (queue.empty()) {
        ui::info("Nothing to review right now — お疲れさま！ Come back later.");
        ui::waitForEnter();
        return;
    }

    std::map<std::string, int> requeued;
    std::vector<int> marks;
    int reviewed = 0, correctCount = 0;
    ui::Table log({"Word", "Reading", "Result", "Next"});

    std::uniform_int_distribution<int> typeDist(0, 2);
    const VocabQuestionType types[] = {VocabQuestionType::Meaning, VocabQuestionType::KanjiReading,
                                       VocabQuestionType::ReverseMeaning};

    while (!queue.empty()) {
        const std::string id = queue.front();
        queue.pop_front();
        const VocabEntry* entry = ctx.bank.findVocab(id);
        if (!entry) continue;

        const SrsCard card = ctx.srs.card(id);
        Question q = ctx.bank.makeVocabQuestion(*entry, types[typeDist(ctx.rng)], ctx.rng);

        std::vector<int> headerMarks = marks;
        headerMarks.push_back(-1);
        quiz::renderHeader("復習 SRS " + ctx.level, marks.size(), marks.size() + queue.size() + 1, headerMarks);
        const std::string state = card.isNew() ? ui::paint("NEW", {ui::style::Bold, ui::style::BrightBlue})
                                               : ui::paint("interval " + std::to_string(card.intervalDays) + "d · ease " +
                                                               std::to_string(card.ease).substr(0, 4) + " · lapses " +
                                                               std::to_string(card.lapses), ui::style::Gray);
        std::cout << "  " << state << "\n\n";
        quiz::renderQuestion(q, false, false);

        const auto ans = quiz::askChoice(q.options.size(), false);
        if (ans.kind == quiz::InputKind::Quit) break;

        const bool correct = ans.index == q.answer;
        quiz::showVerdict(q, ans.index);

        Grade grade = SRSManager::gradeFromAnswer(correct, ans.seconds);
        const auto at = util::nowSeconds();
        auto chips = [&](Grade selected) {
            std::string line = "  Next review:  ";
            for (Grade g : {Grade::Again, Grade::Hard, Grade::Good, Grade::Easy}) {
                if (correct == (g == Grade::Again)) continue;  // wrong ⇒ only Again; correct ⇒ Hard/Good/Easy
                line += gradeChip(g, SRSManager::schedule(card, g, at), g == selected) + "  ";
            }
            return line;
        };
        std::cout << chips(grade) << ui::paint("(" + std::to_string(static_cast<int>(ans.seconds + 0.5)) + "s)", ui::style::Gray) << "\n\n";

        bool quit = false;
        if (correct) {
            for (;;) {
                const auto in = ui::readLine(ui::paint(std::string("[Enter] accept ") + toString(grade) +
                                                           " · [h]ard [g]ood [e]asy · ", ui::style::Gray) +
                                             quiz::studyHint() + ui::paint(" · [q] quit ", ui::style::Gray));
                if (!in) { quit = true; break; }
                const std::string c = util::toLower(*in);
                if (c.empty()) break;
                if (c == "q") { quit = true; break; }
                if (quiz::handleStudyCommand(ctx, q, ans.index, c)) continue;
                if (c == "h" || c == "g" || c == "e") {
                    grade = c == "h" ? Grade::Hard : c == "g" ? Grade::Good : Grade::Easy;
                    std::cout << chips(grade) << "\n";
                    continue;
                }
                ui::warn("Type h, g, e, w, t, f or press Enter.");
            }
        } else {
            for (;;) {
                const auto in = ui::readLine(ui::paint("[Enter] continue · ", ui::style::Gray) + quiz::studyHint() +
                                             ui::paint(" · [q] quit ", ui::style::Gray));
                if (in && quiz::handleStudyCommand(ctx, q, ans.index, util::toLower(*in))) continue;
                quit = !in || util::toLower(*in) == "q";
                break;
            }
        }

        const SrsCard updated = ctx.srs.review(id, grade, at);
        ctx.progress.record(QuestionBank::vocabTrackKey(*entry), group::Kanji, entry->word, correct);
        ++reviewed;
        if (correct) ++correctCount;
        marks.push_back(correct ? 1 : 0);
        log.addRow({entry->word, entry->reading,
                    correct ? ui::paint(std::string("✔ ") + toString(grade), ui::style::Green) : ui::paint("✘ Again", ui::style::Red),
                    updated.intervalDays == 0 ? "10m" : util::formatInterval(updated.intervalDays * 86400LL)});

        if (!correct && requeued[id]++ < kMaxRequeue) queue.push_back(id);  // relearn within the session
        if (quit) break;
    }

    if (reviewed == 0) return;
    ctx.progress.recordSession("SRS", correctCount, reviewed);
    ui::printHeader("復習 完了  Review finished", "JLPT " + ctx.level);
    std::cout << '\n';
    log.print();
    const double ratio = static_cast<double>(correctCount) / reviewed;
    std::cout << "\n  Reviewed " << reviewed << " · correct " << correctCount << "  "
              << ui::progressBar(ratio, 24) << ' ' << util::formatPercent(ratio) << "\n\n";
    ui::waitForEnter();
}
