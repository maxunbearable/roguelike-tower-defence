#include "core/Synth.h"

#include <algorithm>
#include <cmath>

namespace td::core {
namespace {

constexpr float kPi = 3.14159265358979f;

float envelopeAt(const Envelope& e, float t, float total) {
    if (t < e.attack) return e.attack > 0.0f ? t / e.attack : 1.0f;
    const float afterAttack = t - e.attack;
    if (afterAttack < e.decay) {
        const float k = e.decay > 0.0f ? afterAttack / e.decay : 1.0f;
        return 1.0f + (e.sustain - 1.0f) * k;
    }
    const float releaseStart = std::max(0.0f, total - e.release);
    if (t >= releaseStart && e.release > 0.0f) {
        return e.sustain * std::max(0.0f, 1.0f - (t - releaseStart) / e.release);
    }
    return e.sustain;
}

int16_t clamp16(float v) {
    return static_cast<int16_t>(std::clamp(v, -32000.0f, 32000.0f));
}

uint32_t nextRand(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

}  // namespace

Pcm square(float seconds, float startHz, float endHz, float duty, const Envelope& env,
           float gain) {
    const int n = std::max(1, static_cast<int>(seconds * kSampleRate));
    Pcm out(static_cast<size_t>(n));
    float phase = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        const float k = static_cast<float>(i) / static_cast<float>(n);
        const float hz = startHz + (endHz - startHz) * k;
        phase += hz / kSampleRate;
        phase -= std::floor(phase);
        const float s = phase < duty ? 1.0f : -1.0f;
        out[static_cast<size_t>(i)] =
            clamp16(s * envelopeAt(env, t, seconds) * gain * 9000.0f);
    }
    return out;
}

Pcm noise(float seconds, const Envelope& env, float gain, uint32_t seed) {
    const int n = std::max(1, static_cast<int>(seconds * kSampleRate));
    Pcm out(static_cast<size_t>(n));
    uint32_t s = seed ? seed : 1u;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        const float r = static_cast<float>(nextRand(s) >> 8) / 8388608.0f - 1.0f;
        out[static_cast<size_t>(i)] = clamp16(r * envelopeAt(env, t, seconds) * gain * 9000.0f);
    }
    return out;
}

Pcm thud(float seconds, float toneHz, const Envelope& env, float gain, uint32_t seed) {
    const int n = std::max(1, static_cast<int>(seconds * kSampleRate));
    Pcm out(static_cast<size_t>(n));
    uint32_t s = seed ? seed : 1u;
    float lowpass = 0.0f;
    float phase = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        const float k = static_cast<float>(i) / static_cast<float>(n);

        const float r = static_cast<float>(nextRand(s) >> 8) / 8388608.0f - 1.0f;
        // Brightness falls away across the sound, which is what turns a hiss
        // into an impact.
        const float cutoff = 0.55f * (1.0f - k) + 0.03f;
        lowpass += (r - lowpass) * cutoff;

        phase += (toneHz * (1.0f - 0.4f * k)) / kSampleRate;
        phase -= std::floor(phase);
        const float body = std::sin(phase * 2.0f * kPi);

        const float v = lowpass * 0.7f + body * 0.6f;
        out[static_cast<size_t>(i)] = clamp16(v * envelopeAt(env, t, seconds) * gain * 9000.0f);
    }
    return out;
}

Pcm mix(const Pcm& a, const Pcm& b) {
    Pcm out(std::max(a.size(), b.size()), 0);
    for (size_t i = 0; i < out.size(); ++i) {
        const float av = i < a.size() ? static_cast<float>(a[i]) : 0.0f;
        const float bv = i < b.size() ? static_cast<float>(b[i]) : 0.0f;
        out[i] = clamp16(av + bv);
    }
    return out;
}

Pcm concat(const std::vector<Pcm>& parts) {
    Pcm out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

}  // namespace td::core
