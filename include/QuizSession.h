// Reusable multiple-choice session engine (used by Vocabulary, Grammar,
// Listening and My Mistakes) plus the rendering/input helpers that the SRS
// trainer reuses for its own flow.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "AppContext.h"
#include "Question.h"

namespace quiz {

// ---- Rendering / input helpers --------------------------------------------

// results: 1 = correct, 0 = wrong, -1 = not answered yet.
void renderHeader(const AppContext& ctx, const std::string& title, std::size_t index, std::size_t total,
                  const std::vector<int>& results);
void renderQuestion(const AppContext& ctx, const Question& q, bool showPassage, bool hideOptionText);
std::string optionLabel(std::size_t i);  // "A", "B", ...

enum class InputKind { Choice, Quit, Replay };
struct Answer {
    InputKind kind = InputKind::Quit;
    int index = -1;
    double seconds = 0.0;
};
Answer askChoice(std::size_t optionCount, bool allowReplay);

void showVerdict(const AppContext& ctx, const Question& q, int chosen);
// Calls DeepSeek (or shows the cached explanation) for a wrong answer.
void explainWithDeepSeek(AppContext& ctx, const Question& q, int chosen);
// Asks DeepSeek for a translation of the sentence (with the correct answer filled in).
void translateWithDeepSeek(AppContext& ctx, const Question& q);
// Shows the sentence (blank filled in), passage and options with kanji readings.
void showFurigana(AppContext& ctx, const Question& q);

// Study commands available after any answer: [w]hy, [t]ranslate, [f]urigana (Japanese only).
std::string studyHint(const AppContext& ctx);
// Runs the command if `input` is one of them; returns false otherwise.
bool handleStudyCommand(AppContext& ctx, const Question& q, int chosen, const std::string& input);

// ---- Session engine ---------------------------------------------------------

struct SessionOptions {
    std::string title;            // shown in the header, e.g. "文字・語彙  Vocabulary"
    std::string moduleName;       // stored in session history
    std::string mistakeSource;    // tag stored with mistakes
    bool allowWhy = true;
    bool resolveMistakesOnCorrect = false;
    bool listeningMode = false;   // transcript & spoken options hidden until answered
    std::function<void(std::size_t index)> present;  // e.g. play audio; also used for replay
};

struct SessionResult {
    int correct = 0;
    int answered = 0;
    bool aborted = false;
};

class QuizSession {
public:
    QuizSession(AppContext& ctx, SessionOptions options);
    SessionResult run(const std::vector<Question>& questions);

private:
    // Returns false when the user wants to leave the session.
    bool postAnswerLoop(const Question& q, int chosen, bool correct, std::size_t index);
    void showSummary(const std::vector<Question>& questions, const std::vector<int>& chosen,
                     const SessionResult& result) const;

    AppContext& ctx_;
    SessionOptions opt_;
};

}  // namespace quiz
