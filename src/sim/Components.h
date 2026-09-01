#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "core/Vec2.h"

namespace td::sim {

// Position convention, used identically by enemies, towers and projectiles:
// coordinates are in TILE UNITS, where integers are tile corners. Tile (x,y) has
// its centre at (x+0.5, y+0.5). Rendering multiplies by the pixel tile size.

struct Position {
    core::Vec2 v;
};
struct PrevPosition {
    core::Vec2 v;  // previous tick's position, for render interpolation
};
struct PathFollower {
    float distance = 0.0f;  // arc length travelled along the route
};
struct Health {
    float hp = 1.0f;
    float maxHp = 1.0f;
};
struct Armor {
    float value = 0.0f;
};
struct Speed {
    float base = 1.0f;  // tiles per second
};
struct EnemyTag {
    std::string defId;
    int bounty = 0;
    int shardValue = 0;
};

// --- towers --------------------------------------------------------------

enum class TargetPriority { First, Last, Strongest, Weakest, Closest };

// A tower's build is assembled DURING a run with gold, not chosen at a menu:
// build the tower, imbue it with an element, specialise the tower, specialise
// the element. Empty strings mean "not taken yet".
struct TowerTag {
    std::string defId;
    int level = 1;
    int goldSpent = 0;  // total invested, so selling refunds proportionally
    std::string elementId;
    std::string towerSpec;
    std::string elementSpec;
    // The PLAYER's targeting choice, which is why it lives here and not on
    // TowerStats: statsFor() re-reads targetPriority from the tower definition,
    // and rebuildTower() runs on every upgrade, imbue and specialisation. Held
    // on the stats it would be silently reset the next time the tower changed.
    TargetPriority priority = TargetPriority::First;
};

class ElementBehavior;
struct ElementRef {
    ElementBehavior* behavior = nullptr;  // owned by World, shared between towers
};
struct LastHitBy {
    entt::entity tower = entt::null;  // so on-kill effects use the killer's element
};
struct TileCoord {
    int x = 0;
    int y = 0;
};
struct TowerStats {
    std::string damageType = "physical";
    float damage = 0.0f;
    float fireRate = 1.0f;
    float range = 1.0f;
    float projectileSpeed = 1.0f;
    int projectileCount = 1;
    int pierce = 0;
    float critChance = 0.0f;
    float critMult = 1.0f;
    float armorPen = 0.0f;
    TargetPriority priority = TargetPriority::First;
};
struct Cooldown {
    float remaining = 0.0f;
};
struct TargetRef {
    entt::entity e = entt::null;
};

// --- projectiles ---------------------------------------------------------

struct Projectile {
    std::string damageType = "physical";
    core::Vec2 dir;
    float speed = 1.0f;
    float damage = 0.0f;
    float armorPen = 0.0f;
    float critMult = 1.0f;
    bool crit = false;
    int pierceLeft = 0;
    entt::entity source = entt::null;
    std::vector<entt::entity> alreadyHit;  // pierce must not re-hit the same enemy
};
struct Lifetime {
    float remaining = 0.0f;
};

// --- tower spec traits ----------------------------------------------------
// Attached at spawn from `flag` modifiers in the resolved stat block, so which
// traits a spec grants is decided in TOML, not in C++.

// --- tower traits ---------------------------------------------------------
// Granted by flag from a spec's modifiers, so a new archetype is authored in
// TOML and mapped here once, never per pairing.

struct Splash {  // Cannon: the shell damages around what it hits
    float radius = 0.0f;
    float damagePct = 0.0f;  // fraction of the landed hit
    float falloff = 0.5f;    // multiplier at the rim
};
struct Chain {  // Arcane tempest: the bolt jumps onward
    int jumps = 0;
    float radius = 0.0f;
    float damagePct = 0.0f;
    float falloff = 0.7f;  // each jump carries this fraction of the last
};
struct Amplify {  // Arcane hex: marks a target so everything hurts it more
    float pct = 0.0f;
    float duration = 0.0f;
};
struct Drain {  // Arcane drain: kills pay gold
    int goldPerKill = 0;
};
struct SlowOnHit {  // Ballista javelin: the bolt's impact knocks the target down
    float pct = 0.0f;
    float duration = 0.0f;
};
struct TowerBuff {  // Brazier forge: raises the output of neighbouring towers
    float radius = 0.0f;
    float damagePct = 0.0f;
    float fireRatePct = 0.0f;
};
// Recomputed every tick from whichever TowerBuff auras cover this tower. Held as
// a component rather than folded into TowerStats because TowerStats is a pure
// function of what has been BOUGHT, and this changes as neighbours are built and
// sold.
struct Buffed {
    float damageMult = 1.0f;
    float fireRateMult = 1.0f;
};

struct Execute {  // Sniper
    float threshold = 0.6f;  // health fraction at or above which the bonus applies
    float mult = 1.0f;
};
struct RampUp {  // Elf
    float perSec = 0.0f;
    float maxMult = 1.0f;
    float current = 1.0f;
    entt::entity lastTarget = entt::null;
};
struct ShotCounter {  // used by Earthquake to fire every Nth shot
    int n = 0;
};
struct CyclonePayload {};

struct QuakePayload {};  // marks a projectile that will erupt on impact

// --- status effects -------------------------------------------------------
// Statuses refresh duration and take the maximum magnitude; they do not stack
// in count. Poison is the sole exception, stacking up to maxStacks.

struct Poisoned {
    float stacks = 0.0f;
    float maxStacks = 1.0f;
    float dpsPerStack = 0.0f;
    float remaining = 0.0f;
    std::string damageType = "physical";
};
struct ArmorShred {
    float amount = 0.0f;
    float remaining = 0.0f;
};
struct Slowed {
    float pct = 0.0f;
    float remaining = 0.0f;
};
// Applied to an ENEMY by a hexing tower: all incoming damage is multiplied.
struct Amplified {
    float pct = 0.0f;
    float remaining = 0.0f;
};
// Applied to an ENEMY by Fire's melt: resistance is peeled away, so later hits
// of any type land harder.
struct Melted {
    float amount = 0.0f;  // resistance added back toward neutral
    float remaining = 0.0f;
};
struct Frozen {  // Water's freeze: a hard stop, capped for bosses in movement
    float remaining = 0.0f;
};
// Fire's burn. Unlike poison, which counts STACKS, burn accumulates a damage
// rate taken from the magnitude of each hit -- so one heavy shot ignites harder
// than a dozen light ones.
// Shadow's wither. The only effect in the game with no timer: it accumulates
// across a whole wave and is only lost when the enemy dies, which is what makes
// Shadow an attrition element rather than a damage one.
struct Withered {
    float pct = 0.0f;
};
// Light's beacon. While marked, hits on this enemy from ANY tower share a
// fraction of their damage with the enemies around it -- so one beacon tower
// turns the whole board's single-target fire into crowd fire.
struct Beaconed {
    float radius = 0.0f;
    float sharePct = 0.0f;
    float remaining = 0.0f;
};
// A rift left behind by a Shadow kill. Lives on its own entity, not on an enemy,
// because the enemy that created it is gone.
struct Rift {
    core::Vec2 pos;
    float radius = 0.0f;
    float dps = 0.0f;
    float remaining = 0.0f;
    std::string damageType = "void";
};

struct Burning {
    float dps = 0.0f;
    float maxDps = 0.0f;
    float remaining = 0.0f;
    std::string damageType = "fire";
};
// Marks a boss. Emplaced at spawn from EnemyDef::boss so the hot movement loop
// tests a component rather than looking up a definition every frame.
struct Boss {};

struct Petrified {
    float remaining = 0.0f;
};

// Brief white flash after taking a hit. Lives in the sim so the renderer needs
// no per-entity bookkeeping of its own.
struct HitFlash {
    float remaining = 0.0f;
};

}  // namespace td::sim
