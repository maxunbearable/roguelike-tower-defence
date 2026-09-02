// The two player abilities.
//
// Every game in this genre ships a pair of these -- Kingdom Rush's Rain of Fire
// and Reinforcements -- because they are what makes a wave something the player
// PLAYS rather than watches. This game had none: once towers were placed there
// was nothing to do until the next build phase.
//
// Tested through observable effects (enemies lose health, enemies move slower)
// rather than through the cooldown fields, because a test that only reads back
// what it wrote would pass on an ability that did nothing.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

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

core::Loadout owning() {
    core::Loadout lo;
    lo.ownAll = true;
    return lo;
}

// An enemy parked at a known point on the route, with enough health to survive
// being measured.
entt::entity parkEnemy(sim::World& w, const content::Registry& reg, float distance,
                       float hpMult = 400.0f) {
    auto& r = w.reg();
    const auto& def = reg.enemy("slime");
    const auto e = r.create();
    const core::Vec2 pos = w.path().positionAt(distance);
    const float hp = def.maxHp * hpMult;
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

TEST_CASE("both abilities start ready", "[abilities]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning());
    CHECK(w.abilityReady(sim::Ability::Strike));
    CHECK(w.abilityReady(sim::Ability::Ward));
    CHECK(w.abilityCooldown(sim::Ability::Strike) == 0.0f);
}

TEST_CASE("a strike damages everything in its blast", "[abilities]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning());
    w.enterSandbox();
    const auto near = parkEnemy(w, reg, 5.0f);
    const auto far = parkEnemy(w, reg, 45.0f);
    const float nearFull = w.reg().get<sim::Health>(near).hp;
    const float farFull = w.reg().get<sim::Health>(far).hp;

    // Sandbox is not a castable phase -- abilities belong to a real run.
    CHECK_FALSE(w.abilityReady(sim::Ability::Strike));

    sim::World live(reg, reg.map("greenfields"), 1, owning());
    const auto a = parkEnemy(live, reg, 5.0f);
    const auto b = parkEnemy(live, reg, 45.0f);
    const float aFull = live.reg().get<sim::Health>(a).hp;
    const float bFull = live.reg().get<sim::Health>(b).hp;

    REQUIRE(live.castAbility(sim::Ability::Strike, live.path().positionAt(5.0f)));
    CHECK(live.reg().get<sim::Health>(a).hp < aFull);   // in the blast
    CHECK(live.reg().get<sim::Health>(b).hp == bFull);  // far away, untouched

    (void)nearFull; (void)farFull; (void)near; (void)far;
}

TEST_CASE("a strike goes on cooldown and recovers", "[abilities]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning());
    REQUIRE(w.castAbility(sim::Ability::Strike, w.path().positionAt(5.0f)));

    CHECK_FALSE(w.abilityReady(sim::Ability::Strike));
    CHECK_FALSE(w.castAbility(sim::Ability::Strike, w.path().positionAt(5.0f)));
    // The other ability is on its own timer.
    CHECK(w.abilityReady(sim::Ability::Ward));

    // Cooldowns must run during the BUILD phase too, or calling the next wave
    // early would be a way to freeze them.
    REQUIRE(w.phase() == sim::Phase::Build);
    const float before = w.abilityCooldown(sim::Ability::Strike);
    for (int i = 0; i < 60; ++i) w.tick(sim::kFixedDt);
    CHECK(w.abilityCooldown(sim::Ability::Strike) < before);

    for (int i = 0; i < 60 * 60; ++i) w.tick(sim::kFixedDt);
    CHECK(w.abilityReady(sim::Ability::Strike));
}

TEST_CASE("a ward slows what walks through it and then expires", "[abilities]") {
    const auto reg = loadReg();

    // Same route, same enemy, same elapsed time -- the only difference is the
    // ward, so any difference in distance travelled is the ward's doing.
    const auto travelled = [&](bool withWard) {
        sim::World w(reg, reg.map("greenfields"), 1, owning());
        const auto e = parkEnemy(w, reg, 3.0f);
        w.startNextWave();  // movement only runs in a live wave
        if (withWard) REQUIRE(w.castAbility(sim::Ability::Ward, w.path().positionAt(4.0f)));
        for (int i = 0; i < 60 * 3; ++i) w.tick(sim::kFixedDt);
        return w.reg().valid(e) ? w.reg().get<sim::PathFollower>(e).distance : 999.0f;
    };

    const float plain = travelled(false);
    const float warded = travelled(true);
    UNSCOPED_INFO("plain " << plain << " vs warded " << warded);
    CHECK(warded < plain);
}

TEST_CASE("a ward does no damage", "[abilities]") {
    // It is a tempo tool, not a second damage source. If it ever starts dealing
    // damage the balance measurements stop meaning what they say.
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning());
    const auto e = parkEnemy(w, reg, 4.0f);
    const float full = w.reg().get<sim::Health>(e).hp;
    REQUIRE(w.castAbility(sim::Ability::Ward, w.path().positionAt(4.0f)));
    for (int i = 0; i < 60 * 2; ++i) w.tick(sim::kFixedDt);
    CHECK(w.reg().get<sim::Health>(e).hp == full);
}

TEST_CASE("ward fields are cleaned up when they run out", "[abilities]") {
    const auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, owning());
    REQUIRE(w.castAbility(sim::Ability::Ward, w.path().positionAt(4.0f)));
    CHECK(w.wards().size() == 1);
    // Ask the WORLD how long its ward lasts rather than assuming the authored
    // constant. This profile owns every node, including the ward duration
    // upgrades, so the constant is the base and not the answer -- which is
    // exactly what this test failed on when ability progression was added.
    const float lasts = w.abilityTuning().wardDuration;
    for (int i = 0; i < static_cast<int>(60 * (lasts + 1.0f)); ++i) {
        w.tick(sim::kFixedDt);
    }
    CHECK(w.wards().empty());
}

TEST_CASE("strike damage keeps pace with the wave", "[abilities]") {
    // A flat number is a panic button on wave 3 and confetti on wave 50: enemy
    // health rises about 55x across a map. The blast is scaled by the wave's own
    // health multiplier so it means the same thing throughout.
    const auto reg = loadReg();
    const auto hitAt = [&](int wave) {
        sim::World w(reg, reg.map("greenfields"), 1, owning());
        w.devSetWave(wave);
        const auto e = parkEnemy(w, reg, 6.0f, 40000.0f);
        const float full = w.reg().get<sim::Health>(e).hp;
        REQUIRE(w.castAbility(sim::Ability::Strike, w.path().positionAt(6.0f)));
        return full - w.reg().get<sim::Health>(e).hp;
    };
    const float early = hitAt(1);
    const float late = hitAt(45);
    UNSCOPED_INFO("wave 1 blast " << early << " vs wave 45 blast " << late);
    CHECK(late > early * 5.0f);
}
