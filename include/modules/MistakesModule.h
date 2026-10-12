// 5. My Mistakes — browse, re-practice and get explanations for wrong answers.
#pragma once

#include "modules/Module.h"

class MistakesModule : public Module {
public:
    std::string key() const override { return "mistakes"; }
    std::string badge(const AppContext& ctx) const override;
    void run(AppContext& ctx) override;

private:
    void practice(AppContext& ctx) const;
    void viewOne(AppContext& ctx, std::size_t index) const;
};
