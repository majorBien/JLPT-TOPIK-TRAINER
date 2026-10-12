#include "AppContext.h"

#include <filesystem>

namespace {
std::string userFile(const std::string& dir, const char* name) {
    return (std::filesystem::path(dir) / name).string();
}

DeepSeekSettings deepseekSettings(const Config& cfg) {
    DeepSeekSettings s;
    s.keyFile = cfg.deepseekKeyFile;
    s.model = cfg.deepseekModel;
    s.language = cfg.explanationLanguage;
    s.timeoutSec = cfg.apiTimeoutSec;
    return s;
}
}  // namespace

AppContext::AppContext(Config cfg, std::string lvl)
    : config(std::move(cfg)),
      lang(languageProfile(config.language)),
      level(std::move(lvl)),
      userDir(config.levelUserDir(level)),
      bank(QuestionBank::load(config.levelDataDir(level), level, lang)),
      progress(userFile(userDir, "progress.json")),
      mistakes(userFile(userDir, "mistakes.json")),
      srs(userFile(userDir, "srs.json")),
      deepseek(deepseekSettings(config)),
      tts(makeTtsEngine(config)),
      player(config.audioPlayer),
      rng(std::random_device{}()) {}

std::vector<std::string> AppContext::vocabDeck() const {
    std::vector<std::string> ids;
    for (const auto& e : bank.vocabulary()) ids.push_back(e.id);
    return ids;
}
