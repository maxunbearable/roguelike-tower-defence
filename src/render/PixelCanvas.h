#pragma once

#include "raylib.h"

#include "core/Vec2.h"

namespace td::render {

// The art is authored on a 64px grid, so the game uses one. Downscaling
// detailed pixel art halves it into mush; matching the source resolution is
// what keeps it looking like the pack it came from.
inline constexpr int kTile = 64;
inline constexpr int kPlayCols = 22;
inline constexpr int kPlayRows = 11;
inline constexpr int kPlayW = kTile * kPlayCols;   // 1408
inline constexpr int kPlayH = kTile * kPlayRows;   // 704
inline constexpr int kHudH = 96;
inline constexpr int kHudY = kPlayH;
inline constexpr int kVirtualW = kPlayW;           // 1408
inline constexpr int kVirtualH = kPlayH + kHudH;   // 800

// Draws the whole game at a fixed virtual resolution, then blits it to the
// window at an INTEGER scale. Non-integer scaling is what makes pixel art
// shimmer, so it is never allowed here.
class PixelCanvas {
public:
    PixelCanvas();
    ~PixelCanvas();
    PixelCanvas(const PixelCanvas&) = delete;
    PixelCanvas& operator=(const PixelCanvas&) = delete;

    void begin();
    void end();
    void blitToWindow() const;

    float scale() const;
    // Crisp (integer) or Fill (fractional). Integer keeps pixel art exact but
    // letterboxes: at 1920x1080 the canvas uses 54% of the screen and at
    // 2560x1440 only 31%. Some players would rather fill the display.
    static void setIntegerScaling(bool on);
    core::Vec2 windowToVirtual(core::Vec2 windowPos) const;

private:
    RenderTexture2D rt_{};
};

}  // namespace td::render
