// The element dispatch table. This is the ONLY file that knows every element,
// which is what keeps adding one to a single registration line.

#include <string>

#include "sim/elements/Factories.h"

namespace td::sim {

std::unique_ptr<ElementBehavior> makeElement(const std::string& elementId, const std::string& spec,
                                            const core::StatBlock& stats,
                                            const std::string& damageType) {
    if (elementId == "earth") return makeEarth(spec, stats, damageType);
    if (elementId == "fire") return makeFire(spec, stats, damageType);
    if (elementId == "water") return makeWater(spec, stats, damageType);
    if (elementId == "wind") return makeWind(spec, stats, damageType);
    if (elementId == "shadow") return makeShadow(spec, stats, damageType);
    if (elementId == "light") return makeLight(spec, stats, damageType);
    return std::make_unique<ElementBehavior>();  // neutral: no hooks fire
}

}  // namespace td::sim
