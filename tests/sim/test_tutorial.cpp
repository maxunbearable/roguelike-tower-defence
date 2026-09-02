// The guided first run.
//
// Tested by DOING each step against a real World and checking the tutorial
// notices, rather than by setting a flag and reading it back. A tutorial that
// advances on anything other than the player's actual action is worse than none:
// it teaches the wrong thing and then gets in the way.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "content/Registry.h"

#include "support/Plots.h"
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
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
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
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    CHECK_FALSE(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));
    REQUIRE(w.upgradeTower(PLOT(0)));
    CHECK(sim::tutorialSatisfied(sim::TutorialStep::Upgrade, w, false));

    // Selling the tower it was taught on must not un-teach the lesson... but it
    // does leave no levelled tower, so the step asks for ANY tower past level 1
    // rather than a specific one. Build and level a second to prove that.
    REQUIRE(w.placeTower(PLOT(1), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.upgradeTower(PLOT(1)));
    REQUIRE(w.sellTower(PLOT(0)));
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

namespace {

// Walks the tutorial the way the game does, performing everything the profile is
// actually able to perform, and reports the steps it managed to teach.
std::vector<sim::TutorialStep> walkTutorial(sim::World& w) {
    std::vector<sim::TutorialStep> taught;
    auto step = sim::TutorialStep::Build;
    int guard = 0;
    while (step != sim::TutorialStep::Done && guard++ < 24) {
        if (sim::tutorialSatisfied(step, w, /*menuOnTower=*/true)) {
            taught.push_back(step);
            step = sim::tutorialNext(step);
            continue;
        }
        switch (step) {
            case sim::TutorialStep::Build:
                w.placeTower(PLOT(0), "arrow");
                break;
            case sim::TutorialStep::StartWave:
                w.startNextWave();
                break;
            case sim::TutorialStep::Ability:
                w.castAbility(sim::Ability::Strike, w.path().positionAt(3.0f));
                break;
            case sim::TutorialStep::Upgrade:
                if (!w.upgradeTower(PLOT(0))) return taught;  // cannot: levels are bought
                break;
            default:
                return taught;
        }
    }
    return taught;
}

bool taughtStep(const std::vector<sim::TutorialStep>& v, sim::TutorialStep s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

}  // namespace

TEST_CASE("a first run is taught everything it can actually do", "[tutorial]") {
    // Tower levels are bought in the skill trees, so on a first run no tower can
    // pass level 1 and the step asking the player to "level the tower up" cannot
    // be satisfied. That step used to sit FOURTH, ahead of the ability lesson, so
    // the tutorial stalled there and a new player was never shown Strike -- a
    // free ability on a cooldown, and the only thing they get for nothing.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, core::Loadout{}, /*goldOverride=*/5000);
    REQUIRE_FALSE(w.levelUnlocked(2));

    const auto taught = walkTutorial(w);
    INFO("steps taught: " << taught.size());
    CHECK(taughtStep(taught, sim::TutorialStep::Build));
    CHECK(taughtStep(taught, sim::TutorialStep::StartWave));
    CHECK(taughtStep(taught, sim::TutorialStep::Inspect));
    CHECK(taughtStep(taught, sim::TutorialStep::Ability));
    // ...and it waits on the one thing the profile cannot do yet, rather than
    // blocking the rest behind it.
    CHECK_FALSE(taughtStep(taught, sim::TutorialStep::Upgrade));
}

TEST_CASE("the tutorial finishes once tower levels are bought", "[tutorial]") {
    const auto reg = loadReg();
    core::Loadout lo;
    lo.ownedNodes.insert("global.level2");
    sim::World w(reg, reg.map("greenfields"), 1, lo, /*goldOverride=*/5000);
    REQUIRE(w.levelUnlocked(2));

    const auto taught = walkTutorial(w);
    CHECK(taughtStep(taught, sim::TutorialStep::Ability));
    CHECK(taughtStep(taught, sim::TutorialStep::Upgrade));
}

TEST_CASE("the level-up step says where levels come from when they are locked",
          "[tutorial]") {
    // Asking for something impossible is worse than saying nothing; the step
    // names the node and its place instead.
    const auto locked = sim::tutorialPrompt(sim::TutorialStep::Upgrade, /*levelsUnlocked=*/false);
    const auto open = sim::tutorialPrompt(sim::TutorialStep::Upgrade, /*levelsUnlocked=*/true);
    REQUIRE(std::string(locked.title) != std::string(open.title));
    CHECK(std::string(locked.body).find("skill trees") != std::string::npos);
    CHECK(std::string(open.body).find("Level the tower up") != std::string::npos);
}
