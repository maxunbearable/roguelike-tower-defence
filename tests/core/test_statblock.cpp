#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/StatBlock.h"

using td::core::ModOp;
using td::core::Modifier;
using td::core::StatBlock;
using Catch::Matchers::WithinAbs;

TEST_CASE("adds are summed and mults multiplied, adds first", "[stats]") {
    StatBlock s;
    s.setBase("arrow.damage", 10.0f);
    s.apply({"arrow.damage", ModOp::Add, 5.0f});
    s.apply({"arrow.damage", ModOp::Mult, 2.0f});
    // (10 + 5) * 2 = 30, not 10 + (5 * 2) and not (10 * 2) + 5
    REQUIRE_THAT(s.get("arrow.damage"), WithinAbs(30.0f, 1e-4f));
}

TEST_CASE("resolution is independent of the order modifiers arrive in", "[stats]") {
    StatBlock a, b;
    a.setBase("x", 100.0f);
    b.setBase("x", 100.0f);

    a.apply({"x", ModOp::Add, 10.0f});
    a.apply({"x", ModOp::Mult, 1.5f});
    a.apply({"x", ModOp::Add, 20.0f});
    a.apply({"x", ModOp::Mult, 0.5f});

    b.apply({"x", ModOp::Mult, 0.5f});
    b.apply({"x", ModOp::Add, 20.0f});
    b.apply({"x", ModOp::Mult, 1.5f});
    b.apply({"x", ModOp::Add, 10.0f});

    REQUIRE_THAT(a.get("x"), WithinAbs(b.get("x"), 1e-4f));
    REQUIRE_THAT(a.get("x"), WithinAbs(97.5f, 1e-4f));  // (100+30)*0.75
}

TEST_CASE("set overrides base, adds and mults", "[stats]") {
    StatBlock s;
    s.setBase("x", 10.0f);
    s.apply({"x", ModOp::Add, 100.0f});
    s.apply({"x", ModOp::Mult, 9.0f});
    s.apply({"x", ModOp::Set, 3.0f});
    REQUIRE_THAT(s.get("x"), WithinAbs(3.0f, 1e-4f));
}

TEST_CASE("flags form a union and are idempotent", "[stats]") {
    StatBlock s;
    REQUIRE_FALSE(s.flag("arrow.trait.rampUp"));
    s.apply({"arrow.trait.rampUp", ModOp::Flag, 0.0f});
    s.apply({"arrow.trait.rampUp", ModOp::Flag, 0.0f});
    s.apply({"arrow.trait.execute", ModOp::Flag, 0.0f});
    REQUIRE(s.flag("arrow.trait.rampUp"));
    REQUIRE(s.flag("arrow.trait.execute"));
    REQUIRE(s.flags().size() == 2);
}

TEST_CASE("an unknown path returns the caller's fallback", "[stats]") {
    StatBlock s;
    REQUIRE_THAT(s.get("nope", 7.0f), WithinAbs(7.0f, 1e-4f));
    REQUIRE_FALSE(s.has("nope"));
}

TEST_CASE("multiplying a stat that has no base stays at zero", "[stats]") {
    // A tree node that multiplies a stat the tower never declared must not
    // conjure value out of nothing.
    StatBlock s;
    s.apply({"ghost", ModOp::Mult, 5.0f});
    REQUIRE_THAT(s.get("ghost"), WithinAbs(0.0f, 1e-4f));
}

TEST_CASE("a mult of zero is honoured rather than treated as absent", "[stats]") {
    StatBlock s;
    s.setBase("x", 50.0f);
    s.apply({"x", ModOp::Mult, 0.0f});
    REQUIRE_THAT(s.get("x"), WithinAbs(0.0f, 1e-4f));
}
