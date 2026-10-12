// 4. SRS Vocabulary Trainer — Anki-style reviews with ABCD answers.
#pragma once

#include "modules/Module.h"

class SRSModule : public Module {
public:
    std::string japaneseTitle() const override { return "復習 SRS"; }
    std::string title() const override { return "SRS Vocabulary Trainer"; }
    std::string badge(const AppContext& ctx) const override;
    void run(AppContext& ctx) override;

private:
    void showDeckStatus(AppContext& ctx) const;
    void runReviews(AppContext& ctx) const;
};
