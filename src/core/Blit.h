#pragma once

#include <algorithm>
#include <cmath>

namespace td::core {

// Where the virtual canvas lands inside the real window.
//
// Pure geometry, raylib-free, so the thing that decides whether the player can
// see the HUD is covered by tests rather than by hoping.
//
// It replaces `max(1, min(sw / vw, sh / vh))` with integer division, which had
// two problems measured across common displays:
//
//   1366x768   scale 1 -> 1408x800, i.e. 107% of the screen. The canvas was
//              LARGER than the window and the bottom of the HUD -- gold, lives,
//              the wave button -- was cropped off a very ordinary laptop.
//   2560x1440  scale 1 -> 31% of the screen used, the rest black.
//
// The floor of 1 is what caused the first, and integer-only is what causes the
// second. Both are now decisions rather than accidents.
struct BlitRect {
    float scale = 1.0f;
    float offX = 0.0f;
    float offY = 0.0f;
};

// `integerOnly` keeps pixel art crisp: at a whole-number scale with nearest
// neighbour, nothing is lost. It is honoured only while the canvas still FITS.
// Below 1:1 there is no integer option left except cropping, and a soft HUD
// beats an invisible one.
inline BlitRect fitCanvas(int screenW, int screenH, int virtW, int virtH,
                          bool integerOnly = true) {
    if (virtW <= 0 || virtH <= 0 || screenW <= 0 || screenH <= 0) return {};

    const float raw = std::min(static_cast<float>(screenW) / static_cast<float>(virtW),
                               static_cast<float>(screenH) / static_cast<float>(virtH));
    float scale = raw;
    if (integerOnly && raw >= 1.0f) scale = std::floor(raw);
    if (scale <= 0.0f) scale = raw;  // never zero, however small the window

    BlitRect out;
    out.scale = scale;
    out.offX = std::round((static_cast<float>(screenW) - virtW * scale) * 0.5f);
    out.offY = std::round((static_cast<float>(screenH) - virtH * scale) * 0.5f);
    return out;
}

}  // namespace td::core
