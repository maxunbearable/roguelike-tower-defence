// The player choosing a tower's targeting priority.
//
// Five modes were implemented in the targeting system and validated at content
// load from the beginning, and the player could never pick one: targetPriority
// was authored per tower and frozen for the run. These tests are written against
// the OBSERVABLE effect -- which enemy actually takes damage -- because a
// getter/setter test would pass just as happily if targeting ignored the field
// entirely, which is the bug worth catching.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

#include "content/Registry.h"

#include "support/Plots.h"
#include "sim/Components.h"
#include "core/Vec2.h"
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

TEST_CASE("a tower's targeting priority can be changed and is remembered", "[targeting]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);

    // Whatever the tower was authored with, the player can pick another.
    REQUIRE(w.setTowerPriority(PLOT(0), sim::TargetPriority::Strongest));
    CHECK(w.towerPriority(PLOT(0)) == sim::TargetPriority::Strongest);

    REQUIRE(w.setTowerPriority(PLOT(0), sim::TargetPriority::Last));
    CHECK(w.towerPriority(PLOT(0)) == sim::TargetPriority::Last);

    // An empty tile has nothing to set.
    CHECK_FALSE(w.setTowerPriority(PLOT(1), sim::TargetPriority::First));
}

TEST_CASE("the choice survives an upgrade", "[targeting]") {
    // The regression this guards: statsFor() re-reads targetPriority from the
    // tower DEFINITION, and rebuildTower() runs on every upgrade, imbue and
    // specialisation. Held on TowerStats the player's choice was wiped the next
    // time the tower changed in any way.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
    REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
    REQUIRE(w.setTowerPriority(PLOT(0), sim::TargetPriority::Weakest));

    REQUIRE(w.upgradeTower(PLOT(0)));
    CHECK(w.towerPriority(PLOT(0)) == sim::TargetPriority::Weakest);

    // And the stats the targeting system actually reads agree with the tag.
    const auto e = w.towerAt(PLOT(0));
    REQUIRE((e != entt::null));
    CHECK(w.reg().get<sim::TowerStats>(e).priority == sim::TargetPriority::Weakest);
}

TEST_CASE("targeting priority decides which enemy is shot", "[targeting]") {
    const auto reg = loadReg();

    // Two stationary enemies, both inside one tower's range, one further along
    // the path than the other. "First" must hit the leader, "Last" the trailer.
    // The distances are DERIVED from the path and the tower's actual range
    // rather than hardcoded: hardcoded path coordinates in this suite have
    // already been invalidated once by a map rework.
    const auto runWith = [&](sim::TargetPriority p) {
        sim::World w(reg, reg.map("greenfields"), 1, owning(), 100000);
        REQUIRE(w.placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
        const auto tower = w.towerAt(PLOT(0));
        REQUIRE((tower != entt::null));
        const float range = w.reg().get<sim::TowerStats>(tower).range;
        const core::Vec2 tp = w.reg().get<sim::Position>(tower).v;

        std::vector<float> inRange;
        for (float d = 0.0f; d < w.path().totalLength(); d += 0.25f) {
            if (core::distance(w.path().positionAt(d), tp) <= range * 0.85f) inRange.push_back(d);
        }
        REQUIRE(inRange.size() >= 2);
        const float behind = inRange.front();
        const float ahead = inRange.back();
        REQUIRE(ahead > behind);

        w.enterSandbox();
        auto& r = w.reg();
        const auto& def = reg.enemy("slime");
        const auto make = [&](float d) {
            const auto e = r.create();
            const core::Vec2 pos = w.path().positionAt(d);
            const float hp = def.maxHp * 500.0f;  // never dies, so targeting never re-picks
            r.emplace<sim::Position>(e, pos);
            r.emplace<sim::PrevPosition>(e, pos);
            r.emplace<sim::PathFollower>(e, d);
            r.emplace<sim::Health>(e, hp, hp);
            r.emplace<sim::Armor>(e, 0.0f);
            r.emplace<sim::Speed>(e, 0.0f);  // stationary: only targeting decides
            r.emplace<sim::EnemyTag>(e, def.id, def.bounty, def.shardValue);
            return e;
        };
        const auto lead = make(ahead);
        const auto trail = make(behind);
        REQUIRE(w.setTowerPriority(PLOT(0), p));

        const float leadFull = r.get<sim::Health>(lead).hp;
        const float trailFull = r.get<sim::Health>(trail).hp;
        for (int i = 0; i < 600; ++i) w.tick(sim::kFixedDt);
        return std::pair<float, float>{leadFull - r.get<sim::Health>(lead).hp,
                                       trailFull - r.get<sim::Health>(trail).hp};
    };

    const auto [firstLead, firstTrail] = runWith(sim::TargetPriority::First);
    const auto [lastLead, lastTrail] = runWith(sim::TargetPriority::Last);

    UNSCOPED_INFO("First: lead took " << firstLead << ", trail took " << firstTrail);
    UNSCOPED_INFO("Last:  lead took " << lastLead << ", trail took " << lastTrail);

    // Something was shot at all, or everything below is vacuous.
    REQUIRE((firstLead + firstTrail) > 0.0f);
    REQUIRE((lastLead + lastTrail) > 0.0f);

    CHECK(firstLead > firstTrail);   // "first" favours the enemy nearest the goal
    CHECK(lastTrail > lastLead);     // "last" favours the one behind it
}
