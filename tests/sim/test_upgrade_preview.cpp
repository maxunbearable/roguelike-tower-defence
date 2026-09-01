#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;
using Catch::Matchers::WithinAbs;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
}  // namespace

TEST_CASE("the upgrade preview matches what upgrading actually produces",
          "[towers][preview]") {
    // The whole point of the preview is that a player can trust it. If it can
    // drift from the real result, it is worse than showing nothing.
    const auto reg = loadReg();
    for (const auto& [towerId, def] : reg.towers()) {
        sim::World w(reg, reg.map("greenfields"), 55, core::Loadout{}, 1000000);
        REQUIRE(w.placeTower(5, 0, towerId) == sim::World::PlaceResult::Ok);

        while (w.upgradeCost(5, 0) > 0) {
            sim::TowerStats predicted;
            REQUIRE(w.previewUpgrade(5, 0, predicted));
            REQUIRE(w.upgradeTower(5, 0));
            const auto& actual = w.reg().get<sim::TowerStats>(w.towerAt(5, 0));

            UNSCOPED_INFO(towerId << " at level " << w.reg().get<sim::TowerTag>(w.towerAt(5, 0)).level);
            REQUIRE_THAT(predicted.damage, WithinAbs(actual.damage, 1e-3f));
            REQUIRE_THAT(predicted.fireRate, WithinAbs(actual.fireRate, 1e-3f));
            REQUIRE_THAT(predicted.range, WithinAbs(actual.range, 1e-3f));
            REQUIRE(predicted.pierce == actual.pierce);
            REQUIRE(predicted.damageType == actual.damageType);
        }
        // At max level there is nothing to preview.
        sim::TowerStats unused;
        REQUIRE_FALSE(w.previewUpgrade(5, 0, unused));
    }
}

TEST_CASE("an upgrade preview improves the stats it claims to", "[towers][preview]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 55, core::Loadout{}, 1000000);
    REQUIRE(w.placeTower(5, 0, "arrow") == sim::World::PlaceResult::Ok);
    const auto now = w.reg().get<sim::TowerStats>(w.towerAt(5, 0));
    sim::TowerStats next;
    REQUIRE(w.previewUpgrade(5, 0, next));
    REQUIRE(next.damage > now.damage);
    REQUIRE(next.range >= now.range);
    REQUIRE(next.fireRate >= now.fireRate);
}
