#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "content/Registry.h"

namespace td::sim {

enum class ScenarioKind {
    LoneTank,  // one high-armour target: rewards heavy single hits and penetration
    Swarm,     // many weak targets in a line: rewards rate, spread and pierce
    Mixed,     // a realistic blend, including an earth-resistant and an earth-weak enemy
};

struct ComboResult {
    float totalHp = 0.0f;      // health spawned
    float damageDealt = 0.0f;  // health removed
    int spawned = 0;
    int kills = 0;

    float clearedFraction() const { return totalHp > 0.0f ? damageDealt / totalHp : 0.0f; }
};

// Runs one (tower, tower spec) x (element, element spec) pairing against a fixed
// scenario with a fixed seed, and reports how much health it removed. Enemies are
// stationary by design: the matrix measures damage throughput, so control effects
// (quake's slow, freeze, gust) are undervalued here and must be judged in play
// as well.
//
// The tower and element IDS are parameters, not baked in: with three towers and
// four elements there are 108 pairings, and the guarantee this harness exists to
// protect is that all of them work with no per-pair code.
ComboResult simulateCombo(const content::Registry& reg, const std::string& towerId,
                          const std::string& towerSpec, const std::string& elementId,
                          const std::string& elementSpec, ScenarioKind kind, float seconds,
                          uint64_t seed);

const char* name(ScenarioKind k);

}  // namespace td::sim
