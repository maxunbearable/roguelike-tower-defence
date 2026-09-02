#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"

#include "support/Plots.h"
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
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Occupied);
    REQUIRE(w.placeTower(5, 1, "arrow") == sim::World::PlaceResult::NotBuildable);   // path
    REQUIRE(w.placeTower(0, 1, "arrow") == sim::World::PlaceResult::NotBuildable);   // spawn
    REQUIRE(w.placeTower(-1, 0, "arrow") == sim::World::PlaceResult::OutOfBounds);
    REQUIRE(w.placeTower(30, 0, "arrow") == sim::World::PlaceResult::OutOfBounds);
    // A deliberately non-existent id. This used to say "ballista", which became
    // a real tower.
    REQUIRE(w.placeTower(PLOT(1), "no_such_tower") == sim::World::PlaceResult::UnknownTower);
}

TEST_CASE("placing deducts gold and selling refunds a fraction", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    const int before = w.gold();
    // Read the cost rather than hardcoding it: tools/balance.py scales build
    // costs per profile, and this test is about the bookkeeping, not the price.
    const int cost = reg.tower("arrow").buildCost;
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.gold() == before - cost);
    const int refund = w.sellValue(PLOT(0));
    REQUIRE(refund > 0);
    REQUIRE(refund < cost);  // selling is a loss, never a free move
    REQUIRE(w.sellTower(PLOT(0)));
    REQUIRE(w.gold() == before - cost + refund);
    REQUIRE((w.towerAt(PLOT(0)) == entt::null));  // parens: Catch2 vs EnTT null_t ambiguity
    REQUIRE_FALSE(w.sellTower(PLOT(0)));
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
        REQUIRE(w.placeTower(PLOT(i), "arrow") == sim::World::PlaceResult::Ok);
    }
    REQUIRE(w.gold() < cost);
    REQUIRE(w.placeTower(PLOT(affordable), "arrow") == sim::World::PlaceResult::TooPoor);
}

TEST_CASE("upgrading raises level and applies the authored multiplier", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, tdtest::owningAll(), /*goldOverride=*/1000);
    w.placeTower(PLOT(0), "arrow");
    const auto t = w.towerAt(PLOT(0));
    const float baseDamage = w.reg().get<sim::TowerStats>(t).damage;

    // Authored level costs, read from content: balance profiles scale them.
    const auto& def = reg.tower("arrow");
    REQUIRE(w.upgradeCost(PLOT(0)) == def.levels[0].cost);
    REQUIRE(w.upgradeTower(PLOT(0)));
    REQUIRE(w.reg().get<sim::TowerTag>(t).level == 2);
    REQUIRE(w.reg().get<sim::TowerStats>(t).damage == baseDamage * 1.6f);

    REQUIRE(w.upgradeCost(PLOT(0)) == def.levels[1].cost);
    REQUIRE(w.upgradeTower(PLOT(0)));
    REQUIRE(w.reg().get<sim::TowerTag>(t).level == 3);
    // multipliers are absolute against base, not cumulative
    REQUIRE(w.reg().get<sim::TowerStats>(t).damage == baseDamage * 2.4f);

    REQUIRE(w.upgradeCost(PLOT(0)) == -1);  // no level 4 authored
    REQUIRE_FALSE(w.upgradeTower(PLOT(0)));
}

TEST_CASE("a tower kills enemies walking past it and earns bounty", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(PLOT(2), "arrow");  // flanks the y=2 path run
    w.placeTower(PLOT(3), "arrow");
    const int goldAfterBuilding = w.gold();
    w.startNextWave();
    advance(w, 50.0f);
    REQUIRE(w.gold() > goldAfterBuilding);              // bounty collected
    REQUIRE(w.lives() > sim::kStartingLives - 8);       // not everything leaked
}

TEST_CASE("towers do not shoot beyond their range", "[towers]") {
    // This used to place a tower in a "far corner" and assert the whole wave
    // leaked. Once the routes were lengthened into dense serpentines there is no
    // longer ANY buildable tile out of range of the path, so the premise became
    // impossible. It now measures range directly, which is what it was ever
    // really about.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    const auto tower = w.towerAt(PLOT(0));
    const float range = w.reg().get<sim::TowerStats>(tower).range;
    const core::Vec2 tpos = w.reg().get<sim::Position>(tower).v;

    w.enterSandbox();
    w.spawnEnemy("goblin", 40.0f);
    entt::entity e = entt::null;
    w.reg().view<const sim::EnemyTag>().each([&](entt::entity x, const sim::EnemyTag&) {
        if (e == entt::null) e = x;
    });
    REQUIRE((e != entt::null));

    // Park it a long way along the route and hold it still, so the only question
    // is whether the tower reaches.
    w.reg().get<sim::PathFollower>(e).distance = 55.0f;
    w.reg().get<sim::Speed>(e).base = 0.0f;
    for (int i = 0; i < 4; ++i) w.tick(sim::kFixedDt);  // settle its position

    const core::Vec2 epos = w.reg().get<sim::Position>(e).v;
    REQUIRE(core::distance(tpos, epos) > range);  // the premise, asserted not assumed

    const float before = w.reg().get<sim::Health>(e).hp;
    advance(w, 3.0f);
    REQUIRE(w.reg().get<sim::Health>(e).hp == before);
}

TEST_CASE("targeting first picks the enemy furthest along the path", "[towers]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(PLOT(2), "arrow");
    w.startNextWave();
    advance(w, 6.0f);  // several slimes strung out along the route

    const auto t = w.towerAt(PLOT(2));
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
        w.placeTower(PLOT(2), "arrow");
        w.placeTower(PLOT(3), "arrow");
        w.startNextWave();
        advance(w, 40.0f);
        return std::make_pair(w.gold(), w.lives());
    };
    REQUIRE(run() == run());
}
