// 4. SRS Vocabulary Trainer — Anki-style reviews with ABCD answers.
#pragma once

#include "modules/Module.h"

class SRSModule : public Module {
public:
    std::string key() const override { return "srs"; }
    std::string badge(const AppContext& ctx) const override;
    void run(AppContext& ctx) override;

private:
    void showDeckStatus(AppContext& ctx) const;
    void runReviews(AppContext& ctx) const;
};
