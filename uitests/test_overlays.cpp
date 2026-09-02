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

#include <filesystem>
#include "content/Registry.h"
#include "core/SaveGame.h"
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

// --- skill tree wiring ----------------------------------------------------

namespace {

// How many pixels inside the tree panel are meaningfully darker than the
// parchment they sit on. The connectors are the only thing in that band that is
// darker than the panel and wider than a glyph stroke, so this counts wiring.
int wiringPixels(const Image& shot, int minContrast) {
    const auto lum = [](Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; };
    // The panel's own colour, taken as the most common luma inside the box
    // rather than from a fixed coordinate: with no sprite atlas the panel is
    // drawn by the fallback path, so a hardcoded sample landed off it and every
    // measurement came back zero.
    int hist[256] = {0};
    std::vector<int> lums;
    for (int y = 320; y < 700; y += 2) {
        for (int x = 60; x < 1340; x += 2) {
            const int l = lum(GetImageColor(shot, x, y));
            ++hist[l];
            lums.push_back(l);
        }
    }
    int panel = 0;
    for (int i = 0; i < 256; ++i) {
        if (hist[i] > hist[panel]) panel = i;
    }
    // ABSOLUTE contrast, not "darker than the panel". Which side of the panel
    // the wiring falls on depends on whether the sprite atlas is loaded: with
    // art the panel is parchment and the links are darker, without it the
    // fallback panel is dark navy and they are lighter. Signing the comparison
    // made this measure zero in the test and 64 in the game.
    int n = 0;
    for (const int l : lums) {
        if (std::abs(panel - l) >= minContrast) ++n;
    }
    return n;
}

td::core::SaveSlot profileOwning(const td::content::Registry& reg, bool all) {
    td::core::SaveSlot s;
    s.used = true;
    s.meta.shards = 900;
    if (all) {
        for (const auto& [id, tree] : reg.trees()) {
            for (const auto& node : tree.nodes) s.meta.ownedNodes.insert(node.id);
        }
    }
    return s;
}

}  // namespace

TEST_CASE("a prerequisite link contrasts with the panel it is drawn on", "[tree]") {
    // The defect, in numbers: a walked link was drawn in the raw branch tint,
    // and the trunk tint is (198,178,148) against a panel of (204,184,141) --
    // nine luma. Rendered, the tree showed a new player its structure and hid it
    // the moment they bought anything.
    //
    // Measured here rather than counted off a screenshot: a first attempt did
    // count dark pixels in the rendered panel and was VACUOUS -- node circles
    // and captions dominated the count, and with no sprite atlas the fallback
    // panel is dark navy, so even the pale tint contrasted. It reported
    // identical figures with the bug reintroduced.
    const auto luma = [](Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; };
    const int panel = luma(ui::paint::kTreePanel);

    for (const auto& branch : ui::paint::branchNames()) {
        const Color tint = ui::paint::branchTint(branch);
        for (const bool walked : {false, true}) {
            const Color c = ui::paint::linkColour(tint, walked);
            const int d = std::abs(luma(c) - panel);
            INFO(branch << (walked ? " walked" : " unwalked") << ": luma contrast " << d);
            // 25 is the floor at which a 4px line reads on this parchment; the
            // shipped values land between 45 and 75.
            CHECK(d >= 25);
        }
    }
}

TEST_CASE("the raw branch tints are what could not be used", "[tree]") {
    // Keeps the fix honest. If somebody ever "simplifies" linkColour back to the
    // tint, the test above fails -- and this one says why it had to exist: the
    // trunk tint on its own is invisible on the panel.
    const auto luma = [](Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; };
    const int panel = luma(ui::paint::kTreePanel);
    const int raw = std::abs(luma(ui::paint::branchTint("trunk")) - panel);
    INFO("raw trunk tint against the panel: " << raw << " luma");
    CHECK(raw < 15);  // the original bug, recorded
}
