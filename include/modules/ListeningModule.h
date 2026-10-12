// 3. Listening — 聴解 sessions with audio synthesized by the TTS engine.
#pragma once

#include "modules/Module.h"

class ListeningModule : public Module {
public:
    std::string japaneseTitle() const override { return "聴解"; }
    std::string title() const override { return "Listening"; }
    std::string badge(const AppContext& ctx) const override;
    void run(AppContext& ctx) override;

private:
    // Builds (or reuses from cache) the WAV for an item. Empty string on failure.
    std::string audioFor(AppContext& ctx, const ListeningItem& item) const;
    void pregenerateAll(AppContext& ctx) const;
    void runSession(AppContext& ctx) const;
};
