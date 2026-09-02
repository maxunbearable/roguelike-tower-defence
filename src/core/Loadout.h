#pragma once

#include <set>
#include <string>

namespace td::core {

// Describes ONE tower's build plus which skill nodes the player permanently
// owns. Owned nodes in a branch this tower has not taken contribute nothing --
// that is how "you own everything, but only one spec plays" works without any
// special-casing.
struct Loadout {
    std::string towerId = "arrow";
    // Empty means "not bought yet". A tower starts unspecialised with no
    // element and is built up during the run, so these fill in over time.
    std::string towerSpec;
    std::string elementId;
    std::string elementSpec;

    std::set<std::string> ownedNodes;
    // A loadout owns NOTHING unless told otherwise. This defaulted to true as
    // plan 3 scaffolding -- "own everything until the meta save exists" -- and
    // the meta save arrived twenty-odd plans ago while the default stayed. It
    // meant any `Loadout{}` silently granted the entire skill tree, which is the
    // most expensive thing in the game to hand out by accident.
    bool ownAll = false;

    bool owns(const std::string& nodeId) const {
        return ownAll || ownedNodes.count(nodeId) > 0;
    }
};

}  // namespace td::core
