#include "core/Onboarding.h"

namespace td::core {

Landing landingFor(const MetaSave& meta, int cheapestNode) {
    // Somebody who has played before knows where they are and may well be
    // returning to spend; send them to the trees even if they are broke.
    if (meta.runsPlayed > 0) return Landing::Hub;
    if (cheapestNode > 0 && meta.shards < cheapestNode) return Landing::Maps;
    return Landing::Hub;
}

}  // namespace td::core
