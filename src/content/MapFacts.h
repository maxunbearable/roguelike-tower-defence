#pragma once

#include <string>

namespace td::content {

class Registry;
struct MapDef;

// What a map's roster is made of, derived from its enemies rather than authored
// a second time.
//
// This exists because the game's whole replayability claim is that "each map
// resists a different element, so the build that cleared the last map is the
// wrong build for the next one" -- and the map select screen never said which.
// Deriving it means the screen cannot drift out of step with the content.
struct MapBias {
    std::string resistant;    // damage type the roster shrugs off most
    std::string vulnerable;   // damage type it suffers most from
    float resistantMult = 1.0f;
    float vulnerableMult = 1.0f;
    bool valid = false;       // false when the roster is uniformly neutral
};

// Averages every resistance across the map's whole enemy pool, bosses included,
// weighting each enemy equally. Absent entries count as neutral (1.0), which is
// what the damage system does.
MapBias mapBias(const Registry& reg, const MapDef& map);

}  // namespace td::content
