// 2. Language Knowledge (Grammar) & Reading — 文法・読解 sessions.
#pragma once

#include "modules/Module.h"

class GrammarModule : public Module {
public:
    std::string key() const override { return "grammar"; }
    void run(AppContext& ctx) override;

private:
    std::vector<Question> buildSession(AppContext& ctx) const;
};
