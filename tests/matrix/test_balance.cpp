// Difficulty-curve measurement. These are the tests that answer "can a new
// player actually play this", which no amount of staring at the code will.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>
#include <utility>
#include <vector>

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

TEST_CASE("the campaign is ordered: map 1 falls before the last map does",
          "[balance]") {
    // Two earlier versions of this test were both wrong, in opposite directions.
    //
    // It first asserted a fully-upgraded profile could NOT clear greenfields.
    // That was true when the global tree was 5 nodes and the campaign was
    // unreachable, and it became exactly backwards: map 1 of 5 SHOULD fall to a
    // player who has ground out the tree.
    //
    // I then asserted owning everything must not clear the FINAL map. That is
    // also wrong -- it would mean the game is never completable at any level of
    // investment, and no amount of extra enemy health makes a wall rather than a
    // grind.
    //
    // What is actually worth defending is ORDERING: partway through the tree,
    // map 1 is winnable and the last map is not. Owning literally everything
    // finishing the campaign is the reward, not a bug.
    const auto reg = loadReg();

    // A mid-game profile: the whole global tree, no tower or element branches
    // beyond what is needed to specialise at all.
    core::Loadout mid;
    mid.ownAll = false;
    for (const auto& n : reg.tree("global").nodes) mid.ownedNodes.insert(n.id);
    for (const auto& sp : reg.tree("arrow").specs) {
        mid.ownedNodes.insert("arrow." + sp + ".core");
    }
    for (const auto& sp : reg.tree("earth").specs) {
        mid.ownedNodes.insert("earth." + sp + ".core");
    }

    const auto easy = sim::autoPlay(reg, reg.map("greenfields"), mid, 1);
    const auto hard = sim::autoPlay(reg, reg.map("obsidian-gate"), mid, 1);
    UNSCOPED_INFO("mid-game profile: greenfields wave " << easy.wavesSurvived << " (cleared "
                                                        << easy.cleared << "), obsidian-gate wave "
                                                        << hard.wavesSurvived << " (cleared "
                                                        << hard.cleared << ")");
    REQUIRE(easy.wavesSurvived > hard.wavesSurvived);  // the last map is harder
    REQUIRE_FALSE(hard.cleared);                       // and not yet beatable
}

TEST_CASE("the endgame is actually reachable, on every map", "[balance]") {
    // THE guardrail whose absence let the game ship unwinnable through three
    // passes. The curve was hpPerWave ^ (wave ^ 1.18) -- an exponential of a
    // power -- so enemy health multiplied 7x between wave 34 and wave 50 while
    // the player, already fully built, gained nothing. A profile owning all 126
    // nodes died at wave 34 of 50, which made maps 2-5 and 8 of the 10 bosses
    // unreachable content.
    //
    // The autoplayer only builds arrow towers, so it is a LOWER bound on what a
    // real player can field. If it can get within a few waves of the end, a
    // human choosing cannon or ballista and matching elements to the map's
    // weakness can finish.
    const auto reg = loadReg();
    for (const auto& [mapId, def] : reg.maps()) {
        const auto r = sim::autoPlay(reg, def, fullyUpgraded(), 1);
        // 76% of the map. The broken state had a fully-upgraded profile dying at
        // 68% (wave 34 of 50) on the EASIEST map, so this catches any drift back
        // toward that wall. It deliberately does not demand a clear: the
        // autoplayer builds only arrow towers, so it is a lower bound on a real
        // player who has five tower types and can match a spec to the map.
        const int target = static_cast<int>(def.recipe.count * 0.76);
        UNSCOPED_INFO(mapId << ": fully upgraded reached wave " << r.wavesSurvived << " of "
                            << def.recipe.count << ", needs " << target);
        REQUIRE(r.wavesSurvived >= target);
    }
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
    const auto strongRun = sim::autoPlay(reg, reg.map("greenfields"), fullyUpgraded(), 1);

    // The two bounds are measured against DIFFERENT runs on purpose. Shard
    // income is not flat -- it rises with how far a run gets, measured at ~12x
    // between a first run and a strong one -- so comparing the total tree cost
    // against first-run income overstates the grind by an order of magnitude.
    // The floor belongs against a first run (the tree must outlast run one) and
    // the ceiling against a strong one (that is the income you actually have
    // when buying the dear nodes at the top of the tree).
    UNSCOPED_INFO("tree costs " << treeCost << "; a first run earns " << firstRun.shards
                  << ", a strong run " << strongRun.shards);
    REQUIRE(firstRun.shards > 0);
    REQUIRE(strongRun.shards > firstRun.shards);  // progress must pay
    REQUIRE(treeCost > firstRun.shards * 5);      // many runs of work
    REQUIRE(treeCost < strongRun.shards * 30);    // but not an endless grind
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

    REQUIRE(w5 < 1.6f);  // the opening barely scales at all

    // The SHAPE, not an absolute. This used to assert `w50 > 50`, which pinned
    // the old super-exponential curve in place: enemy health multiplied 197x by
    // wave 50 while the player's damage plateaued around wave 34, so the map was
    // unwinnable and this test was defending that. An upper bound now guards the
    // opposite failure -- a tail that outruns anything the player can build.
    REQUIRE(w50 > 12.0f);   // the ending is still a different game
    REQUIRE(w50 < 60.0f);   // but it is a wall a full board can break
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

TEST_CASE("plots bind the endgame board and gold binds the opening", "[balance]") {
    // The design intent of finite build plots, stated as a measurement.
    //
    // Greenfields used to offer 144 buildable tiles and the strongest possible
    // run used 34 of them, so a plot was never scarce and a tower's only cost
    // was gold. Every placement was interchangeable with a hundred others. Now
    // the map authors its plots and the late board fills them.
    //
    // Note what this does NOT claim. The endgame is not rich: it spends what it
    // has on levels rather than banking it, so both resources are tight at the
    // end. What changed is that one of them is now a position on the map.
    const auto reg = loadReg();
    const auto& map = reg.map("greenfields");
    int plots = 0;
    for (int y = 0; y < map.gridH; ++y)
        for (int x = 0; x < map.gridW; ++x)
            if (map.buildableAt(x, y)) ++plots;
    REQUIRE(plots > 0);

    const auto strong = sim::autoPlay(reg, map, fullyUpgraded(), 1);
    UNSCOPED_INFO("plots " << plots << ", endgame built " << strong.towersBuilt);
    CHECK(strong.towersBuilt >= plots - 2);  // it fills the map
    CHECK(strong.towersBuilt <= plots);      // and cannot exceed it

    // Early on the opposite must hold, or the opening becomes a formality: a
    // fresh profile is stopped by its purse, not by the map.
    const auto weak = sim::autoPlay(reg, map, fresh(), 1);
    UNSCOPED_INFO("fresh built " << weak.towersBuilt << " of " << plots << " plots");
    CHECK(weak.towersBuilt < plots - 4);
}

TEST_CASE("a build plot has neighbours a support aura can reach", "[balance]") {
    // The brazier's forge spec trades 61% of its own output for an aura over
    // towers within 2.6 tiles, so it breaks even only at 1.57 buffed
    // neighbours. A map generator that scattered its plots further apart than
    // that would delete a tower specialisation without touching a line of its
    // content -- and the first version of the plot layout did exactly that,
    // measuring forge below plain arrow.
    const auto reg = loadReg();
    for (const auto& [id, map] : reg.maps()) {
        std::vector<std::pair<int, int>> plots;
        for (int y = 0; y < map.gridH; ++y)
            for (int x = 0; x < map.gridW; ++x)
                if (map.buildableAt(x, y)) plots.emplace_back(x, y);
        REQUIRE(plots.size() > 8);
        float total = 0.0f;
        for (const auto& p : plots) {
            for (const auto& q : plots) {
                if (p == q) continue;
                const float dx = static_cast<float>(p.first - q.first);
                const float dy = static_cast<float>(p.second - q.second);
                if (dx * dx + dy * dy <= 2.6f * 2.6f) total += 1.0f;
            }
        }
        const float mean = total / static_cast<float>(plots.size());
        UNSCOPED_INFO(id << ": " << plots.size() << " plots, mean aura neighbours " << mean);
        CHECK(mean > 1.57f);
    }
}
