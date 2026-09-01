#pragma once

namespace td::sim {

class World;

// Spawns the active wave's enemies on their authored cadence and closes the wave
// out when every group is exhausted and the field is clear.
void runWaveSystem(World& w, float dt);

// Advances path followers and writes both Position and PrevPosition.
void runMovementSystem(World& w, float dt);

// Enemies that reach the end of the route cost a life and despawn.
void runLeakSystem(World& w, float dt);

}  // namespace td::sim
