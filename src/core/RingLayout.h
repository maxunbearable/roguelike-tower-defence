#pragma once

#include <algorithm>

#include "core/Vec2.h"

namespace td::core {

// Where a radial menu's ring can actually be centred.
//
// Pure geometry, and raylib-free, so it can be tested headlessly -- the same
// reason NinePatch lives here. The bug it exists to prevent: the menu's side
// panel clamped to the viewport while the RING of buttons did not, so a tower in
// the buildable right-hand column of any map opened a menu whose options hung
// off the screen edge and could not be clicked.
//
// `reach` is the distance from the centre to the outer edge of a button, so the
// whole ring fits exactly when the centre is at least `reach` from every side.
inline Vec2 clampRingCentre(Vec2 c, float reach, float areaW, float areaH) {
    // A ring wider than the area cannot fit; centring it is the least-bad
    // answer and keeps the result inside the box on that axis.
    const float x = areaW < reach * 2.0f ? areaW * 0.5f
                                         : std::clamp(c.x, reach, areaW - reach);
    const float y = areaH < reach * 2.0f ? areaH * 0.5f
                                         : std::clamp(c.y, reach, areaH - reach);
    return {x, y};
}

}  // namespace td::core
