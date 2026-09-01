#pragma once

#include <cstdint>
#include <map>
#include <algorithm>
#include <memory>
#include <string>
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
inline constexpr int kShardsPerWave = 2;
inline constexpr int kShardsPerMapClear = 40;

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
          const core::Loadout& meta = core::Loadout{}, int goldOverride = -1);
    ~World();

    void tick(float dt);

    entt::registry& reg() { return ecs_; }
    const entt::registry& reg() const { return ecs_; }
    const core::Path& path() const { return path_; }
    const content::MapDef& map() const { return *map_; }
    const content::Registry& defs() const { return *defs_; }
    core::Rng& rng() { return rng_; }
    const core::Loadout& meta() const { return meta_; }
    // A specialisation is UNIQUE ON THE MAP: one sniper, one elf, one hunter,
    // and as many plain arrow towers as you like. The interesting decision is
    // therefore which three pairings you field, not one choice you are stuck
    // with for the whole run.
    std::vector<std::string> activeTowerSpecs() const;
    std::vector<std::string> activeElementSpecs() const;
    bool towerSpecInUse(const std::string& spec) const;
    bool elementSpecInUse(const std::string& spec) const;

    int lives() const { return lives_; }
    int gold() const { return gold_; }
    int waveIndex() const { return waveIndex_; }
    int waveCount() const { return static_cast<int>(map_->waves.size()); }
    Phase phase() const { return phase_; }
    bool waveInProgress() const { return phase_ == Phase::Wave; }
    int aliveEnemies() const;

    // Shards are the meta currency: they survive the run that earned them and
    // are spent in the skill trees between runs.
    int shardsEarned() const { return shardsEarned_; }
    int shardsForRun() const;
    void addShards(int n) { shardsEarned_ += n; }

    // --- tower management -------------------------------------------------
    enum class PlaceResult { Ok, NotBuildable, Occupied, TooPoor, OutOfBounds, UnknownTower };
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
    int shardsEarned_ = 0;
    int gold_ = 0;
    int waveIndex_ = 0;
    Phase phase_ = Phase::Build;
    float buildTimer_ = 0.0f;

    struct GroupRuntime {
        int remaining = 0;
        float timer = 0.0f;
    };
    std::vector<GroupRuntime> groups_;
    std::vector<VisualEvent> events_;

    friend void runWaveSystem(World&, float);
    friend void runStatusSystem(World&, float);
    friend void runLeakSystem(World&, float);
};

}  // namespace td::sim
