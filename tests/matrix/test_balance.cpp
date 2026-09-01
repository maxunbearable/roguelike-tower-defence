// Difficulty-curve measurement. These are the tests that answer "can a new
// player actually play this", which no amount of staring at the code will.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

#include "content/Registry.h"
#include "sim/AutoPlayer.h"

using namespace td;

static content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

static core::Loadout fresh() {
    core::Loadout lo;
    lo.ownAll = false;  // a brand new profile owns nothing
    return lo;
}

static core::Loadout fullyUpgraded() {
    core::Loadout lo;
    lo.ownAll = true;
    return lo;
}

TEST_CASE("report the difficulty curve", "[balance][.report]") {
    const auto reg = loadReg();
    std::ostringstream os;
    os << "\n  profile            seed   waves  towers  shards  cleared\n";
    for (const uint64_t seed : {1u, 7u, 42u}) {
        const auto a = sim::autoPlay(reg, reg.map("greenfields"), fresh(), seed);
        os << "  fresh             " << seed << "\t" << a.wavesSurvived << "\t" << a.towersBuilt
           << "\t" << a.shards << "\t" << (a.cleared ? "yes" : "no") << "\n";
    }
    for (const uint64_t seed : {1u, 7u, 42u}) {
        const auto b = sim::autoPlay(reg, reg.map("greenfields"), fullyUpgraded(), seed);
        os << "  fully upgraded    " << seed << "\t" << b.wavesSurvived << "\t" << b.towersBuilt
           << "\t" << b.shards << "\t" << (b.cleared ? "yes" : "no") << "\n";
    }
    WARN(os.str());
}

TEST_CASE("a brand new profile survives the opening waves", "[balance]") {
    // The single most important playability property: a first run must not die
    // immediately. This started at wave 4 -- a death spiral where the opening
    // defence was too weak to earn the gold needed to improve it.
    const auto reg = loadReg();
    for (const uint64_t seed : {1u, 7u, 42u}) {
        const auto r = sim::autoPlay(reg, reg.map("greenfields"), fresh(), seed);
        UNSCOPED_INFO("seed " << seed << " reached wave " << r.wavesSurvived << " with "
                              << r.towersBuilt << " towers");
        REQUIRE(r.wavesSurvived >= 8);
        REQUIRE(r.towersBuilt >= 4);  // the opening must fund a real defence
    }
}

TEST_CASE("even a fully upgraded profile is tested by the late waves", "[balance]") {
    // The other end of the curve. If owning everything walked the map, there
    // would be no reason to play well -- and no reason to keep playing.
    const auto reg = loadReg();
    const auto r = sim::autoPlay(reg, reg.map("greenfields"), fullyUpgraded(), 1);
    UNSCOPED_INFO("fully upgraded reached wave " << r.wavesSurvived);
    REQUIRE(r.wavesSurvived >= 25);  // the tree must be worth buying
    REQUIRE(r.wavesSurvived < 50);   // but mastery, not shopping, clears the map
}

TEST_CASE("the meta progression outlasts a single run", "[balance]") {
    // A run once paid for the entire tree several times over, which ended the
    // progression before it began.
    const auto reg = loadReg();
    // EVERY tree, not a hardcoded three. This used to name global/arrow/earth,
    // so adding five more trees left it silently measuring a third of the real
    // cost of owning everything.
    int treeCost = 0;
    for (const auto& [id, tree] : reg.trees()) {
        for (const auto& n : tree.nodes) treeCost += n.cost;
    }
    const auto firstRun = sim::autoPlay(reg, reg.map("greenfields"), fresh(), 1);
    UNSCOPED_INFO("tree costs " << treeCost << ", a first run earns " << firstRun.shards);
    REQUIRE(firstRun.shards > 0);
    REQUIRE(treeCost > firstRun.shards * 5);   // many runs of work
    REQUIRE(treeCost < firstRun.shards * 60);  // but not an endless grind
}

TEST_CASE("the difficulty curve is gentle early and steep late", "[balance]") {
    // A single flat exponent cannot do both. This asserts the SHAPE, so a future
    // tuning pass cannot quietly flatten the late game or spike the opening.
    const auto reg = loadReg();
    const auto& waves = reg.map("greenfields").waves;
    REQUIRE(waves.size() >= 50);
    const float w5 = waves[4].groups[0].hpMult;
    const float w15 = waves[14].groups[0].hpMult;
    const float w50 = waves[49].groups[0].hpMult;

    REQUIRE(w5 < 1.6f);    // the opening barely scales at all
    REQUIRE(w50 > 50.0f);  // the ending is a different game
    // Later growth must outpace earlier growth, which is what "bent" means.
    REQUIRE((w50 / w15) > (w15 / w5));
}

TEST_CASE("a fresh run still ends, so the meta loop turns over", "[balance]") {
    const auto reg = loadReg();
    const auto r = sim::autoPlay(reg, reg.map("greenfields"), fresh(), 1);
    REQUIRE(r.wavesSurvived < 50);  // not trivially winnable from nothing
    REQUIRE(r.shards > 0);          // and it pays out either way
}

TEST_CASE("skill tree investment measurably extends a run", "[balance]") {
    // If owning the whole tree did not get you meaningfully further, the entire
    // meta progression would be decoration.
    const auto reg = loadReg();
    const auto weak = sim::autoPlay(reg, reg.map("greenfields"), fresh(), 7);
    const auto strong = sim::autoPlay(reg, reg.map("greenfields"), fullyUpgraded(), 7);
    UNSCOPED_INFO("fresh " << weak.wavesSurvived << " vs upgraded " << strong.wavesSurvived);
    REQUIRE(strong.wavesSurvived > weak.wavesSurvived);
}
