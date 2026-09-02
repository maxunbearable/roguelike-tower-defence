#pragma once

#include "core/SaveGame.h"

namespace td::core {

// Where opening a profile should land the player.
//
// It used to be the skill trees, always. For a profile that has never played
// that is a wall: every node is locked, every price is red, and the only way
// forward is a button at the bottom of the screen. The slot card promises "click
// to begin a new game" and delivered a shop the player cannot shop in, because
// shards are earned by playing and a new profile has none.
enum class Landing { Maps, Hub };

// `cheapestNode` is the lowest price anywhere in the trees, so this asks the
// real question -- can this profile buy anything at all? -- rather than checking
// a flag that happens to correlate.
Landing landingFor(const MetaSave& meta, int cheapestNode);

}  // namespace td::core
