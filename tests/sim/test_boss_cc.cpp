#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "content/Registry.h"
#include "sim/World.h"

using namespace td;
using Catch::Matchers::WithinAbs;

namespace {

content::Registry loadRegistry() {
    content::Registry reg;
    reg.loadAll(TD_CONTENT_DIR);
    return reg;
}

// Walks one enemy forward for a fixed time and reports how far it travelled.
float distanceAfter(sim::World& w, entt::entity e, float seconds) {
    const auto& pf0 = w.reg().get<sim::PathFollower>(e);
    const float start = pf0.distance;
    for (float t = 0.0f; t < seconds; t += sim::kFixedDt) w.tick(sim::kFixedDt);
    return w.reg().get<sim::PathFollower>(e).distance - start;
}

entt::entity firstEnemy(sim::World& w) {
    entt::entity found = entt::null;
    w.reg().view<const sim::EnemyTag>().each([&](entt::entity e, const sim::EnemyTag&) {
        if (found == entt::null) found = e;
    });
    return found;
}

}  // namespace

TEST_CASE("petrify stops a normal enemy outright", "[boss][cc]") {
    auto reg = loadRegistry();
    sim::World w(reg, reg.map("greenfields"), 4242, core::Loadout{}, 500);
    // Sandbox, so enemies walk without a wave being in progress.
    w.enterSandbox();
    w.spawnEnemy("slime");
    const auto e = firstEnemy(w);
    REQUIRE((e != entt::null));

    // Guard against a vacuous pass: confirm it moves at all before freezing it.
    REQUIRE(distanceAfter(w, e, 0.25f) > 0.0f);
    w.reg().emplace<sim::Petrified>(e, 10.0f);
    REQUIRE_THAT(distanceAfter(w, e, 0.5f), WithinAbs(0.0f, 1e-4f));
}

TEST_CASE("petrify only slows a boss, and never to a standstill", "[boss][cc]") {
    auto reg = loadRegistry();
    sim::World w(reg, reg.map("greenfields"), 4242, core::Loadout{}, 500);
    w.enterSandbox();
    w.spawnEnemy("boss_warlord_grulk");
    const auto e = firstEnemy(w);
    REQUIRE((e != entt::null));

    const float free = distanceAfter(w, e, 0.5f);
    REQUIRE(free > 0.0f);

    w.reg().emplace<sim::Petrified>(e, 10.0f);
    const float held = distanceAfter(w, e, 0.5f);

    // A boss must keep moving -- a fight that can be paused indefinitely by one
    // cheap status is not a fight.
    REQUIRE(held > 0.0f);
    // But the status must still matter.
    REQUIRE(held < free * 0.5f);
}

TEST_CASE("the shipped bosses are flagged as bosses", "[boss][cc]") {
    auto reg = loadRegistry();
    int bosses = 0;
    for (const auto& [id, def] : reg.enemies()) {
        if (def.boss) {
            ++bosses;
            // A boss that dies to a stray arrow is not a boss.
            REQUIRE(def.maxHp > 500.0f);
            REQUIRE(def.shardValue > 0);
        }
    }
    REQUIRE(bosses >= 2);
}
