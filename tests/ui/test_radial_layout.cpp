// The radial menu must fit on screen wherever it is opened.
//
// The bug: the menu's side PANEL clamped to the viewport but the ring of buttons
// did not. Every map has a buildable right-hand column, so opening a menu there
// put options off the edge where they could not be clicked. It was caught by
// looking at a screenshot, not by any test -- hence this one.
#include <catch2/catch_test_macros.hpp>

#include "core/RingLayout.h"

using namespace td;

namespace {
constexpr float kW = 1408.0f, kH = 700.0f, kReach = 88.0f;
}

TEST_CASE("the ring stays inside the play area wherever it opens", "[ui]") {
    const core::Vec2 asked[] = {
        {0.0f, 0.0f},   {kW, 0.0f},          {0.0f, kH},   {kW, kH},
        {kW * 0.5f, 0.0f}, {kW, kH * 0.5f},  {-500.0f, -500.0f}, {9999.0f, 9999.0f},
    };
    for (const auto& a : asked) {
        const auto c = core::clampRingCentre(a, kReach, kW, kH);
        // A button at any angle is within `reach` of the centre, so the whole
        // ring fits exactly when the centre is `reach` clear of every edge.
        CHECK(c.x - kReach >= 0.0f);
        CHECK(c.y - kReach >= 0.0f);
        CHECK(c.x + kReach <= kW);
        CHECK(c.y + kReach <= kH);
    }
}

TEST_CASE("a menu in open space is not moved", "[ui]") {
    // Clamping must only ever pull inward: a menu with room around it has to
    // appear on the tile the player actually clicked.
    const core::Vec2 mid{kW * 0.5f, kH * 0.5f};
    const auto c = core::clampRingCentre(mid, kReach, kW, kH);
    CHECK(c.x == mid.x);
    CHECK(c.y == mid.y);
}

TEST_CASE("a ring too big for the area is centred rather than flung out", "[ui]") {
    // std::clamp(v, lo, hi) with lo > hi is undefined behaviour, which is what a
    // naive clamp would hit on a viewport narrower than the ring.
    const auto c = core::clampRingCentre({10.0f, 10.0f}, 400.0f, 100.0f, 80.0f);
    CHECK(c.x == 50.0f);
    CHECK(c.y == 40.0f);
}
