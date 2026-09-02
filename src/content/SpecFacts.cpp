#include "content/SpecFacts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace td::content {
namespace {

// Trailing component of a modifier target: "arrow.execute.mult" -> "execute.mult".
std::string suffixOf(const std::string& target) {
    const auto dot = target.find('.');
    return dot == std::string::npos ? target : target.substr(dot + 1);
}

// Human labels. Anything not listed is skipped rather than shown raw: a player
// reading "trait.execute" learns nothing, and a wrong label is worse than none.
const std::map<std::string, const char*>& labels() {
    static const std::map<std::string, const char*> m = {
        {"damage", "damage"},
        {"fireRate", "fire rate"},
        {"range", "range"},
        {"projectileCount", "shots"},
        {"projectileSpeed", "shot speed"},
        {"pierce", "pierce"},
        {"critChance", "crit"},
        {"critMult", "crit damage"},
        {"armorPen", "armour pen"},
        {"execute.threshold", "execute below"},
        {"execute.mult", "execute damage"},
        {"rampUp.perSec", "ramp per second"},
        {"rampUp.maxMult", "ramp cap"},
        {"splash.radius", "splash radius"},
        {"splash.damagePct", "splash damage"},
        {"splash.falloff", "splash falloff"},
        {"chain.jumps", "chain jumps"},
        {"chain.radius", "chain radius"},
        {"chain.damagePct", "chain damage"},
        {"amplify.pct", "damage taken"},
        {"amplify.duration", "mark lasts"},
        {"drain.goldPerKill", "gold per kill"},
        {"slowOnHit.pct", "slow"},
        {"slowOnHit.duration", "slow lasts"},
        {"towerBuff.radius", "aura radius"},
        {"towerBuff.damagePct", "aura damage"},
        {"towerBuff.fireRatePct", "aura fire rate"},
        {"chain.falloff", "chain falloff"},
        // Element spec parameters. Each element names its own, so these are
        // listed rather than pattern-matched: a label that guesses is a label
        // that will one day be wrong.
        {"poison.dpsPerStack", "venom per stack"},
        {"poison.maxStacks", "max stacks"},
        {"rock.shredPerHit", "armour shred per hit"},
        {"rock.flatBonus", "bonus vs armour"},
        {"quake.damage", "quake damage"},
        {"quake.radius", "quake radius"},
        {"burn.pctPerHit", "burn per hit"},
        {"blast.damagePct", "blast damage"},
        {"melt.resistPerHit", "resistance stripped per hit"},
        {"chill.slowPerHit", "slow per hit"},
        {"shatter.bonusPerSlowPct", "bonus per slow"},
        {"freeze.chance", "freeze chance"},
        {"shock.damagePct", "shock damage"},
        {"gust.pushTiles", "push"},
        {"cyclone.damage", "cyclone damage"},
        {"wither.pctPerHit", "wither per hit"},
        {"siphon.lifeChance", "life steal chance"},
        {"rift.dps", "rift damage per second"},
        {"sear.flatPerHit", "sear damage per hit"},
        {"judgement.damage", "judgement damage"},
        {"beacon.sharePct", "shared to allies"},
    };
    return m;
}

// Stats stored as a fraction of 1 read far better as a percentage: a crit chance
// of 0.20 is "+20%", not "+0.2".
bool isFraction(const std::string& suffix) {
    static const char* kFractions[] = {
        "critChance",        "execute.threshold",   "splash.damagePct",
        "splash.falloff",    "chain.damagePct",     "chain.falloff",
        "amplify.pct",       "slowOnHit.pct",       "towerBuff.damagePct",
        "towerBuff.fireRatePct", "burn.pctPerHit",  "blast.damagePct",
        "melt.resistPerHit", "chill.slowPerHit",    "shatter.bonusPerSlowPct",
        "freeze.chance",     "shock.damagePct",     "wither.pctPerHit",
        "siphon.lifeChance", "beacon.sharePct"};
    for (const char* f : kFractions) {
        if (suffix == f) return true;
    }
    return false;
}

// Formats the number the player will actually SEE. The integrality test has to
// run on the displayed value, not the stored one: a 0.30 fraction shown as a
// percentage is 30, which is whole, and testing the raw 0.30 printed "30.0%".
std::string num(float shown) {
    const bool whole = std::abs(shown - std::round(shown)) < 0.005f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), whole ? "%.0f" : "%.2f", static_cast<double>(shown));
    std::string s = buf;
    // 1.40 -> 1.4
    if (s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    return s;
}

// Durations read as bare numbers otherwise: "mark lasts 3.5" against "3.5s".
bool isSeconds(const std::string& suffix) {
    return suffix == "amplify.duration" || suffix == "slowOnHit.duration";
}

}  // namespace

std::vector<std::string> specNumbers(const core::SkillTree& tree, const std::string& nodeId) {
    std::vector<std::string> out;
    const auto* node = tree.find(nodeId);
    if (!node) return out;

    for (const auto& m : node->modifiers) {
        // A flag turns a trait on; the prose already says what the trait is.
        if (m.op == core::ModOp::Flag) continue;
        const auto suffix = suffixOf(m.target);
        const auto it = labels().find(suffix);
        if (it == labels().end()) continue;

        const bool pct = isFraction(suffix);
        const float shown = pct ? m.value * 100.0f : m.value;
        const char* unit = pct ? "%" : (isSeconds(suffix) ? "s" : "");
        std::string line = it->second;
        switch (m.op) {
            case core::ModOp::Mult:
                // A multiplier on a fraction is still a multiplier, not a
                // percentage point, so it never takes the x100.
                line += " x" + num(m.value);
                break;
            case core::ModOp::Add:
                line += (m.value < 0.0f ? " " : " +") + num(shown) + unit;
                break;
            case core::ModOp::Set:
                line += " " + num(shown) + unit;
                break;
            case core::ModOp::Flag:
                break;
        }
        out.push_back(std::move(line));
    }
    return out;
}

std::string specNumbersLine(const core::SkillTree& tree, const std::string& nodeId) {
    const auto parts = specNumbers(tree, nodeId);
    std::string out;
    for (const auto& p : parts) {
        if (!out.empty()) out += ", ";
        out += p;
    }
    return out;
}

}  // namespace td::content
