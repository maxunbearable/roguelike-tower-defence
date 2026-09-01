// The guided first run.
//
// Tested by DOING each step against a real World and checking the tutorial
// notices, rather than by setting a flag and reading it back. A tutorial that
// advances on anything other than the player's actual action is worse than none:
// it teaches the wrong thing and then gets in the way.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"
#include "sim/Tutorial.h"
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

TEST_CASE("every step has something to say", "[tutorial]") {
    for (int i = 0; i < sim::tutorialToIndex(sim::TutorialStep::Done); ++i) {
        const auto p = sim::tutorialPrompt(sim::tutorialFromIndex(i));
        INFO("step " << i);
        CHECK(std::string(p.title).size() > 0);
        CHECK(std::string(p.body).size() > 0);
    }
}

TEST_CASE("the steps run in order and terminate", "[tutorial]") {
    auto s = sim::TutorialStep::Build;
    int guard = 0;
    while (s != sim::TutorialStep::Done && guard++ < 20) s = sim::tutorialNext(s);
    CHECK(s == sim::TutorialStep::Done);
    CHECK(guard < 20);
    CHECK(sim::tutorialNext(sim::TutorialStep::Done) == sim::TutorialStep::Done);
}

TEST_CASE("the step index round trips through a save", "[tutorial]") {
    for (int i = 0; i <= sim::tutorialToIndex(sim::TutorialStep::Done); ++i) {
        CHECK(sim::tutorialToIndex(sim::tutorialFromIndex(i)) == i);
    }
    // Anything out of range means "finished", never a crash or a restart.
    CHECK(sim::tutorialFromIndex(-1) == sim::TutorialStep::Done);
    CHECK(sim::tutorialFromIndex(999) == sim::TutorialStep::Done);
}

TEST_CASE("no step passes before the player has done it", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    // A fresh board: nothing built, nothing started, nothing used.
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Build, w, false));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::StartWave, w, false));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Inspect, w, false));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Ability, w, false));
}

TEST_CASE("building satisfies the build step and nothing else", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(2, 0, "arrow") == sim::World::PlaceResult::Ok);
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Build, w, false));
    // Placing a tower must not accidentally tick off later lessons.
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::StartWave, w, false));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));
}

TEST_CASE("starting the wave satisfies the wave step", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    w.startNextWave();
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::StartWave, w, false));
}

TEST_CASE("upgrading any tower satisfies the upgrade step", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(2, 0, "arrow") == sim::World::PlaceResult::Ok);
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));
    REQUIRE(w.upgradeTower(2, 0));
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));

    // Selling the tower it was taught on must not un-teach the lesson... but it
    // does leave no levelled tower, so the step asks for ANY tower past level 1
    // rather than a specific one. Build and level a second to prove that.
    REQUIRE(w.placeTower(4, 0, "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.upgradeTower(4, 0));
    REQUIRE(w.sellTower(2, 0));
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));
}

TEST_CASE("firing Strike satisfies the ability step", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Ability, w, false));
    // The OTHER ability must not count -- the step names Strike.
    REQUIRE(w.castAbility(sim::Ability::Ward, w.path().positionAt(5.0f)));
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Ability, w, false));
    REQUIRE(w.castAbility(sim::Ability::Strike, w.path().positionAt(5.0f)));
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Ability, w, false));
}

TEST_CASE("the inspect step reads UI state", "[tutorial]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Inspect, w, false));
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Inspect, w, true));
}
