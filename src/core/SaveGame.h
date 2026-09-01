#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace td::core {

inline constexpr int kSaveVersion = 1;
inline constexpr int kSlotCount = 3;

// One placed tower, exactly as much as is needed to rebuild it.
struct TowerSave {
    int x = 0, y = 0;
    // Which tower this IS. Its absence was a data-loss bug: restore() hardcoded
    // "arrow", so every saved cannon, arcane spire, ballista and brazier came
    // back as an arrow tower. Harmless while arrow was the only tower; not once
    // the others became unlockable purchases. Defaults to "arrow" so saves
    // written before this field still load.
    std::string towerId = "arrow";
    int level = 1;
    int goldSpent = 0;
    std::string elementId;
    std::string towerSpec;
    std::string elementSpec;
    // The player's targeting choice, by name. See sim::TargetPriority.
    std::string priority = "first";
};

// A run paused between waves. Runs are only ever saved during a build phase,
// when the field is empty -- which removes any need to serialise enemies,
// projectiles or in-flight status effects.
struct RunSave {
    std::string mapId;
    uint64_t seed = 0;
    std::string rngState;
    int waveIndex = 0;
    int gold = 0;
    int lives = 0;
    float buildTimer = 0.0f;
    std::vector<TowerSave> towers;
};

// How far a profile has got on one map.
struct MapProgress {
    int bestWave = 0;
    bool cleared = false;
};

// Permanent progress, which outlives any individual run.
struct MetaSave {
    int shards = 0;
    int runsPlayed = 0;
    int bestWave = 0;  // best across all maps; kept for the slot summary
    std::set<std::string> ownedNodes;
    // Per map, keyed by map id. Absent means never played, which is why this is
    // a map and not a vector: adding or reordering maps must not shift anyone's
    // recorded progress.
    std::map<std::string, MapProgress> mapProgress;
    // Audio preferences. Stored per profile because that is the only persistent
    // thing the game has; there is no separate settings file.
    float musicVolume = 0.5f;
    float sfxVolume = 0.85f;
    // One-shot tutorial hints already shown. Per profile rather than global so a
    // fresh profile is a fresh player, and stored by ID so reordering or
    // rewording a hint never re-fires one someone has already read.
    std::set<std::string> seenHints;
    // How far through the guided first run this profile is, as a small integer
    // (sim::TutorialStep). Stored rather than a bare "done" flag so a player who
    // quits halfway resumes where they were instead of starting over.
    int tutorialStep = 0;
};

// A map is playable when it is the first, or when the one before it in `order`
// has been CLEARED. Reaching a high wave is not enough -- otherwise the last map
// would open to a player who never finished the first.
bool mapUnlocked(const MetaSave& meta, const std::vector<std::string>& order, int index);

struct SaveSlot {
    int version = kSaveVersion;
    bool used = false;
    std::string profileName;
    MetaSave meta;
    std::optional<RunSave> run;  // absent when no run is in progress

    bool hasRunInProgress() const { return run.has_value(); }
};

// Serialisation is separated from file IO so it can be round-tripped in tests
// without touching the disk.
std::string toJson(const SaveSlot& slot);
SaveSlot fromJson(const std::string& json);  // throws std::runtime_error on bad data

}  // namespace td::core
