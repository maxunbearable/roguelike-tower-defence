// Rendering tests. These exist because eleven rounds of UI work shipped without
// anyone being able to look at it: macOS denies a locked screen's processes an
// active display, so the GL build cannot open a window here. raylib's software
// backend has no such requirement, so the UI can be drawn, read back, and
// asserted on -- which is the only way a "the overlay covers the screen" claim
// can be checked at all.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>
#include <cstdlib>

#include "raylib.h"

#include "render/Capture.h"
#include "render/PixelCanvas.h"
#include "render/SpriteAtlas.h"
#include "ui/Paint.h"
#include "ui/Screens.h"

using namespace td;

namespace {

// One window for the whole binary: raylib's window is process-global, and
// tearing it down between tests loses the software framebuffer.
struct Display {
    Display() {
        SetTraceLogLevel(LOG_NONE);
        InitWindow(render::kVirtualW, render::kVirtualH, "uitests");
    }
    ~Display() { CloseWindow(); }
};
const Display& display() {
    static Display d;
    return d;
}

int luma(Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; }

// Draws `body` over a flat white field and reads the result back.
template <typename F>
Image renderOver(Color bg, F&& body) {
    (void)display();
    BeginDrawing();
    ClearBackground(bg);
    body();
    EndDrawing();
    return render::captureScreen();
}

}  // namespace

TEST_CASE("the settings modal dims the HUD strip, not only the board", "[overlay]") {
    render::SpriteAtlas atlas;  // no art: the scrim is what is under test
    const Image shot = renderOver(WHITE, [&] {
        ui::drawPause(atlas, 0.5f, 0.85f, /*colorAlt=*/false, /*shake=*/1.0f,
                      /*integerScaling=*/true, core::Vec2{-1.0f, -1.0f});
    });

    // The scrim used to stop at kPlayH, leaving the bottom kHudH of the canvas
    // at full brightness while the board behind it darkened.
    const int inPlay = luma(GetImageColor(shot, 40, render::kPlayH / 2));
    const int inHud = luma(GetImageColor(shot, 40, render::kPlayH + 40));
    INFO("play area luma " << inPlay << ", HUD strip luma " << inHud);

    CHECK(inPlay < 200);                      // the board is dimmed
    CHECK(inHud < 200);                       // and so is the HUD strip
    CHECK(std::abs(inPlay - inHud) < 24);     // to the same degree
    UnloadImage(shot);
}

TEST_CASE("the modal scrim reaches the bottom row of the canvas", "[overlay]") {
    render::SpriteAtlas atlas;
    const Image shot = renderOver(WHITE, [&] {
        ui::drawPause(atlas, 0.5f, 0.5f, false, 1.0f, true, core::Vec2{-1.0f, -1.0f});
    });
    // The very last row: an off-by-one in the scrim height would leave a bright
    // seam here that nothing else in the frame would reveal.
    CHECK(luma(GetImageColor(shot, render::kVirtualW / 2, render::kVirtualH - 1)) < 200);
    UnloadImage(shot);
}

TEST_CASE("capture readback is oriented the same way it was drawn", "[overlay]") {
    // Guards the capture helper itself. A flipped readback would make every
    // other rendering test assert against the wrong half of the frame, and the
    // software backend does hand its framebuffer back bottom-up.
    const Image shot = renderOver(BLACK, [] {
        DrawRectangle(0, 0, render::kVirtualW, 40, WHITE);
    });
    const int top = luma(GetImageColor(shot, render::kVirtualW / 2, 8));
    const int bottom = luma(GetImageColor(shot, render::kVirtualW / 2, render::kVirtualH - 8));
    INFO("top " << top << " bottom " << bottom);
    CHECK(top > 200);
    CHECK(bottom < 60);
    UnloadImage(shot);
}

TEST_CASE("capture readback preserves red and blue", "[overlay]") {
    // The software backend transposes them; without the correction RED reads as
    // blue, which would silently invert every colour assertion made here.
    const Image shot = renderOver(BLACK, [] {
        DrawRectangle(0, 0, 200, 200, RED);
        DrawRectangle(240, 0, 200, 200, BLUE);
    });
    const Color red = GetImageColor(shot, 100, 100);
    const Color blue = GetImageColor(shot, 340, 100);
    INFO("red sample " << (int)red.r << "," << (int)red.g << "," << (int)red.b);
    CHECK(red.r > 180);
    CHECK(red.b < 80);
    CHECK(blue.b > 180);
    CHECK(blue.r < 80);
    UnloadImage(shot);
}

TEST_CASE("wrapped text stays inside the width it was given", "[overlay]") {
    // The map cards drew their blurbs with no width limit at all, so all five
    // overflowed and ran into each other. This asserts the property the fix
    // depends on, measured with the same MeasureText the drawing uses.
    (void)display();
    const std::vector<std::string> blurbs = {
        "Open meadow. Where every build starts.",
        "Three long straights: good for reach, punishing for short range.",
        "A long winding route. Every tower gets several passes if placed well.",
        "Wide open ground with few corners: coverage matters more than depth.",
        "The last map. Heavily armoured, resistant to everything, and long.",
    };
    constexpr int kW = 250 - 36;  // the card, less its padding
    for (const auto& b : blurbs) {
        const auto lines = ui::paint::wrapToWidth(b, kW, 10, 2);
        INFO("blurb: " << b << " -> " << lines.size() << " lines");
        REQUIRE_FALSE(lines.empty());
        CHECK(lines.size() <= 2);
        for (const auto& l : lines) {
            INFO("line: \"" << l << "\" measures " << MeasureText(l.c_str(), 10));
            CHECK(MeasureText(l.c_str(), 10) <= kW);
        }
        // The unwrapped string is what used to be drawn; it must not have fit,
        // or this test would pass without the wrapping doing anything.
        if (b.size() > 45) CHECK(MeasureText(b.c_str(), 10) > kW);
    }
}
