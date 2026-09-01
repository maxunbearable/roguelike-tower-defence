// Where the virtual canvas lands inside the real window.
//
// This decides whether the player can see the HUD, and it was wrong. The old
// rule was max(1, min(sw / vw, sh / vh)) with integer division, and the floor of
// 1 meant a window SMALLER than the canvas still drew at 1:1 -- so on a 1366x768
// laptop the game rendered 1408x800, 107% of the screen, and the bottom of the
// HUD (gold, lives, the wave button) was cropped away and unclickable.
//
// Tested against real display sizes, because that is what the bug was about.
#include <catch2/catch_test_macros.hpp>

#include "core/Blit.h"

using namespace td;

namespace {
constexpr int kVW = 1408, kVH = 800;

struct Display { const char* name; int w, h; };
constexpr Display kDisplays[] = {
    {"1366x768 laptop", 1366, 768}, {"1440x900", 1440, 900},
    {"1600x900", 1600, 900},        {"1920x1080", 1920, 1080},
    {"2560x1440", 2560, 1440},      {"3440x1440 ultrawide", 3440, 1440},
    {"3840x2160", 3840, 2160},      {"1280x720", 1280, 720},
};
}  // namespace

TEST_CASE("the canvas never overflows the window", "[blit]") {
    // The actual bug. If this fails, someone cannot see part of the game.
    for (const auto& d : kDisplays) {
        for (bool integerOnly : {true, false}) {
            INFO(d.name << (integerOnly ? " crisp" : " fill"));
            const auto b = core::fitCanvas(d.w, d.h, kVW, kVH, integerOnly);
            CHECK(kVW * b.scale <= static_cast<float>(d.w) + 0.5f);
            CHECK(kVH * b.scale <= static_cast<float>(d.h) + 0.5f);
            // And it must not hang off the top or left either.
            CHECK(b.offX >= -0.5f);
            CHECK(b.offY >= -0.5f);
        }
    }
}

TEST_CASE("a window smaller than the canvas scales down rather than cropping", "[blit]") {
    // 1366x768 is the case that was broken: 1366/1408 = 0.97, so the only
    // correct answer is below 1, and the old rule refused to go there.
    const auto b = core::fitCanvas(1366, 768, kVW, kVH, /*integerOnly=*/true);
    CHECK(b.scale < 1.0f);
    CHECK(b.scale > 0.9f);  // and it is a fit, not a collapse
    CHECK(kVH * b.scale <= 768.0f + 0.5f);
}

TEST_CASE("crisp mode uses whole-number scales whenever it fits", "[blit]") {
    // Integer scaling with nearest neighbour is lossless; that is the whole
    // reason to letterbox rather than fill.
    for (const auto& d : kDisplays) {
        const auto b = core::fitCanvas(d.w, d.h, kVW, kVH, /*integerOnly=*/true);
        if (b.scale < 1.0f) continue;  // below 1:1 there is no integer left
        INFO(d.name << " scale " << b.scale);
        CHECK(b.scale == static_cast<float>(static_cast<int>(b.scale)));
    }
}

TEST_CASE("fill mode fills at least one axis", "[blit]") {
    // The point of the option: no black bars on the constrained axis.
    for (const auto& d : kDisplays) {
        const auto b = core::fitCanvas(d.w, d.h, kVW, kVH, /*integerOnly=*/false);
        INFO(d.name);
        const bool fillsW = kVW * b.scale >= d.w - 1.0f;
        const bool fillsH = kVH * b.scale >= d.h - 1.0f;
        CHECK((fillsW || fillsH));
    }
}

TEST_CASE("fill mode uses more of the screen than crisp, or the same", "[blit]") {
    for (const auto& d : kDisplays) {
        const auto crisp = core::fitCanvas(d.w, d.h, kVW, kVH, true);
        const auto fill = core::fitCanvas(d.w, d.h, kVW, kVH, false);
        INFO(d.name << " crisp " << crisp.scale << " fill " << fill.scale);
        CHECK(fill.scale >= crisp.scale);
    }
}

TEST_CASE("the canvas is centred", "[blit]") {
    for (const auto& d : kDisplays) {
        const auto b = core::fitCanvas(d.w, d.h, kVW, kVH, true);
        INFO(d.name);
        // Left margin equals right margin to within a rounded pixel.
        const float right = d.w - (b.offX + kVW * b.scale);
        const float bottom = d.h - (b.offY + kVH * b.scale);
        CHECK(std::abs(right - b.offX) <= 1.0f);
        CHECK(std::abs(bottom - b.offY) <= 1.0f);
    }
}

TEST_CASE("a click maps back to the pixel it was drawn on", "[blit]") {
    // Draw and mouse mapping share this maths. If they ever disagree, clicks
    // land somewhere the player is not looking.
    for (const auto& d : kDisplays) {
        for (bool integerOnly : {true, false}) {
            const auto b = core::fitCanvas(d.w, d.h, kVW, kVH, integerOnly);
            for (const auto& p : {std::pair<float, float>{0.0f, 0.0f},
                                  {704.0f, 400.0f},
                                  {1407.0f, 799.0f}}) {
                const float wx = b.offX + p.first * b.scale;
                const float wy = b.offY + p.second * b.scale;
                const float backX = (wx - b.offX) / b.scale;
                const float backY = (wy - b.offY) / b.scale;
                INFO(d.name << " at " << p.first << "," << p.second);
                CHECK(std::abs(backX - p.first) < 0.01f);
                CHECK(std::abs(backY - p.second) < 0.01f);
            }
        }
    }
}

TEST_CASE("degenerate sizes are safe", "[blit]") {
    // A minimised window reports zero, and dividing by a zero scale would take
    // every later click with it.
    for (const auto& b : {core::fitCanvas(0, 0, kVW, kVH), core::fitCanvas(-5, 100, kVW, kVH),
                          core::fitCanvas(100, 100, 0, 0), core::fitCanvas(1, 1, kVW, kVH)}) {
        CHECK(b.scale > 0.0f);
    }
}
