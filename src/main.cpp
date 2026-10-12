// JLPT Trainer — terminal app for JLPT practice (N4 data included).
#include <clocale>
#include <cstring>
#include <iostream>

#include "App.h"
#include "Config.h"

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "");
    // Keep '.' as the decimal separator (e.g. LC_NUMERIC=pl_PL would turn
    // 0.3 into "0,3" and break the JSON we send to and read from disk/API).
    std::setlocale(LC_NUMERIC, "C");
    std::ios::sync_with_stdio(true);

    Config config = Config::load();
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--level") == 0 || std::strcmp(argv[i], "-l") == 0) && i + 1 < argc) {
            config.defaultLevel = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: jlpt-trainer [--level N4]\n"
                         "Env: JLPT_HOME (project root), DEEPSEEK_API_KEY, NO_COLOR\n";
            return 0;
        }
    }
    return App(std::move(config)).run();
}
