// Buying ability upgrades in the skill tree.
//
// Strike and Ward were the only capability in the game a player could not
// improve. Every other one -- tower levels, elements, both kinds of
// specialisation, tower types -- is bought in a tree, while the two abilities
// were fixed constants, identical on run 1 and run 200, in a game whose whole
// meta progression is buying capability.
//
// Tested through what the abilities DO to enemies, not by reading the resolved
// numbers back: a test that checks the tuning struct would pass on a world that
// resolved it and then ignored it.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/Registry.h"
#include "sim/Components.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

core::Loadout withNodes(std::set<std::string> nodes) {
    core::Loadout lo;
    lo.ownAll = false;
    lo.ownedNodes = std::move(nodes);
    return lo;
}

// An enemy parked on the route with enough health to survive being measured.
entt::entity park(sim::World& w, const content::Registry& reg, float distance) {
    auto& r = w.reg();
    const auto& def = reg.enemy("slime");
    const auto e = r.create();
    const core::Vec2 pos = w.path().positionAt(distance);
    const float hp = def.maxHp * 4000.0f;
    r.emplace<sim::Position>(e, pos);
    r.emplace<sim::PrevPosition>(e, pos);
    r.emplace<sim::PathFollower>(e, distance);
    r.emplace<sim::Health>(e, hp, hp);
    r.emplace<sim::Armor>(e, 0.0f);
    r.emplace<sim::Speed>(e, def.speed);
    r.emplace<sim::EnemyTag>(e, def.id, def.bounty, def.shardValue);
    return e;
}

}  // namespace

TEST_CASE("a fresh profile gets the authored ability values", "[abilityprog]") {
    // The tree MODIFIES the constants; it does not replace them. If a fresh
    // profile were changed by this work, every ability measurement taken before
    // it would silently stop meaning what it says.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, withNodes({}));
    const auto& t = w.abilityTuning();
    CHECK(t.strikeDamage == sim::kStrikeBaseDamage);
    CHECK(t.strikeRadius == sim::kStrikeRadius);
    CHECK(t.strikeCooldown == sim::kStrikeCooldown);
    CHECK(t.wardDuration == sim::kWardDuration);
    CHECK(t.wardSlow == sim::kWardSlowPct);
    CHECK(t.wardCooldown == sim::kWardCooldown);
}

TEST_CASE("buying strike damage makes the blast hurt more", "[abilityprog]") {
    const auto reg = loadReg();
    const auto blastOn = [&](std::set<std::string> nodes) {
        sim::World w(reg, reg.map("greenfields"), 1, withNodes(std::move(nodes)));
        const auto e = park(w, reg, 6.0f);
        const float full = w.reg().get<sim::Health>(e).hp;
        REQUIRE(w.castAbility(sim::Ability::Strike, w.path().positionAt(6.0f)));
        return full - w.reg().get<sim::Health>(e).hp;
    };
    const float base = blastOn({});
    const float upgraded = blastOn({"global.level2", "global.strike1"});
    UNSCOPED_INFO("blast " << base << " -> " << upgraded);
    REQUIRE(base > 0.0f);
    CHECK(upgraded > base * 1.2f);
}

TEST_CASE("buying strike radius widens the blast", "[abilityprog]") {
    // Measured by whether an enemy OUTSIDE the base radius is hit at all, which
    // a damage-only test would miss entirely.
    const auto reg = loadReg();
    const float justOutside = sim::kStrikeRadius + 0.45f;

    const auto hitsFarEnemy = [&](std::set<std::string> nodes) {
        sim::World w(reg, reg.map("greenfields"), 1, withNodes(std::move(nodes)));
        // Place the enemy along the path, then aim the strike a fixed distance
        // away from it in a straight line, so "outside the radius" is exact.
        const auto e = park(w, reg, 6.0f);
        const auto pos = w.reg().get<sim::Position>(e).v;
        const core::Vec2 aim{pos.x - justOutside, pos.y};
        const float full = w.reg().get<sim::Health>(e).hp;
        REQUIRE(w.castAbility(sim::Ability::Strike, aim));
        return full - w.reg().get<sim::Health>(e).hp > 0.0f;
    };

    CHECK_FALSE(hitsFarEnemy({}));
    CHECK(hitsFarEnemy({"global.level2", "global.strike1", "global.strike2"}));
}

TEST_CASE("buying strike cooldown brings it back sooner", "[abilityprog]") {
    const auto reg = loadReg();
    const auto cooldownFor = [&](std::set<std::string> nodes) {
        sim::World w(reg, reg.map("greenfields"), 1, withNodes(std::move(nodes)));
        REQUIRE(w.castAbility(sim::Ability::Strike, w.path().positionAt(5.0f)));
        return w.abilityCooldown(sim::Ability::Strike);
    };
    const float base = cooldownFor({});
    const float fast = cooldownFor(
        {"global.level2", "global.strike1", "global.strike2", "global.strike3"});
    UNSCOPED_INFO("cooldown " << base << "s -> " << fast << "s");
    CHECK(fast < base);
    CHECK(fast > 0.0f);  // never free, however much is bought
}

TEST_CASE("buying ward upgrades holds enemies longer and harder", "[abilityprog]") {
    const auto reg = loadReg();
    // Same route, same enemy, same elapsed time: any difference in distance
    // travelled is the ward's doing.
    const auto travelled = [&](std::set<std::string> nodes) {
        sim::World w(reg, reg.map("greenfields"), 1, withNodes(std::move(nodes)));
        const auto e = park(w, reg, 3.0f);
        w.startNextWave();
        REQUIRE(w.castAbility(sim::Ability::Ward, w.path().positionAt(4.0f)));
        for (int i = 0; i < 60 * 8; ++i) w.tick(sim::kFixedDt);
        return w.reg().valid(e) ? w.reg().get<sim::PathFollower>(e).distance : 999.0f;
    };
    const float plain = travelled({});
    const float upgraded = travelled(
        {"global.level2", "global.ward1", "global.ward2"});
    UNSCOPED_INFO("travelled " << plain << " -> " << upgraded);
    CHECK(upgraded < plain);
}

TEST_CASE("ward slow is capped short of a total stop", "[abilityprog]") {
    // An enemy frozen solid by a free, repeatable ability is not a tempo tool,
    // it is a win button.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1,
                 withNodes({"global.level2", "global.ward1", "global.ward2", "global.ward3"}));
    CHECK(w.abilityTuning().wardSlow <= 0.85f);
    CHECK(w.abilityTuning().wardCooldown >= 8.0f);
}
