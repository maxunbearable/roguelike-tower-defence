#include "core/Music.h"

#include <cmath>
#include <cstring>

namespace td::core {
namespace {

// A natural minor scale in semitones. Minor because the game is dark fantasy and
// a major key would fight the art.
constexpr int kMinor[7] = {0, 2, 3, 5, 7, 8, 10};

// A i - VI - III - VII progression: the standard minor loop that resolves
// without ever brightening. Values are scale degrees, not semitones.
constexpr int kProgression[4] = {0, 5, 2, 6};

constexpr float kRootHz = 110.0f;  // A2
constexpr float kBeat = 0.5f;      // 120 bpm
constexpr int kBeatsPerBar = 4;
constexpr int kBars = 8;

float hzOf(int degree, int octave) {
    // Degrees beyond the scale wrap and carry an octave, so an arpeggio can walk
    // upward without leaving the key.
    const int wrapped = ((degree % 7) + 7) % 7;
    const int extra = static_cast<int>(std::floor(static_cast<float>(degree) / 7.0f));
    const int semis = kMinor[wrapped] + 12 * (octave + extra);
    return kRootHz * std::pow(2.0f, static_cast<float>(semis) / 12.0f);
}

size_t samplesAt(float seconds) {
    return static_cast<size_t>(seconds * static_cast<float>(kSampleRate));
}

}  // namespace

Pcm composeTrack(Track track) {
    const float barLen = kBeat * static_cast<float>(kBeatsPerBar);
    const float total = barLen * static_cast<float>(kBars);

    Pcm out(samplesAt(total), 0);

    for (int bar = 0; bar < kBars; ++bar) {
        const int chord = kProgression[bar % 4];
        const float barAt = barLen * static_cast<float>(bar);

        // --- pad: the chord, held for the whole bar -----------------------
        // Long attack and release so consecutive bars overlap into each other
        // rather than restarting audibly.
        const Envelope padEnv{0.35f, 0.2f, 0.75f, 0.5f};
        for (const int interval : {0, 2, 4}) {  // root, third, fifth
            const float hz = hzOf(chord + interval, track == Track::Hub ? 1 : 0);
            overlay(out, tone(barLen, hz, Wave::Triangle, padEnv, 0.30f), samplesAt(barAt));
        }

        // --- arpeggio: eighth notes climbing the chord --------------------
        const Envelope plucked{0.005f, 0.18f, 0.0f, 0.06f};
        const int steps = kBeatsPerBar * 2;
        for (int i = 0; i < steps; ++i) {
            // A shape that rises then falls, so it wanders instead of scrubbing.
            const int seq[8] = {0, 2, 4, 6, 4, 2, 4, 1};
            const float hz = hzOf(chord + seq[i % 8], 2);
            const float at = barAt + kBeat * 0.5f * static_cast<float>(i);
            overlay(out, tone(kBeat * 0.45f, hz, Wave::Sine, plucked, 0.22f), samplesAt(at));
        }

        if (track == Track::Battle) {
            // --- bass: root on every beat, driving --------------------------
            const Envelope bassEnv{0.004f, 0.22f, 0.15f, 0.05f};
            for (int b = 0; b < kBeatsPerBar; ++b) {
                const float at = barAt + kBeat * static_cast<float>(b);
                overlay(out, tone(kBeat * 0.8f, hzOf(chord, -1), Wave::Saw, bassEnv, 0.34f),
                        samplesAt(at));
            }

            // --- percussion: kick on 1 and 3, hat on the eighths ------------
            const Envelope kickEnv{0.002f, 0.12f, 0.0f, 0.03f};
            const Envelope hatEnv{0.001f, 0.035f, 0.0f, 0.01f};
            for (int b = 0; b < kBeatsPerBar; ++b) {
                const float at = barAt + kBeat * static_cast<float>(b);
                if (b % 2 == 0) {
                    overlay(out, thud(0.16f, 62.0f, kickEnv, 0.5f, 7u), samplesAt(at));
                }
                overlay(out, noise(0.05f, hatEnv, 0.10f, static_cast<uint32_t>(bar * 8 + b + 1)),
                        samplesAt(at + kBeat * 0.5f));
            }
        }
    }

    // The loop must be seamless: anything still ringing past the end would be
    // truncated into a click when the player wraps to the start. Trim back to an
    // exact bar boundary and fade the last few milliseconds to zero.
    out.resize(samplesAt(total), 0);
    const size_t fade = samplesAt(0.012f);
    for (size_t i = 0; i < fade && i < out.size(); ++i) {
        const float k = static_cast<float>(i) / static_cast<float>(fade);
        const size_t tail = out.size() - 1 - i;
        out[tail] = static_cast<int16_t>(static_cast<float>(out[tail]) * k);
        out[i] = static_cast<int16_t>(static_cast<float>(out[i]) * k);
    }
    return out;
}

std::vector<uint8_t> toWav(const Pcm& pcm) {
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    const uint32_t riffSize = 36 + dataBytes;
    std::vector<uint8_t> out;
    out.reserve(44 + dataBytes);

    const auto put = [&out](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };
    const auto put32 = [&](uint32_t v) { put(&v, 4); };
    const auto put16 = [&](uint16_t v) { put(&v, 2); };

    put("RIFF", 4);
    put32(riffSize);
    put("WAVE", 4);
    put("fmt ", 4);
    put32(16);                                        // fmt chunk size
    put16(1);                                         // PCM
    put16(1);                                         // mono
    put32(static_cast<uint32_t>(kSampleRate));
    put32(static_cast<uint32_t>(kSampleRate) * 2);    // byte rate
    put16(2);                                         // block align
    put16(16);                                        // bits per sample
    put("data", 4);
    put32(dataBytes);
    if (dataBytes > 0) put(pcm.data(), dataBytes);
    return out;
}

}  // namespace td::core
