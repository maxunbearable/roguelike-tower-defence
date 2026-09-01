#pragma once

namespace td::sim {

class World;

// Ticks every status effect, applies damage-over-time, and expires finished
// ones. Runs before movement so a slow or petrify takes effect the same tick it
// is applied.
void runStatusSystem(World& w, float dt);

}  // namespace td::sim
