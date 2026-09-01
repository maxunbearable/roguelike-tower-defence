#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/Damage.h"

using td::core::computeDamage;
using td::core::DamageInput;
using Catch::Matchers::WithinAbs;

TEST_CASE("an unarmoured target takes raw damage", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw = 100}), WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("armour subtracts flat", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw = 100, .targetArmor = 30}), WithinAbs(70.0f, 1e-4f));
}

TEST_CASE("crit multiplies before armour is applied", "[damage]") {
    // 100 * 2 = 200, minus 30 armour = 170. If armour were applied first this
    // would be 140, which is a meaningfully different game.
    REQUIRE_THAT(computeDamage({.raw = 100, .crit = true, .critMult = 2.0f, .targetArmor = 30}),
                 WithinAbs(170.0f, 1e-4f));
}

TEST_CASE("shred and pen both reduce effective armour and stack", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw = 100, .targetArmor = 50, .armorShred = 20, .armorPen = 10}),
                 WithinAbs(80.0f, 1e-4f));
}

TEST_CASE("effective armour never goes negative, so pen never heals", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw = 100, .targetArmor = 10, .armorPen = 999}),
                 WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("the ten percent floor applies against overwhelming armour", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw = 100, .targetArmor = 1000}), WithinAbs(10.0f, 1e-4f));
}

TEST_CASE("the floor is computed after crit, not before", "[damage]") {
    // raw 100, crit x2 -> 200; floor is 10% of 200 = 20, not 10.
    REQUIRE_THAT(
        computeDamage({.raw = 100, .crit = true, .critMult = 2.0f, .targetArmor = 5000}),
        WithinAbs(20.0f, 1e-4f));
}

TEST_CASE("damage is never negative", "[damage]") {
    REQUIRE(computeDamage({.raw = 0, .targetArmor = 500}) >= 0.0f);
    REQUIRE(computeDamage({}) >= 0.0f);
}
