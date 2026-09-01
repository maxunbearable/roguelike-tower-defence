#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>

#include "core/Music.h"

using namespace td::core;

namespace {
int peak(const Pcm& p) {
    int m = 0;
    for (const auto s : p) m = std::max(m, std::abs(static_cast<int>(s)));
    return m;
}
}  // namespace

TEST_CASE("both tracks compose to a bar-aligned length", "[music]") {
    for (const auto t : {Track::Hub, Track::Battle}) {
        const auto pcm = composeTrack(t);
        // 8 bars of 4 beats at 0.5s = 16 seconds.
        REQUIRE(pcm.size() == static_cast<size_t>(16.0f * kSampleRate));
    }
}

TEST_CASE("music never clips", "[music]") {
    // Layering a pad triad, an arpeggio, bass and percussion is exactly where a
    // naive sum wraps round into a buzz.
    for (const auto t : {Track::Hub, Track::Battle}) {
        REQUIRE(peak(composeTrack(t)) <= 32000);
    }
}

TEST_CASE("the loop seam is silent, so it does not click", "[music]") {
    for (const auto t : {Track::Hub, Track::Battle}) {
        const auto pcm = composeTrack(t);
        REQUIRE(pcm.size() > 100);
        // A loop wraps last sample -> first sample. If either end is loud, that
        // discontinuity is an audible click every 16 seconds.
        REQUIRE(std::abs(static_cast<int>(pcm.front())) < 400);
        REQUIRE(std::abs(static_cast<int>(pcm.back())) < 400);
    }
}

TEST_CASE("music has no DC offset", "[music]") {
    // A constant offset wastes headroom and can thump on some hardware.
    for (const auto t : {Track::Hub, Track::Battle}) {
        const auto pcm = composeTrack(t);
        double sum = 0.0;
        for (const auto s : pcm) sum += s;
        const double mean = sum / static_cast<double>(pcm.size());
        REQUIRE(std::abs(mean) < 200.0);
    }
}

TEST_CASE("battle is denser than hub", "[music]") {
    // Not a stylistic opinion: the battle track adds bass and percussion, so if
    // its RMS is not higher the layers are not actually being written.
    const auto rms = [](const Pcm& p) {
        double sum = 0.0;
        for (const auto s : p) sum += static_cast<double>(s) * s;
        return std::sqrt(sum / static_cast<double>(p.size()));
    };
    REQUIRE(rms(composeTrack(Track::Battle)) > rms(composeTrack(Track::Hub)));
}

TEST_CASE("composition is deterministic", "[music]") {
    REQUIRE(composeTrack(Track::Battle) == composeTrack(Track::Battle));
}

TEST_CASE("the WAV wrapper is a valid RIFF header", "[music]") {
    const auto pcm = composeTrack(Track::Hub);
    const auto wav = toWav(pcm);
    REQUIRE(wav.size() == 44 + pcm.size() * 2);
    REQUIRE(std::memcmp(wav.data(), "RIFF", 4) == 0);
    REQUIRE(std::memcmp(wav.data() + 8, "WAVE", 4) == 0);
    REQUIRE(std::memcmp(wav.data() + 36, "data", 4) == 0);

    uint32_t dataBytes = 0;
    std::memcpy(&dataBytes, wav.data() + 40, 4);
    REQUIRE(dataBytes == pcm.size() * 2);

    uint32_t rate = 0;
    std::memcpy(&rate, wav.data() + 24, 4);
    REQUIRE(rate == static_cast<uint32_t>(kSampleRate));
}
