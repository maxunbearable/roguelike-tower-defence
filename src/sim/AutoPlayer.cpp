#include "sim/AutoPlayer.h"

#include <algorithm>

namespace td::sim {
namespace {

struct Spot {
    int x = 0, y = 0;
    int coverage = 0;  // path tiles within a typical tower's reach
};

// Ranks buildable tiles by how much of the route they cover, which is roughly
// how a human picks spots.
std::vector<Spot> rankSpots(const content::MapDef& m, float range) {
    std::vector<Spot> out;
    for (int y = 0; y < m.gridH; ++y) {
        for (int x = 0; x < m.gridW; ++x) {
            if (!m.buildableAt(x, y)) continue;
            int cover = 0;
            const int r = static_cast<int>(range) + 1;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    const char c = m.tileAt(x + dx, y + dy);
                    if (c != '=' && c != 'S' && c != 'E') continue;
                    if (static_cast<float>(dx * dx + dy * dy) <= range * range) ++cover;
                }
            }
            if (cover > 0) out.push_back({x, y, cover});
        }
    }
    std::sort(out.begin(), out.end(), [](const Spot& a, const Spot& b) {
        if (a.coverage != b.coverage) return a.coverage > b.coverage;
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    return out;
}

}  // namespace

AutoPlayResult autoPlay(const content::Registry& reg, const content::MapDef& map,
                        const core::Loadout& meta, uint64_t seed, int maxWaves) {
    World w(reg, map, seed, meta);
    const auto& def = reg.tower("arrow");
    const auto spots = rankSpots(map, def.range);

    AutoPlayResult res;
    std::vector<std::pair<int, int>> built;

    // Priorities in the order a competent player actually uses them: get enough
    // coverage down first, THEN commit to a build, then deepen it. Specialising
    // a lone tower before the map is covered is a beginner mistake, and
    // measuring against it would tune the game to the wrong player.
    constexpr size_t kCoverageFirst = 6;

    auto buildOne = [&] {
        for (const auto& s2 : spots) {
            if (w.towerAt(s2.x, s2.y) != entt::null) continue;
            if (w.placeTower(s2.x, s2.y, "arrow") == World::PlaceResult::Ok) {
                built.emplace_back(s2.x, s2.y);
                ++res.towersBuilt;
                return true;
            }
        }
        return false;
    };

    auto spend = [&] {
        bool acted = true;
        while (acted) {
            acted = false;

            // 1. Coverage first.
            if (built.size() < kCoverageFirst && w.gold() >= def.buildCost) {
                if (buildOne()) { acted = true; continue; }
            }

            // 2. Specialising now requires a fully levelled tower, so drive one
            //    tower to max before spreading gold around. A real player picks a
            //    champion rather than levelling everything evenly.
            for (const auto& [tx, ty] : built) {
                if (w.atMaxLevel(tx, ty)) continue;
                const int c = w.upgradeCost(tx, ty);
                const bool anySpecLeft = !w.activeTowerSpecs().empty()
                                             ? w.activeTowerSpecs().size() < 3
                                             : true;
                if (anySpecLeft && c > 0 && w.gold() >= c && w.upgradeTower(tx, ty)) {
                    acted = true;
                    break;
                }
            }
            if (acted) continue;

            for (const auto& [tx, ty] : built) {
                const auto specs = w.availableTowerSpecs(tx, ty);
                if (!specs.empty() && w.gold() >= w.towerSpecCost(tx, ty)) {
                    if (w.specialiseTower(tx, ty, specs.front())) { acted = true; break; }
                }
            }
            if (acted) continue;

            // 3. Element, then its specialisation, tower by tower.
            for (const auto& [tx, ty] : built) {
                const auto& tag = w.reg().get<TowerTag>(w.towerAt(tx, ty));
                if (tag.elementId.empty() && w.gold() >= w.attachElementCost("earth")) {
                    if (w.attachElement(tx, ty, "earth")) { acted = true; break; }
                }
                const auto es = w.availableElementSpecs(tx, ty);
                if (!es.empty() && w.gold() >= w.elementSpecCost(tx, ty)) {
                    if (w.specialiseElement(tx, ty, es.front())) { acted = true; break; }
                }
            }
            if (acted) continue;

            // 4. Then widen and deepen, preferring a new tower to a third level.
            if (built.size() < spots.size() && w.gold() >= def.buildCost) {
                if (buildOne()) { acted = true; continue; }
            }
            for (const auto& [tx, ty] : built) {
                const int c = w.upgradeCost(tx, ty);
                if (c > 0 && w.gold() >= c && w.upgradeTower(tx, ty)) { acted = true; break; }
            }
        }
    };

    for (int guard = 0; guard < maxWaves * 4; ++guard) {
        if (w.phase() == Phase::Cleared || w.phase() == Phase::Defeated) break;
        if (w.waveIndex() >= maxWaves) break;

        if (w.phase() == Phase::Build) {
            spend();
            res.peakGold = std::max(res.peakGold, w.gold());
            w.startNextWave();
        }
        // Run the wave out.
        for (int i = 0; i < 120 * 60 && w.phase() == Phase::Wave; ++i) w.tick(kFixedDt);
    }

    res.wavesSurvived = w.waveIndex();
    res.cleared = w.phase() == Phase::Cleared;
    res.shards = w.shardsForRun();
    return res;
}

}  // namespace td::sim
