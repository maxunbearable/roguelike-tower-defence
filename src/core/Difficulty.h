#pragma once

#include <string>

namespace td::core {

// How hard the player wants the run to be.
//
// This exists to settle a contradiction the project carried for several rounds
// and recorded in its own README: "hardcore, limited resources" and "map 1 in
// 8-10 losses" are the same dial pulled opposite ways. They are not reconcilable
// as one tuning. They are perfectly reconcilable as a choice.
enum class Difficulty { Relaxed, Standard, Brutal };

inline constexpr int kDifficultyCount = 3;

// Difficulty scales SEVERAL modest things rather than one large health number.
// Scaling health alone is the lazy version and it changes only how long a wave
// takes; changing the size of a wave changes what a wave IS, which is the part
// players actually feel.
//
// Nothing here reduces shard payout below the baseline. Reducing meta rewards on
// easier settings is the design players most often say makes them "doubt the
// game's balance", and it punishes exactly the people who needed the easier
// setting. Brutal instead pays a bonus, which is the Slay the Spire / Hades
// framing: harder is rewarded, easier is not taxed.
struct DifficultyMods {
    float enemyHp = 1.0f;
    float enemyCount = 1.0f;  // how many arrive per group
    float startGold = 1.0f;
    float lives = 1.0f;
    float shards = 1.0f;  // >= 1 always; harder pays more, easier pays normal
};

inline DifficultyMods modsFor(Difficulty d) {
    switch (d) {
        case Difficulty::Relaxed:
            // Aimed at the "map 1 in 8-10 losses" target: fewer and softer
            // enemies, a wider margin for error, and a real purse to open with.
            return {0.72f, 0.85f, 1.45f, 1.5f, 1.0f};
        case Difficulty::Brutal:
            // The tuning the game shipped with, which measured at 21 runs to
            // clear map 1. Kept intact rather than softened, and paid for.
            return {1.15f, 1.10f, 0.85f, 0.8f, 1.35f};
        case Difficulty::Standard:
            break;
    }
    return {};  // Standard is the authored balance, untouched
}

inline const char* difficultyName(Difficulty d) {
    switch (d) {
        case Difficulty::Relaxed: return "Relaxed";
        case Difficulty::Brutal: return "Brutal";
        case Difficulty::Standard: break;
    }
    return "Standard";
}

// Two short lines rather than one long one: at 10px a single sentence overran
// its button and collided with the neighbouring option.
inline const char* difficultyBlurb(Difficulty d) {
    switch (d) {
        case Difficulty::Relaxed: return "Softer, smaller waves. A wider margin.";
        case Difficulty::Brutal: return "Bigger, tougher waves. A thinner purse.";
        case Difficulty::Standard: break;
    }
    return "The authored balance, untouched.";
}

// The payout line, kept separate because it is the part a player weighs.
inline const char* difficultyReward(Difficulty d) {
    switch (d) {
        case Difficulty::Relaxed: return "Full shard payout";
        case Difficulty::Brutal: return "+35% shards";
        case Difficulty::Standard: break;
    }
    return "Full shard payout";
}

inline int difficultyToIndex(Difficulty d) { return static_cast<int>(d); }

inline Difficulty difficultyFromIndex(int i) {
    if (i < 0 || i >= kDifficultyCount) return Difficulty::Standard;
    return static_cast<Difficulty>(i);
}

}  // namespace td::core
