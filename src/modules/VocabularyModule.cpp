#include "modules/VocabularyModule.h"

#include <algorithm>
#include <iostream>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

std::vector<Question> VocabularyModule::buildSession(AppContext& ctx) const {
    const auto& vocab = ctx.bank.vocabulary();
    std::vector<double> weights;
    for (const auto& e : vocab) weights.push_back(ctx.progress.weightFor(QuestionBank::vocabTrackKey(e)));
    const auto picked = util::weightedSample(weights, static_cast<std::size_t>(ctx.config.sessionSize), ctx.rng);

    // Mix the four JLPT 文字・語彙 question types within a session.
    std::vector<VocabQuestionType> types = {VocabQuestionType::KanjiReading, VocabQuestionType::Orthography,
                                            VocabQuestionType::Context, VocabQuestionType::Meaning};
    std::shuffle(types.begin(), types.end(), ctx.rng);

    std::vector<Question> questions;
    for (std::size_t i = 0; i < picked.size(); ++i)
        questions.push_back(ctx.bank.makeVocabQuestion(vocab[picked[i]], types[i % types.size()], ctx.rng));

    // One hand-written 言い換え／用法 item per session (replacing a generated one), weighted like the words.
    const auto& extra = ctx.bank.vocabQuestions();
    if (!extra.empty() && questions.size() >= 4) {
        std::vector<double> extraWeights;
        for (const auto& q : extra) extraWeights.push_back(ctx.progress.weightFor(q.trackKey));
        Question q = extra[util::weightedSample(extraWeights, 1, ctx.rng).front()];
        const std::string correct = q.correctOption();
        std::shuffle(q.options.begin(), q.options.end(), ctx.rng);
        q.answer = static_cast<int>(std::find(q.options.begin(), q.options.end(), correct) - q.options.begin());
        questions.back() = std::move(q);
    }
    // Group by type like the real test paper (問題1, 問題2, ...).
    std::stable_sort(questions.begin(), questions.end(), [](const Question& a, const Question& b) { return a.type < b.type; });
    return questions;
}

void VocabularyModule::run(AppContext& ctx) {
    if (ctx.bank.vocabulary().size() < 4) {
        ui::printHeader("文字・語彙", title());
        ui::warn("This level has no vocabulary data yet (need at least 4 entries).");
        ui::waitForEnter();
        return;
    }
    quiz::SessionOptions opt;
    opt.title = "文字・語彙  Vocabulary " + ctx.level;
    opt.moduleName = "Vocabulary";
    opt.mistakeSource = "Vocabulary";

    for (;;) {
        quiz::QuizSession session(ctx, opt);
        const auto result = session.run(buildSession(ctx));
        if (result.answered == 0) return;
        const auto again = ui::readLine(ui::paint("[Enter] new session · [b] back to menu ", ui::style::Gray));
        if (!again || util::toLower(*again) == "b" || util::toLower(*again) == "q") return;
    }
}
