#include "core/Resolve.h"

#include "content/Registry.h"

namespace td::core {
namespace {

void seedTower(StatBlock& sb, const content::TowerDef& t) {
    const std::string p = t.id + ".";
    sb.setBase(p + "damage", t.damage);
    sb.setBase(p + "fireRate", t.fireRate);
    sb.setBase(p + "range", t.range);
    sb.setBase(p + "projectileSpeed", t.projectileSpeed);
    sb.setBase(p + "projectileCount", static_cast<float>(t.projectileCount));
    sb.setBase(p + "pierce", static_cast<float>(t.pierce));
    sb.setBase(p + "critChance", t.critChance);
    sb.setBase(p + "critMult", t.critMult);
    sb.setBase(p + "armorPen", t.armorPen);
    // Trait parameters have no meaningful base; a spec that grants the trait
    // also sets them. Seeding them keeps `has()` honest for validation.
    sb.setBase(p + "execute.threshold", 0.0f);
    sb.setBase(p + "execute.mult", 1.0f);
    sb.setBase(p + "rampUp.perSec", 0.0f);
    sb.setBase(p + "rampUp.maxMult", 1.0f);
}

void seedElement(StatBlock& sb, const content::ElementDef& e) {
    for (const auto& [k, v] : e.base) sb.setBase(e.id + "." + k, v);
    for (const auto& [spec, params] : e.specs) {
        for (const auto& [k, v] : params) sb.setBase(e.id + "." + spec + "." + k, v);
    }
}

// Applies a tree's owned nodes, skipping branches that are not equipped.
void foldTree(StatBlock& sb, const SkillTree& tree, const Loadout& lo,
              const std::string& equippedBranch) {
    for (const auto& n : tree.nodes) {
        if (!lo.owns(n.id)) continue;
        if (n.branch != "trunk" && n.branch != equippedBranch) continue;
        sb.apply(n.modifiers);
    }
}

}  // namespace

StatBlock resolveStats(const content::Registry& reg, const Loadout& lo) {
    StatBlock sb;
    sb.setBase("global.startGold", 0.0f);
    sb.setBase("global.lives", 0.0f);
    // Ability tuning. A MULTIPLIED path must start at 1 -- StatBlock computes
    // (base + adds) * mults, so a multiplier on an unseeded path yields zero and
    // the ability silently does nothing. Additive paths start at 0 and are added
    // to the constants in World.
    sb.setBase("global.strike.damage", 1.0f);
    sb.setBase("global.strike.radius", 0.0f);
    sb.setBase("global.strike.cooldown", 0.0f);
    sb.setBase("global.ward.duration", 0.0f);
    sb.setBase("global.ward.slow", 0.0f);
    sb.setBase("global.ward.cooldown", 0.0f);

    if (reg.hasTower(lo.towerId)) seedTower(sb, reg.tower(lo.towerId));
    if (reg.hasElement(lo.elementId)) seedElement(sb, reg.element(lo.elementId));

    if (reg.hasTree("global")) foldTree(sb, reg.tree("global"), lo, "");
    if (reg.hasTree(lo.towerId)) foldTree(sb, reg.tree(lo.towerId), lo, lo.towerSpec);
    if (reg.hasTree(lo.elementId)) foldTree(sb, reg.tree(lo.elementId), lo, lo.elementSpec);
    return sb;
}

std::set<std::string> knownStatPaths(const content::Registry& reg) {
    std::set<std::string> out;
    out.insert("global.startGold");
    out.insert("global.lives");
    // Unlock flags: what a tower is permitted to do, bought in the global tree.
    out.insert("global.unlock.level2");
    out.insert("global.unlock.level3");
    // The two player abilities. They were the only system in the game with no
    // progression at all -- every other capability (levels, elements, tower
    // specialisations, element specialisations, tower types) is bought in a
    // tree, while Strike and Ward were fixed constants a player could never
    // improve. Kingdom Rush upgrades its Rain of Fire along exactly these axes:
    // damage, radius, and a flat cooldown reduction.
    out.insert("global.strike.damage");
    out.insert("global.strike.radius");
    out.insert("global.strike.cooldown");
    out.insert("global.ward.duration");
    out.insert("global.ward.slow");
    out.insert("global.ward.cooldown");
    for (const auto& [id, t] : reg.towers()) {
        // Trait parameter paths live here too. A spec grants its trait with a
        // `flag` modifier and then tunes it through these, so every new trait
        // adds its parameters to this list or the validator rejects the tree.
        for (const char* k : {"damage", "fireRate", "range", "projectileSpeed", "projectileCount",
                              "pierce", "critChance", "critMult", "armorPen", "execute.threshold",
                              "execute.mult", "rampUp.perSec", "rampUp.maxMult",
                              "splash.radius", "splash.damagePct", "splash.falloff",
                              "chain.jumps", "chain.radius", "chain.damagePct", "chain.falloff",
                              "amplify.pct", "amplify.duration",
                              "drain.goldPerKill",
                              "slowOnHit.pct", "slowOnHit.duration",
                              "towerBuff.radius", "towerBuff.damagePct",
                              "towerBuff.fireRatePct"}) {
            out.insert(t.id + "." + k);
        }
    }
    for (const auto& [id, e] : reg.elements()) {
        for (const auto& [k, v] : e.base) out.insert(e.id + "." + k);
        for (const auto& [spec, params] : e.specs) {
            for (const auto& [k, v] : params) out.insert(e.id + "." + spec + "." + k);
        }
    }
    return out;
}

}  // namespace td::core
