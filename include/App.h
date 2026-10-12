// Main menu / application shell. Owns the modules and the per-level context.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AppContext.h"
#include "Config.h"
#include "modules/Module.h"

class App {
public:
    explicit App(Config config);
    int run();

private:
    void loadLevel(const std::string& level);  // throws on missing/invalid data
    void drawMainMenu() const;
    void chooseLevel();
    void settingsMenu();
    void chooseExplanationLanguage();

    Config config_;
    std::unique_ptr<AppContext> ctx_;
    std::vector<std::unique_ptr<Module>> modules_;
};
