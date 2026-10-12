#include "App.h"

#include <algorithm>
#include <iostream>

#include "Terminal.h"
#include "Util.h"
#include "modules/GrammarModule.h"
#include "modules/ListeningModule.h"
#include "modules/MistakesModule.h"
#include "modules/ProgressModule.h"
#include "modules/SRSModule.h"
#include "modules/VocabularyModule.h"

App::App(Config config) : config_(std::move(config)) {
    // Menu order = registration order. Add new modules here.
    modules_.push_back(std::make_unique<VocabularyModule>());
    modules_.push_back(std::make_unique<GrammarModule>());
    modules_.push_back(std::make_unique<ListeningModule>());
    modules_.push_back(std::make_unique<SRSModule>());
    modules_.push_back(std::make_unique<MistakesModule>());
    modules_.push_back(std::make_unique<ProgressModule>());
}

void App::loadLevel(const std::string& level) {
    ctx_ = std::make_unique<AppContext>(config_, level);
}

void App::drawMainMenu() const {
    ui::clearScreen();
    const std::size_t w = ui::contentWidth();
    static const char* gradient[] = {ui::style::BrightGreen, ui::style::BrightGreen, ui::style::BrightMagenta,
                                     ui::style::BrightMagenta, ui::style::Magenta, ui::style::Magenta};
    std::vector<std::string> banner;
    banner.emplace_back();
    const LanguageProfile& lang = ctx_->lang;
    for (std::size_t i = 0; i < lang.banner.size() && i < 6; ++i)
        banner.push_back(ui::center(ui::paint(lang.banner[i], {ui::style::Bold, gradient[i]}), w));
    banner.emplace_back();
    const std::string tagline = util::replaceAll(lang.tagline, "{level}", lang.levelLabel(ctx_->level));
    banner.push_back(ui::center(ui::paint(tagline, {ui::style::Bold, ui::style::BrightYellow}), w));
    banner.push_back(ui::center(ui::paint(lang.subtitle, ui::style::Gray), w));
    ui::printBox(banner, {}, ui::style::Magenta, true);
    std::cout << '\n';

    std::vector<std::string> menu;
    for (std::size_t i = 0; i < modules_.size(); ++i) {
        const auto& m = *modules_[i];
        const std::string key = ui::paint("[" + std::to_string(i + 1) + "]", {ui::style::Bold, ui::style::BrightMagenta});
        const std::string badge = m.badge(*ctx_);
        std::string line = key + "  " + ui::padRight(ui::paint(m.nativeTitle(*ctx_), ui::style::BrightYellow), 12) + m.title(*ctx_);
        if (!badge.empty()) line = ui::padRight(line, w - ui::displayWidth(badge) - 2) + ui::paint("‹" + badge + "›", ui::style::BrightCyan);
        menu.push_back(line);
    }
    menu.emplace_back();
    const auto levels = config_.availableLevels();
    menu.push_back(ui::paint("[" + std::to_string(modules_.size() + 1) + "]", {ui::style::Bold, ui::style::BrightMagenta}) + "  " +
                   ui::padRight(ui::paint(lang.levelTitle, ui::style::BrightYellow), 12) + "Change level " +
                   ui::paint("(" + lang.levelLabel(ctx_->level) + "; available: " + std::to_string(levels.size()) + ")", ui::style::Gray));
    menu.push_back(ui::paint("[" + std::to_string(modules_.size() + 2) + "]", {ui::style::Bold, ui::style::BrightMagenta}) + "  " +
                   ui::padRight(ui::paint(lang.settingsTitle, ui::style::BrightYellow), 12) + "Settings " +
                   ui::paint("(" + lang.nativeName + " · " + lang.exam + "; explanations: " + ctx_->deepseek.language() + ")", ui::style::Gray));
    menu.push_back(ui::paint("[" + std::to_string(modules_.size() + 3) + "]", {ui::style::Bold, ui::style::BrightMagenta}) + "  " +
                   ui::padRight(ui::paint(lang.exitTitle, ui::style::BrightYellow), 12) + "Exit");
    ui::printBox(menu, lang.menuTitle, ui::style::Cyan);

    const bool ds = ctx_->deepseek.isConfigured();
    const bool tts = ctx_->tts->available() && ctx_->player.available();
    std::cout << "  " << ui::paint(ds ? "●" : "○", ds ? ui::style::BrightGreen : ui::style::BrightRed) << ' '
              << ui::paint(ctx_->deepseek.statusLine(), ui::style::Gray) << "   "
              << ui::paint(tts ? "●" : "○", tts ? ui::style::BrightGreen : ui::style::BrightYellow) << ' '
              << ui::paint(tts ? "TTS: " + ctx_->tts->name() : "TTS: " + ctx_->tts->status(), ui::style::Gray) << "\n\n";
}

void App::chooseLevel() {
    const auto levels = config_.availableLevels();
    ui::printHeader(ctx_->lang.levelTitle + "  Choose level", "Data folders found in data/" + config_.language + "/levels/");
    std::cout << '\n';
    for (std::size_t i = 0; i < levels.size(); ++i)
        std::cout << "   " << ui::paint("[" + std::to_string(i + 1) + "]", ui::style::BrightMagenta) << ' ' << ctx_->lang.levelLabel(levels[i])
                  << (levels[i] == ctx_->level ? ui::paint("  ← current", ui::style::Gray) : "") << '\n';
    std::cout << '\n';
    const auto in = ui::readLine("Level number (Enter = cancel): ");
    if (!in || in->empty()) return;
    try {
        const std::size_t n = std::stoul(*in);
        if (n < 1 || n > levels.size()) throw std::out_of_range("level");
        auto previous = ctx_->level;
        try {
            loadLevel(levels[n - 1]);
        } catch (const std::exception& e) {
            ui::error(std::string("Cannot load level: ") + e.what());
            loadLevel(previous);
            ui::waitForEnter();
        }
    } catch (const std::exception&) {
        ui::warn("Invalid choice.");
        ui::waitForEnter();
    }
}

void App::settingsMenu() {
    for (;;) {
        ui::clearScreen();
        ui::printHeader("設定  Settings", "Saved to settings.json");
        std::cout << '\n';
        std::cout << "   " << ui::paint("[1]", ui::style::BrightMagenta) << " Study language / exam:            "
                  << ui::paint(ctx_->lang.nativeName + " · " + ctx_->lang.exam, {ui::style::Bold, ui::style::BrightGreen}) << '\n';
        std::cout << "   " << ui::paint("[2]", ui::style::BrightMagenta) << " Explanation language (DeepSeek):  "
                  << ui::paint(ctx_->deepseek.language(), {ui::style::Bold, ui::style::BrightGreen}) << "\n\n";
        const auto in = ui::readLine("Setting number (Enter = back): ");
        if (!in || in->empty() || util::toLower(*in) == "b" || util::toLower(*in) == "q") return;
        if (*in == "1") chooseStudyLanguage();
        if (*in == "2") chooseExplanationLanguage();
    }
}

void App::chooseStudyLanguage() {
    ui::clearScreen();
    ui::printHeader("Study language", "Switches the whole trainer: data, exam format, voices");
    std::cout << '\n';
    const auto& ids = languageIds();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const LanguageProfile& p = languageProfile(ids[i]);
        std::cout << "   " << ui::paint("[" + std::to_string(i + 1) + "]", ui::style::BrightMagenta) << ' '
                  << ui::padRight(p.nativeName, 8) << ui::padRight(p.name, 10) << ui::paint(p.exam, ui::style::Gray)
                  << (p.id == ctx_->lang.id ? ui::paint("  ← current", ui::style::BrightGreen) : "") << '\n';
    }
    std::cout << '\n';
    const auto in = ui::readLine("Language number (Enter = cancel): ");
    if (!in || in->empty()) return;
    std::size_t n = 0;
    try {
        n = std::stoul(*in);
    } catch (const std::exception&) {
    }
    if (n < 1 || n > ids.size()) {
        ui::warn("Invalid choice.");
        ui::waitForEnter();
        return;
    }
    const std::string previous = config_.language;
    const std::string chosen = ids[n - 1];
    if (chosen == previous) return;

    config_.language = chosen;
    std::string level = config_.defaultLevelFor(chosen);
    const auto levels = config_.availableLevels();
    if (std::find(levels.begin(), levels.end(), level) == levels.end() && !levels.empty()) level = levels.front();
    try {
        loadLevel(level);
    } catch (const std::exception& e) {
        ui::error(std::string("Cannot load ") + languageProfile(chosen).name + " data: " + e.what());
        config_.language = previous;
        loadLevel(config_.defaultLevelFor(previous));
        ui::waitForEnter();
        return;
    }
    if (!config_.saveStudyLanguage(chosen)) ui::warn("Switched for now, but settings.json could not be saved.");
}

void App::chooseExplanationLanguage() {
    // The name is passed to DeepSeek as-is ("Always answer in <language>"), so any language works.
    static const std::vector<std::pair<std::string, std::string>> languages = {
        {"Polish", "polski"},   {"English", "English"},   {"Japanese", "日本語"},   {"Korean", "한국어"},   {"German", "Deutsch"},
        {"Spanish", "español"}, {"French", "français"},   {"Ukrainian", "українська"}, {"Italian", "italiano"},
    };
    ui::clearScreen();
    ui::printHeader("言語  Explanation language", "Language DeepSeek uses to explain your mistakes");
    std::cout << '\n';
    for (std::size_t i = 0; i < languages.size(); ++i) {
        const auto& [name, native] = languages[i];
        std::cout << "   " << ui::paint("[" + std::to_string(i + 1) + "]", ui::style::BrightMagenta) << ' '
                  << ui::padRight(name, 11) << ui::paint(native, ui::style::Gray)
                  << (name == ctx_->deepseek.language() ? ui::paint("  ← current", ui::style::BrightGreen) : "") << '\n';
    }
    std::cout << "   " << ui::paint("[o]", ui::style::BrightMagenta) << " Other… " << ui::paint("(type any language)", ui::style::Gray)
              << "\n\n";

    const auto in = ui::readLine("Language number (Enter = cancel): ");
    if (!in || in->empty()) return;
    std::string chosen;
    if (util::toLower(*in) == "o") {
        const auto custom = ui::readLine("Language name in English, e.g. Czech: ");
        if (!custom) return;
        chosen = util::trim(*custom);
    } else {
        try {
            const std::size_t n = std::stoul(*in);
            if (n >= 1 && n <= languages.size()) chosen = languages[n - 1].first;
        } catch (const std::exception&) {
        }
    }
    if (chosen.empty()) {
        ui::warn("Invalid choice.");
        ui::waitForEnter();
        return;
    }

    ctx_->deepseek.setLanguage(chosen);
    if (config_.saveExplanationLanguage(chosen)) ui::info("Explanations will now be in " + chosen + ".");
    else ui::warn("Using " + chosen + " for now, but settings.json could not be saved.");
    ui::waitForEnter();
}

int App::run() {
    try {
        loadLevel(config_.defaultLevelFor(config_.language));
    } catch (const std::exception& e) {
        ui::error(std::string("Failed to load level ") + config_.defaultLevelFor(config_.language) + ": " + e.what());
        ui::info("Data directory: " + config_.levelsDir() + "  (set JLPT_HOME to the project root)");
        return 1;
    }

    for (;;) {
        drawMainMenu();
        const auto in = ui::readLine("Choose an option: ");
        if (!in) break;
        const std::string cmd = util::toLower(*in);
        // Modules 1…n, then level, settings, exit — numbers only.
        const std::string levelKey = std::to_string(modules_.size() + 1);
        const std::string settingsKey = std::to_string(modules_.size() + 2);
        const std::string exitKey = std::to_string(modules_.size() + 3);
        if (cmd == exitKey || cmd == "q" || cmd == "0" || cmd == "exit") break;
        if (cmd == levelKey) { chooseLevel(); continue; }
        if (cmd == settingsKey) { settingsMenu(); continue; }
        try {
            const std::size_t n = std::stoul(cmd);
            if (n >= 1 && n <= modules_.size()) modules_[n - 1]->run(*ctx_);
        } catch (const std::invalid_argument&) {
            // not a number — just redraw the menu
        } catch (const std::out_of_range&) {
        }
    }
    ui::clearScreen();
    std::cout << "\n  " << ui::paint(ctx_->lang.goodbye, {ui::style::Bold, ui::style::BrightYellow}) << "\n\n";
    return 0;
}
