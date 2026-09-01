#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/Rng.h"

using td::core::Rng;

static std::vector<float> draw(uint64_t seed, int n) {
    Rng r(seed);
    std::vector<float> v;
    for (int i = 0; i < n; ++i) v.push_back(r.unit());
    return v;
}

TEST_CASE("the same seed reproduces the same sequence", "[rng]") {
    REQUIRE(draw(1234, 50) == draw(1234, 50));
}

TEST_CASE("different seeds diverge", "[rng]") {
    REQUIRE(draw(1234, 50) != draw(5678, 50));
}

TEST_CASE("unit stays in [0,1)", "[rng]") {
    Rng r(99);
    for (int i = 0; i < 10000; ++i) {
        const float v = r.unit();
        REQUIRE(v >= 0.0f);
        REQUIRE(v < 1.0f);
    }
}

TEST_CASE("chance(0) never fires, chance(1) always fires, neither consumes a draw", "[rng]") {
    Rng guarded(7);
    for (int i = 0; i < 200; ++i) {
        REQUIRE_FALSE(guarded.chance(0.0f));
        REQUIRE(guarded.chance(1.0f));
    }
    // The guard branches must not perturb the stream, or a tower with 0% crit
    // would desync the simulation from one with crit disabled another way.
    Rng untouched(7);
    REQUIRE(guarded.unit() == untouched.unit());
}

TEST_CASE("range is inclusive at both ends and never escapes", "[rng]") {
    Rng r(3);
    bool sawLo = false, sawHi = false;
    for (int i = 0; i < 500; ++i) {
        const int v = r.range(2, 5);
        REQUIRE(v >= 2);
        REQUIRE(v <= 5);
        if (v == 2) sawLo = true;
        if (v == 5) sawHi = true;
    }
    REQUIRE(sawLo);
    REQUIRE(sawHi);
}

TEST_CASE("the seed is retrievable for save files", "[rng]") {
    REQUIRE(Rng(424242).seed() == 424242u);
}
