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
    bool ownAll = true;  // Plan 3 replaces this with the meta save

    bool owns(const std::string& nodeId) const {
        return ownAll || ownedNodes.count(nodeId) > 0;
    }
};

}  // namespace td::core
