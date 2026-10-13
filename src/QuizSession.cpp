#include "QuizSession.h"

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>

#include "Furigana.h"
#include "Terminal.h"
#include "Util.h"

namespace quiz {

namespace {

// Explanations fetched during this run, keyed by question id + chosen option.
std::map<std::string, std::string>& explanationCache() {
    static std::map<std::string, std::string> cache;
    return cache;
}

// Translations fetched during this run, keyed by question id.
std::map<std::string, std::string>& translationCache() {
    static std::map<std::string, std::string> cache;
    return cache;
}

// Furigana boxes built during this run, keyed by question id.
std::map<std::string, std::vector<std::string>>& furiganaCache() {
    static std::map<std::string, std::vector<std::string>> cache;
    return cache;
}

// The prompt as a plain sentence: the blank filled with the correct answer, underline markup removed.
// Blanks are "（　　）" (Japanese) or "(　　)" (Korean): a bracket around ideographic spaces.
std::string completedSentence(const Question& q) {
    std::string s = q.prompt;
    const std::string space = "　";
    for (const auto& [open, close] : {std::pair<std::string, std::string>{"（", "）"}, {"(", ")"}}) {
        const auto start = s.find(open + space);
        if (start == std::string::npos) continue;
        std::size_t end = start + open.size();
        while (s.compare(end, space.size(), space) == 0) end += space.size();
        if (s.compare(end, close.size(), close) == 0) {
            s.replace(start, end + close.size() - start, q.correctOption());
            break;
        }
    }
    return util::replaceAll(util::replaceAll(s, "[[", ""), "]]", "");
}

bool isSpokenOptionType(const Question& q) {
    return q.section == "listening" && (q.type == "summary" || q.type == "utterance" || q.type == "response");
}

}  // namespace

// ---- Helpers ----------------------------------------------------------------

std::string optionLabel(std::size_t i) { return std::string(1, static_cast<char>('A' + i)); }

void renderHeader(const AppContext& ctx, const std::string& title, std::size_t index, std::size_t total,
                  const std::vector<int>& results) {
    ui::clearScreen();
    std::string dots;
    for (int r : results) {
        if (r == 1) dots += ui::paint("●", ui::style::BrightGreen);
        else if (r == 0) dots += ui::paint("●", ui::style::BrightRed);
        else dots += ui::paint("○", ui::style::Gray);
        dots += ' ';
    }
    const std::string left = ui::paint(title, {ui::style::Bold, ui::style::BrightYellow});
    const std::string right = ui::paint(ctx.lang.questionNo + " " + std::to_string(index + 1) + "/" + std::to_string(total), ui::style::BrightWhite) + "  " + dots;
    const std::size_t w = ui::contentWidth();
    const std::size_t used = ui::displayWidth(left) + ui::displayWidth(right);
    ui::printBox({left + std::string(used < w ? w - used : 1, ' ') + right}, {}, ui::style::Magenta, true);
    std::cout << '\n';
}

void renderQuestion(const AppContext& ctx, const Question& q, bool showPassage, bool hideOptionText) {
    if (showPassage && !q.passage.empty()) {
        const bool listening = q.section == "listening";
        ui::printBox(ui::wrap(q.passage, ui::contentWidth()), listening ? ctx.lang.transcriptTitle : ctx.lang.passageTitle,
                     ui::style::Blue);
        std::cout << '\n';
    }
    std::cout << "  " << ui::paint(q.instruction, {ui::style::Bold, ui::style::BrightCyan}) << '\n';
    if (!q.instructionEn.empty()) std::cout << "  " << ui::paint(q.instructionEn, ui::style::Gray) << "\n";
    std::cout << '\n';
    if (q.prompt != q.instruction) {  // some reading items ask exactly what the instruction says
        for (const auto& line : ui::wrap(ui::renderMarkup(q.prompt), ui::contentWidth()))
            std::cout << "    " << ui::paint(line, ui::style::BrightWhite) << '\n';
        std::cout << '\n';
    }

    for (std::size_t i = 0; i < q.options.size(); ++i) {
        const std::string label = ui::paint(" " + optionLabel(i) + " ", {ui::style::Bold, ui::style::BrightMagenta});
        const std::string text = hideOptionText ? ui::paint(ctx.lang.listenToAudio, ui::style::Gray) : q.options[i];
        std::cout << "   " << label << " " << text << '\n';
    }
    std::cout << '\n';
}

Answer askChoice(std::size_t optionCount, bool allowReplay) {
    const std::string range = "A-" + optionLabel(optionCount - 1);
    const std::string hint = ui::paint("[" + range + "] answer" + (allowReplay ? " · [r] replay" : "") + " · [q] quit", ui::style::Gray);
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        const auto line = ui::readLine("Your answer " + hint + ": ");
        if (!line) return {InputKind::Quit, -1, 0};
        const std::string in = util::toLower(*line);
        if (in == "q" || in == "quit") return {InputKind::Quit, -1, 0};
        if (allowReplay && (in == "r" || in == "replay")) return {InputKind::Replay, -1, 0};

        int idx = -1;
        if (in.size() == 1 && in[0] >= 'a' && in[0] < 'a' + static_cast<char>(optionCount)) idx = in[0] - 'a';
        else if (in.size() == 1 && in[0] >= '1' && in[0] < '1' + static_cast<char>(optionCount)) idx = in[0] - '1';
        if (idx >= 0) {
            const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            return {InputKind::Choice, idx, secs};
        }
        ui::warn("Please type " + range + " (or 1-" + std::to_string(optionCount) + ").");
    }
}

void showVerdict(const AppContext& ctx, const Question& q, int chosen) {
    const bool correct = chosen == q.answer;
    if (correct) {
        std::cout << "  " << ui::paint(ctx.lang.correct, {ui::style::Bold, ui::style::BrightGreen}) << "  "
                  << ui::paint(optionLabel(static_cast<std::size_t>(q.answer)) + ") " + q.correctOption(), ui::style::Green) << '\n';
    } else {
        std::cout << "  " << ui::paint(ctx.lang.incorrect, {ui::style::Bold, ui::style::BrightRed}) << '\n';
        std::cout << "    Your answer:    "
                  << ui::paint(optionLabel(static_cast<std::size_t>(chosen)) + ") " + q.options[static_cast<std::size_t>(chosen)], ui::style::Red) << '\n';
        std::cout << "    Correct answer: "
                  << ui::paint(optionLabel(static_cast<std::size_t>(q.answer)) + ") " + q.correctOption(), {ui::style::Bold, ui::style::BrightGreen}) << '\n';
    }
    if (!q.note.empty()) {
        std::cout << '\n';
        for (const auto& line : ui::wrap(q.note, ui::contentWidth() - 2))
            std::cout << "    " << ui::paint(line, ui::style::Gray) << '\n';
    }
    std::cout << '\n';
}

void explainWithDeepSeek(AppContext& ctx, const Question& q, int chosen) {
    const std::string& chosenText = q.options[static_cast<std::size_t>(chosen)];
    const std::string cacheKey = q.id + "#" + chosenText;
    auto& cache = explanationCache();
    std::string text;

    if (const auto it = cache.find(cacheKey); it != cache.end()) {
        text = it->second;
    } else if (const MistakeRecord* r = ctx.mistakes.find(q.id);
               r && !r->explanation.empty() && r->userAnswer == chosenText) {
        text = r->explanation;
    } else {
        if (!ctx.deepseek.isConfigured()) {
            ui::warn(ctx.deepseek.statusLine());
            return;
        }
        ExplainRequest req;
        req.level = ctx.levelName();
        req.studyLanguage = ctx.lang.name;
        req.exampleStyle = ctx.lang.exampleStyle;
        req.section = q.section;
        req.instruction = q.instruction;
        req.passage = q.passage;
        req.question = ui::plainMarkup(q.prompt);
        req.options = q.options;
        req.userAnswer = chosenText;
        req.correctAnswer = q.correctOption();

        const ApiResult res = ui::withSpinner("Asking DeepSeek…", [&] { return ctx.deepseek.explain(req); });
        if (!res.ok) {
            ui::error("DeepSeek request failed: " + res.error);
            return;
        }
        text = res.text;
        cache[cacheKey] = text;
        if (chosen != q.answer) ctx.mistakes.attachExplanation(q.id, text);  // saved with the mistake
    }

    std::cout << '\n';
    ui::printBox(ui::renderMarkdown(text, ui::contentWidth()), ctx.lang.whyTitle, ui::style::BrightMagenta);
    std::cout << '\n';
}

void translateWithDeepSeek(AppContext& ctx, const Question& q) {
    auto& cache = translationCache();
    std::string text;

    if (const auto it = cache.find(q.id); it != cache.end()) {
        text = it->second;
    } else {
        if (!ctx.deepseek.isConfigured()) {
            ui::warn(ctx.deepseek.statusLine());
            return;
        }
        TranslateRequest req;
        req.level = ctx.levelName();
        req.studyLanguage = ctx.lang.name;
        req.section = q.section;
        req.passage = q.passage;
        req.question = ui::plainMarkup(q.prompt);
        req.correctAnswer = q.correctOption();

        const ApiResult res = ui::withSpinner("DeepSeek is translating…", [&] { return ctx.deepseek.translate(req); });
        if (!res.ok) {
            ui::error("DeepSeek request failed: " + res.error);
            return;
        }
        text = res.text;
        cache[q.id] = text;
    }

    std::cout << '\n';
    ui::printBox(ui::renderMarkdown(text, ui::contentWidth()), ctx.lang.translationTitle, ui::style::BrightCyan);
    std::cout << '\n';
}

std::string studyHint(const AppContext& ctx) {
    std::string hint = ui::paint("[w]", {ui::style::Bold, ui::style::BrightMagenta}) + ui::paint(" why · ", ui::style::Gray) +
                       ui::paint("[t]", {ui::style::Bold, ui::style::BrightCyan}) + ui::paint(" translate", ui::style::Gray);
    if (ctx.lang.furigana)
        hint += ui::paint(" · ", ui::style::Gray) + ui::paint("[f]", {ui::style::Bold, ui::style::BrightCyan}) +
                ui::paint(" furigana", ui::style::Gray);
    return hint;
}

bool handleStudyCommand(AppContext& ctx, const Question& q, int chosen, const std::string& input) {
    if (input == "w" || input == "why" || input == "?") explainWithDeepSeek(ctx, q, chosen);
    else if (input == "t" || input == "translate") translateWithDeepSeek(ctx, q);
    else if (ctx.lang.furigana && (input == "f" || input == "furigana")) showFurigana(ctx, q);
    else return false;
    return true;
}

void showFurigana(AppContext& ctx, const Question& q) {
    auto& cache = furiganaCache();
    auto it = cache.find(q.id);
    if (it == cache.end()) {
        // Everything worth reading: passage/transcript lines, the completed sentence, the options.
        struct Segment { std::string label; std::string text; bool sentence; };
        std::vector<Segment> segments;
        std::istringstream passage(q.passage);
        for (std::string line; std::getline(passage, line);)
            if (!line.empty()) segments.push_back({"", line, false});
        segments.push_back({"", completedSentence(q), true});
        for (std::size_t i = 0; i < q.options.size(); ++i) segments.push_back({optionLabel(i) + ") ", q.options[i], false});

        // One analyser run for all kanji-bearing segments, joined by a separator that comes back as its own word.
        static const std::string separator = "〓";
        std::string joined;
        for (const auto& seg : segments)
            if (util::hasKanji(seg.text)) joined += (joined.empty() ? "" : separator) + seg.text;
        if (joined.empty()) {
            ui::info("No kanji here — nothing to add furigana to.");
            return;
        }
        if (!ctx.tts->available()) {
            ui::warn("Furigana needs Open JTalk: " + ctx.tts->status());
            return;
        }
        const auto words = ui::withSpinner("読み方を調べています… looking up readings", [&] { return ctx.tts->analyze(joined); });
        if (words.empty()) {
            ui::error("Could not analyse the sentence with Open JTalk.");
            return;
        }

        std::vector<std::vector<Morpheme>> parts(1);
        for (const auto& w : words) {
            if (w.surface == separator) parts.emplace_back();
            else parts.back().push_back(w);
        }

        const std::size_t width = ui::contentWidth();
        std::vector<std::string> lines;
        auto addWrapped = [&](const std::string& text, const char* style) {
            for (auto& l : ui::wrap(text, width)) lines.push_back(style ? ui::paint(l, style) : l);
        };
        std::size_t next = 0;
        for (const auto& seg : segments) {
            if (seg.sentence) lines.emplace_back();
            if (!util::hasKanji(seg.text) || next >= parts.size()) {
                addWrapped(seg.label + seg.text, seg.sentence ? ui::style::BrightWhite : nullptr);
            } else {
                const auto& part = parts[next++];
                for (auto& l : furigana::wrapChunks(furigana::inlineChunks(part, ui::style::Green), width, seg.label))
                    lines.push_back(std::move(l));
                if (seg.sentence) addWrapped(furigana::hiraganaText(part), ui::style::Gray);
            }
            if (seg.sentence) lines.emplace_back();
        }
        it = cache.emplace(q.id, std::move(lines)).first;
    }

    std::cout << '\n';
    ui::printBox(it->second, "振り仮名 Furigana", ui::style::Green);
    std::cout << '\n';
}

// ---- Session ----------------------------------------------------------------

QuizSession::QuizSession(AppContext& ctx, SessionOptions options) : ctx_(ctx), opt_(std::move(options)) {}

SessionResult QuizSession::run(const std::vector<Question>& questions) {
    SessionResult result;
    std::vector<int> marks(questions.size(), -1);
    std::vector<int> chosen(questions.size(), -1);

    for (std::size_t i = 0; i < questions.size(); ++i) {
        const Question& q = questions[i];
        const bool hideOptions = opt_.listeningMode && isSpokenOptionType(q);

        renderHeader(ctx_, opt_.title, i, questions.size(), marks);
        renderQuestion(ctx_, q, !opt_.listeningMode, hideOptions);
        if (opt_.present) opt_.present(i);

        Answer ans;
        for (;;) {
            ans = askChoice(q.options.size(), opt_.listeningMode && opt_.present);
            if (ans.kind != InputKind::Replay) break;
            opt_.present(i);
        }
        if (ans.kind == InputKind::Quit) {
            result.aborted = true;
            break;
        }

        const bool correct = ans.index == q.answer;
        chosen[i] = ans.index;
        marks[i] = correct ? 1 : 0;
        ++result.answered;
        if (correct) ++result.correct;

        ctx_.progress.record(q, correct);
        if (!correct) ctx_.mistakes.add(q, ans.index, opt_.mistakeSource);
        else if (opt_.resolveMistakesOnCorrect) ctx_.mistakes.resolve(q.id);

        if (opt_.listeningMode) {
            // Reveal the transcript and the spoken options now.
            renderHeader(ctx_, opt_.title, i, questions.size(), marks);
            renderQuestion(ctx_, q, true, false);
        } else {
            std::cout << '\n';
        }
        showVerdict(ctx_, q, ans.index);
        if (!postAnswerLoop(q, ans.index, correct, i)) {
            result.aborted = true;
            break;
        }
    }

    if (result.answered > 0) {
        ctx_.progress.recordSession(opt_.moduleName, result.correct, result.answered);
        showSummary(questions, chosen, result);
    }
    return result;
}

bool QuizSession::postAnswerLoop(const Question& q, int chosen, bool /*correct*/, std::size_t index) {
    const bool canReplay = opt_.listeningMode && opt_.present;
    for (;;) {
        std::string hint = ui::paint("[Enter] next · ", ui::style::Gray);
        if (opt_.allowWhy) hint += studyHint(ctx_);
        if (canReplay) hint += ui::paint(" · [r] replay", ui::style::Gray);
        hint += ui::paint(" · [q] quit", ui::style::Gray);

        const auto line = ui::readLine(hint + " ");
        if (!line) return false;
        const std::string in = util::toLower(*line);
        if (in.empty() || in == "n" || in == "next") return true;
        if (in == "q" || in == "quit") return false;
        if (opt_.allowWhy && handleStudyCommand(ctx_, q, chosen, in)) continue;
        if (canReplay && (in == "r" || in == "replay")) {
            opt_.present(index);
            continue;
        }
        ui::warn("Unknown command.");
    }
}

void QuizSession::showSummary(const std::vector<Question>& questions, const std::vector<int>& chosen,
                              const SessionResult& result) const {
    ui::clearScreen();
    ui::printHeader(ctx_.lang.resultsTitle, opt_.title);
    std::cout << '\n';

    ui::Table table({"#", "Item", "Your answer", "Correct", ""});
    table.setAlign(0, ui::Table::Align::Right);
    table.setMaxWidth(1, 22);
    for (std::size_t i = 0; i < questions.size(); ++i) {
        if (chosen[i] < 0) continue;
        const Question& q = questions[i];
        const bool ok = chosen[i] == q.answer;
        table.addRow({std::to_string(i + 1), q.trackLabel,
                      ui::paint(q.options[static_cast<std::size_t>(chosen[i])], ok ? ui::style::Green : ui::style::Red),
                      q.correctOption(), ok ? ui::paint("✔", ui::style::BrightGreen) : ui::paint("✘", ui::style::BrightRed)});
    }
    table.print();

    const double ratio = result.answered ? static_cast<double>(result.correct) / result.answered : 0.0;
    const char* color = ratio >= 0.8 ? ui::style::BrightGreen : ratio >= 0.6 ? ui::style::BrightYellow : ui::style::BrightRed;
    std::cout << "\n  Score: " << ui::paint(std::to_string(result.correct) + "/" + std::to_string(result.answered), {ui::style::Bold, color})
              << "  " << ui::progressBar(ratio, 30, color) << ' ' << util::formatPercent(ratio) << '\n';
    const std::string& cheer = ratio >= 0.999 ? ctx_.lang.cheerPerfect : ratio >= 0.6 ? ctx_.lang.cheerGood : ctx_.lang.cheerKeepGoing;
    std::cout << "  " << ui::paint(cheer, {ui::style::Bold, ui::style::BrightYellow}) << "\n";
    if (result.answered > result.correct)
        std::cout << "  " << ui::paint("Wrong answers were saved to 「My Mistakes」.", ui::style::Gray) << '\n';
    std::cout << '\n';
}

}  // namespace quiz
