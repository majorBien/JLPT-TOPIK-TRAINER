// 2. Language Knowledge (Grammar) & Reading — 文法・読解 sessions.
#pragma once

#include "modules/Module.h"

class GrammarModule : public Module {
public:
    std::string japaneseTitle() const override { return "文法・読解"; }
    std::string title() const override { return "Language Knowledge (Grammar) & Reading"; }
    void run(AppContext& ctx) override;

private:
    std::vector<Question> buildSession(AppContext& ctx) const;
};
