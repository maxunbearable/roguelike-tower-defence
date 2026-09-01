#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/Vec2.h"

using td::core::Vec2;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 arithmetic", "[vec2]") {
    REQUIRE((Vec2{1, 2} + Vec2{3, 4}) == Vec2{4, 6});
    REQUIRE((Vec2{5, 7} - Vec2{1, 2}) == Vec2{4, 5});
    REQUIRE((Vec2{2, 3} * 2.0f) == Vec2{4, 6});
}

TEST_CASE("Vec2 length and distance", "[vec2]") {
    REQUIRE_THAT(td::core::length(Vec2{3, 4}), WithinAbs(5.0f, 1e-5f));
    REQUIRE_THAT(td::core::distance(Vec2{1, 1}, Vec2{4, 5}), WithinAbs(5.0f, 1e-5f));
}

TEST_CASE("normalized returns unit length, and zero for the zero vector", "[vec2]") {
    REQUIRE_THAT(td::core::length(td::core::normalized(Vec2{0, 9})), WithinAbs(1.0f, 1e-5f));
    REQUIRE(td::core::normalized(Vec2{0, 0}) == Vec2{0, 0});
}

TEST_CASE("lerp interpolates and clamps at the endpoints", "[vec2]") {
    REQUIRE(td::core::lerp(Vec2{0, 0}, Vec2{10, 20}, 0.0f) == Vec2{0, 0});
    REQUIRE(td::core::lerp(Vec2{0, 0}, Vec2{10, 20}, 1.0f) == Vec2{10, 20});
    REQUIRE(td::core::lerp(Vec2{0, 0}, Vec2{10, 20}, 0.5f) == Vec2{5, 10});
}
