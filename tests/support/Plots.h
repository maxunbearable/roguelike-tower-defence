#pragma once

// Build plots are a finite authored set now, not "any open grass", so a test
// that only needs "somewhere buildable" must ask the map where that is rather
// than knowing. PLOT(n) yields the nth plot in reading order as a coordinate
// pair, so distinct n stay distinct tiles -- which is all these tests ever
// relied on. Tests that genuinely care WHERE a tower stands (range, path
// distance, adjacency) still name their tile.

#include <filesystem>
#include <utility>
#include <vector>

#include "content/Registry.h"

namespace tdtest {

inline const std::vector<std::pair<int, int>>& plots() {
    static const std::vector<std::pair<int, int>> v = [] {
        td::content::Registry r;
        r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
        const auto& m = r.map("greenfields");
        std::vector<std::pair<int, int>> out;
        for (int y = 0; y < m.gridH; ++y)
            for (int x = 0; x < m.gridW; ++x)
                if (m.buildableAt(x, y)) out.emplace_back(x, y);
        return out;
    }();
    return v;
}

inline int plotX(int n) { return plots()[static_cast<size_t>(n) % plots().size()].first; }
inline int plotY(int n) { return plots()[static_cast<size_t>(n) % plots().size()].second; }

}  // namespace tdtest

#define PLOT(n) tdtest::plotX(n), tdtest::plotY(n)

namespace tdtest {

// The plot nearest a point on the board, for tests that need a tower which can
// actually reach a particular stretch of road. PLOT(n) says "somewhere
// buildable"; this says "somewhere buildable NEAR HERE", which is what a test
// about range or knockback actually depends on.
inline std::pair<int, int> plotNear(float x, float y) {
    std::pair<int, int> best = plots().front();
    float bestD = 1e30f;
    for (const auto& p : plots()) {
        const float dx = static_cast<float>(p.first) + 0.5f - x;
        const float dy = static_cast<float>(p.second) + 0.5f - y;
        if (dx * dx + dy * dy < bestD) {
            bestD = dx * dx + dy * dy;
            best = p;
        }
    }
    return best;
}

}  // namespace tdtest
