// Application configuration (settings.json) and filesystem layout.
//
//   <root>/settings.json              user-tweakable settings
//   <root>/data/levels/<LEVEL>/       read-only question banks (one dir per JLPT level)
//   <root>/userdata/<LEVEL>/          SRS state, mistakes, progress (written by the app)
#pragma once

#include <string>
#include <vector>

struct Config {
    std::string rootDir;
    std::string defaultLevel = "N4";
    int sessionSize = 5;
    int srsNewPerSession = 5;
    int srsMaxReviews = 20;

    std::string deepseekKeyFile = "~/Documents/deepseek";
    std::string deepseekModel = "deepseek-chat";
    std::string explanationLanguage = "Polish";
    int apiTimeoutSec = 60;

    // Listening / TTS (empty paths = auto-detect system or third_party/open_jtalk).
    std::string ttsBinary;
    std::string ttsDictionary;
    std::string ttsVoiceMale;
    std::string ttsVoiceFemale;  // optional second voice (e.g. MMDAgent "Mei")
    double ttsSpeed = 0.95;
    std::string audioPlayer;     // optional override, e.g. "aplay -q"

    // Locates the project root ($JLPT_HOME, ./, ../, or the CMake source dir)
    // and reads settings.json from it (missing keys keep their defaults).
    static Config load();
    // Updates explanationLanguage and stores it in settings.json (other keys are kept as they are).
    bool saveExplanationLanguage(const std::string& language);

    std::string levelsDir() const;
    std::string levelDataDir(const std::string& level) const;
    std::string levelUserDir(const std::string& level) const;  // created on demand
    std::vector<std::string> availableLevels() const;          // sorted N5..N1
};
