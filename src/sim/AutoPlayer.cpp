#include "sim/AutoPlayer.h"

#include <algorithm>
#include <set>

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

// Defined below, used by autoPlay above them. towerPreferenceFor is declared in
// the header instead, because the tests inspect the harness's own choices.
std::string bestElementFor(const content::Registry& reg, const content::MapDef& m);

AutoPlayResult autoPlay(const content::Registry& reg, const content::MapDef& map,
                        const core::Loadout& meta, uint64_t seed, int maxWaves,
                        core::Difficulty difficulty) {
    World w(reg, map, seed, meta, -1, difficulty);

    // Which towers this profile would actually field. Every measurement in this
    // project used to come from a player who built ONLY arrow towers, on a game
    // with five tower types whose entire pitch is 270 combinations -- so the
    // instrument exercised a fifth of the tower content and none of the
    // combinations. A playtesting agent does not have to be skilled, but it does
    // have to be REPRESENTATIVE, or its readings cannot correlate with what a
    // real player meets.
    const auto prefs = towerPreferenceFor(reg, map, w);
    const auto& def = reg.tower(prefs.empty() ? std::string(kStartingTower) : prefs.front());

    // Cheapest thing this profile can put down, for the "can I afford anything"
    // checks. Using the preferred tower's price would stall a poor board that
    // could still afford an arrow.
    int cheapest = def.buildCost;
    for (const auto& id : prefs) cheapest = std::min(cheapest, reg.tower(id).buildCost);

    const auto spots = rankSpots(map, def.range);
    // Read the map before committing, the way a player reads the dossier.
    const std::string element = bestElementFor(reg, map);

    AutoPlayResult res;
    std::vector<std::pair<int, int>> built;

    // Priorities in the order a competent player actually uses them: get enough
    // coverage down first, THEN commit to a build, then deepen it. Specialising
    // a lone tower before the map is covered is a beginner mistake, and
    // measuring against it would tune the game to the wrong player.
    constexpr size_t kCoverageFirst = 6;

    // A ceiling on how many towers to field. `spots.size()` is every buildable
    // tile -- around 140 on a 22x11 map -- and no human builds that. Without a
    // cap, a profile that cannot yet upgrade (levelling is gated on the skill
    // tree) just keeps buying towers, and since targeting is O(towers x enemies)
    // per tick the harness slowed by more than an order of magnitude while
    // measuring a board nobody would ever build.
    constexpr size_t kMaxTowers = 34;

    auto buildOne = [&] {
        for (const auto& s2 : spots) {
            if (w.towerAt(s2.x, s2.y) != entt::null) continue;
            // Rotate through the top few preferences rather than stacking one
            // type. A real board is mixed, and a bot that builds 34 identical
            // towers measures a board nobody plays. Deliberately not optimal:
            // the agent needs to correlate with a human's experience, not beat
            // one.
            const size_t spread = std::min<size_t>(3, prefs.size());
            for (size_t i = 0; i < prefs.size(); ++i) {
                const size_t k = spread > 0 ? (built.size() + i) % spread : 0;
                const std::string& pick = prefs[i < spread ? k : i];
                if (w.placeTower(s2.x, s2.y, pick) == World::PlaceResult::Ok) {
                    built.emplace_back(s2.x, s2.y);
                    ++res.towersBuilt;
                    return true;
                }
            }
        }
        return false;
    };

    auto spend = [&] {
        bool acted = true;
        while (acted) {
            acted = false;

            // 1. Coverage first.
            if (built.size() < kCoverageFirst && w.gold() >= cheapest) {
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
                if (tag.elementId.empty() && w.gold() >= w.attachElementCost(element)) {
                    if (w.attachElement(tx, ty, element)) { acted = true; break; }
                }
                const auto es = w.availableElementSpecs(tx, ty);
                if (!es.empty() && w.gold() >= w.elementSpecCost(tx, ty)) {
                    if (w.specialiseElement(tx, ty, es.front())) { acted = true; break; }
                }
            }
            if (acted) continue;

            // 4. Then widen and deepen, preferring a new tower to a third level.
            if (built.size() < std::min(spots.size(), kMaxTowers) &&
                w.gold() >= cheapest) {
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

    {
        std::set<std::string> kinds;
        w.reg().view<const TowerTag>().each(
            [&](const TowerTag& t) { kinds.insert(t.defId); });
        res.distinctTowerTypes = static_cast<int>(kinds.size());
    }
    res.wavesSurvived = w.waveIndex();
    res.cleared = w.phase() == Phase::Cleared;
    res.shards = w.shardsForRun();
    return res;
}

// Picks the element whose damage type this map's roster is LEAST resistant to.
//
// The autoplayer used to hardcode "earth", which made it a bad proxy on exactly
// the maps that matter: blightmarsh resists earth at 0.5 by design, so the
// harness was measuring a player deliberately bringing the wrong element and
// reporting the map as too hard. A real player reads the dossier and brings the
// counter, so the measuring instrument has to as well.
// Towers this profile can build, ordered by how well their damage type fares
// against the map's roster. Same reasoning as bestElementFor: a player reads the
// dossier and brings a counter, so the measuring instrument has to as well.
std::vector<std::string> towerPreferenceFor(const content::Registry& reg,
                                            const content::MapDef& m, const World& w) {
    std::vector<std::pair<float, std::string>> scored;
    for (const auto& [id, tdef] : reg.towers()) {
        if (!w.towerUnlocked(id)) continue;  // locked towers are not an option
        float total = 0.0f;
        int n = 0;
        for (const auto& p : m.recipe.pool) {
            if (!reg.hasEnemy(p.enemyId)) continue;
            total += reg.enemy(p.enemyId).resistTo(tdef.damageType);
            ++n;
        }
        for (const auto& b : m.recipe.bosses) {
            if (!reg.hasEnemy(b.enemyId)) continue;
            total += reg.enemy(b.enemyId).resistTo(tdef.damageType);
            ++n;
        }
        scored.emplace_back(n > 0 ? total / static_cast<float>(n) : 1.0f, id);
    }
    // Best first; ties broken by id so a run is reproducible.
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    std::vector<std::string> out;
    out.reserve(scored.size());
    for (auto& [score, id] : scored) out.push_back(std::move(id));
    return out;
}

std::string bestElementFor(const content::Registry& reg, const content::MapDef& m) {
    std::string best;
    float bestScore = -1.0f;
    for (const auto& [id, edef] : reg.elements()) {
        float total = 0.0f;
        int n = 0;
        for (const auto& p : m.recipe.pool) {
            if (!reg.hasEnemy(p.enemyId)) continue;
            total += reg.enemy(p.enemyId).resistTo(edef.damageType);
            ++n;
        }
        // Bosses count too: they are the wall the map builds to.
        for (const auto& b : m.recipe.bosses) {
            if (!reg.hasEnemy(b.enemyId)) continue;
            total += reg.enemy(b.enemyId).resistTo(edef.damageType);
            ++n;
        }
        const float score = n > 0 ? total / static_cast<float>(n) : 1.0f;
        // Ties broken by id, so the choice is deterministic across runs.
        if (score > bestScore || (score == bestScore && id < best)) {
            bestScore = score;
            best = id;
        }
    }
    return best.empty() ? "earth" : best;
}

}  // namespace td::sim
