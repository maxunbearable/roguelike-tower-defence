#include "sim/Scenario.h"

#include <vector>

#include "sim/World.h"

namespace td::sim {
namespace {

// Greenfields' first path run is the straight y=2 row from x=0 to x=13, so an
// enemy at path distance d sits at x = 0.5 + d. Towers flank it at y=1 and y=3.
struct Placement {
    std::string enemyId;
    float distance;
};

// Enemies are scaled as if around wave 35, and only two towers defend. An
// easier scenario saturates -- every pairing clears 100% and the matrix measures
// nothing. The differences between builds only appear under real pressure.
// Raised when specialising began to require a MAXED tower: a level-3 specced
// tower is several times stronger than the level-1 one this was first tuned
// against, and every pairing was clearing 100% of every scenario. A saturated
// matrix measures nothing.
constexpr float kHpMult = 34.0f;
constexpr float kArmorAdd = 9.0f;

std::vector<Placement> layout(ScenarioKind kind) {
    switch (kind) {
        case ScenarioKind::LoneTank:
            return {{"goblin", 5.5f}};
        case ScenarioKind::Swarm: {
            std::vector<Placement> v;
            for (int i = 0; i < 12; ++i) {
                v.push_back({"slime", 3.0f + static_cast<float>(i) * 0.5f});
            }
            return v;
        }
        case ScenarioKind::Mixed: {
            std::vector<Placement> v;
            for (int i = 0; i < 6; ++i) v.push_back({"slime", 3.0f + static_cast<float>(i) * 0.6f});
            for (int i = 0; i < 3; ++i) v.push_back({"goblin", 4.5f + static_cast<float>(i) * 0.9f});
            for (int i = 0; i < 2; ++i) v.push_back({"wraith", 4.0f + static_cast<float>(i) * 1.1f});
            return v;
        }
    }
    return {};
}

}  // namespace

const char* name(ScenarioKind k) {
    switch (k) {
        case ScenarioKind::LoneTank: return "LoneTank";
        case ScenarioKind::Swarm: return "Swarm";
        case ScenarioKind::Mixed: return "Mixed";
    }
    return "?";
}

ComboResult simulateCombo(const content::Registry& reg, const std::string& towerId,
                          const std::string& towerSpec, const std::string& elementId,
                          const std::string& elementSpec, ScenarioKind kind, float seconds,
                          uint64_t seed) {
    core::Loadout meta;  // owns every skill-tree node; specs are bought below
    meta.ownAll = true;

    World w(reg, reg.map("greenfields"), seed, meta, /*goldOverride=*/1000000);

    // ONE specialised tower, because a specialisation is unique on the map --
    // two snipers is not a configuration a player can ever build. A pair of
    // plain arrow towers stands in for the supporting fire a real board has.
    w.placeTower(5, 1, towerId);
    // Specialising requires a fully levelled tower, so the matrix measures a
    // maxed tower -- which is the only configuration that can carry a spec.
    while (w.upgradeCost(5, 1) > 0) w.upgradeTower(5, 1);
    w.attachElement(5, 1, elementId);
    w.specialiseTower(5, 1, towerSpec);
    w.specialiseElement(5, 1, elementSpec);
    w.placeTower(5, 3, towerId);
    w.enterSandbox();

    ComboResult out;
    auto& r = w.reg();
    for (const auto& p : layout(kind)) {
        const auto& def = reg.enemy(p.enemyId);
        const auto e = r.create();
        const core::Vec2 pos = w.path().positionAt(p.distance);
        const float hp = def.maxHp * kHpMult;
        r.emplace<Position>(e, pos);
        r.emplace<PrevPosition>(e, pos);
        r.emplace<PathFollower>(e, p.distance);
        r.emplace<Health>(e, hp, hp);
        r.emplace<Armor>(e, def.armor + kArmorAdd);
        r.emplace<Speed>(e, 0.0f);  // stationary: this measures throughput, not leaks
        r.emplace<EnemyTag>(e, def.id, def.bounty, def.shardValue);
        out.totalHp += hp;
        ++out.spawned;
    }

    const int steps = static_cast<int>(seconds / kFixedDt);
    for (int i = 0; i < steps; ++i) w.tick(kFixedDt);

    float remaining = 0.0f;
    int alive = 0;
    r.view<const Health, const EnemyTag>().each([&](const Health& hp, const EnemyTag&) {
        remaining += hp.hp > 0.0f ? hp.hp : 0.0f;
        ++alive;
    });
    out.kills = out.spawned - alive;
    out.damageDealt = out.totalHp - remaining;
    return out;
}

}  // namespace td::sim
