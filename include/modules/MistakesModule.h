// 5. My Mistakes — browse, re-practice and get explanations for wrong answers.
#pragma once

#include "modules/Module.h"

class MistakesModule : public Module {
public:
    std::string japaneseTitle() const override { return "間違い"; }
    std::string title() const override { return "My Mistakes"; }
    std::string badge(const AppContext& ctx) const override;
    void run(AppContext& ctx) override;

private:
    void practice(AppContext& ctx) const;
    void viewOne(AppContext& ctx, std::size_t index) const;
};
