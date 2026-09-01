#pragma once

#include <cstdint>
#include <vector>

namespace td::core {

// Procedural sound synthesis. Kept free of raylib so the waveform maths can be
// unit-tested, and kept in the project so every sound is original work with no
// licensing question attached to it.
inline constexpr int kSampleRate = 22050;

struct Envelope {
    float attack = 0.002f;
    float decay = 0.10f;
    float sustain = 0.0f;   // level held after decay, 0..1
    float release = 0.02f;
};

// All generators return 16-bit mono PCM at kSampleRate.
using Pcm = std::vector<int16_t>;

// A square wave that sweeps from one pitch to another over its length. Sweeps
// are what make a blip read as "up" (good) or "down" (bad) without any words.
Pcm square(float seconds, float startHz, float endHz, float duty, const Envelope& env,
           float gain = 1.0f);
Pcm noise(float seconds, const Envelope& env, float gain = 1.0f, uint32_t seed = 1);
// Noise whose brightness falls over time, which is what an impact sounds like.
Pcm thud(float seconds, float toneHz, const Envelope& env, float gain = 1.0f, uint32_t seed = 1);

Pcm mix(const Pcm& a, const Pcm& b);
Pcm concat(const std::vector<Pcm>& parts);

}  // namespace td::core
