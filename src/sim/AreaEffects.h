#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "core/Vec2.h"

namespace td::sim {

class World;

// Shared area and typed-damage helpers.
//
// Splash, chains, poison spread, quake pulses and cyclone vortices all need the
// same two things: "which living enemies are near here" and "deal typed damage
// outside the projectile path". These lived privately inside Earth.cpp; every
// new element and every area trait would otherwise copy them.

float resistOf(World& w, entt::entity target, const std::string& damageType);

// Applies raw typed damage outside the projectile path. Armour is deliberately
// NOT applied: these are element and trait effects, and routing them through
// armour would make them useless against exactly the enemies they exist to
// counter.
void dealTyped(World& w, entt::entity target, float amount, const std::string& damageType);

// Damage that ignores resistance entirely. Light's sear is the answer to a map
// built to resist whatever element you brought, so it deliberately bypasses the
// multiplier every other source respects.
void dealFlat(World& w, entt::entity target, float amount);

// Living enemies within `radius` tiles of `centre`, nearest first, excluding any
// listed. Nearest-first ordering is what makes chain jumps deterministic.
std::vector<entt::entity> enemiesWithin(World& w, core::Vec2 centre, float radius,
                                        const std::vector<entt::entity>& exclude = {});

// Damage everything in a radius, scaling linearly from full at the centre down
// to `falloff` of it at the rim.
void areaDamage(World& w, core::Vec2 centre, float radius, float amount,
                const std::string& damageType, float falloff,
                const std::vector<entt::entity>& exclude = {});

}  // namespace td::sim
