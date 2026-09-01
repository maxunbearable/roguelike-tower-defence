#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/NinePatch.h"

using td::core::ninePatch;
using td::core::tileRuns;
using td::core::Rect;
using Catch::Matchers::WithinAbs;

namespace {

// The nine quads come back in row-major order, so the corners are at these
// indices regardless of how the middles were sized.
constexpr int kTopLeft = 0, kTopRight = 2, kBottomLeft = 6, kBottomRight = 8;

float right(const Rect& r) { return r.x + r.w; }
float bottom(const Rect& r) { return r.y + r.h; }

}  // namespace

TEST_CASE("corners keep their source size so the border never smears", "[ninepatch]") {
    const auto q = ninePatch(64, 64, 16, 100, 200, 400, 300);
    for (int i : {kTopLeft, kTopRight, kBottomLeft, kBottomRight}) {
        REQUIRE_THAT(q[static_cast<size_t>(i)].dst.w, WithinAbs(16.0f, 1e-4f));
        REQUIRE_THAT(q[static_cast<size_t>(i)].dst.h, WithinAbs(16.0f, 1e-4f));
    }
}

TEST_CASE("the nine destination quads exactly tile the requested rectangle", "[ninepatch]") {
    const float x = 100, y = 200, w = 400, h = 300;
    const auto q = ninePatch(64, 64, 16, x, y, w, h);

    // Union of the outer edges is the requested rect: no gap, no overhang.
    REQUIRE_THAT(q[kTopLeft].dst.x, WithinAbs(x, 1e-4f));
    REQUIRE_THAT(q[kTopLeft].dst.y, WithinAbs(y, 1e-4f));
    REQUIRE_THAT(right(q[kBottomRight].dst), WithinAbs(x + w, 1e-4f));
    REQUIRE_THAT(bottom(q[kBottomRight].dst), WithinAbs(y + h, 1e-4f));

    // Every quad butts against its neighbours. Any seam here shows as a
    // transparent hairline through a painted panel.
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 2; ++col) {
            const auto& a = q[static_cast<size_t>(row * 3 + col)].dst;
            const auto& b = q[static_cast<size_t>(row * 3 + col + 1)].dst;
            REQUIRE_THAT(right(a), WithinAbs(b.x, 1e-4f));
        }
    }
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 2; ++row) {
            const auto& a = q[static_cast<size_t>(row * 3 + col)].dst;
            const auto& b = q[static_cast<size_t>((row + 1) * 3 + col)].dst;
            REQUIRE_THAT(bottom(a), WithinAbs(b.y, 1e-4f));
        }
    }
}

TEST_CASE("source quads tile the source texture", "[ninepatch]") {
    const auto q = ninePatch(64, 64, 16, 0, 0, 400, 300);
    REQUIRE_THAT(q[kTopLeft].src.x, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(q[kTopLeft].src.y, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(right(q[kBottomRight].src), WithinAbs(64.0f, 1e-4f));
    REQUIRE_THAT(bottom(q[kBottomRight].src), WithinAbs(64.0f, 1e-4f));
    // The centre source is what stretches, and it is what is left over.
    REQUIRE_THAT(q[4].src.w, WithinAbs(32.0f, 1e-4f));
    REQUIRE_THAT(q[4].src.h, WithinAbs(32.0f, 1e-4f));
}

TEST_CASE("a panel narrower than two insets clamps instead of inverting", "[ninepatch]") {
    // 20px wide with a 16px inset would give the corners 32px of a 20px box and
    // leave the centre at -12. Negative widths draw as garbage in raylib, so the
    // inset has to give way instead.
    const auto q = ninePatch(64, 64, 16, 0, 0, 20, 10);
    for (const auto& p : q) {
        REQUIRE(p.dst.w >= 0.0f);
        REQUIRE(p.dst.h >= 0.0f);
    }
    REQUIRE_THAT(right(q[kBottomRight].dst), WithinAbs(20.0f, 1e-4f));
    REQUIRE_THAT(bottom(q[kBottomRight].dst), WithinAbs(10.0f, 1e-4f));
}

TEST_CASE("an exact double-inset panel has a zero-width centre, not a negative one",
          "[ninepatch]") {
    const auto q = ninePatch(64, 64, 16, 0, 0, 32, 32);
    REQUIRE_THAT(q[4].dst.w, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(q[4].dst.h, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(q[kTopLeft].dst.w, WithinAbs(16.0f, 1e-4f));
}

TEST_CASE("tile runs cover the destination exactly", "[ninepatch]") {
    const auto runs = tileRuns(32.0f, 100.0f);
    REQUIRE(runs.size() == 4);  // 32 + 32 + 32 + 4
    float covered = 0.0f;
    for (const auto& [at, len] : runs) {
        REQUIRE_THAT(at, WithinAbs(covered, 1e-4f));  // butts against the previous run
        REQUIRE(len <= 32.0f + 1e-4f);                // never samples past the source
        REQUIRE(len > 0.0f);
        covered += len;
    }
    REQUIRE_THAT(covered, WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("an exact multiple tiles without a clipped remainder", "[ninepatch]") {
    const auto runs = tileRuns(32.0f, 96.0f);
    REQUIRE(runs.size() == 3);
    for (const auto& [at, len] : runs) REQUIRE_THAT(len, WithinAbs(32.0f, 1e-4f));
}

TEST_CASE("a degenerate region produces no tiles rather than looping forever",
          "[ninepatch]") {
    REQUIRE(tileRuns(0.0f, 100.0f).empty());
    REQUIRE(tileRuns(32.0f, 0.0f).empty());
    REQUIRE(tileRuns(32.0f, -5.0f).empty());
}
