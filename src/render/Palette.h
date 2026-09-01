#pragma once

#include <string>

#include "core/DamageTags.h"

#include "raylib.h"

// Every colour the game draws lives here, so the Phase 6 art pass has exactly
// one file to replace when sprites arrive.
namespace td::render::palette {

inline constexpr Color kGrassA{106, 163, 42, 255};
inline constexpr Color kGrassB{96, 150, 38, 255};
inline constexpr Color kPath{185, 139, 94, 255};
inline constexpr Color kPathEdge{143, 103, 63, 255};
inline constexpr Color kBlocked{58, 58, 70, 255};
inline constexpr Color kSpawn{178, 84, 58, 255};
inline constexpr Color kExit{74, 122, 178, 255};

inline constexpr Color kTowerBase{92, 96, 116, 255};
inline constexpr Color kTowerTop{200, 208, 224, 255};
inline constexpr Color kGhostOk{255, 255, 255, 110};
inline constexpr Color kGhostBad{220, 70, 70, 110};
inline constexpr Color kRangeRing{255, 255, 255, 40};

inline constexpr Color kProjectile{240, 227, 160, 255};
inline constexpr Color kHpBack{30, 26, 38, 220};
inline constexpr Color kHpFill{138, 201, 58, 255};

inline constexpr Color kHudBg{27, 26, 38, 255};
inline constexpr Color kHudText{232, 228, 217, 255};
inline constexpr Color kHudDim{140, 136, 128, 255};
inline constexpr Color kHudWarn{224, 132, 72, 255};

inline constexpr Color kBackdrop{16, 14, 24, 255};

// One colour per damage type, used EVERYWHERE a type appears -- resistance pips,
// the enemy dossier, floating damage numbers -- so the association is learnable
// rather than decorative. Tower types are cool and metallic, element types take
// their element's obvious hue.
using core::damageTypeTag;  // re-exported: callers say palette::damageTypeTag

inline Color damageTypeColor(const std::string& type) {
    if (type == "piercing") return Color{206, 214, 226, 255};  // steel
    if (type == "siege") return Color{168, 150, 128, 255};     // stone
    if (type == "arcane") return Color{186, 148, 232, 255};    // violet
    if (type == "impact") return Color{226, 196, 140, 255};    // brass
    if (type == "searing") return Color{250, 176, 96, 255};    // hot iron
    if (type == "earth") return Color{146, 196, 104, 255};
    if (type == "fire") return Color{242, 116, 66, 255};
    if (type == "frost") return Color{130, 198, 244, 255};
    if (type == "shock") return Color{244, 224, 108, 255};
    if (type == "void") return Color{158, 120, 200, 255};
    if (type == "radiant") return Color{252, 240, 176, 255};
    return Color{200, 200, 200, 255};
}


// Resistant reads cold and dim, vulnerable reads hot and bright, independent of
// the type hue -- so direction is legible even to a colour-blind player.
inline constexpr Color kResistant{104, 132, 168, 255};
inline constexpr Color kVulnerable{236, 118, 84, 255};

}  // namespace td::render::palette
