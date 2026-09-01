// How much of the route a tower can actually shoot at.
//
// This is the figure that decides whether one buildable tile is better than
// another, and it was never computed or shown. Tested against a straight path
// where the answer is known by hand, so the test does not just agree with the
// implementation.
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <vector>

#include "core/Coverage.h"
#include "core/Path.h"

using namespace td;

namespace {
// A straight horizontal run from (0,0) to (100,0): arc length equals x.
core::Path straight() { return core::Path({{0.0f, 0.0f}, {100.0f, 0.0f}}); }
}  // namespace

TEST_CASE("a ring centred on a straight path covers its own diameter", "[coverage]") {
    const auto p = straight();
    // Centre on the path at x=50 with range 10: the ring spans x 40..60, so it
    // covers 20 tiles of a 100-tile route.
    const auto c = core::coverageOf(p, {50.0f, 0.0f}, 10.0f, 0.01f);
    CHECK(std::abs(c.tiles - 20.0f) < 0.2f);
    CHECK(std::abs(c.fraction - 0.20f) < 0.002f);
}

TEST_CASE("standing off the path covers a chord, not the diameter", "[coverage]") {
    const auto p = straight();
    // Offset 6 tiles away with range 10: half-chord = sqrt(100-36) = 8, so the
    // covered run is 16 tiles. This is the case that makes placement a decision
    // -- distance from the route costs coverage.
    const auto c = core::coverageOf(p, {50.0f, 6.0f}, 10.0f, 0.01f);
    CHECK(std::abs(c.tiles - 16.0f) < 0.2f);
}

TEST_CASE("a tower out of reach covers nothing", "[coverage]") {
    const auto p = straight();
    const auto c = core::coverageOf(p, {50.0f, 40.0f}, 10.0f);
    CHECK(c.tiles == 0.0f);
    CHECK(c.fraction == 0.0f);
}

TEST_CASE("coverage is clipped by the ends of the route", "[coverage]") {
    const auto p = straight();
    // At the very start, half the ring hangs off the end of the path, so only
    // 10 tiles are covered rather than 20.
    const auto c = core::coverageOf(p, {0.0f, 0.0f}, 10.0f, 0.01f);
    CHECK(std::abs(c.tiles - 10.0f) < 0.2f);
}

TEST_CASE("the inside of a bend is worth more than a straight", "[coverage]") {
    // The placement advice every guide gives: put rings on corners. Getting this
    // to reproduce took correcting my own model. A ring centred ON the route
    // always covers about 2x its range whatever the shape, so "corner" and
    // "straight" tie. What actually makes a bend valuable is that a tower set
    // BACK from the inside of it is near both legs at once, while the same
    // set-back on a straight only ever sees one.
    const core::Path ell({{0.0f, 0.0f}, {40.0f, 0.0f}, {40.0f, 40.0f}});

    // 6 tiles off both legs of the corner, range 10: half-chord sqrt(100-36)=8,
    // so 16 tiles from each leg, 32 in total.
    const auto inside = core::coverageOf(ell, {34.0f, 6.0f}, 10.0f, 0.01f);
    // The same 6-tile set-back against a straight stretch sees one leg: 16.
    const auto beside = core::coverageOf(ell, {20.0f, 6.0f}, 10.0f, 0.01f);

    UNSCOPED_INFO("inside corner " << inside.tiles << " vs beside straight " << beside.tiles);
    CHECK(inside.tiles > beside.tiles * 1.5f);
    CHECK(std::abs(beside.tiles - 16.0f) < 0.3f);
}

TEST_CASE("degenerate inputs are safe", "[coverage]") {
    const auto p = straight();
    CHECK(core::coverageOf(p, {50.0f, 0.0f}, 0.0f).tiles == 0.0f);
    CHECK(core::coverageOf(p, {50.0f, 0.0f}, -5.0f).tiles == 0.0f);
    CHECK(core::coverageOf(p, {50.0f, 0.0f}, 10.0f, 0.0f).tiles == 0.0f);
    const std::vector<core::Vec2> none;
    const core::Path empty(none);
    CHECK(core::coverageOf(empty, {0.0f, 0.0f}, 10.0f).tiles == 0.0f);
}
