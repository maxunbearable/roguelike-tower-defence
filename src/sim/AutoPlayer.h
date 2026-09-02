#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include "core/Difficulty.h"
#include "sim/World.h"

namespace td::sim {

// A reasonable-but-not-expert player, used to measure the difficulty curve
// headlessly. It exists because "is this game playable from a cold start" is a
// question that should be answered by measurement, not by guessing -- and a
// fresh profile owns no skill nodes at all, which is nothing like the fully
// upgraded worlds the combination matrix is tuned against.
struct AutoPlayResult {
    int wavesSurvived = 0;
    int towersBuilt = 0;
    // How many DIFFERENT tower types ended up on the board. Reported because a
    // board of 34 identical towers and a mixed one are different measurements,
    // and the harness used to only ever produce the first.
    int distinctTowerTypes = 0;
    bool cleared = false;
    int shards = 0;
    int peakGold = 0;
};

// Plays a whole run to its end and reports how far it got.
// Towers this profile could field on this map, best first. Exposed so the
// harness's own choices can be tested rather than inferred from its output.
std::vector<std::string> towerPreferenceFor(const content::Registry& reg,
                                            const content::MapDef& m, const World& w);

AutoPlayResult autoPlay(const content::Registry& reg, const content::MapDef& map,
                        const core::Loadout& meta, uint64_t seed, int maxWaves = 60,
                        core::Difficulty difficulty = core::Difficulty::Standard);

}  // namespace td::sim
