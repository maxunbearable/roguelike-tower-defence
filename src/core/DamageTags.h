#pragma once

#include <string>

namespace td::core {

// A two-letter tag per damage type, shown alongside the hue when colour
// alternatives are switched on.
//
// The accessibility guideline is blunt: essential information must never be
// conveyed by a fixed colour alone. Eleven damage types were separated purely by
// hue in the one readout that tells a player what to build -- and two pairs are
// barely separable with full colour vision, let alone without it: arcane
// (186,148,232) against void (158,120,200), and radiant against shock.
//
// Tags rather than shapes because eleven distinguishable shapes at 7px do not
// exist, and text is what the guidance actually recommends.
inline const char* damageTypeTag(const std::string& type) {
    if (type == "piercing") return "PI";
    if (type == "siege") return "SG";
    if (type == "arcane") return "AR";
    if (type == "impact") return "IM";
    if (type == "searing") return "SR";
    if (type == "earth") return "EA";
    if (type == "fire") return "FI";
    if (type == "frost") return "FR";
    if (type == "shock") return "SH";
    if (type == "void") return "VD";
    if (type == "radiant") return "RD";
    return "??";
}

}  // namespace td::core
