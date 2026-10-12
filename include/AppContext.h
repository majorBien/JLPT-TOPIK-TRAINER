// Everything a module needs for the currently selected language and level.
// Rebuilt from scratch when the user switches level.
#pragma once

#include <memory>
#include <random>
#include <string>

#include "AudioPlayer.h"
#include "Config.h"
#include "DeepSeekClient.h"
#include "Language.h"
#include "MistakeTracker.h"
#include "ProgressTracker.h"
#include "QuestionBank.h"
#include "SRSManager.h"
#include "TtsEngine.h"

struct AppContext {
    AppContext(Config cfg, std::string lvl);

    Config config;
    const LanguageProfile& lang;
    std::string level;
    std::string userDir;
    QuestionBank bank;
    ProgressTracker progress;
    MistakeTracker mistakes;
    SRSManager srs;
    DeepSeekClient deepseek;
    std::unique_ptr<TtsEngine> tts;
    audio::AudioPlayer player;
    std::mt19937 rng;

    std::vector<std::string> vocabDeck() const;  // SRS deck = all vocabulary ids
    // "JLPT N4" / "TOPIK I · 1급 (A1)" for headers.
    std::string levelName() const {
        const std::string label = lang.levelLabel(level);
        return label == level ? lang.exam + " " + level : label;
    }
};
