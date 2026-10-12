// Everything a module needs for the currently selected JLPT level.
// Rebuilt from scratch when the user switches level.
#pragma once

#include <memory>
#include <random>
#include <string>

#include "AudioPlayer.h"
#include "Config.h"
#include "DeepSeekClient.h"
#include "MistakeTracker.h"
#include "ProgressTracker.h"
#include "QuestionBank.h"
#include "SRSManager.h"
#include "TtsEngine.h"

struct AppContext {
    AppContext(Config cfg, std::string lvl);

    Config config;
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
};
