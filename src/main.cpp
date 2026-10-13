// jp-kor-trainer: JLPT / TOPIK Trainer — terminal app for Japanese (JLPT) and Korean (TOPIK I) exam practice.
#include <clocale>
#include <cstring>
#include <iostream>

#include "App.h"
#include "Config.h"
#include "Platform.h"

int main(int argc, char** argv) {
    platform::initConsole();
    std::setlocale(LC_ALL, "");
    // Keep '.' as the decimal separator (e.g. LC_NUMERIC=pl_PL would turn
    // 0.3 into "0,3" and break the JSON we send to and read from disk/API).
    std::setlocale(LC_NUMERIC, "C");
    std::ios::sync_with_stdio(true);

    Config config = Config::load();
    const char* level = nullptr;
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--level") == 0 || std::strcmp(argv[i], "-l") == 0) && i + 1 < argc) {
            level = argv[++i];
        } else if (std::strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
            config.language = std::strcmp(argv[++i], "ko") == 0 ? "ko" : "ja";
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: jp-kor-trainer [--lang ja|ko] [--level N4|A1|A2…]\n"
                         "Env: JP_KOR_TRAINER_HOME (data root), DEEPSEEK_API_KEY, NO_COLOR\n";
            return 0;
        }
    }
    if (level) (config.language == "ko" ? config.defaultLevelKo : config.defaultLevel) = level;
    return App(std::move(config)).run();
}
