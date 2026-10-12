#include "modules/ListeningModule.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>

#include "QuizSession.h"
#include "Terminal.h"
#include "Util.h"

namespace fs = std::filesystem;

namespace {

struct Segment {
    std::string speaker;
    std::string text;
    int pauseMs;
};

// Lays out the audio like the real exam for each question type.
std::vector<Segment> buildScript(const LanguageProfile& lang, const ListeningItem& item) {
    std::vector<Segment> s;
    if (!item.situation.empty()) s.push_back({"N", item.situation, item.spokenOptions() ? 1500 : 1200});
    for (std::size_t i = 0; i < item.lines.size(); ++i)
        s.push_back({item.lines[i].speaker, item.lines[i].text, i + 1 == item.lines.size() ? 1200 : 600});
    if (item.spokenOptions()) {
        for (std::size_t i = 0; i < item.options.size(); ++i) {
            if (i < lang.spokenNumbers.size()) s.push_back({"N", lang.spokenNumbers[i], 350});
            s.push_back({item.optionSpeaker, item.options[i], 900});
        }
    } else if (!item.question.empty() && lang.speakListeningQuestion) {
        s.push_back({"N", item.question, 300});
    }
    return s;
}

std::string hashHex(const std::string& s) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016zx", std::hash<std::string>{}(s));
    return buf;
}

}  // namespace

std::string ListeningModule::badge(const AppContext& ctx) const {
    if (ctx.bank.listeningItems().empty()) return "no data";
    return ctx.tts->available() && ctx.player.available() ? "" : "text only";
}

std::string ListeningModule::audioFor(AppContext& ctx, const ListeningItem& item) const {
    if (!ctx.tts->available() || !ctx.player.available()) return {};

    const auto script = buildScript(ctx.lang, item);
    std::string signature = ctx.tts->voiceSignature();
    for (const auto& seg : script) signature += "|" + seg.speaker + ":" + seg.text + ":" + std::to_string(seg.pauseMs);

    const fs::path dir = fs::path(ctx.userDir) / "audio";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path out = dir / (item.id + "-" + hashHex(signature) + ".wav");
    if (fs::exists(out, ec)) return out.string();

    std::vector<std::pair<std::string, int>> clips;
    bool ok = true;
    for (std::size_t i = 0; i < script.size() && ok; ++i) {
        const std::string part = (dir / (item.id + ".part" + std::to_string(i) + ".wav")).string();
        ok = ctx.tts->synthesize(script[i].text, script[i].speaker, part);
        clips.emplace_back(part, script[i].pauseMs);
    }
    ok = ok && audio::concatWav(clips, out.string());
    for (const auto& [part, pause] : clips) fs::remove(part, ec);
    if (!ok) fs::remove(out, ec);
    return ok ? out.string() : std::string{};
}

void ListeningModule::pregenerateAll(AppContext& ctx) const {
    const auto& items = ctx.bank.listeningItems();
    int ok = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const std::string msg = "Synthesizing " + std::to_string(i + 1) + "/" + std::to_string(items.size()) + " (" + items[i].id + ")";
        if (!ui::withSpinner(msg, [&] { return audioFor(ctx, items[i]); }).empty()) ++ok;
    }
    ui::info("Audio ready for " + std::to_string(ok) + "/" + std::to_string(items.size()) + " items.");
}

void ListeningModule::runSession(AppContext& ctx) const {
    for (;;) {
        const auto& items = ctx.bank.listeningItems();
        std::vector<double> weights;
        for (const auto& item : items) weights.push_back(ctx.progress.weightFor("listening:" + item.type));
        auto picked = util::weightedSample(weights, static_cast<std::size_t>(ctx.config.sessionSize), ctx.rng);

        // Exam order, e.g. 課題理解 → ポイント理解 → 発話表現 → 即時応答.
        auto rank = [&](std::size_t idx) {
            const auto& order = ctx.lang.listeningOrder;
            return std::find(order.begin(), order.end(), items[idx].type) - order.begin();
        };
        std::stable_sort(picked.begin(), picked.end(), [&](auto a, auto b) { return rank(a) < rank(b); });

        std::vector<Question> questions;
        std::vector<std::string> audioFiles;
        for (std::size_t i = 0; i < picked.size(); ++i) {
            const ListeningItem& item = items[picked[i]];
            questions.push_back(item.toQuestion(ctx.lang));
            const std::string msg = ctx.lang.preparingAudio + " " + std::to_string(i + 1) + "/" + std::to_string(picked.size());
            audioFiles.push_back(ui::withSpinner(msg, [&] { return audioFor(ctx, item); }));
        }

        quiz::SessionOptions opt;
        opt.title = nativeTitle(ctx) + "  Listening " + ctx.level;
        opt.moduleName = "Listening";
        opt.mistakeSource = "Listening";
        opt.listeningMode = true;
        opt.present = [&](std::size_t i) {
            if (audioFiles[i].empty()) {
                ui::warn("No audio available (" + ctx.tts->status() + ") — showing the transcript instead.");
                ui::printBox(ui::wrap(questions[i].passage, ui::contentWidth()), ctx.lang.transcriptTitle, ui::style::Blue);
                return;
            }
            std::cout << "  " << ui::paint(ctx.lang.playing, {ui::style::Bold, ui::style::BrightCyan})
                      << ui::paint("  (Ctrl+C stops playback)", ui::style::Gray) << std::flush;
            ctx.player.play(audioFiles[i]);
            std::cout << "\r\033[2K  " << ui::paint(ctx.lang.finished, ui::style::Cyan) << '\n';
        };

        quiz::QuizSession session(ctx, opt);
        const auto result = session.run(questions);
        if (result.answered == 0) return;
        const auto again = ui::readLine(ui::paint("[Enter] new session · [b] back ", ui::style::Gray));
        if (!again || util::toLower(*again) == "b" || util::toLower(*again) == "q") return;
    }
}

void ListeningModule::run(AppContext& ctx) {
    for (;;) {
        ui::printHeader(nativeTitle(ctx) + "  Listening", ctx.levelName() + " · sessions of " + std::to_string(ctx.config.sessionSize) + " items");
        std::cout << '\n';
        const auto& items = ctx.bank.listeningItems();
        if (items.empty()) {
            ui::warn("This level has no listening data (" + ctx.config.levelDataDir(ctx.level) + "/listening.json).");
            ui::waitForEnter();
            return;
        }

        const bool ttsOk = ctx.tts->available();
        const bool playerOk = ctx.player.available();
        ui::printBox({
            "Items in bank:  " + ui::paint(std::to_string(items.size()), ui::style::BrightWhite),
            "TTS engine:     " + (ttsOk ? ui::paint(ctx.tts->status(), ui::style::Green) : ui::paint(ctx.tts->status(), ui::style::Yellow)),
            "Audio player:   " + (playerOk ? ui::paint(ctx.player.command(), ui::style::Green) : ui::paint("none found (install pipewire/pulseaudio/alsa-utils)", ui::style::Yellow)),
            "",
            ui::paint(ctx.lang.speakListeningQuestion ? "Each item plays like the real exam: situation → dialogue → question."
                                                      : "Each item plays like the real exam; [r] replays it (TOPIK plays it twice).",
                      ui::style::Gray),
            ui::paint(ctx.lang.listeningHint, ui::style::Gray),
        }, nativeTitle(ctx), ui::style::Cyan);
        std::cout << '\n';
        std::cout << "   " << ui::paint("[Enter]", ui::style::BrightMagenta) << " start session   "
                  << ui::paint("[g]", ui::style::BrightMagenta) << " pre-generate all audio   "
                  << ui::paint("[b]", ui::style::BrightMagenta) << " back\n\n";

        const auto in = ui::readLine("");
        if (!in) return;
        const std::string cmd = util::toLower(*in);
        if (cmd == "b" || cmd == "q") return;
        if (cmd == "g") {
            if (!ttsOk) ui::warn(ctx.tts->status());
            else pregenerateAll(ctx);
            ui::waitForEnter();
            continue;
        }
        if (cmd.empty()) {
            runSession(ctx);
            return;
        }
    }
}
