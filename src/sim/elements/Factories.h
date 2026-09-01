#pragma once

#include <memory>
#include <string>

#include "core/StatBlock.h"
#include "sim/ElementBehavior.h"

namespace td::sim {

// One factory per element. `makeElement` in Elements.cpp is the only place that
// knows the full set, so adding an element is a new file plus one line there --
// never a change to the combination engine or to any other element.
using ElementFactory = std::unique_ptr<ElementBehavior> (*)(const std::string& spec,
                                                            const core::StatBlock& stats,
                                                            const std::string& damageType);

std::unique_ptr<ElementBehavior> makeEarth(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType);
std::unique_ptr<ElementBehavior> makeFire(const std::string& spec, const core::StatBlock& stats,
                                          const std::string& damageType);
std::unique_ptr<ElementBehavior> makeWater(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType);
std::unique_ptr<ElementBehavior> makeShadow(const std::string& spec, const core::StatBlock& stats,
                                            const std::string& damageType);
std::unique_ptr<ElementBehavior> makeLight(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType);
std::unique_ptr<ElementBehavior> makeWind(const std::string& spec, const core::StatBlock& stats,
                                          const std::string& damageType);

}  // namespace td::sim
