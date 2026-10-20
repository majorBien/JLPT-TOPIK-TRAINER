#include "TtsEngine.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "AudioPlayer.h"
#include "Config.h"
#include "Platform.h"
#include "Util.h"

namespace fs = std::filesystem;

namespace {

std::string firstExisting(std::initializer_list<std::string> candidates) {
    std::error_code ec;
    for (const auto& c : candidates)
        if (!c.empty() && fs::exists(c, ec)) return c;
    return {};
}

// Text for the TTS goes through a UTF-8 file: no quoting issues, works the same everywhere.
std::string writeTextFile(const std::string& text) { return platform::writeTempFile(text + "\n", ".txt"); }

bool fileHasAudio(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::file_size(path, ec) > 44;
}

// Per-speaker prosody. Without a dedicated female voice the male voice is
// pitched up and its all-pass constant raised (shorter vocal tract feel).
struct Prosody {
    double pitchShift;  // half-tones (-fm)
    double alpha;       // all-pass constant (-a), 0.55 is the 48 kHz default
    double speedScale;
};

Prosody prosodyFor(const std::string& speaker, bool hasFemaleVoice) {
    if (speaker == "F") return hasFemaleVoice ? Prosody{0.0, 0.55, 1.0} : Prosody{5.0, 0.58, 1.02};
    if (speaker == "M") return {-1.5, 0.54, 1.0};
    return {0.0, 0.55, 0.95};  // narrator: neutral, a little slower
}

}  // namespace

// ---- Open JTalk -------------------------------------------------------------

OpenJTalkEngine::OpenJTalkEngine(const Config& cfg) : speed_(cfg.ttsSpeed) {
    // Release archives ship a ready-made build in open_jtalk/ (scripts/build_open_jtalk.sh);
    // scripts/setup_tts.sh unpacks the Debian packages into third_party/open_jtalk/.
    const fs::path release = fs::path(cfg.rootDir) / "open_jtalk";
    const std::string releaseBin = (release / (platform::kWindows ? "open_jtalk.exe" : "open_jtalk")).string();
    const fs::path bundled = fs::path(cfg.rootDir) / "third_party" / "open_jtalk";
    const std::string bundledBin = (bundled / "usr/bin/open_jtalk").string();

    if (!cfg.ttsBinary.empty()) binary_ = platform::findCommand(cfg.ttsBinary);
    else if (fs::exists(releaseBin)) binary_ = releaseBin;
    else if (platform::commandExists("open_jtalk")) binary_ = platform::findCommand("open_jtalk");
    else if (fs::exists(bundledBin)) binary_ = bundledBin;

    if (!platform::kWindows && binary_ == bundledBin) {
        libDir_ = firstExisting({(bundled / "usr/lib/x86_64-linux-gnu").string(),
                                 (bundled / "usr/lib/aarch64-linux-gnu").string(), (bundled / "usr/lib").string()});
    }
    dictionary_ = firstExisting({cfg.ttsDictionary, (release / "dic").string(), "/var/lib/mecab/dic/open-jtalk/naist-jdic",
                                 (bundled / "var/lib/mecab/dic/open-jtalk/naist-jdic").string()});
    const std::string voiceRel = "usr/share/hts-voice/nitech-jp-atr503-m001/nitech_jp_atr503_m001.htsvoice";
    voiceMale_ = firstExisting({cfg.ttsVoiceMale, (release / "voice/nitech_jp_atr503_m001.htsvoice").string(),
                                "/" + voiceRel, (bundled / voiceRel).string()});
    voiceFemale_ = firstExisting({cfg.ttsVoiceFemale});

    if (binary_.empty()) status_ = "open_jtalk not found";
    else if (dictionary_.empty()) status_ = "Open JTalk dictionary (naist-jdic) not found";
    else if (voiceMale_.empty()) status_ = "HTS voice file not found";
    else {
        available_ = true;
        status_ = std::string("Open JTalk ready") + (voiceFemale_.empty() ? "" : " (2 voices)");
    }
}

std::string OpenJTalkEngine::voiceSignature() const {
    char buf[64];
    std::snprintf(buf, sizeof buf, "ojt|%.2f|", speed_);
    return buf + voiceMale_ + "|" + voiceFemale_;
}

std::string OpenJTalkEngine::command(const std::string& voice, const std::string& params, const std::string& outWav,
                                     const std::string& textFile, const std::string& traceFile) const {
    std::string cmd;
    if (!libDir_.empty()) cmd += "LD_LIBRARY_PATH=" + platform::quote(libDir_) + " ";
    cmd += platform::quote(binary_) + " -x " + platform::quote(dictionary_) + " -m " + platform::quote(voice) + params +
           " -ow " + platform::quote(outWav);
    if (!traceFile.empty()) cmd += " -ot " + platform::quote(traceFile);
    return cmd + " " + platform::quote(textFile);
}

bool OpenJTalkEngine::synthesize(const std::string& text, const std::string& speaker,
                                 const std::string& outWav) const {
    if (!available_) return false;
    // JLPT-style spacing between phrases is for readers only; drop it for TTS.
    const std::string spoken = util::replaceAll(util::replaceAll(text, " ", ""), "　", "");
    if (spoken.empty()) return false;
    const std::string textFile = writeTextFile(spoken);
    if (textFile.empty()) return false;

    const Prosody p = prosodyFor(speaker, !voiceFemale_.empty());
    const std::string& voice = speaker == "F" && !voiceFemale_.empty() ? voiceFemale_ : voiceMale_;
    char params[128];
    std::snprintf(params, sizeof params, " -fm %.2f -a %.2f -r %.2f", p.pitchShift, p.alpha, speed_ * p.speedScale);
    const int rc = platform::runQuiet(command(voice, params, outWav, textFile));
    std::remove(textFile.c_str());
    return rc == 0 && fileHasAudio(outWav);
}

std::vector<Morpheme> OpenJTalkEngine::analyze(const std::string& text) const {
    if (!available_ || text.empty()) return {};
    const std::string textFile = writeTextFile(text);
    if (textFile.empty()) return {};
    const std::string traceFile = textFile + ".trace";
    const std::string wavFile = textFile + ".wav";

    // -ot writes the text analysis (MeCab + NJD) next to the synthesis; the audio itself is discarded.
    const int rc = platform::runQuiet(command(voiceMale_, "", wavFile, textFile, traceFile));

    std::vector<Morpheme> words;
    std::ifstream in(traceFile);
    std::string line;
    bool inResult = false;
    while (rc == 0 && std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[Text analysis result]") { inResult = true; continue; }
        if (!inResult) continue;
        if (line.empty()) break;
        // surface,pos,pos1,pos2,pos3,ctype,cform,base,reading,pronunciation,...
        std::vector<std::string> fields;
        std::stringstream ss(line);
        for (std::string f; std::getline(ss, f, ',');) fields.push_back(f);
        if (fields.size() < 9) continue;
        words.push_back({fields[0], fields[8] == "*" ? "" : fields[8]});
    }
    in.close();
    for (const auto& f : {textFile, traceFile, wavFile}) std::remove(f.c_str());
    return words;
}

// ---- edge-tts ---------------------------------------------------------------

EdgeTtsEngine::EdgeTtsEngine(const Config& cfg, const std::string& language) : language_(language), rate_(cfg.koRate) {
    const fs::path venv = fs::path(cfg.rootDir) / "third_party" / "edge-tts" / "venv";
    const std::string venvBin =
        (platform::kWindows ? venv / "Scripts" / "edge-tts.exe" : venv / "bin" / "edge-tts").string();
    if (!cfg.koEdgeTts.empty()) binary_ = platform::findCommand(cfg.koEdgeTts);
    else if (fs::exists(venvBin)) binary_ = venvBin;
    else binary_ = platform::findCommand("edge-tts");
    ffmpeg_ = platform::findCommand("ffmpeg");

    if (language == "ko") {
        voiceFemale_ = cfg.koVoiceFemale;
        voiceMale_ = cfg.koVoiceMale;
        voiceNarrator_ = cfg.koVoiceNarrator;
    } else {
        voiceFemale_ = voiceNarrator_ = "ja-JP-NanamiNeural";
        voiceMale_ = "ja-JP-KeitaNeural";
    }
}

std::string EdgeTtsEngine::voiceSignature() const {
    return language_ + "|edge|" + voiceFemale_ + "|" + voiceMale_ + "|" + voiceNarrator_ + "|" + rate_;
}

bool EdgeTtsEngine::synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const {
    if (!available() || text.empty()) return false;
    const std::string textFile = writeTextFile(text);
    if (textFile.empty()) return false;
    const std::string& voice = speaker == "M" ? voiceMale_ : speaker == "F" ? voiceFemale_ : voiceNarrator_;
    const std::string mp3 = outWav + ".mp3";
    bool ok = platform::runQuiet(platform::quote(binary_) + " --voice " + platform::quote(voice) + " --rate=" +
                                 platform::quote(rate_) + " -f " + platform::quote(textFile) + " --write-media " +
                                 platform::quote(mp3)) == 0;
    ok = ok && platform::runQuiet(platform::quote(ffmpeg_) + " -nostdin -loglevel error -y -i " + platform::quote(mp3) +
                                  " -ar 24000 -ac 1 -sample_fmt s16 " + platform::quote(outWav)) == 0;
    std::remove(mp3.c_str());
    std::remove(textFile.c_str());
    return ok && fileHasAudio(outWav);
}

// ---- espeak-ng ----------------------------------------------------------------

EspeakEngine::EspeakEngine(const Config& cfg, const std::string& language) : language_(language) {
    const fs::path bundled = fs::path(cfg.rootDir) / "third_party" / "espeak-ng";
    if (platform::commandExists("espeak-ng")) {
        binary_ = platform::findCommand("espeak-ng");
    } else if (!platform::kWindows && fs::exists(bundled / "usr/bin/espeak-ng")) {
        binary_ = (bundled / "usr/bin/espeak-ng").string();
        libDir_ = firstExisting({(bundled / "usr/lib/x86_64-linux-gnu").string(),
                                 (bundled / "usr/lib/aarch64-linux-gnu").string()});
        dataDir_ = firstExisting({(bundled / "usr/lib/x86_64-linux-gnu/espeak-ng-data").string(),
                                  (bundled / "usr/lib/aarch64-linux-gnu/espeak-ng-data").string(),
                                  (bundled / "usr/share/espeak-ng-data").string()});
    }
}

bool EspeakEngine::synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const {
    if (!available() || text.empty()) return false;
    const std::string textFile = writeTextFile(text);
    if (textFile.empty()) return false;
    std::string cmd;
    if (!libDir_.empty()) cmd += "LD_LIBRARY_PATH=" + platform::quote(libDir_) + " ";
    if (!dataDir_.empty()) cmd += "ESPEAK_DATA_PATH=" + platform::quote(dataDir_) + " ";
    cmd += platform::quote(binary_) + " -v " + language_ + " -s 140 -p " + (speaker == "M" ? "35" : "65") + " -w " +
           platform::quote(outWav) + " -f " + platform::quote(textFile);
    const int rc = platform::runQuiet(cmd);
    std::remove(textFile.c_str());
    return rc == 0 && fileHasAudio(outWav);
}

// ---- macOS `say` ----------------------------------------------------------------

MacSayEngine::MacSayEngine(const std::string& language) {
    if (!platform::kMac || !platform::commandExists("say")) return;
    // `say -v '?'` lists "Name   locale   # sample"; keep the voices installed for our locale.
    const std::string locale = language == "ko" ? "ko_KR" : "ja_JP";
    std::vector<std::string> voices;
    std::istringstream list(platform::runCapture("say -v '?'").output);
    for (std::string line; std::getline(list, line);) {
        const auto pos = line.find(locale);
        if (pos == std::string::npos) continue;
        voices.push_back(util::trim(line.substr(0, pos)));
    }
    auto pick = [&](std::initializer_list<const char*> preferred) -> std::string {
        for (const char* p : preferred)
            for (const auto& v : voices)
                if (v.rfind(p, 0) == 0) return v;
        return {};
    };
    voiceFemale_ = language == "ko" ? pick({"Yuna", "Sora"}) : pick({"Kyoko", "O-ren"});
    voiceMale_ = language == "ko" ? pick({"Minsu", "Jian"}) : pick({"Otoya", "Hattori"});
    if (voiceFemale_.empty() && !voices.empty()) voiceFemale_ = voices.front();
    if (voiceMale_.empty()) voiceMale_ = voiceFemale_;
}

bool MacSayEngine::synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const {
    if (!available() || text.empty()) return false;
    const std::string textFile = writeTextFile(text);
    if (textFile.empty()) return false;
    const std::string& voice = speaker == "M" ? voiceMale_ : voiceFemale_;
    const int rc = platform::runQuiet("say -v " + platform::quote(voice) + " -r 170 --file-format=WAVE --data-format=LEI16@24000 -o " +
                                      platform::quote(outWav) + " -f " + platform::quote(textFile));
    std::remove(textFile.c_str());
    return rc == 0 && fileHasAudio(outWav);
}

// ---- Windows SAPI -----------------------------------------------------------------

WindowsSapiEngine::WindowsSapiEngine(const std::string& language)
    : culture_(language == "ko" ? "ko-KR" : "ja-JP") {
    if (!platform::kWindows) return;
    powershell_ = platform::findCommand("powershell");
    if (powershell_.empty()) return;
    script_ = platform::writeTempFile(R"PS(param([string]$In, [string]$Out, [string]$Culture, [string]$Gender)
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$voices = $s.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -eq $Culture }
if (-not $voices) { exit 2 }
$pick = $voices | Where-Object { $_.VoiceInfo.Gender -eq $Gender } | Select-Object -First 1
if (-not $pick) { $pick = $voices | Select-Object -First 1 }
$s.SelectVoice($pick.VoiceInfo.Name)
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(24000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$s.SetOutputToWaveFile($Out, $fmt)
$s.Speak([IO.File]::ReadAllText($In, [Text.Encoding]::UTF8))
$s.Dispose()
)PS", ".ps1");
}

WindowsSapiEngine::~WindowsSapiEngine() {
    if (!script_.empty()) std::remove(script_.c_str());
}

bool WindowsSapiEngine::synthesize(const std::string& text, const std::string& speaker,
                                   const std::string& outWav) const {
    if (!available() || text.empty()) return false;
    const std::string textFile = writeTextFile(text);
    if (textFile.empty()) return false;
    const int rc = platform::runQuiet(platform::quote(powershell_) + " -NoProfile -ExecutionPolicy Bypass -File " +
                                      platform::quote(script_) + " -In " + platform::quote(textFile) + " -Out " +
                                      platform::quote(outWav) + " -Culture " + culture_ + " -Gender " +
                                      (speaker == "M" ? "Male" : "Female"));
    std::remove(textFile.c_str());
    return rc == 0 && fileHasAudio(outWav);
}

// ---- chain ------------------------------------------------------------------------

ChainTtsEngine::ChainTtsEngine(std::vector<std::unique_ptr<TtsEngine>> engines, std::string missingHint)
    : engines_(std::move(engines)), missingHint_(std::move(missingHint)) {}

const TtsEngine* ChainTtsEngine::primary() const {
    for (const auto& e : engines_)
        if (e->available()) return e.get();
    return nullptr;
}

std::string ChainTtsEngine::name() const {
    const TtsEngine* p = primary();
    return p ? p->name() : "none";
}

std::string ChainTtsEngine::status() const {
    const TtsEngine* p = primary();
    if (!p) return missingHint_;
    std::string fallbacks;
    for (const auto& e : engines_)
        if (e.get() != p && e->available()) fallbacks += (fallbacks.empty() ? "" : ", ") + e->name();
    return p->status() + (fallbacks.empty() ? "" : " (fallback: " + fallbacks + ")");
}

std::string ChainTtsEngine::voiceSignature() const {
    const TtsEngine* p = primary();
    return p ? p->voiceSignature() : "none";
}

bool ChainTtsEngine::synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const {
    for (const auto& e : engines_) {
        if (!e->available()) continue;
        // e.g. edge-tts without internet → next engine
        if (e->synthesize(text, speaker, outWav)) return audio::normalizeWav(outWav, 24000);
    }
    return false;
}

std::vector<Morpheme> ChainTtsEngine::analyze(const std::string& text) const {
    for (const auto& e : engines_)
        if (e->available())
            if (auto words = e->analyze(text); !words.empty()) return words;
    return {};
}

std::unique_ptr<TtsEngine> makeTtsEngine(const Config& cfg) {
    std::vector<std::unique_ptr<TtsEngine>> engines;
    std::string hint;
    if (cfg.language == "ko") {
        engines.push_back(std::make_unique<EdgeTtsEngine>(cfg, "ko"));
        if (platform::kMac) engines.push_back(std::make_unique<MacSayEngine>("ko"));
        if (platform::kWindows) engines.push_back(std::make_unique<WindowsSapiEngine>("ko"));
        engines.push_back(std::make_unique<EspeakEngine>(cfg, "ko"));
        hint = platform::kWindows ? "no Korean voice — add Korean speech in Windows Settings › Time & language › Speech"
               : platform::kMac   ? "no Korean voice — add Yuna in System Settings › Accessibility › Spoken Content"
                                  : "no Korean TTS — run scripts/setup_tts_ko.sh";
    } else {
        engines.push_back(std::make_unique<OpenJTalkEngine>(cfg));
        if (platform::kMac) engines.push_back(std::make_unique<MacSayEngine>("ja"));
        engines.push_back(std::make_unique<EdgeTtsEngine>(cfg, "ja"));
        if (platform::kWindows) engines.push_back(std::make_unique<WindowsSapiEngine>("ja"));
        hint = platform::kWindows ? "no Japanese voice — add Japanese speech in Windows Settings › Time & language › Speech"
               : platform::kMac   ? "no Japanese voice — add Kyoko in System Settings › Accessibility › Spoken Content"
                                  : "open_jtalk not found — run scripts/setup_tts.sh";
    }
    return std::make_unique<ChainTtsEngine>(std::move(engines), hint);
}
