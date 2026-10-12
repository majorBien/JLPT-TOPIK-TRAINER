#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>

#include "Json.h"
#include "Util.h"

namespace fs = std::filesystem;

#ifndef JLPT_SOURCE_DIR
#define JLPT_SOURCE_DIR "."
#endif

namespace {

bool looksLikeRoot(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p / "data" / "levels", ec);
}

std::string findRoot() {
    if (const char* env = std::getenv("JLPT_HOME"); env && looksLikeRoot(env)) return env;
    const fs::path cwd = fs::current_path();
    for (const fs::path& candidate : {cwd, cwd.parent_path(), fs::path(JLPT_SOURCE_DIR)})
        if (looksLikeRoot(candidate)) return fs::weakly_canonical(candidate).string();
    return cwd.string();
}

}  // namespace

Config Config::load() {
    Config cfg;
    cfg.rootDir = findRoot();

    const fs::path settingsPath = fs::path(cfg.rootDir) / "settings.json";
    if (!fs::exists(settingsPath)) return cfg;

    try {
        const json::Value s = json::parseFile(settingsPath.string());
        cfg.defaultLevel = s.getString("default_level", cfg.defaultLevel);
        cfg.sessionSize = static_cast<int>(std::clamp<std::int64_t>(s.getInt("session_size", cfg.sessionSize), 1, 50));
        cfg.srsNewPerSession = static_cast<int>(s.getInt("srs_new_per_session", cfg.srsNewPerSession));
        cfg.srsMaxReviews = static_cast<int>(s.getInt("srs_max_reviews", cfg.srsMaxReviews));
        const json::Value& ds = s["deepseek"];
        cfg.deepseekKeyFile = ds.getString("key_file", cfg.deepseekKeyFile);
        cfg.deepseekModel = ds.getString("model", cfg.deepseekModel);
        cfg.explanationLanguage = ds.getString("explanation_language", cfg.explanationLanguage);
        cfg.apiTimeoutSec = static_cast<int>(ds.getInt("timeout_seconds", cfg.apiTimeoutSec));
        const json::Value& tts = s["tts"];
        cfg.ttsBinary = util::expandHome(tts.getString("open_jtalk_bin"));
        cfg.ttsDictionary = util::expandHome(tts.getString("dictionary"));
        cfg.ttsVoiceMale = util::expandHome(tts.getString("voice_male"));
        cfg.ttsVoiceFemale = util::expandHome(tts.getString("voice_female"));
        cfg.ttsSpeed = std::clamp(tts.getNumber("speed", cfg.ttsSpeed), 0.5, 2.0);
        cfg.audioPlayer = tts.getString("player");
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not read settings.json (" << e.what() << "), using defaults.\n";
    }
    cfg.deepseekKeyFile = util::expandHome(cfg.deepseekKeyFile);
    return cfg;
}

bool Config::saveExplanationLanguage(const std::string& language) {
    explanationLanguage = language;
    const fs::path settingsPath = fs::path(rootDir) / "settings.json";
    json::Value s = json::Value::object();
    try {
        if (fs::exists(settingsPath)) s = json::parseFile(settingsPath.string());
    } catch (const std::exception&) {
        return false;  // never overwrite a settings file we could not read
    }
    json::Value ds = s["deepseek"].isObject() ? s["deepseek"] : json::Value::object();
    ds.set("explanation_language", language);
    s.set("deepseek", std::move(ds));
    return json::writeFileAtomic(settingsPath.string(), s.dump(2) + "\n");
}

std::string Config::levelsDir() const { return (fs::path(rootDir) / "data" / "levels").string(); }

std::string Config::levelDataDir(const std::string& level) const {
    return (fs::path(levelsDir()) / level).string();
}

std::string Config::levelUserDir(const std::string& level) const {
    const fs::path dir = fs::path(rootDir) / "userdata" / level;
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

std::vector<std::string> Config::availableLevels() const {
    std::vector<std::string> levels;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(levelsDir(), ec))
        if (entry.is_directory()) levels.push_back(entry.path().filename().string());
    // "N5" < "N4" < ... : easiest level first.
    std::sort(levels.begin(), levels.end(), std::greater<>());
    return levels;
}
