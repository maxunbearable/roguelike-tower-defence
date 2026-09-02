#include "content/Validate.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <utility>

#include "content/Registry.h"
#include "core/Resolve.h"

namespace td::content {
namespace {

std::pair<int, int> tileOf(const core::Vec2& v) {
    return {static_cast<int>(std::lround(v.x)), static_cast<int>(std::lround(v.y))};
}

std::string at(const std::string& mapId) { return "map '" + mapId + "': "; }

// Walks a path segment tile by tile. Segments are axis-aligned (checked
// separately), so this is a plain integer march.
template <typename F>
void marchSegment(std::pair<int, int> a, std::pair<int, int> b, F&& fn) {
    const int dx = (b.first > a.first) - (b.first < a.first);
    const int dy = (b.second > a.second) - (b.second < a.second);
    auto cur = a;
    while (true) {
        fn(cur);
        if (cur == b) break;
        cur.first += dx;
        cur.second += dy;
    }
}

void validateMap(const MapDef& m, const Registry& reg, std::vector<std::string>& out) {
    // 2: grid shape
    if (static_cast<int>(m.tileRows.size()) != m.gridH) {
        out.push_back(at(m.id) + "tiles has " + std::to_string(m.tileRows.size()) +
                      " rows but gridH is " + std::to_string(m.gridH));
        return;  // every later check indexes the grid
    }
    for (size_t y = 0; y < m.tileRows.size(); ++y) {
        if (static_cast<int>(m.tileRows[y].size()) != m.gridW) {
            out.push_back(at(m.id) + "row " + std::to_string(y) + " has " +
                          std::to_string(m.tileRows[y].size()) + " columns but gridW is " +
                          std::to_string(m.gridW));
            return;
        }
    }

    // 4: enough waypoints to form a route
    if (m.pathWaypoints.size() < 2) {
        out.push_back(at(m.id) + "path needs at least 2 waypoints");
        return;
    }

    // 3: waypoints inside the grid
    for (const auto& wp : m.pathWaypoints) {
        const auto [x, y] = tileOf(wp);
        if (x < 0 || y < 0 || x >= m.gridW || y >= m.gridH) {
            out.push_back(at(m.id) + "path waypoint (" + std::to_string(x) + "," +
                          std::to_string(y) + ") is outside the grid");
            return;
        }
    }

    // 5: segments must be axis-aligned
    bool axisOk = true;
    for (size_t i = 1; i < m.pathWaypoints.size(); ++i) {
        const auto a = tileOf(m.pathWaypoints[i - 1]);
        const auto b = tileOf(m.pathWaypoints[i]);
        if (a.first != b.first && a.second != b.second) {
            out.push_back(at(m.id) + "path segment " + std::to_string(i - 1) + "->" +
                          std::to_string(i) + " is not axis-aligned");
            axisOk = false;
        }
    }

    // 6: endpoints
    const auto first = tileOf(m.pathWaypoints.front());
    const auto last = tileOf(m.pathWaypoints.back());
    if (m.tileAt(first.first, first.second) != 'S') {
        out.push_back(at(m.id) + "first waypoint is not on an 'S' tile");
    }
    if (m.tileAt(last.first, last.second) != 'E') {
        out.push_back(at(m.id) + "last waypoint is not on an 'E' tile");
    }

    // A map with no build plots cannot be played at all, and a map with a
    // handful cannot be defended. Towers may only stand on 'o', so this is the
    // difference between a level and a corridor -- worth refusing to start over
    // rather than discovering on the board.
    {
        int plots = 0;
        for (int y = 0; y < m.gridH; ++y) {
            for (int x = 0; x < m.gridW; ++x) {
                if (m.buildableAt(x, y)) ++plots;
            }
        }
        if (plots < 8) {
            out.push_back(at(m.id) + "has only " + std::to_string(plots) +
                          " build plots ('o' tiles); a map needs at least 8 to be playable");
        }
    }

    if (axisOk) {
        // 7: every tile the route crosses must be path, spawn or exit
        std::set<std::pair<int, int>> covered;
        for (size_t i = 1; i < m.pathWaypoints.size(); ++i) {
            marchSegment(tileOf(m.pathWaypoints[i - 1]), tileOf(m.pathWaypoints[i]),
                         [&](std::pair<int, int> t) {
                             covered.insert(t);
                             const char c = m.tileAt(t.first, t.second);
                             if (c != '=' && c != 'S' && c != 'E') {
                                 out.push_back(at(m.id) + "route crosses tile (" +
                                               std::to_string(t.first) + "," +
                                               std::to_string(t.second) + ") which is '" + c +
                                               "', not a path tile");
                             }
                         });
        }
        // 10: no path tile left stranded off the route
        for (int y = 0; y < m.gridH; ++y) {
            for (int x = 0; x < m.gridW; ++x) {
                const char c = m.tileAt(x, y);
                if ((c == '=' || c == 'S' || c == 'E') && covered.count({x, y}) == 0) {
                    out.push_back(at(m.id) + "path tile (" + std::to_string(x) + "," +
                                  std::to_string(y) + ") is not on the waypoint route");
                }
            }
        }
    }

    // 1: waves reference known enemies
    for (size_t w = 0; w < m.waves.size(); ++w) {
        if (m.waves[w].groups.empty()) {
            out.push_back(at(m.id) + "wave " + std::to_string(w) + " has no groups");
        }
        for (const auto& g : m.waves[w].groups) {
            if (!reg.hasEnemy(g.enemyId)) {
                out.push_back(at(m.id) + "wave " + std::to_string(w) +
                              " references unknown enemy '" + g.enemyId + "'");
            }
            if (g.count <= 0) {
                out.push_back(at(m.id) + "wave " + std::to_string(w) + " group '" + g.enemyId +
                              "' has a non-positive count");
            }
            if (g.interval <= 0.0f) {
                out.push_back(at(m.id) + "wave " + std::to_string(w) + " group '" + g.enemyId +
                              "' has a non-positive interval");
            }
        }
    }
    if (m.waves.empty()) out.push_back(at(m.id) + "has no waves");

    // Recipe-specific checks. Generated waves are validated above like any
    // other, but the recipe itself can be malformed in ways that produce a
    // silently empty or nonsensical schedule.
    if (m.hasRecipe) {
        const auto& r = m.recipe;
        if (r.pool.empty()) out.push_back(at(m.id) + "wave recipe has an empty pool");
        bool anyAtWaveOne = false;
        for (const auto& p : r.pool) {
            if (!reg.hasEnemy(p.enemyId)) {
                out.push_back(at(m.id) + "wave pool references unknown enemy '" + p.enemyId + "'");
            }
            if (p.fromWave < 1) {
                out.push_back(at(m.id) + "wave pool entry '" + p.enemyId + "' has fromWave < 1");
            }
            if (p.fromWave <= 1) anyAtWaveOne = true;
        }
        if (!anyAtWaveOne) {
            out.push_back(at(m.id) + "no wave-pool enemy is available at wave 1");
        }

        for (const auto& b : r.bosses) {
            if (!reg.hasEnemy(b.enemyId)) {
                out.push_back(at(m.id) + "boss wave references unknown enemy '" + b.enemyId + "'");
                continue;
            }
            if (b.wave < 1 || b.wave > r.count) {
                out.push_back(at(m.id) + "boss '" + b.enemyId + "' is on wave " +
                              std::to_string(b.wave) + ", outside the map's " +
                              std::to_string(r.count) + " waves");
            }
            if (!reg.enemy(b.enemyId).boss) {
                out.push_back(at(m.id) + "boss wave enemy '" + b.enemyId +
                              "' is not flagged boss = true");
            }
        }
        if (r.countBase <= 0) out.push_back(at(m.id) + "wave recipe countBase must be positive");
        if (r.intervalMin <= 0.0f) {
            out.push_back(at(m.id) + "wave recipe intervalMin must be positive");
        }
        if (r.hpCurveExp < 1.0f) {
            out.push_back(at(m.id) + "wave recipe hpCurveExp below 1 flattens the late game");
        }
        if (r.hpPerWave < 1.0f) {
            out.push_back(at(m.id) + "wave recipe hpPerWave < 1 makes later waves weaker");
        }
        if (static_cast<int>(m.waves.size()) != r.count) {
            out.push_back(at(m.id) + "recipe asked for " + std::to_string(r.count) +
                          " waves but generated " + std::to_string(m.waves.size()));
        }
    }
    if (m.startGold < 0) out.push_back(at(m.id) + "startGold is negative");
}

}  // namespace

std::vector<std::string> validate(const Registry& reg) {
    std::vector<std::string> out;

    // 9: enemies
    for (const auto& [id, e] : reg.enemies()) {
        if (e.maxHp <= 0.0f) out.push_back("enemy '" + id + "': maxHp must be positive");
        if (e.speed <= 0.0f) out.push_back("enemy '" + id + "': speed must be positive");
        if (e.armor < 0.0f) out.push_back("enemy '" + id + "': armor must not be negative");
    }

    // 8: towers
    for (const auto& [id, t] : reg.towers()) {
        if (t.buildCost <= 0) out.push_back("tower '" + id + "': buildCost must be positive");
        if (t.fireRate <= 0.0f) out.push_back("tower '" + id + "': fireRate must be positive");
        if (t.range <= 0.0f) out.push_back("tower '" + id + "': range must be positive");
        if (t.projectileSpeed <= 0.0f) {
            out.push_back("tower '" + id + "': projectileSpeed must be positive");
        }
        if (t.projectileCount < 1) {
            out.push_back("tower '" + id + "': projectileCount must be at least 1");
        }
        if (t.sellRefundPct < 0.0f || t.sellRefundPct > 1.0f) {
            out.push_back("tower '" + id + "': sellRefundPct must be within [0,1]");
        }
        static const std::set<std::string> kPriorities{"first", "last", "strongest", "weakest",
                                                       "closest"};
        if (kPriorities.count(t.targetPriority) == 0) {
            out.push_back("tower '" + id + "': unknown targetPriority '" + t.targetPriority + "'");
        }
        for (size_t i = 0; i < t.levels.size(); ++i) {
            if (t.levels[i].cost <= 0) {
                out.push_back("tower '" + id + "': level " + std::to_string(i + 2) +
                              " cost must be positive");
            }
        }
    }

    for (const auto& [id, m] : reg.maps()) validateMap(m, reg, out);

    // --- skill trees -------------------------------------------------------
    const auto knownPaths = core::knownStatPaths(reg);
    for (const auto& [treeId, tree] : reg.trees()) {
        const std::string where = "tree '" + treeId + "': ";
        std::set<std::string> ids;
        for (const auto& n : tree.nodes) {
            if (!ids.insert(n.id).second) out.push_back(where + "duplicate node id '" + n.id + "'");
            if (n.cost <= 0) out.push_back(where + "node '" + n.id + "' has a non-positive cost");

            if (n.branch != "trunk" &&
                std::find(tree.specs.begin(), tree.specs.end(), n.branch) == tree.specs.end()) {
                out.push_back(where + "node '" + n.id + "' is on undeclared branch '" + n.branch +
                              "'");
            }
            for (const auto& m : n.modifiers) {
                // Trait flags are declared by the node that grants them, so they
                // are not expected to appear in the seeded stat paths.
                const bool isTraitFlag = m.target.find(".trait.") != std::string::npos;
                if (!isTraitFlag && knownPaths.count(m.target) == 0) {
                    out.push_back(where + "node '" + n.id + "' modifies unknown stat path '" +
                                  m.target + "'");
                }
            }
        }
        for (const auto& n : tree.nodes) {
            for (const auto& req : n.prereqs) {
                if (!tree.find(req)) {
                    out.push_back(where + "node '" + n.id + "' requires unknown node '" + req +
                                  "'");
                }
            }
        }

        // Prerequisite cycles, found by DFS. A cycle would make a node
        // permanently unbuyable, which is invisible until a player tries.
        std::map<std::string, int> state;  // 0 unvisited, 1 on stack, 2 done
        std::function<bool(const std::string&)> hasCycle = [&](const std::string& id) -> bool {
            auto& st = state[id];
            if (st == 1) return true;
            if (st == 2) return false;
            st = 1;
            if (const auto* n = tree.find(id)) {
                for (const auto& req : n->prereqs) {
                    if (tree.find(req) && hasCycle(req)) return true;
                }
            }
            st = 2;
            return false;
        };
        for (const auto& n : tree.nodes) {
            if (hasCycle(n.id)) {
                out.push_back(where + "prerequisite cycle involving node '" + n.id + "'");
                break;
            }
        }
    }

    // --- damage types ------------------------------------------------------
    std::set<std::string> knownDamageTypes;
    for (const auto& [id, t] : reg.towers()) knownDamageTypes.insert(t.damageType);
    for (const auto& [id, e] : reg.elements()) knownDamageTypes.insert(e.damageType);
    for (const auto& [id, e] : reg.enemies()) {
        for (const auto& [type, mult] : e.resist) {
            if (knownDamageTypes.count(type) == 0) {
                out.push_back("enemy '" + id + "': resistance to unknown damage type '" + type +
                              "'");
            }
            if (mult < 0.0f) {
                out.push_back("enemy '" + id + "': negative resistance to '" + type + "'");
            }
        }
    }

    return out;
}

}  // namespace td::content
