// WAV utilities (read/normalise/concatenate with pauses) and a blocking audio player:
// Windows PlaySound, macOS afplay, Linux pw-play / paplay / aplay / ffplay / mpv.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace audio {

struct WavData {
    std::uint16_t channels = 1;
    std::uint32_t sampleRate = 48000;
    std::uint16_t bitsPerSample = 16;
    std::vector<char> pcm;
};

std::optional<WavData> readWav(const std::string& path);
bool writeWav(const std::string& path, const WavData& wav);

// Converts a 16-bit PCM WAV in place to mono at `sampleRate` (linear resampling).
bool normalizeWav(const std::string& path, std::uint32_t sampleRate);

// Joins PCM clips of identical format; each clip is followed by `pauseMs` of silence.
bool concatWav(const std::vector<std::pair<std::string, int>>& clipsWithPauseMs, const std::string& outPath);

class AudioPlayer {
public:
    explicit AudioPlayer(const std::string& overrideCommand = {});

    bool available() const { return !command_.empty(); }
    const std::string& command() const { return command_; }
    // Blocks until playback ends.
    bool play(const std::string& wavPath) const;

private:
    std::string command_;
};

}  // namespace audio
