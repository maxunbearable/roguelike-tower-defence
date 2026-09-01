#include "content/MapFacts.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "content/Defs.h"
#include "content/Registry.h"

namespace td::content {
namespace {

// Every enemy that can appear on this map: the recipe's pool plus its bosses,
// or the authored wave groups when a map does not use a recipe.
std::set<std::string> rosterOf(const MapDef& map) {
    std::set<std::string> out;
    if (map.hasRecipe) {
        for (const auto& p : map.recipe.pool) out.insert(p.enemyId);
        for (const auto& b : map.recipe.bosses) {
            if (!b.enemyId.empty()) out.insert(b.enemyId);
        }
    }
    for (const auto& w : map.waves) {
        for (const auto& g : w.groups) out.insert(g.enemyId);
    }
    return out;
}

}  // namespace

MapBias mapBias(const Registry& reg, const MapDef& map) {
    const auto roster = rosterOf(map);
    if (roster.empty()) return {};

    // Sum every damage type mentioned by anyone, counting a silent enemy as
    // neutral for that type -- otherwise one enemy with a single 0.5 entry would
    // read as the whole roster resisting it.
    std::map<std::string, float> total;
    std::set<std::string> types;
    for (const auto& id : roster) {
        if (!reg.hasEnemy(id)) continue;
        for (const auto& [type, mult] : reg.enemy(id).resist) types.insert(type);
    }
    if (types.empty()) return {};

    int counted = 0;
    for (const auto& id : roster) {
        if (!reg.hasEnemy(id)) continue;
        ++counted;
        const auto& e = reg.enemy(id);
        for (const auto& t : types) {
            const auto it = e.resist.find(t);
            total[t] += it == e.resist.end() ? 1.0f : it->second;
        }
    }
    if (counted == 0) return {};

    MapBias out;
    out.resistantMult = 2.0f;
    out.vulnerableMult = 0.0f;
    for (const auto& t : types) {
        const float avg = total[t] / static_cast<float>(counted);
        // Ties break on the type name so the screen never flickers between two
        // equally-weighted answers.
        if (avg < out.resistantMult || (avg == out.resistantMult && t < out.resistant)) {
            out.resistantMult = avg;
            out.resistant = t;
        }
        if (avg > out.vulnerableMult || (avg == out.vulnerableMult && t < out.vulnerable)) {
            out.vulnerableMult = avg;
            out.vulnerable = t;
        }
    }
    // A roster only has a BIAS if the tilt is big enough to change a build.
    // At the old 0.98/1.02 threshold greenfields advertised "resists piercing"
    // off a 0.93 average -- a 7% reduction, inherited from shared enemies rather
    // than designed, and piercing is what the starting arrow tower deals. The
    // tutorial map telling a new player their only weapon is resisted is both
    // discouraging and, at that magnitude, not really true. A map that is
    // genuinely even should say so.
    out.valid = out.resistant != out.vulnerable &&
                (out.resistantMult <= 0.85f || out.vulnerableMult >= 1.15f);
    return out;
}

}  // namespace td::content
