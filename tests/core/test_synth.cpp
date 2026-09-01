// The synthesiser is pure maths, so it can be verified without an audio device.
// These catch the failure modes that produce silence, clicks or blown speakers.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "core/Synth.h"

using namespace td::core;

static int peak(const Pcm& p) {
    int m = 0;
    for (const auto s : p) m = std::max(m, std::abs(static_cast<int>(s)));
    return m;
}

TEST_CASE("generators produce the requested length", "[synth]") {
    const float secs = 0.1f;
    const auto expected = static_cast<size_t>(secs * kSampleRate);
    REQUIRE(square(secs, 440, 440, 0.5f, {}).size() == expected);
    REQUIRE(noise(secs, {}).size() == expected);
    REQUIRE(thud(secs, 200, {}).size() == expected);
}

TEST_CASE("generators are audible but never clip", "[synth]") {
    for (const auto& p : {square(0.1f, 440, 220, 0.5f, {0.002f, 0.09f, 0.0f, 0.01f}),
                          noise(0.1f, {0.002f, 0.09f, 0.0f, 0.01f}),
                          thud(0.1f, 200, {0.002f, 0.09f, 0.0f, 0.01f})}) {
        REQUIRE(peak(p) > 1000);    // not silence
        REQUIRE(peak(p) <= 32000);  // and not clipped
    }
}

TEST_CASE("sounds start and end near zero so they do not click", "[synth]") {
    // A waveform that begins or ends at full amplitude produces an audible pop
    // on every play, which is the classic tell of hand-rolled audio.
    const auto p = square(0.1f, 600, 300, 0.5f, {0.004f, 0.09f, 0.0f, 0.02f});
    REQUIRE(std::abs(static_cast<int>(p.front())) < 2000);
    REQUIRE(std::abs(static_cast<int>(p.back())) < 2000);
}

TEST_CASE("the envelope actually decays", "[synth]") {
    const auto p = square(0.2f, 440, 440, 0.5f, {0.002f, 0.18f, 0.0f, 0.01f});
    const auto quarter = p.size() / 4;
    int early = 0, late = 0;
    for (size_t i = 0; i < quarter; ++i) early = std::max(early, std::abs(static_cast<int>(p[i])));
    for (size_t i = p.size() - quarter; i < p.size(); ++i) {
        late = std::max(late, std::abs(static_cast<int>(p[i])));
    }
    REQUIRE(early > late * 2);
}

TEST_CASE("mix and concat behave", "[synth]") {
    const auto a = square(0.05f, 440, 440, 0.5f, {});
    const auto b = square(0.10f, 220, 220, 0.5f, {});
    REQUIRE(mix(a, b).size() == b.size());        // takes the longer
    REQUIRE(concat({a, b}).size() == a.size() + b.size());
}

TEST_CASE("synthesis is deterministic", "[synth]") {
    REQUIRE(noise(0.05f, {}, 1.0f, 7u) == noise(0.05f, {}, 1.0f, 7u));
    REQUIRE(thud(0.05f, 200, {}, 1.0f, 3u) == thud(0.05f, 200, {}, 1.0f, 3u));
}

TEST_CASE("a zero-length request does not crash", "[synth]") {
    REQUIRE_FALSE(square(0.0f, 440, 440, 0.5f, {}).empty());  // clamped to one sample
    REQUIRE_FALSE(noise(0.0f, {}).empty());
}
