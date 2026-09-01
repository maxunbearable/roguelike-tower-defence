#pragma once

#include <string>
#include <vector>

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
    bool cleared = false;
    int shards = 0;
    int peakGold = 0;
};

// Plays a whole run to its end and reports how far it got.
AutoPlayResult autoPlay(const content::Registry& reg, const content::MapDef& map,
                        const core::Loadout& meta, uint64_t seed, int maxWaves = 60);

}  // namespace td::sim
