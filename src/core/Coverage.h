#pragma once

#include <algorithm>

#include "core/Path.h"
#include "core/Vec2.h"

namespace td::core {

// How much of the route a tower at `centre` with radius `range` can actually
// shoot at.
//
// This is the number that decides whether one grass tile is better than another,
// and the game has never shown it. Placement writing across the genre says the
// same thing: a spot is worth what its range covers, and good play means
// overlapping rings on bends. A game that hides path coverage hides its own
// strategy layer.
//
// Returned as arc length in tiles, and as a fraction of the whole route, sampled
// along the path. `step` is the sampling interval in tiles: smaller is more
// accurate and costs more. The default resolves a 100-tile route to about a
// thousandth of its length, which is far finer than the display needs.
struct Coverage {
    float tiles = 0.0f;     // arc length of route inside the ring
    float fraction = 0.0f;  // 0..1 of the whole route
};

inline Coverage coverageOf(const Path& path, Vec2 centre, float range, float step = 0.1f) {
    Coverage out;
    const float total = path.totalLength();
    if (total <= 0.0f || range <= 0.0f || step <= 0.0f) return out;

    // Sample midpoints of each step rather than endpoints: sampling endpoints
    // double-counts the boundary and biases short routes upward.
    const float r2 = range * range;
    float covered = 0.0f;
    for (float d = 0.0f; d < total; d += step) {
        const float seg = std::min(step, total - d);
        const Vec2 p = path.positionAt(d + seg * 0.5f);
        const float dx = p.x - centre.x, dy = p.y - centre.y;
        if (dx * dx + dy * dy <= r2) covered += seg;
    }
    out.tiles = covered;
    out.fraction = covered / total;
    return out;
}

}  // namespace td::core
