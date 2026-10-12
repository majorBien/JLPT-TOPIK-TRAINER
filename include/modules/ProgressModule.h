// 6. Progress — statistics split into Kanji (vocabulary), Grammar and Listening,
// grouped automatically into "mastered" and "needs review".
#pragma once

#include "modules/Module.h"

class ProgressModule : public Module {
public:
    std::string key() const override { return "progress"; }
    void run(AppContext& ctx) override;

private:
    void showOverview(AppContext& ctx) const;
    void showKanji(AppContext& ctx, bool full) const;
    void showGrammar(AppContext& ctx, bool full) const;
    void showListening(AppContext& ctx) const;
};
