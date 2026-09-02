#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

#include "content/Registry.h"
#include "sim/World.h"

#include "support/Plots.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// A loadout that owns nothing, so these tests measure BASE mechanics rather
// than base-plus-whatever-the-global-tree-currently-grants.
static core::Loadout bare() {
    core::Loadout lo;
    lo.ownAll = false;
    return lo;
}

static void advance(sim::World& w, float seconds) {
    const int steps = static_cast<int>(seconds / sim::kFixedDt);
    for (int i = 0; i < steps; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("a new world starts in the build phase with no enemies", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, bare());
    REQUIRE(w.phase() == sim::Phase::Build);
    REQUIRE(w.aliveEnemies() == 0);
    REQUIRE(w.lives() == sim::kStartingLives);
    REQUIRE(w.waveCount() == 50);
    REQUIRE(w.gold() == reg.map("greenfields").startGold);  // not a hardcoded number
}

TEST_CASE("starting a wave spawns exactly the authored count", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.startNextWave();
    REQUIRE(w.phase() == sim::Phase::Wave);
    advance(w, 12.0f);  // 8 slimes at 0.9s spacing finish spawning by 6.3s
    int spawned = 0;
    w.reg().view<sim::EnemyTag>().each([&](auto&&...) { ++spawned; });
    REQUIRE(spawned == 8);
}

TEST_CASE("enemies advance along the path at their authored speed", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.startNextWave();
    advance(w, 1.0f);

    auto view = w.reg().view<sim::PathFollower, sim::EnemyTag>();
    REQUIRE(view.begin() != view.end());
    const entt::entity first = *view.begin();
    const float before = view.get<sim::PathFollower>(first).distance;
    advance(w, 1.0f);
    const float after = view.get<sim::PathFollower>(first).distance;
    // slime speed is 1.6 tiles/sec
    REQUIRE(after - before > 1.5f);
    REQUIRE(after - before < 1.7f);
}

TEST_CASE("an enemy reaching the exit costs one life and despawns", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, bare());
    w.startNextWave();
    // Long enough for every spawned enemy to walk the whole route, but short of
    // the build timer auto-starting the next wave. Derived from the route so it
    // survives a change of board size.
    const float travel = w.path().totalLength() / reg.enemy("slime").speed;
    advance(w, travel + 12.0f);
    REQUIRE(w.lives() == sim::kStartingLives - 8);
    REQUIRE(w.aliveEnemies() == 0);
    REQUIRE(w.phase() == sim::Phase::Build);
    REQUIRE(w.waveIndex() == 1);
}

TEST_CASE("the build timer auto-starts the next wave", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.phase() == sim::Phase::Build);
    advance(w, 13.0f);  // buildTime is 12s
    REQUIRE(w.phase() == sim::Phase::Wave);
}

TEST_CASE("losing every life ends the run in defeat", "[sim]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    for (int i = 0; i < 8 && w.phase() != sim::Phase::Defeated; ++i) {
        if (w.phase() == sim::Phase::Build) w.startNextWave();
        advance(w, 100.0f);
    }
    REQUIRE(w.phase() == sim::Phase::Defeated);
    REQUIRE(w.lives() == 0);
}

TEST_CASE("the simulation is deterministic for a fixed seed", "[sim]") {
    const auto reg = loadReg();
    auto run = [&] {
        sim::World w(reg, reg.map("greenfields"), 4242);
        w.startNextWave();
        advance(w, 30.0f);
        std::vector<float> d;
        w.reg().view<sim::PathFollower>().each([&](auto& p) { d.push_back(p.distance); });
        return d;
    };
    REQUIRE(run() == run());
}

TEST_CASE("the global tree raises starting gold and lives", "[sim][trees]") {
    const auto reg = loadReg();
    sim::World withTree(reg, reg.map("greenfields"), 1, tdtest::owningAll());
    sim::World without(reg, reg.map("greenfields"), 1, bare());
    REQUIRE(withTree.gold() > without.gold());
    REQUIRE(withTree.lives() > without.lives());
}
