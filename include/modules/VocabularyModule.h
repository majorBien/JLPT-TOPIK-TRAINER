// 1. Language Knowledge (Vocabulary) — 文字・語彙 sessions in JLPT format.
#pragma once

#include "modules/Module.h"

class VocabularyModule : public Module {
public:
    std::string japaneseTitle() const override { return "文字・語彙"; }
    std::string title() const override { return "Language Knowledge (Vocabulary)"; }
    void run(AppContext& ctx) override;

private:
    std::vector<Question> buildSession(AppContext& ctx) const;
};
