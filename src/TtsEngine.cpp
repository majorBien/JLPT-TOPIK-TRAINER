#include "TtsEngine.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <unistd.h>

#include "Config.h"
#include "Util.h"

namespace fs = std::filesystem;

namespace {

std::string firstExisting(std::initializer_list<std::string> candidates) {
    std::error_code ec;
    for (const auto& c : candidates)
        if (!c.empty() && fs::exists(c, ec)) return c;
    return {};
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

OpenJTalkEngine::OpenJTalkEngine(const Config& cfg) : speed_(cfg.ttsSpeed) {
    const fs::path bundled = fs::path(cfg.rootDir) / "third_party" / "open_jtalk";
    const std::string bundledBin = (bundled / "usr/bin/open_jtalk").string();

    if (!cfg.ttsBinary.empty()) binary_ = cfg.ttsBinary;
    else if (util::commandExists("open_jtalk")) binary_ = "open_jtalk";
    else if (fs::exists(bundledBin)) binary_ = bundledBin;

    if (binary_ == bundledBin) {
        libDir_ = firstExisting({(bundled / "usr/lib/x86_64-linux-gnu").string(),
                                 (bundled / "usr/lib/aarch64-linux-gnu").string(), (bundled / "usr/lib").string()});
    }
    dictionary_ = firstExisting({cfg.ttsDictionary, "/var/lib/mecab/dic/open-jtalk/naist-jdic",
                                 (bundled / "var/lib/mecab/dic/open-jtalk/naist-jdic").string()});
    const std::string voiceRel = "usr/share/hts-voice/nitech-jp-atr503-m001/nitech_jp_atr503_m001.htsvoice";
    voiceMale_ = firstExisting({cfg.ttsVoiceMale, "/" + voiceRel, (bundled / voiceRel).string()});
    voiceFemale_ = firstExisting({cfg.ttsVoiceFemale});

    if (binary_.empty()) status_ = "open_jtalk not found — run scripts/setup_tts.sh";
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

bool OpenJTalkEngine::synthesize(const std::string& text, const std::string& speaker,
                                 const std::string& outWav) const {
    if (!available_) return false;

    // JLPT-style spacing between phrases is for readers only; drop it for TTS.
    std::string spoken = util::replaceAll(util::replaceAll(text, " ", ""), "　", "");
    if (spoken.empty()) return false;

    const std::string textFile = outWav + ".txt";
    {
        std::ofstream out(textFile);
        out << spoken << '\n';
        if (!out) return false;
    }

    const Prosody p = prosodyFor(speaker, !voiceFemale_.empty());
    const std::string& voice = speaker == "F" && !voiceFemale_.empty() ? voiceFemale_ : voiceMale_;
    char params[128];
    std::snprintf(params, sizeof params, " -fm %.2f -a %.2f -r %.2f", p.pitchShift, p.alpha, speed_ * p.speedScale);

    std::string cmd;
    if (!libDir_.empty()) cmd += "LD_LIBRARY_PATH=" + util::shellQuote(libDir_) + " ";
    cmd += util::shellQuote(binary_) + " -x " + util::shellQuote(dictionary_) + " -m " + util::shellQuote(voice) +
           params + " -ow " + util::shellQuote(outWav) + " " + util::shellQuote(textFile) + " >/dev/null 2>&1";

    const int rc = std::system(cmd.c_str());
    std::remove(textFile.c_str());
    std::error_code ec;
    return rc == 0 && fs::exists(outWav, ec) && fs::file_size(outWav, ec) > 44;
}

std::vector<Morpheme> OpenJTalkEngine::analyze(const std::string& text) const {
    if (!available_ || text.empty()) return {};

    std::string base = (fs::temp_directory_path() / "jlpt-ojt-XXXXXX").string();
    const int fd = mkstemp(base.data());
    if (fd < 0) return {};
    ::close(fd);
    const std::string traceFile = base + ".trace";
    {
        std::ofstream out(base);
        out << text << '\n';
    }

    // -ot writes the text analysis (MeCab + NJD) next to the synthesis; the audio itself is discarded.
    std::string cmd;
    if (!libDir_.empty()) cmd += "LD_LIBRARY_PATH=" + util::shellQuote(libDir_) + " ";
    cmd += util::shellQuote(binary_) + " -x " + util::shellQuote(dictionary_) + " -m " + util::shellQuote(voiceMale_) +
           " -ow /dev/null -ot " + util::shellQuote(traceFile) + " " + util::shellQuote(base) + " >/dev/null 2>&1";
    const int rc = std::system(cmd.c_str());

    std::vector<Morpheme> words;
    std::ifstream in(traceFile);
    std::string line;
    bool inResult = false;
    while (rc == 0 && std::getline(in, line)) {
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
    std::remove(base.c_str());
    std::remove(traceFile.c_str());
    return words;
}

std::unique_ptr<TtsEngine> makeTtsEngine(const Config& cfg) {
    return std::make_unique<OpenJTalkEngine>(cfg);
}
