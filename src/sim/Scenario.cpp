#include "sim/Scenario.h"

#include <algorithm>
#include <utility>
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
// Back to the values that held for most of development. They were raised when
// every scenario saturated at 100%, but the real cause was the harness silently
// inheriting the whole global tree via ownAll; with the loadout narrowed to just
// the spec unlocks, these discriminate again.
constexpr float kHpMult = 34.0f;
// Armour stays near its old value on purpose. It is SUBTRACTIVE, so raising it
// punishes small frequent hits far more than large ones -- at 22 it inverted
// "fire rate beats hit size for poison uptime", because armour rather than
// poison was deciding the outcome. HP was what had saturated; armour was not.
// Rescaled from 9.0 when the deficit profile cut base tower damage to 0.78x.
// Armour is subtracted per HIT, so its bite is a fraction of hit size: shrinking
// every hit by 22% while holding armour fixed silently hands the advantage to
// big slow hits, and this test inverted -- sniper 11.5% vs elf 10.0% -- for
// exactly the reason recorded above, but from the damage side instead of the
// armour side. The fixture's armour has to track tower damage or it stops
// measuring poison and starts measuring armour again.
constexpr float kArmorAdd = 7.0f;

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
    // Owns exactly what it needs to BUY a specialisation, and nothing else.
    //
    // This used to set ownAll, which quietly included the whole global tree. Once
    // that tree grew to 23 nodes (x1.49 damage, x1.30 fire rate, +2 armour pen
    // for every tower) the matrix stopped measuring specs and started measuring
    // specs-plus-a-uniform-buff: the +2 pen alone erased Sniper's anti-armour
    // niche, because armour was the fast spec's only weakness. No value of
    // kArmorAdd could restore the distinction, which is the tell that the
    // harness was measuring the wrong thing.
    core::Loadout meta;
    meta.ownAll = false;
    meta.ownedNodes.insert("global.level2");   // grants global.unlock.level2
    meta.ownedNodes.insert("global.level3");   // grants global.unlock.level3
    for (const auto& [treeId, tree] : reg.trees()) {
        for (const auto& sp : tree.specs) meta.ownedNodes.insert(treeId + "." + sp + ".core");
    }
    // Tower types are unlocked by a charter at the root of their tree, and this
    // harness places every one of them.
    for (const auto& [towerId, def] : reg.towers()) meta.ownedNodes.insert(towerId + ".unlock");

    World w(reg, reg.map("greenfields"), seed, meta, /*goldOverride=*/1000000);

    // ONE specialised tower, because a specialisation is unique on the map --
    // two snipers is not a configuration a player can ever build. A pair of
    // plain arrow towers stands in for the supporting fire a real board has.
    // Plots are a finite authored set now, so the harness asks the map where it
    // may build instead of naming tiles, which would have silently stopped
    // measuring anything the day the maps grew build plots.
    core::Vec2 focus{0.0f, 0.0f};
    const auto stands = layout(kind);
    for (const auto& p : stands) {
        const auto v = w.path().positionAt(p.distance);
        focus.x += v.x;
        focus.y += v.y;
    }
    focus.x /= static_cast<float>(stands.size());
    focus.y /= static_cast<float>(stands.size());

    std::vector<std::pair<int, int>> plots;
    for (int y = 0; y < w.map().gridH; ++y) {
        for (int x = 0; x < w.map().gridW; ++x) {
            if (w.map().buildableAt(x, y)) plots.emplace_back(x, y);
        }
    }
    const auto d2 = [&](std::pair<int, int> q) {
        const float dx = static_cast<float>(q.first) + 0.5f - focus.x;
        const float dy = static_cast<float>(q.second) + 0.5f - focus.y;
        return dx * dx + dy * dy;
    };
    // Anchor on a plot that has COMPANY near the enemies, not merely the plot
    // nearest to them. Maps place plots in clumps and the closest one to the
    // road is sometimes a lone pad -- a forge sited there buffs a single
    // neighbour, below its 1.57 break-even, and the harness then reports the
    // game's one support spec as worse than taking no spec at all. That
    // measures a bad placement, not the specialisation. A player puts a forge in
    // a clump, so this does too.
    const auto companions = [&](std::pair<int, int> q) {
        int n = 0;
        for (const auto& o : plots) {
            if (o == q) continue;
            const float dx = static_cast<float>(o.first - q.first);
            const float dy = static_cast<float>(o.second - q.second);
            if (dx * dx + dy * dy <= 2.5f * 2.5f) ++n;
        }
        return n;
    };
    std::sort(plots.begin(), plots.end(), [&](auto l, auto r) {
        const int cl = std::min(companions(l), 3), cr = std::min(companions(r), 3);
        if (cl != cr) return cl > cr;
        return d2(l) < d2(r);
    });
    if (!plots.empty()) {
        const auto anchor = plots.front();
        const auto near2 = [&](std::pair<int, int> q) {
            const float dx = static_cast<float>(q.first - anchor.first);
            const float dy = static_cast<float>(q.second - anchor.second);
            return dx * dx + dy * dy;
        };
        std::sort(plots.begin() + 1, plots.end(),
                  [&](auto l, auto r) { return near2(l) < near2(r); });
    }

    // ONE specialised tower, because a specialisation is unique on the map --
    // two snipers is not a configuration a player can ever build.
    const int sx = plots.at(0).first, sy = plots.at(0).second;
    w.placeTower(sx, sy, towerId);
    // Specialising requires a fully levelled tower, so the matrix measures a
    // maxed tower -- the only configuration that can carry a spec.
    while (w.upgradeCost(sx, sy) > 0) w.upgradeTower(sx, sy);
    w.attachElement(sx, sy, elementId);
    w.specialiseTower(sx, sy, towerSpec);
    w.specialiseElement(sx, sy, elementSpec);
    // THREE plain towers, not one, and LEVELLED like the specialised one.
    //
    // Forge trades 61% of its own output (damage x0.55, rate x0.7) for +18%
    // damage and +18% rate on every tower in radius, so it breaks even at 1.57
    // buffed neighbours. With exactly one it is structurally behind, and it
    // measured 3% against a 20% median across the matrix -- read for the whole
    // project as a dead build, when it was a mismeasured one. Leaving the
    // supporters at level 1 made this a maxed tower standing among beginners
    // rather than a board. A support build must be measured against the board a
    // player actually has.
    for (size_t i = 1; i < 4 && i < plots.size(); ++i) {
        const int tx = plots[i].first, ty = plots[i].second;
        w.placeTower(tx, ty, towerId);
        while (w.upgradeCost(tx, ty) > 0 && w.upgradeTower(tx, ty)) {
        }
    }
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
