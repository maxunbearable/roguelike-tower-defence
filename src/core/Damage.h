#pragma once

namespace td::core {

// A hit always lands for at least this fraction of its post-crit raw damage,
// however armoured the target is. Without a floor, flat armour subtraction makes
// high-rate low-damage towers do literally nothing to armoured enemies.
inline constexpr float kMinDamageFloor = 0.10f;

struct DamageInput {
    float raw = 0.0f;
    bool crit = false;
    float critMult = 1.0f;
    float targetArmor = 0.0f;
    float armorShred = 0.0f;  // active shred on the target
    float armorPen = 0.0f;    // flat penetration from the attacker

    // Damage-type resistance of the target. 1.0 neutral, <1 resistant, >1
    // vulnerable. Applied with crit, before armour, so a resistant enemy is
    // still helped by its armour rather than double-dipping.
    float resistMult = 1.0f;
};

float computeDamage(const DamageInput& in);

}  // namespace td::core
