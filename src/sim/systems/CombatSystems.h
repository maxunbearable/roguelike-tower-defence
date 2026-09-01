#pragma once

namespace td::sim {

class World;

// Selects and holds a target per tower, dropping ones that died or left range.
void runTargetingSystem(World& w, float dt);

// Ticks cooldowns and emits projectiles.
// Recomputes each tower's Buffed multipliers from whichever TowerBuff auras
// cover it. Must run before firing, so a shot uses this tick's buff.
void runTowerBuffSystem(World& w, float dt);
void runFiringSystem(World& w, float dt);

// Moves projectiles, resolves hits through core::computeDamage, applies pierce.
void runProjectileSystem(World& w, float dt);

// Reaps enemies at or below zero health and pays out their bounty.
void runDeathSystem(World& w, float dt);

// Gives the live element behaviour a per-tower tick, for auras and timers.
void runTowerElementTick(World& w, float dt);

}  // namespace td::sim
