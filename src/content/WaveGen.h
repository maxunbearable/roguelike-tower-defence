#pragma once

#include <vector>

#include "content/Defs.h"

namespace td::content {

// The canonical expansion of a recipe: no seed, so it is what a map "is".
// Used for validation and for what the map screen reports.
std::vector<WaveDef> generateWaves(const WaveRecipe& r);

// The per-run expansion: same waves, order drawn from the seed.
//
// Composition varies, difficulty does not. A wave that draws a heavier creature
// fields fewer of them, with the remainder taken up in the health multiplier and
// bounty matched the same way, so each wave's health and payout are what the
// canonical expansion would have delivered. Needs the roster for those figures.
class Registry;
std::vector<WaveDef> generateWaves(const WaveRecipe& r, uint64_t seed, const Registry& reg);

}  // namespace td::content
