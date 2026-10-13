#include "modules/MistakesModule.h"

#include <algorithm>
#include <iostream>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

namespace {
std::string sectionTag(const LanguageProfile& lang, const std::string& section) {
    const char* color = section == "vocabulary" ? ui::style::BrightYellow
                        : section == "grammar"  ? ui::style::BrightCyan
                        : section == "reading"  ? ui::style::BrightBlue
                                                : ui::style::BrightMagenta;
    return ui::paint(lang.section(section), color);
}
}  // namespace

std::string MistakesModule::badge(const AppContext& ctx) const {
    const std::size_t due = ctx.mistakes.dueCount(util::nowSeconds());
    if (due) return std::to_string(due) + " due";
    return ctx.mistakes.size() ? std::to_string(ctx.mistakes.size()) : "";
}

void MistakesModule::run(AppContext& ctx) {
    for (;;) {
        const std::int64_t now = util::nowSeconds();
        const std::size_t due = ctx.mistakes.dueCount(now);
        ui::printHeader(nativeTitle(ctx) + "  My Mistakes",
                        ctx.levelName() + " · " + std::to_string(ctx.mistakes.size()) + " saved · " + std::to_string(due) + " due now");
        std::cout << '\n';
        const auto& all = ctx.mistakes.all();
        if (all.empty()) {
            ui::info("No mistakes saved — " + ctx.lang.perfectNoMistakes + " (or you haven't started yet)");
            std::cout << '\n';
            ui::waitForEnter();
            return;
        }

        ui::Table table({"#", "Sec", "Question", "Yours", "Correct", "Errors", "DeepSeek", "Next"});
        table.setAlign(0, ui::Table::Align::Right);
        table.setAlign(5, ui::Table::Align::Right);
        table.setMaxWidth(3, 12);
        table.setMaxWidth(4, 12);
        table.setMaxWidth(2, 26);
        const std::size_t shown = std::min<std::size_t>(all.size(), 15);
        for (std::size_t i = 0; i < shown; ++i) {
            const auto& r = all[i];
            const std::string prompt = util::replaceAll(util::replaceAll(r.question.prompt, "[[", ""), "]]", "");
            table.addRow({std::to_string(i + 1), sectionTag(ctx.lang, r.question.section), prompt,
                          ui::paint(r.userAnswer, ui::style::Red), ui::paint(r.question.correctOption(), ui::style::Green),
                          std::to_string(r.count),
                          r.explanation.empty() ? ui::paint("—", ui::style::Gray) : ui::paint("saved", ui::style::BrightMagenta),
                          r.isDue(now) ? ui::paint("now", {ui::style::Bold, ui::style::BrightYellow})
                                       : ui::paint(util::formatInterval(r.due - now), ui::style::Cyan)});
        }
        table.print();
        if (all.size() > shown) std::cout << "  " << ui::paint("… and " + std::to_string(all.size() - shown) + " more", ui::style::Gray) << '\n';
        std::cout << "  " << ui::paint("Errors: how many times you answered it wrong.  DeepSeek: \"saved\" = [w] explanation stored.\n"
                                       "  Next: when it comes back. Each correct review pushes it further: 1d → 3d → 7d → 14d → 30d;\n"
                                       "  after the 30-day review it is learned and removed. A wrong answer sends it back to the start.",
                                       ui::style::Gray)
                  << "\n\n";

        std::cout << "   " << ui::paint("[p]", ui::style::BrightMagenta) << " practice due (" << due << ")   "
                  << ui::paint("[a]", ui::style::BrightMagenta) << " practice any   "
                  << ui::paint("[1-" + std::to_string(shown) + "]", ui::style::BrightMagenta) << " view / explain   "
                  << ui::paint("[c]", ui::style::BrightMagenta) << " clear all   "
                  << ui::paint("[b]", ui::style::BrightMagenta) << " back\n\n";

        const auto in = ui::readLine("");
        if (!in) return;
        const std::string cmd = util::toLower(*in);
        if (cmd == "b" || cmd == "q") return;
        if (cmd == "p" || cmd.empty()) {
            if (due == 0) {
                ui::info("Nothing due — next review in " + util::formatInterval(ctx.mistakes.nextDue() - now) +
                         ". Use [a] to practise anyway (schedules only change for due items).");
                ui::waitForEnter();
            } else {
                practice(ctx, true);
            }
            continue;
        }
        if (cmd == "a") { practice(ctx, false); continue; }
        if (cmd == "c") {
            const auto confirm = ui::readLine(ui::paint("Delete all saved mistakes? type 'yes': ", ui::style::BrightRed));
            if (confirm && util::toLower(*confirm) == "yes") ctx.mistakes.clear();
            continue;
        }
        try {
            const std::size_t n = std::stoul(cmd);
            if (n >= 1 && n <= shown) viewOne(ctx, n - 1);
        } catch (const std::exception&) {
            ui::warn("Unknown command.");
        }
    }
}

void MistakesModule::practice(AppContext& ctx, bool dueOnly) const {
    // Due items only (or everything), frequent offenders first.
    const std::int64_t now = util::nowSeconds();
    std::vector<double> weights;
    for (const auto& r : ctx.mistakes.all()) weights.push_back(dueOnly && !r.isDue(now) ? 0.0 : 1.0 + r.count);
    std::vector<Question> questions;
    for (auto idx : util::weightedSample(weights, static_cast<std::size_t>(ctx.config.sessionSize), ctx.rng)) {
        if (weights[idx] <= 0.0) continue;
        Question q = ctx.mistakes.all()[idx].question;
        const std::string correct = q.correctOption();
        if (q.type != "sentence_order") {
            std::shuffle(q.options.begin(), q.options.end(), ctx.rng);
            q.answer = static_cast<int>(std::find(q.options.begin(), q.options.end(), correct) - q.options.begin());
        }
        questions.push_back(std::move(q));
    }

    quiz::SessionOptions opt;
    opt.title = nativeTitle(ctx) + "  Mistakes review " + ctx.level;
    opt.moduleName = "Mistakes";
    opt.mistakeSource = "Mistakes";
    opt.reviewMistakes = true;
    quiz::QuizSession session(ctx, opt);
    if (session.run(questions).answered > 0) ui::waitForEnter();
}

void MistakesModule::viewOne(AppContext& ctx, std::size_t index) const {
    const MistakeRecord record = ctx.mistakes.all().at(index);  // copy: the log may change below
    const Question& q = record.question;
    const std::int64_t now = util::nowSeconds();
    ui::printHeader(ctx.lang.moduleNative("mistakes") + " #" + std::to_string(index + 1),
                    "from " + record.source + " · wrong " + std::to_string(record.count) + "× · step " +
                        std::to_string(record.step) + "/" + std::to_string(MistakeTracker::kSteps) + " · next review " +
                        (record.isDue(now) ? std::string("now") : "in " + util::formatInterval(record.due - now)));
    std::cout << '\n';
    quiz::renderQuestion(ctx, q, true, false);

    const auto chosen = std::find(q.options.begin(), q.options.end(), record.userAnswer);
    const int chosenIdx = chosen == q.options.end() ? -1 : static_cast<int>(chosen - q.options.begin());
    if (chosenIdx >= 0) quiz::showVerdict(ctx, q, chosenIdx);

    if (!record.explanation.empty()) {
        ui::printBox(ui::renderMarkdown(record.explanation, ui::contentWidth()), ctx.lang.whyTitle + " (saved)", ui::style::BrightMagenta);
        std::cout << '\n';
        ui::waitForEnter();
        return;
    }
    if (chosenIdx < 0) { ui::waitForEnter(); return; }
    const auto in = ui::readLine(ui::paint("[why] ask DeepSeek · [Enter] back ", ui::style::Gray));
    if (in && (util::toLower(*in) == "why" || util::toLower(*in) == "w")) {
        quiz::explainWithDeepSeek(ctx, q, chosenIdx);
        ui::waitForEnter();
    }
}
