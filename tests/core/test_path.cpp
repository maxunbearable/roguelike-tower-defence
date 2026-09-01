#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/Path.h"

using td::core::Path;
using td::core::Vec2;
using Catch::Matchers::WithinAbs;

TEST_CASE("straight path length", "[path]") {
    Path p({{0, 0}, {10, 0}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("an L-shaped path sums its segments", "[path]") {
    Path p({{0, 0}, {3, 0}, {3, 4}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(7.0f, 1e-5f));
}

TEST_CASE("positionAt walks across the segment boundary", "[path]") {
    Path p({{0, 0}, {3, 0}, {3, 4}});
    REQUIRE(p.positionAt(0.0f) == Vec2{0, 0});
    REQUIRE(p.positionAt(1.5f) == Vec2{1.5f, 0});
    REQUIRE(p.positionAt(3.0f) == Vec2{3, 0});
    REQUIRE(p.positionAt(5.0f) == Vec2{3, 2});
    REQUIRE(p.positionAt(7.0f) == Vec2{3, 4});
}

TEST_CASE("positionAt clamps outside the path", "[path]") {
    Path p({{0, 0}, {10, 0}});
    REQUIRE(p.positionAt(-5.0f) == Vec2{0, 0});
    REQUIRE(p.positionAt(999.0f) == Vec2{10, 0});
}

TEST_CASE("degenerate paths do not crash", "[path]") {
    Path single({{4, 4}});
    REQUIRE_THAT(single.totalLength(), WithinAbs(0.0f, 1e-5f));
    REQUIRE(single.positionAt(3.0f) == Vec2{4, 4});

    Path none{std::vector<Vec2>{}};  // braced-empty would be ambiguous with the copy ctor
    REQUIRE(none.empty());
    REQUIRE(none.positionAt(1.0f) == Vec2{0, 0});
    REQUIRE_THAT(none.totalLength(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("zero-length segments are handled without NaN", "[path]") {
    Path p({{0, 0}, {0, 0}, {5, 0}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(5.0f, 1e-5f));
    const Vec2 mid = p.positionAt(2.5f);
    REQUIRE(mid == Vec2{2.5f, 0});
}

TEST_CASE("positionAt is monotonic along the path", "[path]") {
    Path p({{0, 0}, {3, 0}, {3, 4}, {8, 4}});
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        const float d = p.totalLength() * static_cast<float>(i) / 100.0f;
        const Vec2 at = p.positionAt(d);
        const float travelled = d;
        REQUIRE(travelled >= prev);
        prev = travelled;
        // every sampled point must lie within the path's bounding box
        REQUIRE(at.x >= 0.0f);
        REQUIRE(at.x <= 8.0f);
        REQUIRE(at.y >= 0.0f);
        REQUIRE(at.y <= 4.0f);
    }
}
