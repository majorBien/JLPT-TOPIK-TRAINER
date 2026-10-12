#include "AudioPlayer.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

#include "Util.h"

namespace audio {

namespace {

std::uint32_t readU32(const char* p) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(p[0])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(p[1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(p[2])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(p[3])) << 24;
}

std::uint16_t readU16(const char* p) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(p[0]) | static_cast<unsigned char>(p[1]) << 8);
}

void writeU32(std::ostream& out, std::uint32_t v) {
    const char b[4] = {static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16),
                       static_cast<char>(v >> 24)};
    out.write(b, 4);
}

void writeU16(std::ostream& out, std::uint16_t v) {
    const char b[2] = {static_cast<char>(v), static_cast<char>(v >> 8)};
    out.write(b, 2);
}

}  // namespace

std::optional<WavData> readWav(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::vector<char> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.size() < 12 || std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0)
        return std::nullopt;

    WavData wav;
    bool haveFmt = false;
    std::size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        const char* id = buf.data() + pos;
        const std::uint32_t size = readU32(buf.data() + pos + 4);
        const std::size_t body = pos + 8;
        const std::size_t avail = std::min<std::size_t>(size, buf.size() - body);
        if (std::memcmp(id, "fmt ", 4) == 0 && avail >= 16) {
            if (readU16(buf.data() + body) != 1) return std::nullopt;  // PCM only
            wav.channels = readU16(buf.data() + body + 2);
            wav.sampleRate = readU32(buf.data() + body + 4);
            wav.bitsPerSample = readU16(buf.data() + body + 14);
            haveFmt = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            wav.pcm.assign(buf.begin() + static_cast<std::ptrdiff_t>(body),
                           buf.begin() + static_cast<std::ptrdiff_t>(body + avail));
        }
        pos = body + size + (size & 1);
    }
    if (!haveFmt) return std::nullopt;
    return wav;
}

bool writeWav(const std::string& path, const WavData& wav) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(wav.pcm.size());
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(wav.channels * wav.bitsPerSample / 8);
    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize);
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, wav.channels);
    writeU32(out, wav.sampleRate);
    writeU32(out, wav.sampleRate * blockAlign);
    writeU16(out, blockAlign);
    writeU16(out, wav.bitsPerSample);
    out.write("data", 4);
    writeU32(out, dataSize);
    out.write(wav.pcm.data(), static_cast<std::streamsize>(wav.pcm.size()));
    return out.good();
}

bool concatWav(const std::vector<std::pair<std::string, int>>& clipsWithPauseMs, const std::string& outPath) {
    std::optional<WavData> result;
    for (const auto& [path, pauseMs] : clipsWithPauseMs) {
        auto clip = readWav(path);
        if (!clip) return false;
        if (!result) {
            result = WavData{clip->channels, clip->sampleRate, clip->bitsPerSample, {}};
        } else if (clip->channels != result->channels || clip->sampleRate != result->sampleRate ||
                   clip->bitsPerSample != result->bitsPerSample) {
            return false;
        }
        result->pcm.insert(result->pcm.end(), clip->pcm.begin(), clip->pcm.end());
        const std::size_t frameBytes = static_cast<std::size_t>(result->channels) * result->bitsPerSample / 8;
        const std::size_t silence = static_cast<std::size_t>(result->sampleRate) * static_cast<std::size_t>(pauseMs) / 1000;
        result->pcm.insert(result->pcm.end(), silence * frameBytes, 0);
    }
    return result && writeWav(outPath, *result);
}

AudioPlayer::AudioPlayer(const std::string& overrideCommand) {
    if (!overrideCommand.empty()) {
        command_ = overrideCommand;
        return;
    }
    static const std::pair<const char*, const char*> candidates[] = {
        {"pw-play", "pw-play"},
        {"paplay", "paplay"},
        {"aplay", "aplay -q"},
        {"ffplay", "ffplay -nodisp -autoexit -loglevel quiet"},
        {"mpv", "mpv --really-quiet --no-video"},
        {"afplay", "afplay"},
    };
    for (const auto& [binary, cmd] : candidates) {
        if (util::commandExists(binary)) {
            command_ = cmd;
            return;
        }
    }
}

bool AudioPlayer::play(const std::string& wavPath) const {
    if (command_.empty()) return false;
    const std::string cmd = command_ + " " + util::shellQuote(wavPath) + " >/dev/null 2>&1 </dev/null";
    return std::system(cmd.c_str()) == 0;
}

}  // namespace audio
