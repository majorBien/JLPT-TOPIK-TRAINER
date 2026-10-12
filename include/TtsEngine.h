// Text-to-speech abstraction.
//   Japanese: Open JTalk (open-source HMM-based TTS, BSD license) through its CLI.
//   Korean:   edge-tts (Microsoft neural voices, needs internet; audio is cached by the
//             listening module) with espeak-ng as an offline fallback.
// Another engine (VOICEVOX, Piper, ...) can be added by implementing TtsEngine.
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
    // Splits one line of Japanese into words with readings (empty on failure).
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
    std::string binary_;
    std::string libDir_;       // LD_LIBRARY_PATH for a bundled (third_party) binary
    std::string dictionary_;
    std::string voiceMale_;
    std::string voiceFemale_;  // may be empty => male voice pitched up
    double speed_ = 1.0;
    bool available_ = false;
    std::string status_;
};

class KoreanTtsEngine : public TtsEngine {
public:
    explicit KoreanTtsEngine(const Config& cfg);

    std::string name() const override { return edgeTts_.empty() ? "espeak-ng" : "edge-tts"; }
    bool available() const override { return !edgeTts_.empty() || !espeak_.empty(); }
    std::string status() const override { return status_; }
    bool synthesize(const std::string& text, const std::string& speaker, const std::string& outWav) const override;
    std::string voiceSignature() const override;

private:
    bool viaEdgeTts(const std::string& textFile, const std::string& speaker, const std::string& outWav) const;
    bool viaEspeak(const std::string& textFile, const std::string& speaker, const std::string& outWav) const;

    std::string edgeTts_;      // edge-tts executable (venv in third_party/edge-tts or $PATH)
    std::string espeak_;       // espeak-ng executable
    std::string espeakLibDir_; // LD_LIBRARY_PATH for a bundled espeak-ng
    std::string espeakData_;   // ESPEAK_DATA_PATH for a bundled espeak-ng
    std::string voiceFemale_, voiceMale_, voiceNarrator_;
    std::string rate_;         // edge-tts rate, e.g. "-5%"
    std::string status_;
};

std::unique_ptr<TtsEngine> makeTtsEngine(const Config& cfg);  // picks the engine for cfg.language
