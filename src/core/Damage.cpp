#include "core/Damage.h"

#include <algorithm>

namespace td::core {

float computeDamage(const DamageInput& in) {
    const float raw = (in.crit ? in.raw * in.critMult : in.raw) * in.resistMult;
    const float effectiveArmor = std::max(0.0f, in.targetArmor - in.armorShred - in.armorPen);
    return std::max(raw - effectiveArmor, raw * kMinDamageFloor);
}

}  // namespace td::core
