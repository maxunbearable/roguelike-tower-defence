// The control-oriented element specs, asserted on their MECHANISM.
//
// The combination matrix measures damage throughput against stationary enemies,
// which cannot see these at all: a slow on something that is not moving does
// nothing, shatter needs a slowed target to trigger, and gust's whole value is
// repositioning. So they are excluded from the throughput band there and proved
// here instead, against moving enemies.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/Registry.h"

#include "support/Plots.h"
#include "sim/World.h"

using namespace td;

namespace {

content::Registry loadReg() {
    content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

core::Loadout ownAll() {
    core::Loadout lo;
    lo.ownAll = true;
    return lo;
}

// A world with one fully levelled, fully specialised tower on the path.
std::unique_ptr<sim::World> withTower(const content::Registry& reg, const char* towerId,
                                     const char* towerSpec, const char* elementId,
                                     const char* elementSpec, float coversDistance = 2.0f) {
    auto w = std::make_unique<sim::World>(reg, reg.map("greenfields"), 31337, ownAll(),
                                          /*goldOverride=*/1000000);
    // Sited to cover the stretch of road the caller cares about. Build plots are
    // a finite authored set, so "the first plot" is wherever the map put it --
    // which for a test that parks an enemy nine tiles along is out of range.
    const auto at = w->path().positionAt(coversDistance);
    const auto [px, py] = tdtest::plotNear(at.x, at.y);
    REQUIRE(w->placeTower(px, py, towerId) == sim::World::PlaceResult::Ok);
    while (w->upgradeCost(px, py) > 0) w->upgradeTower(px, py);
    w->attachElement(px, py, elementId);
    w->specialiseTower(px, py, towerSpec);
    w->specialiseElement(px, py, elementSpec);
    w->enterSandbox();
    return w;
}

// A maxed, specialised tower kills a normal enemy in about a second, which is
// too fast to observe a status on. These targets are inflated so the MECHANISM
// is what the test measures, not the time-to-kill.
constexpr float kTanky = 80.0f;

entt::entity firstEnemy(sim::World& w) {
    entt::entity found = entt::null;
    w.reg().view<const sim::EnemyTag>().each([&](entt::entity e, const sim::EnemyTag&) {
        if (found == entt::null) found = e;
    });
    return found;
}

void advance(sim::World& w, float seconds) {
    const int steps = static_cast<int>(seconds / sim::kFixedDt);
    for (int i = 0; i < steps; ++i) w.tick(sim::kFixedDt);
}

}  // namespace

TEST_CASE("chill actually slows what it hits", "[control]") {
    const auto reg = loadReg();
    auto w = withTower(reg, "arrow", "elf", "water", "chill");
    w->spawnEnemy("slime", kTanky);
    advance(*w, 3.0f);

    const auto e = firstEnemy(*w);
    REQUIRE((e != entt::null));
    const auto* slow = w->reg().try_get<sim::Slowed>(e);
    REQUIRE(slow != nullptr);
    REQUIRE(slow->pct > 0.0f);
}

TEST_CASE("chill stacks toward a cap but never stops anything", "[control]") {
    const auto reg = loadReg();
    auto w = withTower(reg, "arrow", "elf", "water", "chill");
    w->spawnEnemy("goblin", kTanky);  // enough health to survive being shot at
    advance(*w, 6.0f);

    const auto e = firstEnemy(*w);
    REQUIRE((e != entt::null));
    const auto* slow = w->reg().try_get<sim::Slowed>(e);
    REQUIRE(slow != nullptr);
    // Stopping things is Freeze's job; chill must always leave movement.
    REQUIRE(slow->pct < 1.0f);
}

TEST_CASE("freeze eventually stops something outright", "[control]") {
    const auto reg = loadReg();
    auto w = withTower(reg, "arrow", "elf", "water", "freeze");
    w->spawnEnemy("goblin", kTanky);
    // Chance-based per hit, so this needs enough hits for the roll to land. A
    // fixed seed makes it deterministic rather than flaky.
    bool everFrozen = false;
    for (int i = 0; i < 600 && !everFrozen; ++i) {
        w->tick(sim::kFixedDt);
        const auto e = firstEnemy(*w);
        if (e != entt::null && w->reg().all_of<sim::Frozen>(e)) everFrozen = true;
    }
    REQUIRE(everFrozen);
}

TEST_CASE("shatter pays out only against an already-slowed target", "[control]") {
    const auto reg = loadReg();

    // Same tower, same seed, same enemy: the only difference is whether the
    // target is slowed when shatter's hits land.
    const auto damageDealt = [&](bool preSlow) {
        auto w = withTower(reg, "arrow", "elf", "water", "shatter");
        w->spawnEnemy("goblin", kTanky);
        const auto e = firstEnemy(*w);
        REQUIRE((e != entt::null));
        if (preSlow) w->reg().emplace<sim::Slowed>(e, 0.5f, 60.0f);
        const float before = w->reg().get<sim::Health>(e).hp;
        advance(*w, 4.0f);
        // The entity may have died; treat that as maximum damage.
        const auto* hp = w->reg().try_get<sim::Health>(e);
        return hp ? before - hp->hp : before;
    };

    const float plain = damageDealt(false);
    const float slowed = damageDealt(true);
    REQUIRE(plain > 0.0f);  // the tower still shoots
    REQUIRE(slowed > plain);
}

TEST_CASE("gust pushes an enemy back down the path", "[control]") {
    const auto reg = loadReg();

    const auto distanceAfter = [&](const char* elementSpec) {
        auto w = withTower(reg, "arrow", "elf", "wind", elementSpec);
        w->spawnEnemy("goblin", kTanky);
        advance(*w, 4.0f);
        const auto e = firstEnemy(*w);
        if (e == entt::null) return -1.0f;  // died; not what this measures
        return w->reg().get<sim::PathFollower>(e).distance;
    };

    const float withGust = distanceAfter("gust");
    const float withShock = distanceAfter("shock");
    REQUIRE(withGust >= 0.0f);
    REQUIRE(withShock >= 0.0f);
    // Same tower and same walk time, so a shorter distance travelled is the
    // shove. This is the tempo gust buys, and the reason the throughput harness
    // reads it as a penalty.
    REQUIRE(withGust < withShock);
}

TEST_CASE("a boss resists the shove far more than a regular enemy", "[control][boss]") {
    const auto reg = loadReg();

    const auto pushedBackBy = [&](const char* enemyId) {
        auto w = withTower(reg, "arrow", "elf", "wind", "gust", /*coversDistance=*/9.0f);
        w->spawnEnemy(enemyId, kTanky);
        const auto e = firstEnemy(*w);
        REQUIRE((e != entt::null));
        auto& pf = w->reg().get<sim::PathFollower>(e);
        // Far enough along that neither enemy can be shoved back to the start,
        // and a short window so the totals stay well clear of it. At distance 6
        // over 4 seconds both hit the clamp at 0 and came out EQUAL, which made
        // the comparison meaningless rather than false.
        pf.distance = 9.0f;
        const float before = pf.distance;
        // Freeze movement so the only thing changing distance is the shove.
        w->reg().get<sim::Speed>(e).base = 0.0f;
        advance(*w, 1.0f);
        const auto* after = w->reg().try_get<sim::PathFollower>(e);
        if (!after) return 0.0f;
        // Guard the degenerate case explicitly, so it fails loudly next time.
        REQUIRE(after->distance > 0.0f);
        return before - after->distance;
    };

    const float regular = pushedBackBy("goblin");
    const float boss = pushedBackBy("boss_warlord_grulk");
    REQUIRE(regular > 0.0f);
    // A boss that could be held at the entrance by knockback would never arrive.
    REQUIRE(boss < regular);
}

TEST_CASE("a forge raises the output of the tower beside it", "[control][support]") {
    const auto reg = loadReg();

    // Same neighbour tower, same target, same seed. The only difference is
    // whether a specialised Forge sits within its buff radius.
    const auto damageFrom = [&](bool withForge) {
        auto w = std::make_unique<sim::World>(reg, reg.map("greenfields"), 8181, ownAll(),
                                             /*goldOverride=*/1000000);
        REQUIRE(w->placeTower(PLOT(0), "arrow") == sim::World::PlaceResult::Ok);
        while (w->upgradeCost(PLOT(0)) > 0) w->upgradeTower(PLOT(0));

        if (withForge) {
            REQUIRE(w->placeTower(PLOT(1), "brazier") == sim::World::PlaceResult::Ok);
            while (w->upgradeCost(PLOT(1)) > 0) w->upgradeTower(PLOT(1));
            w->specialiseTower(PLOT(1), "forge");
        }
        w->enterSandbox();
        w->spawnEnemy("goblin", kTanky);
        const auto e = firstEnemy(*w);
        REQUIRE((e != entt::null));
        const float before = w->reg().get<sim::Health>(e).hp;
        advance(*w, 4.0f);
        const auto* hp = w->reg().try_get<sim::Health>(e);
        return hp ? before - hp->hp : before;
    };

    const float alone = damageFrom(false);
    const float buffed = damageFrom(true);
    REQUIRE(alone > 0.0f);
    // The forge's own contribution is small by design, so a clear increase here
    // is the aura doing the work rather than one more gun on the field.
    REQUIRE(buffed > alone * 1.10f);
}

TEST_CASE("a forge does not buff itself", "[control][support]") {
    // Otherwise a lone forge would be a strictly better tower than a real gun,
    // rather than a support choice with a cost.
    const auto reg = loadReg();
    auto w = withTower(reg, "brazier", "forge", "earth", "rock");
    w->spawnEnemy("goblin", kTanky);
    advance(*w, 1.0f);

    entt::entity forge = entt::null;
    w->reg().view<const sim::TowerTag>().each([&](entt::entity t, const sim::TowerTag&) {
        if (forge == entt::null) forge = t;
    });
    REQUIRE((forge != entt::null));
    REQUIRE_FALSE(w->reg().all_of<sim::Buffed>(forge));
}
