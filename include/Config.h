// Application configuration (settings.json) and filesystem layout.
//
//   <root>/settings.json                    user-tweakable settings
//   <root>/data/<lang>/levels/<LEVEL>/      read-only question banks (ja: N5…N1, ko: A1, A2)
//   <root>/userdata/<LEVEL>/                SRS state, mistakes, progress (written by the app)
#pragma once

#include <string>
#include <vector>

struct Config {
    std::string rootDir;
    std::string language = "ja";      // study language: "ja" (JLPT) | "ko" (TOPIK)
    std::string defaultLevel = "N4";  // Japanese
    std::string defaultLevelKo = "A1";
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

    // Korean listening (empty = defaults / auto-detect).
    std::string koEdgeTts;       // path to the edge-tts executable
    std::string koVoiceFemale = "ko-KR-SunHiNeural";
    std::string koVoiceMale = "ko-KR-InJoonNeural";
    std::string koVoiceNarrator = "ko-KR-SunHiNeural";
    std::string koRate = "-5%";

    // Locates the project root ($JP_KOR_TRAINER_HOME, the executable's dir, ./, ../, or the CMake source dir)
    // and reads settings.json from it (missing keys keep their defaults).
    static Config load();
    // Update one setting and store it in settings.json (other keys are kept as they are).
    bool saveExplanationLanguage(const std::string& language);
    bool saveStudyLanguage(const std::string& language);
    bool saveDeepSeekKeyFile(const std::string& path);  // stored as typed (~ allowed)

    const std::string& defaultLevelFor(const std::string& language) const;
    std::string levelsDir() const;  // for the current study language
    std::string levelDataDir(const std::string& level) const;
    std::string levelUserDir(const std::string& level) const;  // created on demand
    std::vector<std::string> availableLevels() const;          // easiest first (N5…N1, A1, A2)
};
