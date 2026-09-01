#pragma once

#include <vector>

#include "content/Defs.h"

namespace td::content {

// Expands a recipe into concrete waves. Fully deterministic: no RNG, so a map
// always plays identically. Enemy types rotate through the pool entries that
// have unlocked by that wave, and stats scale smoothly with wave number.
std::vector<WaveDef> generateWaves(const WaveRecipe& r);

}  // namespace td::content
