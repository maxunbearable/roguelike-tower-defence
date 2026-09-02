#pragma once

#include "core/SaveGame.h"

namespace td::core {

// Where opening a profile should land. A profile that has never played and
// cannot afford the cheapest node has nothing to do in the skill trees.
enum class Landing { Maps, Hub };

// `cheapestNode` is the lowest price anywhere in the trees.
Landing landingFor(const MetaSave& meta, int cheapestNode);

}  // namespace td::core
