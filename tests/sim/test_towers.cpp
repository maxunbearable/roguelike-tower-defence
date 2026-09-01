#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
static void advance(sim::World& w, float s) {
    const int n = static_cast<int>(s / sim::kFixedDt);
    for (int i = 0; i < n; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("placement obeys buildability, occupancy and bounds", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.placeTower(1, 1, "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.placeTower(1, 1, "arrow") == sim::World::PlaceResult::Occupied);
    REQUIRE(w.placeTower(5, 2, "arrow") == sim::World::PlaceResult::NotBuildable);   // path
    REQUIRE(w.placeTower(0, 2, "arrow") == sim::World::PlaceResult::NotBuildable);   // spawn
    REQUIRE(w.placeTower(-1, 0, "arrow") == sim::World::PlaceResult::OutOfBounds);
    REQUIRE(w.placeTower(30, 0, "arrow") == sim::World::PlaceResult::OutOfBounds);
    // A deliberately non-existent id. This used to say "ballista", which became
    // a real tower.
    REQUIRE(w.placeTower(2, 1, "no_such_tower") == sim::World::PlaceResult::UnknownTower);
}

TEST_CASE("placing deducts gold and selling refunds a fraction", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    const int before = w.gold();
    REQUIRE(w.placeTower(1, 1, "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.gold() == before - 60);
    REQUIRE(w.sellTower(1, 1));
    REQUIRE(w.gold() == before - 60 + 36);  // 60 * 0.6
    REQUIRE((w.towerAt(1, 1) == entt::null));  // parens: Catch2 vs EnTT null_t ambiguity
    REQUIRE_FALSE(w.sellTower(1, 1));
}

TEST_CASE("gold runs out", "[towers]") {
    const auto reg = loadReg();
    // Expressed against the world's actual starting gold rather than a
    // hardcoded 220, so a global-tree economy node cannot silently break it.
    sim::World w(reg, reg.map("greenfields"), 1);
    const int cost = reg.tower("arrow").buildCost;
    const int affordable = w.gold() / cost;
    REQUIRE(affordable >= 1);
    for (int i = 0; i < affordable; ++i) {
        REQUIRE(w.placeTower(1 + i, 1, "arrow") == sim::World::PlaceResult::Ok);
    }
    REQUIRE(w.gold() < cost);
    REQUIRE(w.placeTower(1 + affordable, 1, "arrow") == sim::World::PlaceResult::TooPoor);
}

TEST_CASE("upgrading raises level and applies the authored multiplier", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/1000);
    w.placeTower(1, 1, "arrow");
    const auto t = w.towerAt(1, 1);
    const float baseDamage = w.reg().get<sim::TowerStats>(t).damage;

    REQUIRE(w.upgradeCost(1, 1) == 75);
    REQUIRE(w.upgradeTower(1, 1));
    REQUIRE(w.reg().get<sim::TowerTag>(t).level == 2);
    REQUIRE(w.reg().get<sim::TowerStats>(t).damage == baseDamage * 1.6f);

    REQUIRE(w.upgradeCost(1, 1) == 140);
    REQUIRE(w.upgradeTower(1, 1));
    REQUIRE(w.reg().get<sim::TowerTag>(t).level == 3);
    // multipliers are absolute against base, not cumulative
    REQUIRE(w.reg().get<sim::TowerStats>(t).damage == baseDamage * 2.4f);

    REQUIRE(w.upgradeCost(1, 1) == -1);  // no level 4 authored
    REQUIRE_FALSE(w.upgradeTower(1, 1));
}

TEST_CASE("a tower kills enemies walking past it and earns bounty", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(3, 1, "arrow");  // flanks the y=2 path run
    w.placeTower(6, 1, "arrow");
    const int goldAfterBuilding = w.gold();
    w.startNextWave();
    advance(w, 50.0f);
    REQUIRE(w.gold() > goldAfterBuilding);              // bounty collected
    REQUIRE(w.lives() > sim::kStartingLives - 8);       // not everything leaked
}

TEST_CASE("towers do not shoot beyond their range", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(28, 0, "arrow");  // far corner; nothing on wave 1 comes within 3.5 tiles
    const int lives = w.lives();
    w.startNextWave();
    advance(w, 50.0f);
    REQUIRE(w.lives() == lives - 8);  // every slime leaked, so nothing was shot
}

TEST_CASE("targeting first picks the enemy furthest along the path", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(3, 1, "arrow");
    w.startNextWave();
    advance(w, 6.0f);  // several slimes strung out along the route

    const auto t = w.towerAt(3, 1);
    const auto target = w.reg().get<sim::TargetRef>(t).e;
    REQUIRE((target != entt::null));

    auto& r = w.reg();
    const float chosen = r.get<sim::PathFollower>(target).distance;
    const float range = r.get<sim::TowerStats>(t).range;
    const auto tpos = r.get<sim::Position>(t).v;
    r.view<const sim::PathFollower, const sim::Position, const sim::EnemyTag>().each(
        [&](const sim::PathFollower& pf, const sim::Position& pos, const sim::EnemyTag&) {
            if (core::distance(pos.v, tpos) <= range) REQUIRE(pf.distance <= chosen);
        });
}

TEST_CASE("combat stays deterministic for a fixed seed", "[towers]") {
    const auto reg = loadReg();
    auto run = [&] {
        sim::World w(reg, reg.map("greenfields"), 777);
        w.placeTower(3, 1, "arrow");
        w.placeTower(6, 1, "arrow");
        w.startNextWave();
        advance(w, 40.0f);
        return std::make_pair(w.gold(), w.lives());
    };
    REQUIRE(run() == run());
}
