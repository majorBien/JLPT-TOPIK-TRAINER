#include "modules/GrammarModule.h"

#include <algorithm>
#include <iostream>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

std::vector<Question> GrammarModule::buildSession(AppContext& ctx) const {
    std::vector<const Question*> grammar, reading;
    for (const auto& q : ctx.bank.grammarQuestions()) (q.section == "reading" ? reading : grammar).push_back(&q);

    const auto size = static_cast<std::size_t>(ctx.config.sessionSize);
    // Like the exam: grammar items first, then reading texts (JLPT: one, TOPIK: two).
    const std::size_t wantReading = static_cast<std::size_t>(ctx.lang.readingPerSession);
    const std::size_t readingCount = std::min<std::size_t>(reading.empty() || size < 3 ? 0 : wantReading, reading.size());
    const std::size_t grammarCount = std::min(size - readingCount, grammar.size());

    auto sample = [&](const std::vector<const Question*>& pool, std::size_t k) {
        std::vector<double> weights;
        for (const auto* q : pool) weights.push_back(ctx.progress.weightFor(q->trackKey));
        std::vector<Question> out;
        for (auto idx : util::weightedSample(weights, k, ctx.rng)) out.push_back(*pool[idx]);
        return out;
    };

    std::vector<Question> questions = sample(grammar, grammarCount);
    std::stable_sort(questions.begin(), questions.end(),
                     [](const Question& a, const Question& b) { return a.type < b.type; });
    for (auto& q : sample(reading, readingCount)) questions.push_back(std::move(q));

    // Shuffle option order so answers can't be memorized by position.
    for (auto& q : questions) {
        const std::string correct = q.correctOption();
        if (q.type == "sentence_order") continue;  // pieces keep their printed order
        std::shuffle(q.options.begin(), q.options.end(), ctx.rng);
        q.answer = static_cast<int>(std::find(q.options.begin(), q.options.end(), correct) - q.options.begin());
    }
    return questions;
}

void GrammarModule::run(AppContext& ctx) {
    if (ctx.bank.grammarQuestions().empty()) {
        ui::printHeader(nativeTitle(ctx), title(ctx));
        ui::warn("This level has no grammar data yet.");
        ui::waitForEnter();
        return;
    }
    quiz::SessionOptions opt;
    opt.title = nativeTitle(ctx) + "  " + ctx.level;
    opt.moduleName = "Grammar";
    opt.mistakeSource = "Grammar";

    for (;;) {
        quiz::QuizSession session(ctx, opt);
        const auto result = session.run(buildSession(ctx));
        if (result.answered == 0) return;
        const auto again = ui::readLine(ui::paint("[Enter] new session · [b] back to menu ", ui::style::Gray));
        if (!again || util::toLower(*again) == "b" || util::toLower(*again) == "q") return;
    }
}
