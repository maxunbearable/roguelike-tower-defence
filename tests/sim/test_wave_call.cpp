// Calling the next wave ON TOP of a running one.
//
// The mechanic only pays if nothing is skipped: the enemies the current wave
// still owes must still arrive, or "call early" would be strictly better than
// waiting and the risk would be fictional. That is what these pin down.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

// Runs the sim until `pred` holds or the budget expires.
template <typename P>
bool advanceUntil(sim::World& w, P pred, float budget = 120.0f) {
    for (float t = 0.0f; t < budget; t += sim::kFixedDt) {
        if (pred()) return true;
        w.tick(sim::kFixedDt);
    }
    return pred();
}

}  // namespace

TEST_CASE("the next wave can be called while one is still running", "[wavecall]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);

    REQUIRE(w.phase() == sim::Phase::Build);
    REQUIRE(w.canCallWave());
    w.startNextWave();
    REQUIRE(w.phase() == sim::Phase::Wave);
    const int firstWave = w.waveIndex();

    // Wait for enemies to actually be on the field, so the call overlaps
    // something real rather than an empty wave.
    REQUIRE(advanceUntil(w, [&] { return w.aliveEnemies() > 0; }));

    REQUIRE(w.canCallWave());
    const int bonus = w.overlapCallBonus();
    REQUIRE(bonus > 0);  // there is unresolved work, so the call pays

    const int goldBefore = w.gold();
    w.startNextWave();
    CHECK(w.gold() == goldBefore + bonus);
    CHECK(w.waveIndex() == firstWave + 1);  // the next wave is now also in flight
    CHECK(w.phase() == sim::Phase::Wave);
}

TEST_CASE("an overlap call skips no enemies", "[wavecall]") {
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");

    // Total enemies owed by waves 0 and 1, from the content.
    int owed = 0;
    for (int i = 0; i < 2; ++i) {
        for (const auto& g : map.waves[static_cast<size_t>(i)].groups) owed += g.count;
    }

    REQUIRE(owed > 0);  // otherwise the assertion below is vacuous

    // Immortal board: no towers, and lives are irrelevant to spawn counting.
    sim::World w(reg, map, 1);
    w.startNextWave();
    REQUIRE(advanceUntil(w, [&] { return w.aliveEnemies() > 0; }));
    w.startNextWave();  // stack wave 1 on top of wave 0

    int seen = 0;
    for (float t = 0.0f; t < 600.0f; t += sim::kFixedDt) {
        const int before = w.enemiesSpawned();
        w.tick(sim::kFixedDt);
        seen += w.enemiesSpawned() - before;
        if (w.phase() != sim::Phase::Wave) break;
    }
    // Every enemy from BOTH waves must have been spawned; an overlap call that
    // discarded the remainder of the current wave would come in short.
    CHECK(w.enemiesSpawned() >= owed);
}

TEST_CASE("the call bonus is zero when nothing is left to resolve", "[wavecall]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    // Build phase pays for skipped build time, not for risk.
    CHECK(w.overlapCallBonus() == 0);
    CHECK(w.callBonus() == w.earlyStartBonus());
}

TEST_CASE("the final wave cannot be called on top of itself", "[wavecall]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.devSetWave(w.waveCount() - 1);
    REQUIRE(w.phase() == sim::Phase::Wave);
    REQUIRE(w.waveIndex() == w.waveCount() - 1);

    CHECK_FALSE(w.canCallWave());  // nothing after the last wave
    const int gold = w.gold();
    w.startNextWave();
    CHECK(w.gold() == gold);                      // no free money
    CHECK(w.waveIndex() == w.waveCount() - 1);    // and no phantom wave
}
