// When a run gets written to disk.
//
// The autosave fired on one condition -- the wave index changing -- so it
// captured the state at the START of a build phase and nothing after it.
// Everything the player then did in that phase, which is the entire moment they
// spend their gold, stayed unwritten until the next phase began:
//
//   build phase N begins ..................... autosave
//   build 3 towers, upgrade 2, specialise 1 .. not saved
//   fight wave N ............................. not saved
//   close the window ......................... all of it lost
//
// And the main loop exited without saving at all, so closing the window was
// exactly that case.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/Autosave.h"
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

TEST_CASE("a fresh run is worth writing once", "[autosave]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    sim::SaveMark written;  // nothing written yet

    REQUIRE(sim::shouldAutosave(w, written));
    written = sim::saveMarkOf(w);
    // Nothing has changed, so it must not write again every frame.
    CHECK_FALSE(sim::shouldAutosave(w, written));
}

TEST_CASE("building a tower makes the run worth writing", "[autosave]") {
    // The case that was lost. Under the old rule the wave index had not moved,
    // so this never triggered a write.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    auto written = sim::saveMarkOf(w);
    REQUIRE_FALSE(sim::shouldAutosave(w, written));

    REQUIRE(w.placeTower(2, 0, "arrow") == sim::World::PlaceResult::Ok);
    CHECK(sim::shouldAutosave(w, written));
}

TEST_CASE("upgrading and specialising are noticed too", "[autosave]") {
    // These change gold without changing the tower count, so a count-only check
    // would miss them -- and they are the expensive purchases.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(2, 0, "arrow") == sim::World::PlaceResult::Ok);
    auto written = sim::saveMarkOf(w);

    REQUIRE(w.upgradeTower(2, 0));
    CHECK(sim::shouldAutosave(w, written));

    written = sim::saveMarkOf(w);
    while (w.upgradeCost(2, 0) > 0 && w.upgradeTower(2, 0)) {
    }
    written = sim::saveMarkOf(w);
    REQUIRE(w.attachElement(2, 0, "earth"));
    CHECK(sim::shouldAutosave(w, written));
}

TEST_CASE("selling is noticed", "[autosave]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(2, 0, "arrow") == sim::World::PlaceResult::Ok);
    const auto written = sim::saveMarkOf(w);
    REQUIRE(w.sellTower(2, 0));
    CHECK(sim::shouldAutosave(w, written));
}

TEST_CASE("a run mid-wave is never written", "[autosave]") {
    // Enemies and projectiles are not serialised, so a snapshot taken with a
    // live field would restore a wave that has lost its enemies. The dirty check
    // must never override that.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    const sim::SaveMark nothingWritten;
    w.startNextWave();
    for (int i = 0; i < 60 * 3; ++i) w.tick(sim::kFixedDt);
    REQUIRE(w.aliveEnemies() > 0);

    REQUIRE_FALSE(w.canSnapshot());
    CHECK_FALSE(sim::shouldAutosave(w, nothingWritten));
    // Even building during a wave must not force a write.
    REQUIRE(w.placeTower(4, 0, "arrow") == sim::World::PlaceResult::Ok);
    CHECK_FALSE(sim::shouldAutosave(w, nothingWritten));
}

TEST_CASE("the wave boundary still triggers a write", "[autosave]") {
    // The old behaviour was not wrong, only insufficient; it must still hold.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    for (int x = 2; x <= 10; x += 2) {
        if (w.placeTower(x, 0, "arrow") != sim::World::PlaceResult::Ok) continue;
        while (w.upgradeCost(x, 0) > 0 && w.upgradeTower(x, 0)) {
        }
    }
    const auto written = sim::saveMarkOf(w);
    w.startNextWave();
    for (int i = 0; i < 60 * 200 && w.phase() == sim::Phase::Wave; ++i) w.tick(sim::kFixedDt);
    REQUIRE(w.phase() == sim::Phase::Build);  // survived into the next build phase
    CHECK(sim::shouldAutosave(w, written));
}
