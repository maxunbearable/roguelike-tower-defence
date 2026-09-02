// Run statistics for the results screen.
//
// The end-of-run screen reported the wave reached and nothing else, so a run
// that nearly held looked identical to one that collapsed at the first boss.
// These counters are what make the screen worth reading -- and they are asserted
// against the simulation rather than against themselves, because a counter that
// only agrees with its own getter proves nothing.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"

#include "support/Plots.h"
#include "sim/World.h"

using namespace td;

namespace {
content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}
core::Loadout owning() {
    core::Loadout lo;
    lo.ownAll = true;
    return lo;
}
}  // namespace

TEST_CASE("a fresh run has empty statistics", "[runstats]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 5000);
    const auto& st = w.stats();
    CHECK(st.enemiesKilled == 0);
    CHECK(st.leaked == 0);
    CHECK(st.goldEarned == 0);
    CHECK(st.towersBuilt == 0);
}

TEST_CASE("building towers is counted", "[runstats]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.placeTower(PLOT(1), "arrow") == sim::World::PlaceResult::Ok);
    CHECK(w.stats().towersBuilt == 2);

    // Selling removes the tower but must NOT un-count it: the statistic is what
    // the player did over the run, not what is standing at the end.
    REQUIRE(w.sellTower(PLOT(0)));
    CHECK(w.stats().towersBuilt == 2);
    CHECK(w.towerCount() == 1);
}

TEST_CASE("kills and bounty are counted against the gold actually paid", "[runstats]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    // A dense line of maxed towers, so wave one dies rather than leaks.
    for (int i = 0; i < 5; ++i) {
        if (w.placeTower(PLOT(i), "arrow") != sim::World::PlaceResult::Ok) continue;
        while (w.upgradeCost(PLOT(i)) > 0 && w.upgradeTower(PLOT(i))) {
        }
    }
    w.startNextWave();
    // AFTER the call, deliberately: startNextWave() pays an early-start bonus,
    // and goldEarned counts bounty only. Sampling before the call made the two
    // disagree by exactly that bonus.
    const int goldBefore = w.gold();
    for (int i = 0; i < 60 * 120 && w.phase() == sim::Phase::Wave; ++i) w.tick(sim::kFixedDt);

    const auto& st = w.stats();
    UNSCOPED_INFO("killed " << st.enemiesKilled << ", leaked " << st.leaked << ", bounty "
                            << st.goldEarned);
    REQUIRE(st.enemiesKilled > 0);
    // Every kill paid something, so bounty must have moved with the kills.
    CHECK(st.goldEarned > 0);
    // And the recorded bounty must equal the gold the world gained during the
    // wave: with arrow towers nothing else pays out (the arcane drain trait
    // would, which is why this uses arrows).
    CHECK(w.gold() - goldBefore == st.goldEarned);
}

TEST_CASE("leaks are counted and cost lives", "[runstats]") {
    const auto reg = loadReg();
    // No towers at all: everything walks through.
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 0);
    const int livesBefore = w.lives();
    w.startNextWave();
    for (int i = 0; i < 60 * 400 && w.stats().leaked == 0; ++i) w.tick(sim::kFixedDt);

    REQUIRE(w.stats().leaked > 0);
    CHECK(w.stats().enemiesKilled == 0);
    // Leaks and lives must agree: the counter is incremented in loseLife, so a
    // mismatch means something is losing lives without being counted.
    CHECK(livesBefore - w.lives() == w.stats().leaked);
}
