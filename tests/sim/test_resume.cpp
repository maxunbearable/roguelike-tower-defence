// What a run keeps when you quit and come back.
//
// The existing snapshot test checks gold, lives, wave and the towers, and those
// were always right. Two things were not saved at all, and both are player
// facing:
//
//   Run statistics. The results screen reports enemies killed, leaked, gold
//   earned and towers built. None of it survived a reload, so a player who
//   stopped for lunch was told at the end that they had done nothing.
//
//   Ability cooldowns. Cooldowns tick during the BUILD phase, which is exactly
//   when the game autosaves, so firing an ability and then quitting to the hub
//   and resuming gave it back immediately. Measured before the fix: 13.0s of
//   cooldown before the save, 0s after.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "core/SaveGame.h"
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

// A run that has actually happened: towers built, a wave fought, an ability
// spent. Left in the build phase, which is the only state a run may be saved in.
// World is neither copyable nor movable, so this fills one in place.
void playRun(sim::World& w) {
    for (int x = 2; x <= 8; x += 2) {
        if (w.placeTower(x, 0, "arrow") != sim::World::PlaceResult::Ok) continue;
        while (w.upgradeCost(x, 0) > 0 && w.upgradeTower(x, 0)) {
        }
    }
    w.startNextWave();
    for (int i = 0; i < 60 * 200 && w.phase() == sim::Phase::Wave; ++i) w.tick(sim::kFixedDt);
    w.castAbility(sim::Ability::Strike, w.path().positionAt(5.0f));
    for (int i = 0; i < 60 * 4; ++i) w.tick(sim::kFixedDt);
}

}  // namespace

TEST_CASE("resuming keeps the run statistics", "[resume]") {
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, owning(), 100000);
    playRun(a);
    REQUIRE(a.canSnapshot());
    const auto& before = a.stats();
    // The run has to have done something, or this proves nothing.
    REQUIRE(before.towersBuilt > 0);
    REQUIRE(before.enemiesKilled > 0);

    sim::World b(reg, reg.map("greenfields"), 7, owning());
    b.restore(a.snapshot());

    CHECK(b.stats().towersBuilt == before.towersBuilt);
    CHECK(b.stats().enemiesKilled == before.enemiesKilled);
    CHECK(b.stats().goldEarned == before.goldEarned);
    CHECK(b.stats().leaked == before.leaked);
}

TEST_CASE("resuming does not refresh ability cooldowns", "[resume]") {
    // The save-scum: fire an ability, quit, come back with it ready.
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, owning(), 100000);
    playRun(a);
    const float cooling = a.abilityCooldown(sim::Ability::Strike);
    REQUIRE(cooling > 0.0f);  // the ability really is on cooldown

    sim::World b(reg, reg.map("greenfields"), 7, owning());
    b.restore(a.snapshot());

    CHECK(b.abilityCooldown(sim::Ability::Strike) == cooling);
    CHECK_FALSE(b.abilityReady(sim::Ability::Strike));
}

TEST_CASE("stats and cooldowns survive the JSON round trip", "[resume]") {
    // snapshot/restore in memory is only half of it -- a real resume goes
    // through the save file.
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, owning(), 100000);
    playRun(a);
    const auto before = a.stats();
    const float cooling = a.abilityCooldown(sim::Ability::Strike);

    core::SaveSlot slot;
    slot.used = true;
    slot.run = a.snapshot();
    const auto reloaded = core::fromJson(core::toJson(slot));
    REQUIRE(reloaded.hasRunInProgress());

    sim::World b(reg, reg.map("greenfields"), 7, owning());
    b.restore(*reloaded.run);
    CHECK(b.stats().enemiesKilled == before.enemiesKilled);
    CHECK(b.stats().towersBuilt == before.towersBuilt);
    CHECK(b.abilityCooldown(sim::Ability::Strike) == cooling);
}

TEST_CASE("a save written before cooldowns were stored still loads", "[resume]") {
    // Older saves carry no cooldown list. Reading past the end of that vector
    // would be undefined; the abilities should simply come back ready.
    const auto reg = loadReg();
    sim::World a(reg, reg.map("greenfields"), 7, owning(), 100000);
    playRun(a);
    auto snap = a.snapshot();
    snap.abilityCooldowns.clear();  // as an older save would be

    sim::World b(reg, reg.map("greenfields"), 7, owning());
    b.restore(snap);
    CHECK(b.abilityCooldown(sim::Ability::Strike) == 0.0f);
    CHECK(b.abilityCooldown(sim::Ability::Ward) == 0.0f);
}
