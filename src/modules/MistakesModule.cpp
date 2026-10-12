#include "modules/MistakesModule.h"

#include <algorithm>
#include <iostream>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

namespace {
std::string sectionTag(const std::string& section) {
    if (section == "vocabulary") return ui::paint("語彙", ui::style::BrightYellow);
    if (section == "grammar") return ui::paint("文法", ui::style::BrightCyan);
    if (section == "reading") return ui::paint("読解", ui::style::BrightBlue);
    if (section == "listening") return ui::paint("聴解", ui::style::BrightMagenta);
    return section;
}
}  // namespace

std::string MistakesModule::badge(const AppContext& ctx) const {
    return ctx.mistakes.size() ? std::to_string(ctx.mistakes.size()) : "";
}

void MistakesModule::run(AppContext& ctx) {
    for (;;) {
        ui::printHeader("間違い  My Mistakes", "JLPT " + ctx.level + " · answer correctly to clear an item");
        std::cout << '\n';
        const auto& all = ctx.mistakes.all();
        if (all.empty()) {
            ui::info("No mistakes saved — 完璧！ (or you haven't started yet)");
            std::cout << '\n';
            ui::waitForEnter();
            return;
        }

        ui::Table table({"#", "Sec", "Question", "Your answer", "Correct", "×", "Last"});
        table.setAlign(0, ui::Table::Align::Right);
        table.setAlign(5, ui::Table::Align::Right);
        table.setMaxWidth(2, 26);
        table.setMaxWidth(3, 14);
        table.setMaxWidth(4, 14);
        const std::size_t shown = std::min<std::size_t>(all.size(), 15);
        for (std::size_t i = 0; i < shown; ++i) {
            const auto& r = all[i];
            const std::string prompt = util::replaceAll(util::replaceAll(r.question.prompt, "[[", ""), "]]", "");
            table.addRow({std::to_string(i + 1), sectionTag(r.question.section), prompt,
                          ui::paint(r.userAnswer, ui::style::Red), ui::paint(r.question.correctOption(), ui::style::Green),
                          std::to_string(r.count) + (r.explanation.empty() ? "" : ui::paint("✎", ui::style::BrightMagenta)),
                          util::formatDate(r.lastSeen).substr(5, 5)});
        }
        table.print();
        if (all.size() > shown) std::cout << "  " << ui::paint("… and " + std::to_string(all.size() - shown) + " more", ui::style::Gray) << '\n';
        std::cout << "  " << ui::paint("✎ = saved DeepSeek explanation", ui::style::Gray) << "\n\n";

        std::cout << "   " << ui::paint("[p]", ui::style::BrightMagenta) << " practice " << ctx.config.sessionSize << "   "
                  << ui::paint("[1-" + std::to_string(shown) + "]", ui::style::BrightMagenta) << " view / explain   "
                  << ui::paint("[c]", ui::style::BrightMagenta) << " clear all   "
                  << ui::paint("[b]", ui::style::BrightMagenta) << " back\n\n";

        const auto in = ui::readLine("");
        if (!in) return;
        const std::string cmd = util::toLower(*in);
        if (cmd == "b" || cmd == "q") return;
        if (cmd == "p" || cmd.empty()) { practice(ctx); continue; }
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

void MistakesModule::practice(AppContext& ctx) const {
    // Frequent offenders first, then a random pick among the rest.
    std::vector<double> weights;
    for (const auto& r : ctx.mistakes.all()) weights.push_back(1.0 + r.count);
    std::vector<Question> questions;
    for (auto idx : util::weightedSample(weights, static_cast<std::size_t>(ctx.config.sessionSize), ctx.rng)) {
        Question q = ctx.mistakes.all()[idx].question;
        const std::string correct = q.correctOption();
        if (q.type != "sentence_order") {
            std::shuffle(q.options.begin(), q.options.end(), ctx.rng);
            q.answer = static_cast<int>(std::find(q.options.begin(), q.options.end(), correct) - q.options.begin());
        }
        questions.push_back(std::move(q));
    }

    quiz::SessionOptions opt;
    opt.title = "間違い  Mistakes review " + ctx.level;
    opt.moduleName = "Mistakes";
    opt.mistakeSource = "Mistakes";
    opt.resolveMistakesOnCorrect = true;
    quiz::QuizSession session(ctx, opt);
    if (session.run(questions).answered > 0) ui::waitForEnter();
}

void MistakesModule::viewOne(AppContext& ctx, std::size_t index) const {
    const MistakeRecord record = ctx.mistakes.all().at(index);  // copy: the log may change below
    const Question& q = record.question;
    ui::printHeader("間違い #" + std::to_string(index + 1), "from " + record.source + " · wrong " + std::to_string(record.count) + "×");
    std::cout << '\n';
    quiz::renderQuestion(q, true, false);

    const auto chosen = std::find(q.options.begin(), q.options.end(), record.userAnswer);
    const int chosenIdx = chosen == q.options.end() ? -1 : static_cast<int>(chosen - q.options.begin());
    if (chosenIdx >= 0) quiz::showVerdict(q, chosenIdx);

    if (!record.explanation.empty()) {
        ui::printBox(ui::renderMarkdown(record.explanation, ui::contentWidth()), "なぜ？ Why · DeepSeek (saved)", ui::style::BrightMagenta);
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
