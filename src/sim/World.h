#pragma once

#include <cstdint>
#include <map>
#include <algorithm>
#include <memory>
#include <string>

#include "core/Difficulty.h"
#include <vector>

#include <entt/entt.hpp>

#include "content/Defs.h"
#include "content/Registry.h"
#include "core/Loadout.h"
#include "core/Path.h"
#include "core/Resolve.h"
#include "core/Rng.h"
#include "core/SaveGame.h"
#include "sim/Components.h"
#include "sim/VisualEvent.h"

namespace td::sim {

inline constexpr float kFixedDt = 1.0f / 60.0f;
inline constexpr int kStartingLives = 20;
// Shard payout is the rate the WHOLE meta progression turns at, and it is
// deliberately mean: shards are the one resource that survives a run, so if they
// accumulate freely nothing else being scarce matters. Raising this to 7 was
// tried and reverted -- the stall it was meant to fix came from a broken level
// gate, not from income. Halved again for the hardcore pass.
// Read alongside nodeCost in tools/balance.py: node PRICES are down 38% while
// income is down ~50%, so each shard buys more but there are fewer of them.
inline constexpr int kShardsPerWave = 1;
// The one tower every profile can build from its first run.
inline constexpr const char* kStartingTower = "arrow";
inline constexpr int kShardsPerMapClear = 20;
// Gold per unresolved enemy when the next wave is called on top of a running
// one. This is a SKILL income source, and the only one the player controls, so
// it matters under the gold deficit: a board that is comfortably ahead can turn
// that slack into money, and misjudging it costs lives rather than gold.
inline constexpr int kGoldPerPendingEnemy = 3;

// The two player abilities. Every game in this genre has a pair of these -- in
// Kingdom Rush they are Rain of Fire and Reinforcements -- and they are what
// makes a wave something the player PLAYS rather than watches. This game had
// none: once the towers were placed there was nothing to do until the next build
// phase.
//
// Both are free and on a cooldown rather than costing gold. That is deliberate
// under the gold deficit: they are the one form of agency scarcity cannot take
// away, so a losing board is never simply a spectator.
enum class Ability { Strike, Ward };
inline constexpr int kAbilityCount = 2;

// Strike: an immediate blast at a point. Damage scales with the WAVE's health
// multiplier, because a flat number is a panic button on wave 3 and confetti on
// wave 50 -- enemy health rises about 55x across a map.
inline constexpr float kStrikeCooldown = 24.0f;
inline constexpr float kStrikeRadius = 2.3f;
inline constexpr float kStrikeBaseDamage = 42.0f;

// Ward: a field that holds enemies inside it. No damage at all, so it is a
// tempo tool rather than a second damage source -- it buys the towers time.
inline constexpr float kWardCooldown = 30.0f;
inline constexpr float kWardRadius = 2.6f;
inline constexpr float kWardSlowPct = 0.55f;
inline constexpr float kWardDuration = 6.0f;

// Sandbox runs every combat system but no wave logic, so scenarios can be set
// up by hand and measured. It is how the combination matrix is tested.
enum class Phase { Build, Wave, Cleared, Defeated, Sandbox };

class ElementBehavior;

class World {
public:
    // goldOverride < 0 uses the map's authored startGold. It exists so balance
    // tests can assert properties of the design without being blocked by economy
    // tuning; production code never passes it.
    // `meta` carries which skill-tree nodes the player permanently owns. Which
    // SPEC is live is decided per-tower during the run, not here.
    World(const content::Registry& reg, const content::MapDef& map, uint64_t seed,
          const core::Loadout& meta = core::Loadout{}, int goldOverride = -1,
          core::Difficulty difficulty = core::Difficulty::Standard);

    core::Difficulty difficulty() const { return difficulty_; }
    ~World();

    void tick(float dt);

    entt::registry& reg() { return ecs_; }
    const entt::registry& reg() const { return ecs_; }
    const core::Path& path() const { return path_; }
    const content::MapDef& map() const { return *map_; }
    const content::Registry& defs() const { return *defs_; }
    core::Rng& rng() { return rng_; }
    const core::Loadout& meta() const { return meta_; }

    // --- what the skill tree permits -------------------------------------
    // Levelling, imbuing and specialising are all gated on owning the relevant
    // node, so the meta progression is what turns plain level-1 towers into a
    // real board rather than being a pile of passive percentages.
    // Arrow is the starting tower. Every other type is bought once, at the root
    // of its own tree, so the roster a player fields is itself progression.
    bool towerUnlocked(const std::string& towerId) const;
    // The range a freshly built tower of this type would have, skill tree
    // included. Needed to draw a range ring BEFORE any gold is spent.
    float buildRange(const std::string& towerId) const;
    // The cheapest tower this profile can actually build. What the board should
    // preview when the player is only hovering, rather than always assuming the
    // starting tower.
    std::string cheapestUnlockedTower() const;
    bool levelUnlocked(int level) const;
    bool elementUnlocked(const std::string& elementId) const;
    bool towerSpecUnlocked(const std::string& towerId, const std::string& spec) const;
    bool elementSpecUnlocked(const std::string& elementId, const std::string& spec) const;
    // A specialisation is UNIQUE ON THE MAP: one sniper, one elf, one hunter,
    // and as many plain arrow towers as you like. The interesting decision is
    // therefore which three pairings you field, not one choice you are stuck
    // with for the whole run.
    std::vector<std::string> activeTowerSpecs() const;
    std::vector<std::string> activeElementSpecs() const;
    bool towerSpecInUse(const std::string& spec) const;
    bool elementSpecInUse(const std::string& spec) const;

    int lives() const { return lives_; }
    int startingLives() const { return startingLives_; }
    int gold() const { return gold_; }
    int waveIndex() const { return waveIndex_; }
    int waveCount() const { return static_cast<int>(map_->waves.size()); }
    Phase phase() const { return phase_; }
    bool waveInProgress() const { return phase_ == Phase::Wave; }
    int aliveEnemies() const;
    int towerCount() const;

    // Run statistics for the results screen. A roguelike's end-of-run screen is
    // where a loss becomes an investment rather than a dead end, and this one
    // reported only the wave reached -- nothing about how the run actually went.
    struct RunStats {
        int enemiesKilled = 0;
        int leaked = 0;       // enemies that reached the goal
        int goldEarned = 0;   // bounty only, not the starting purse
        int towersBuilt = 0;
    };
    const RunStats& stats() const { return stats_; }
    // Cumulative spawns for the whole run. Exists so tests can prove an early
    // overlap call does not quietly discard the rest of the current wave.
    int enemiesSpawned() const { return enemiesSpawned_; }

    // Shards are the meta currency: they survive the run that earned them and
    // are spent in the skill trees between runs.
    int shardsEarned() const { return shardsEarned_; }
    int shardsForRun() const;
    void addShards(int n) { shardsEarned_ += n; }
    void noteKill(int bounty);  // one enemy died and paid out

    // --- tower management -------------------------------------------------
    enum class PlaceResult {
        Ok, NotBuildable, Occupied, TooPoor, OutOfBounds, UnknownTower,
        Locked,  // the tower type has not been unlocked in the skill tree
    };
    PlaceResult placeTower(int tileX, int tileY, const std::string& towerId);
    bool upgradeTower(int tileX, int tileY);

    // In-run build-up. Each returns false if the step is unavailable or unaffordable.
    bool attachElement(int tileX, int tileY, const std::string& elementId);
    bool specialiseTower(int tileX, int tileY, const std::string& spec);
    bool specialiseElement(int tileX, int tileY, const std::string& spec);

    int attachElementCost(const std::string& elementId) const;
    int towerSpecCost(int tileX, int tileY) const;
    int elementSpecCost(int tileX, int tileY) const;

    // Which choices are legal right now, honouring the run lock.
    std::vector<std::string> availableTowerSpecs(int tileX, int tileY) const;
    std::vector<std::string> availableElementSpecs(int tileX, int tileY) const;
    bool sellTower(int tileX, int tileY);
    // What selling would actually pay. The menu said "refunds part of
    // everything invested", which is not a number anyone can act on.
    int sellValue(int tileX, int tileY) const;
    entt::entity towerAt(int tileX, int tileY) const;
    int upgradeCost(int tileX, int tileY) const;  // -1 if not upgradable
    // A tower must be fully levelled before it may specialise. Levelling is the
    // commitment; specialising is the reward for having made it.
    bool atMaxLevel(int tileX, int tileY) const;
    // The stats this tower WOULD have one level up. The upgrade button showed a
    // cost and nothing else, which makes an upgrade a gamble rather than a
    // decision. Returns false at max level.
    bool previewUpgrade(int tileX, int tileY, TowerStats& out) const;

    float buildTimeRemaining() const { return buildTimer_; }
    int earlyStartBonus() const;
    // Gold for calling the next wave ON TOP of one still running. Pays for the
    // risk taken, so it scales with how much of the current wave is still
    // unresolved -- enemies alive plus enemies yet to spawn. 0 outside a wave.
    int overlapCallBonus() const;
    // True when there is a further wave that could be called right now, either
    // from the build phase or on top of a running wave.
    bool canCallWave() const;
    // Gold the NextWave button would pay right now, whichever kind of call it is.
    int callBonus() const;
    void startNextWave();
    void enterSandbox() { phase_ = Phase::Sandbox; }
    // Dev capture only: drop straight onto a wave without playing the ones
    // before it. Sets the index and starts that wave, rather than looping
    // startNextWave, which only advances out of the build phase.
    void devSetWave(int wave) {
        waveIndex_ = std::clamp(wave, 0, waveCount() - 1);
        phase_ = Phase::Build;
        buildTimer_ = 0.0f;
        startNextWave();
    }

    // --- persistence -------------------------------------------------------
    // Runs are only ever captured during a build phase, when the field is empty.
    // That single rule removes any need to serialise enemies, projectiles or
    // in-flight status effects.
    bool canSnapshot() const { return phase_ == Phase::Build && aliveEnemies() == 0; }
    core::RunSave snapshot() const;
    void restore(const core::RunSave& s);

    // Visual events accumulate here and are drained by the renderer each frame.
    // Headless runs never drain, so the queue is capped rather than unbounded.
    void emit(const VisualEvent& e);
    std::vector<VisualEvent> drainEvents();

    // --- called by systems ------------------------------------------------
    void loseLife(int n);
    // Shadow's siphon can hand a life back. Capped at the starting count so a
    // long run cannot bank an unlosable buffer.
    void gainLife(int n);

    // --- player abilities --------------------------------------------------
    bool abilityReady(Ability a) const;
    float abilityCooldown(Ability a) const;      // seconds remaining, 0 when ready
    static float abilityCooldownMax(Ability a);
    // Casts at a board position in TILE coordinates. False when still cooling
    // down or when the run is not in a state that accepts a cast.
    bool castAbility(Ability a, core::Vec2 target);

    struct WardField {
        core::Vec2 pos;
        float radius = 0.0f;
        float pct = 0.0f;
        float remaining = 0.0f;
    };
    const std::vector<WardField>& wards() const { return wards_; }
    void updateAbilities(float dt);

    // Targeting. Five modes were implemented in the targeting system and
    // validated at content load from the day it was written, but were authored
    // per tower and frozen for the whole run -- the player could never pick one.
    bool setTowerPriority(int tileX, int tileY, TargetPriority p);
    TargetPriority towerPriority(int tileX, int tileY) const;
    static const char* priorityLabel(TargetPriority p);
    void addGold(int n);
    bool spendGold(int n);
    void spawnEnemy(const std::string& enemyId, float hpMult = 1.0f, float armorAdd = 0.0f,
                    float bountyMult = 1.0f);
    bool allGroupsExhausted() const;

private:
    const content::Registry* defs_;
    const content::MapDef* map_;
    entt::registry ecs_;
    core::Path path_;
    core::Rng rng_;
    core::Loadout meta_;
    // One behaviour instance per element spec, shared by every tower using it.
    std::map<std::string, std::unique_ptr<ElementBehavior>> elementCache_;

    void rebuildTower(entt::entity tower);
    ElementBehavior* behaviorFor(const std::string& elementId, const std::string& spec);
    core::Loadout loadoutFor(const TowerTag& tag) const;

    int lives_ = kStartingLives;
    // What this run STARTED with. gainLife caps against this rather than the
    // raw constant: on Relaxed a run opens with more lives than kStartingLives,
    // and capping at the constant would silently confiscate them.
    int startingLives_ = kStartingLives;
    int shardsEarned_ = 0;
    int gold_ = 0;
    int waveIndex_ = 0;
    int enemiesSpawned_ = 0;
    RunStats stats_;
    core::Difficulty difficulty_ = core::Difficulty::Standard;
    core::DifficultyMods mods_{};
    float abilityCd_[kAbilityCount] = {0.0f, 0.0f};
    std::vector<WardField> wards_;
    Phase phase_ = Phase::Build;
    float buildTimer_ = 0.0f;

    // Carries its OWN spawn parameters rather than indexing into
    // map_->waves[waveIndex_].groups[i]. That indexing is what made overlapping
    // waves impossible: calling the next wave early while one is still running
    // needs two waves spawning at once, and an index can only name one.
    struct GroupRuntime {
        int remaining = 0;
        float timer = 0.0f;
        std::string enemyId;
        float interval = 1.0f;
        float hpMult = 1.0f;
        float armorAdd = 0.0f;
        float bountyMult = 1.0f;
    };
    std::vector<GroupRuntime> groups_;
    std::vector<VisualEvent> events_;

    friend void runWaveSystem(World&, float);
    friend void runStatusSystem(World&, float);
    friend void runLeakSystem(World&, float);
};

}  // namespace td::sim
