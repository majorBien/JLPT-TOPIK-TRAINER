// Text-to-speech. Each engine wraps one external tool; makeTtsEngine() chains the
// ones that make sense for the study language and the operating system:
//   Japanese: Open JTalk (offline, also used for furigana) → edge-tts → macOS `say` / Windows SAPI
//   Korean:   edge-tts (neural, needs internet; audio is cached) → macOS `say` / Windows SAPI → espeak-ng
// Every clip is normalised to 24 kHz mono 16-bit WAV so clips from different engines can be joined.
#pragma once

#include <memory>
#include <string>
#include <vector>

struct Config;

// One word from the text analyser. `reading` is in katakana ("" if unknown).
struct Morpheme {
    std::string surface;
    std::string reading;
};

class TtsEngine {
public:
    virtual ~TtsEngine() = default;
    virtual std::string name() const = 0;
    virtual bool available() const = 0;
    virtual std::string status() const = 0;  // why it's (un)available, for the UI
    // Synthesizes `text` spoken by `speaker` ("M", "F", "N") into a WAV file.
    virtual bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const = 0;
    // Identifies voice settings; part of the audio cache key.
    virtual std::string voiceSignature() const = 0;
    // Splits one line of Japanese into words with readings (empty when unsupported).
    virtual std::vector<Morpheme> analyze(const std::string& text) const { (void)text; return {}; }
};

class OpenJTalkEngine : public TtsEngine {
public:
    explicit OpenJTalkEngine(const Config& cfg);

    std::string name() const override { return "Open JTalk"; }
    bool available() const override { return available_; }
    std::string status() const override { return status_; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override;
    std::vector<Morpheme> analyze(const std::string& text) const override;

private:
    std::string command(const std::string& voice, const std::string& params, const std::string& outWav,
                        const std::string& textFile, const std::string& traceFile = {}) const;

    std::string binary_;
    std::string libDir_;       // LD_LIBRARY_PATH for a bundled (third_party) Linux binary
    std::string dictionary_;
    std::string voiceMale_;
    std::string voiceFemale_;  // may be empty => male voice pitched up
    double speed_ = 1.0;
    bool available_ = false;
    std::string status_;
};

// Microsoft neural voices through the edge-tts CLI (mp3 → WAV with ffmpeg).
class EdgeTtsEngine : public TtsEngine {
public:
    EdgeTtsEngine(const Config& cfg, const std::string& language);

    std::string name() const override { return "edge-tts"; }
    bool available() const override { return !binary_.empty() && !ffmpeg_.empty(); }
    std::string status() const override { return "edge-tts neural voices"; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override;

private:
    std::string language_, binary_, ffmpeg_;
    std::string voiceFemale_, voiceMale_, voiceNarrator_, rate_;
};

// espeak-ng (robotic, but offline and tiny).
class EspeakEngine : public TtsEngine {
public:
    EspeakEngine(const Config& cfg, const std::string& language);

    std::string name() const override { return "espeak-ng"; }
    bool available() const override { return !binary_.empty(); }
    std::string status() const override { return "espeak-ng (robotic)"; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override { return language_ + "|espeak"; }

private:
    std::string language_, binary_, libDir_, dataDir_;
};

// macOS built-in voices (`say`): Kyoko/Otoya for Japanese, Yuna for Korean.
class MacSayEngine : public TtsEngine {
public:
    explicit MacSayEngine(const std::string& language);

    std::string name() const override { return "macOS voices"; }
    bool available() const override { return !voiceFemale_.empty(); }
    std::string status() const override { return "macOS voice " + voiceFemale_; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override { return "say|" + voiceFemale_ + "|" + voiceMale_; }

private:
    std::string voiceFemale_, voiceMale_;
};

// Windows built-in voices (SAPI via PowerShell): needs the Japanese / Korean speech pack.
class WindowsSapiEngine : public TtsEngine {
public:
    explicit WindowsSapiEngine(const std::string& language);
    ~WindowsSapiEngine() override;

    std::string name() const override { return "Windows voices"; }
    bool available() const override { return !script_.empty(); }
    std::string status() const override { return "Windows voices (" + culture_ + ")"; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override { return "sapi|" + culture_; }

private:
    std::string culture_, powershell_, script_;
};

// Tries its engines in order; every clip is normalised to the same WAV format.
class ChainTtsEngine : public TtsEngine {
public:
    ChainTtsEngine(std::vector<std::unique_ptr<TtsEngine>> engines, std::string missingHint);

    std::string name() const override;
    bool available() const override { return primary() != nullptr; }
    std::string status() const override;
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override;
    std::vector<Morpheme> analyze(const std::string& text) const override;

private:
    const TtsEngine* primary() const;

    std::vector<std::unique_ptr<TtsEngine>> engines_;
    std::string missingHint_;
};

std::unique_ptr<TtsEngine> makeTtsEngine(const Config& cfg);  // picks engines for cfg.language and the OS
