#pragma once

#include <vector>

#include "content/Defs.h"

namespace td::content {

// Expands a recipe into concrete waves. Stats scale smoothly with wave number,
// and enemy types cycle through the pool entries that have unlocked by then.
//
// This overload is the CANONICAL expansion, used for validation and for what the
// map screen reports about a map. It takes no seed and is what a map "is".
std::vector<WaveDef> generateWaves(const WaveRecipe& r);

// The per-RUN expansion. Same waves, shuffled.
//
// Every run of a map used to face the identical fifty waves, in a game whose
// meta loop asks the player to replay maps many times over to bank shards. The
// order enemies arrive in is now drawn from the run's seed.
//
// What varies is COMPOSITION, never difficulty. Reordering alone is not enough
// to promise that: creatures differ nearly fourfold in health, so with only one
// to three types unlocked, whether goblins lead wave 9 or wave 11 swung the
// first twelve waves by 51% across seeds. Measured, that is not variety, it is a
// lottery on the part of the game every player replays most.
//
// So a wave that draws a heavier creature fields FEWER of them, and a lighter
// one more, such that the health it brings and the gold it pays out both match
// what the canonical expansion would have delivered. A run can meet five
// armoured brutes where another met twenty slimes -- which asks for a different
// board -- while the difficulty curve stays exactly where it was calibrated.
//
// Needs the enemy roster, hence the registry: the compensation is computed from
// creature health and bounty.
class Registry;
std::vector<WaveDef> generateWaves(const WaveRecipe& r, uint64_t seed, const Registry& reg);

}  // namespace td::content
