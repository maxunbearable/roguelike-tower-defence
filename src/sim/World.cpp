#include "sim/World.h"

#include <algorithm>

#include "sim/ElementBehavior.h"
#include "sim/systems/CombatSystems.h"
#include "sim/systems/StatusSystem.h"
#include "sim/systems/EnemySystems.h"

namespace td::sim {
namespace {

// TOML waypoints are tile indices; the route runs through tile centres.
core::Path buildPath(const content::MapDef& m) {
    std::vector<core::Vec2> pts;
    pts.reserve(m.pathWaypoints.size());
    for (const auto& wp : m.pathWaypoints) pts.push_back(core::Vec2{wp.x + 0.5f, wp.y + 0.5f});
    return core::Path(std::move(pts));
}

const char* priorityName(TargetPriority p) {
    switch (p) {
        case TargetPriority::Last: return "last";
        case TargetPriority::Strongest: return "strongest";
        case TargetPriority::Weakest: return "weakest";
        case TargetPriority::Closest: return "closest";
        case TargetPriority::First: break;
    }
    return "first";
}

TargetPriority parsePriority(const std::string& s) {
    if (s == "last") return TargetPriority::Last;
    if (s == "strongest") return TargetPriority::Strongest;
    if (s == "weakest") return TargetPriority::Weakest;
    if (s == "closest") return TargetPriority::Closest;
    return TargetPriority::First;  // validated at content load, so this is safe
}

// Level multipliers are ABSOLUTE against base, not cumulative: level 3 damage is
// base * 2.4, never base * 1.6 * 2.4. `base` here means the SPEC-RESOLVED stat,
// so gold upgrades sit on top of skill-tree power rather than replacing it.
TowerStats statsFor(const content::TowerDef& d, int level, const core::StatBlock& sb) {
    const std::string p = d.id + ".";
    TowerStats s;
    s.damageType = d.damageType;
    s.damage = sb.get(p + "damage", d.damage);
    s.fireRate = sb.get(p + "fireRate", d.fireRate);
    s.range = sb.get(p + "range", d.range);
    s.projectileSpeed = sb.get(p + "projectileSpeed", d.projectileSpeed);
    s.projectileCount = static_cast<int>(sb.get(p + "projectileCount",
                                                static_cast<float>(d.projectileCount)));
    s.pierce = static_cast<int>(sb.get(p + "pierce", static_cast<float>(d.pierce)));
    s.critChance = sb.get(p + "critChance", d.critChance);
    s.critMult = sb.get(p + "critMult", d.critMult);
    s.armorPen = sb.get(p + "armorPen", d.armorPen);
    s.priority = parsePriority(d.targetPriority);

    const float baseDamage = s.damage;
    const float baseRange = s.range;
    const float baseRate = s.fireRate;

    const int idx = level - 2;
    if (idx >= 0 && idx < static_cast<int>(d.levels.size())) {
        const auto& lv = d.levels[static_cast<size_t>(idx)];
        s.damage = baseDamage * lv.damageMult;
        s.range = baseRange * lv.rangeMult;
        s.fireRate = baseRate * lv.fireRateMult;
    }
    return s;
}

}  // namespace

World::World(const content::Registry& reg, const content::MapDef& map, uint64_t seed,
             const core::Loadout& meta, int goldOverride)
    : defs_(&reg), map_(&map), path_(buildPath(map)), rng_(seed), meta_(meta),
      buildTimer_(map.buildTime) {
    // Global-tree power applies from the first second; spec power is bought
    // per-tower during the run.
    core::Loadout globalOnly = meta;
    globalOnly.towerSpec.clear();
    globalOnly.elementId.clear();
    globalOnly.elementSpec.clear();
    const auto globalStats = core::resolveStats(reg, globalOnly);

    gold_ = goldOverride >= 0
                ? goldOverride
                : map.startGold + static_cast<int>(globalStats.get("global.startGold"));
    lives_ = kStartingLives + static_cast<int>(globalStats.get("global.lives"));
}

World::~World() = default;

core::Loadout World::loadoutFor(const TowerTag& tag) const {
    core::Loadout lo = meta_;
    lo.towerId = tag.defId;
    lo.towerSpec = tag.towerSpec;    // empty means only the trunk applies
    lo.elementId = tag.elementId;    // empty means no element is folded in
    lo.elementSpec = tag.elementSpec;
    return lo;
}

ElementBehavior* World::behaviorFor(const std::string& elementId, const std::string& spec) {
    if (elementId.empty() || spec.empty()) return nullptr;
    const std::string key = elementId + "." + spec;
    const auto it = elementCache_.find(key);
    if (it != elementCache_.end()) return it->second.get();

    core::Loadout lo = meta_;
    lo.towerId.clear();
    lo.towerSpec.clear();
    lo.elementId = elementId;
    lo.elementSpec = spec;
    const auto stats = core::resolveStats(*defs_, lo);
    const std::string dmgType =
        defs_->hasElement(elementId) ? defs_->element(elementId).damageType : "physical";
    auto b = makeElement(elementId, spec, stats, dmgType);
    auto* raw = b.get();
    elementCache_[key] = std::move(b);
    return raw;
}

// Recomputes everything derived from a tower's build: stats, spec traits and
// which element behaviour its shots carry. Called after every purchase, so a
// tower's state is always a pure function of what has been bought for it.
void World::rebuildTower(entt::entity tower) {
    auto& tag = ecs_.get<TowerTag>(tower);
    const auto& def = defs_->tower(tag.defId);
    const auto stats = core::resolveStats(*defs_, loadoutFor(tag));

    auto rebuilt = statsFor(def, tag.level, stats);
    rebuilt.priority = tag.priority;  // the player's choice outranks the def
    ecs_.emplace_or_replace<TowerStats>(tower, rebuilt);

    const std::string p = def.id + ".";
    ecs_.remove<Execute>(tower);
    ecs_.remove<RampUp>(tower);
    ecs_.remove<Splash>(tower);
    ecs_.remove<Chain>(tower);
    ecs_.remove<Amplify>(tower);
    ecs_.remove<Drain>(tower);
    ecs_.remove<SlowOnHit>(tower);
    ecs_.remove<TowerBuff>(tower);
    if (stats.flag(p + "trait.execute")) {
        ecs_.emplace<Execute>(tower, stats.get(p + "execute.threshold", 0.6f),
                              stats.get(p + "execute.mult", 1.0f));
    }
    if (stats.flag(p + "trait.rampUp")) {
        ecs_.emplace<RampUp>(tower, stats.get(p + "rampUp.perSec", 0.0f),
                             stats.get(p + "rampUp.maxMult", 1.0f), 1.0f, entt::null);
    }

    if (stats.flag(p + "trait.splash")) {
        ecs_.emplace<Splash>(tower, stats.get(p + "splash.radius", 1.0f),
                             stats.get(p + "splash.damagePct", 0.5f),
                             stats.get(p + "splash.falloff", 0.5f));
    }
    if (stats.flag(p + "trait.chain")) {
        ecs_.emplace<Chain>(tower, static_cast<int>(stats.get(p + "chain.jumps", 1.0f)),
                            stats.get(p + "chain.radius", 2.0f),
                            stats.get(p + "chain.damagePct", 0.6f),
                            stats.get(p + "chain.falloff", 0.7f));
    }
    if (stats.flag(p + "trait.amplify")) {
        ecs_.emplace<Amplify>(tower, stats.get(p + "amplify.pct", 0.2f),
                              stats.get(p + "amplify.duration", 3.0f));
    }
    if (stats.flag(p + "trait.drain")) {
        ecs_.emplace<Drain>(tower,
                            static_cast<int>(stats.get(p + "drain.goldPerKill", 0.0f)));
    }
    if (stats.flag(p + "trait.slowOnHit")) {
        ecs_.emplace<SlowOnHit>(tower, stats.get(p + "slowOnHit.pct", 0.2f),
                                stats.get(p + "slowOnHit.duration", 1.5f));
    }
    if (stats.flag(p + "trait.towerBuff")) {
        ecs_.emplace<TowerBuff>(tower, stats.get(p + "towerBuff.radius", 2.5f),
                                stats.get(p + "towerBuff.damagePct", 0.15f),
                                stats.get(p + "towerBuff.fireRatePct", 0.15f));
    }

    if (auto* b = behaviorFor(tag.elementId, tag.elementSpec)) {
        ecs_.emplace_or_replace<ElementRef>(tower, b);
    } else {
        ecs_.remove<ElementRef>(tower);
    }
}

int World::towerCount() const {
    return static_cast<int>(ecs_.view<const TowerTag>().size());
}

int World::aliveEnemies() const {
    int n = 0;
    ecs_.view<const EnemyTag>().each([&](auto&&...) { ++n; });
    return n;
}

int World::earlyStartBonus() const {
    if (phase_ != Phase::Build) return 0;
    return static_cast<int>(buildTimer_) * 2;
}

int World::overlapCallBonus() const {
    if (phase_ != Phase::Wave) return 0;
    // Everything still unresolved in the waves already on the field.
    int pending = aliveEnemies();
    for (const auto& g : groups_) pending += std::max(0, g.remaining);
    return pending * kGoldPerPendingEnemy;
}

bool World::canCallWave() const {
    if (phase_ == Phase::Build) return waveIndex_ < waveCount();
    // Overlapping: the wave AFTER the one being fought must exist.
    return phase_ == Phase::Wave && waveIndex_ + 1 < waveCount();
}

int World::callBonus() const {
    if (phase_ == Phase::Build) return earlyStartBonus();
    return overlapCallBonus();
}

void World::startNextWave() {
    if (!canCallWave()) return;

    // Two kinds of call. From the build phase this begins the next wave and pays
    // for the build time skipped. From inside a running wave it stacks the next
    // wave ON TOP, and pays for the risk instead -- nothing is skipped, so the
    // enemies still owed by the current wave still arrive.
    const bool overlapping = phase_ == Phase::Wave;
    addGold(callBonus());
    if (overlapping) ++waveIndex_;

    const auto& wave = map_->waves[static_cast<size_t>(waveIndex_)];

    // Drop groups that have finished spawning so this cannot grow across 50
    // waves; anything still owing enemies is kept and keeps spawning.
    groups_.erase(std::remove_if(groups_.begin(), groups_.end(),
                                 [](const GroupRuntime& g) { return g.remaining <= 0; }),
                  groups_.end());
    groups_.reserve(groups_.size() + wave.groups.size());
    for (const auto& g : wave.groups) {
        groups_.push_back(GroupRuntime{g.count, g.startDelay, g.enemyId, g.interval,
                                       g.hpMult, g.armorAdd, g.bountyMult});
    }

    phase_ = Phase::Wave;
}

void World::loseLife(int n) {
    stats_.leaked += n;
    lives_ -= n;
    if (lives_ <= 0) {
        lives_ = 0;
        phase_ = Phase::Defeated;
    }
}

void World::gainLife(int n) {
    if (phase_ == Phase::Defeated) return;  // no resurrections
    lives_ = std::min(lives_ + n, kStartingLives);
}

void World::addGold(int n) { gold_ += n; }

bool World::setTowerPriority(int tileX, int tileY, TargetPriority p) {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return false;
    ecs_.get<TowerTag>(e).priority = p;
    if (auto* st = ecs_.try_get<TowerStats>(e)) st->priority = p;
    return true;
}

TargetPriority World::towerPriority(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return TargetPriority::First;
    return ecs_.get<TowerTag>(e).priority;
}

const char* World::priorityLabel(TargetPriority p) { return priorityName(p); }

void World::noteKill(int bounty) {
    ++stats_.enemiesKilled;
    stats_.goldEarned += bounty;
}

int World::shardsForRun() const {
    // Kills pay directly; surviving waves pays a steady rate; clearing the map
    // pays a lump sum. A losing run still earns something, which is what keeps
    // a failed roguelike run from feeling wasted.
    const int waveBonus = waveIndex_ * kShardsPerWave;
    const int clearBonus = phase_ == Phase::Cleared ? kShardsPerMapClear : 0;
    return shardsEarned_ + waveBonus + clearBonus;
}

void World::emit(const VisualEvent& e) {
    // A headless run never drains, so cap rather than grow without bound.
    constexpr size_t kMaxPending = 512;
    if (events_.size() >= kMaxPending) return;
    events_.push_back(e);
}

std::vector<VisualEvent> World::drainEvents() {
    std::vector<VisualEvent> out;
    out.swap(events_);
    return out;
}

bool World::spendGold(int n) {
    if (n > gold_) return false;
    gold_ -= n;
    return true;
}

bool World::allGroupsExhausted() const {
    return std::all_of(groups_.begin(), groups_.end(),
                       [](const GroupRuntime& g) { return g.remaining <= 0; });
}

void World::spawnEnemy(const std::string& enemyId, float hpMult, float armorAdd,
                       float bountyMult) {
    ++enemiesSpawned_;
    const auto& def = defs_->enemy(enemyId);
    const auto e = ecs_.create();
    const core::Vec2 start = path_.positionAt(0.0f);
    const float hp = def.maxHp * hpMult;
    ecs_.emplace<Position>(e, start);
    ecs_.emplace<PrevPosition>(e, start);
    ecs_.emplace<PathFollower>(e, 0.0f);
    ecs_.emplace<Health>(e, hp, hp);
    ecs_.emplace<Armor>(e, def.armor + armorAdd);
    ecs_.emplace<Speed>(e, def.speed);
    if (def.boss) ecs_.emplace<Boss>(e);
    ecs_.emplace<EnemyTag>(e, def.id, static_cast<int>(static_cast<float>(def.bounty) * bountyMult),
                           def.shardValue);
}

entt::entity World::towerAt(int tileX, int tileY) const {
    entt::entity found = entt::null;
    ecs_.view<const TowerTag, const TileCoord>().each(
        [&](entt::entity e, const TowerTag&, const TileCoord& tc) {
            if (tc.x == tileX && tc.y == tileY) found = e;
        });
    return found;
}

World::PlaceResult World::placeTower(int tileX, int tileY, const std::string& towerId) {
    if (tileX < 0 || tileY < 0 || tileX >= map_->gridW || tileY >= map_->gridH) {
        return PlaceResult::OutOfBounds;
    }
    if (!defs_->hasTower(towerId)) return PlaceResult::UnknownTower;
    if (!towerUnlocked(towerId)) return PlaceResult::Locked;
    if (!map_->buildableAt(tileX, tileY)) return PlaceResult::NotBuildable;
    if (towerAt(tileX, tileY) != entt::null) return PlaceResult::Occupied;

    const auto& def = defs_->tower(towerId);
    if (gold_ < def.buildCost) return PlaceResult::TooPoor;
    spendGold(def.buildCost);

    const auto e = ecs_.create();
    const core::Vec2 centre{static_cast<float>(tileX) + 0.5f, static_cast<float>(tileY) + 0.5f};
    ecs_.emplace<Position>(e, centre);
    ecs_.emplace<TileCoord>(e, tileX, tileY);
    ecs_.emplace<TowerTag>(e, def.id, 1, def.buildCost, std::string{}, std::string{},
                           std::string{}, parsePriority(def.targetPriority));
    ++stats_.towersBuilt;
    ecs_.emplace<Cooldown>(e, 0.0f);
    ecs_.emplace<TargetRef>(e);
    ecs_.emplace<ShotCounter>(e);
    rebuildTower(e);
    return PlaceResult::Ok;
}

bool World::towerUnlocked(const std::string& towerId) const {
    // The starting tower: without one buildable tower a first run has no game.
    if (towerId == kStartingTower) return true;
    return meta_.owns(towerId + ".unlock");
}

bool World::levelUnlocked(int level) const {
    if (level <= 1) return true;  // level 1 is what building gives you

    // Resolved from the tree rather than by node id, because the node that
    // GRANTS this is not named after it: "global.level2" grants the flag
    // "global.unlock.level2". Checking meta_.owns(flag) -- which this did --
    // locked levels 2 and 3 forever, since ownedNodes holds node ids and never
    // granted flags, so nothing but the test-only ownAll shortcut passed. It
    // measured as a profile owning ALL 150 nodes stalling at wave 42 of 50 with
    // every tower stuck at level 1, while ownAll cleared all 50.
    const std::string flag = level == 2 ? "global.unlock.level2" : "global.unlock.level3";
    if (!defs_->hasTree("global")) return false;
    for (const auto& n : defs_->tree("global").nodes) {
        if (!meta_.owns(n.id)) continue;
        for (const auto& m : n.modifiers) {
            if (m.target == flag) return true;
        }
    }
    return false;
}

bool World::elementUnlocked(const std::string& elementId) const {
    // Owning ANY node of an element's tree grants the right to imbue it; the
    // tree's root is the cheapest way in.
    if (meta_.ownAll) return true;
    if (!defs_->hasTree(elementId)) return false;
    for (const auto& n : defs_->tree(elementId).nodes) {
        if (meta_.owns(n.id)) return true;
    }
    return false;
}

bool World::towerSpecUnlocked(const std::string& towerId, const std::string& spec) const {
    return meta_.owns(towerId + "." + spec + ".core");
}

bool World::elementSpecUnlocked(const std::string& elementId, const std::string& spec) const {
    return meta_.owns(elementId + "." + spec + ".core");
}

int World::upgradeCost(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return -1;
    const auto& tag = ecs_.get<TowerTag>(e);
    const auto& def = defs_->tower(tag.defId);
    const int idx = tag.level - 1;  // level 1 buys levels[0]
    if (idx < 0 || idx >= static_cast<int>(def.levels.size())) return -1;
    // Gated: an unbought level is not for sale at any price.
    if (!levelUnlocked(tag.level + 1)) return -1;
    return def.levels[static_cast<size_t>(idx)].cost;
}

bool World::previewUpgrade(int tileX, int tileY, TowerStats& out) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return false;
    if (upgradeCost(tileX, tileY) < 0) return false;  // already max
    const auto& tag = ecs_.get<TowerTag>(e);
    // Exactly the path rebuildTower takes, one level higher, so the preview
    // cannot drift from what the upgrade actually produces.
    const auto stats = core::resolveStats(*defs_, loadoutFor(tag));
    out = statsFor(defs_->tower(tag.defId), tag.level + 1, stats);
    return true;
}

bool World::atMaxLevel(int tileX, int tileY) const {
    return towerAt(tileX, tileY) != entt::null && upgradeCost(tileX, tileY) < 0;
}

bool World::upgradeTower(int tileX, int tileY) {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return false;
    const int cost = upgradeCost(tileX, tileY);
    if (cost < 0 || gold_ < cost) return false;

    spendGold(cost);
    auto& tag = ecs_.get<TowerTag>(e);
    ++tag.level;
    tag.goldSpent += cost;
    rebuildTower(e);
    return true;
}

int World::attachElementCost(const std::string& elementId) const {
    if (!defs_->hasElement(elementId)) return -1;
    return defs_->element(elementId).attachCost;
}

int World::towerSpecCost(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return -1;
    const auto& tag = ecs_.get<TowerTag>(e);
    if (!tag.towerSpec.empty()) return -1;   // already specialised
    if (!atMaxLevel(tileX, tileY)) return -1;  // not yet earned
    return defs_->tower(tag.defId).specCost;
}

int World::elementSpecCost(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return -1;
    const auto& tag = ecs_.get<TowerTag>(e);
    if (tag.elementId.empty() || !tag.elementSpec.empty()) return -1;
    return defs_->element(tag.elementId).specCost;
}

std::vector<std::string> World::activeTowerSpecs() const {
    std::vector<std::string> out;
    ecs_.view<const TowerTag>().each([&](const TowerTag& t) {
        if (!t.towerSpec.empty()) out.push_back(t.towerSpec);
    });
    return out;
}

std::vector<std::string> World::activeElementSpecs() const {
    std::vector<std::string> out;
    ecs_.view<const TowerTag>().each([&](const TowerTag& t) {
        if (!t.elementSpec.empty()) out.push_back(t.elementSpec);
    });
    return out;
}

bool World::towerSpecInUse(const std::string& spec) const {
    const auto v = activeTowerSpecs();
    return std::find(v.begin(), v.end(), spec) != v.end();
}

bool World::elementSpecInUse(const std::string& spec) const {
    const auto v = activeElementSpecs();
    return std::find(v.begin(), v.end(), spec) != v.end();
}

std::vector<std::string> World::availableTowerSpecs(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return {};
    const auto& tag = ecs_.get<TowerTag>(e);
    if (!tag.towerSpec.empty()) return {};
    // Fully levelled first. This is the gate the whole upgrade path builds to.
    if (!atMaxLevel(tileX, tileY)) return {};
    if (!defs_->hasTree(tag.defId)) return {};

    // Only specs nobody else is already using.
    std::vector<std::string> out;
    for (const auto& spec : defs_->tree(tag.defId).specs) {
        // Unlocked by the tree AND not already fielded elsewhere on the map.
        if (!towerSpecUnlocked(tag.defId, spec)) continue;
        if (!towerSpecInUse(spec)) out.push_back(spec);
    }
    return out;
}

std::vector<std::string> World::availableElementSpecs(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return {};
    const auto& tag = ecs_.get<TowerTag>(e);
    if (tag.elementId.empty() || !tag.elementSpec.empty()) return {};
    if (!defs_->hasTree(tag.elementId)) return {};

    std::vector<std::string> out;
    for (const auto& spec : defs_->tree(tag.elementId).specs) {
        if (!elementSpecUnlocked(tag.elementId, spec)) continue;
        if (!elementSpecInUse(spec)) out.push_back(spec);
    }
    return out;
}

bool World::attachElement(int tileX, int tileY, const std::string& elementId) {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return false;
    auto& tag = ecs_.get<TowerTag>(e);
    if (!tag.elementId.empty()) return false;  // one element per tower
    if (!defs_->hasElement(elementId)) return false;
    if (!elementUnlocked(elementId)) return false;  // gated on its skill tree

    const int cost = defs_->element(elementId).attachCost;
    if (!spendGold(cost)) return false;
    tag.elementId = elementId;
    tag.goldSpent += cost;
    rebuildTower(e);
    return true;
}

bool World::specialiseTower(int tileX, int tileY, const std::string& spec) {
    const auto avail = availableTowerSpecs(tileX, tileY);
    if (std::find(avail.begin(), avail.end(), spec) == avail.end()) return false;

    const auto e = towerAt(tileX, tileY);
    auto& tag = ecs_.get<TowerTag>(e);
    const int cost = defs_->tower(tag.defId).specCost;
    if (!spendGold(cost)) return false;

    tag.towerSpec = spec;
    tag.goldSpent += cost;
    rebuildTower(e);
    return true;
}

bool World::specialiseElement(int tileX, int tileY, const std::string& spec) {
    const auto avail = availableElementSpecs(tileX, tileY);
    if (std::find(avail.begin(), avail.end(), spec) == avail.end()) return false;

    const auto e = towerAt(tileX, tileY);
    auto& tag = ecs_.get<TowerTag>(e);
    const int cost = defs_->element(tag.elementId).specCost;
    if (!spendGold(cost)) return false;

    tag.elementSpec = spec;
    tag.goldSpent += cost;
    rebuildTower(e);
    return true;
}

int World::sellValue(int tileX, int tileY) const {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return 0;
    const auto& tag = ecs_.get<TowerTag>(e);
    const auto& def = defs_->tower(tag.defId);
    return static_cast<int>(static_cast<float>(tag.goldSpent) * def.sellRefundPct);
}

bool World::sellTower(int tileX, int tileY) {
    const auto e = towerAt(tileX, tileY);
    if (e == entt::null) return false;
    const auto& tag = ecs_.get<TowerTag>(e);
    const auto& def = defs_->tower(tag.defId);
    (void)def;
    addGold(sellValue(tileX, tileY));
    ecs_.destroy(e);
    return true;
}

core::RunSave World::snapshot() const {
    core::RunSave s;
    s.mapId = map_->id;
    s.seed = rng_.seed();
    s.rngState = rng_.state();
    s.waveIndex = waveIndex_;
    s.gold = gold_;
    s.lives = lives_;
    s.buildTimer = buildTimer_;

    ecs_.view<const TowerTag, const TileCoord>().each(
        [&](const TowerTag& tag, const TileCoord& tc) {
            s.towers.push_back(core::TowerSave{tc.x, tc.y, tag.defId, tag.level, tag.goldSpent,
                                               tag.elementId, tag.towerSpec, tag.elementSpec,
                                               priorityName(tag.priority)});
        });
    return s;
}

void World::restore(const core::RunSave& s) {
    ecs_.clear();
    events_.clear();
    groups_.clear();

    rng_ = core::Rng(s.seed);
    if (!s.rngState.empty()) rng_.setState(s.rngState);

    waveIndex_ = s.waveIndex;
    gold_ = s.gold;
    lives_ = s.lives;
    buildTimer_ = s.buildTimer;
    phase_ = Phase::Build;

    // Towers are rebuilt directly rather than replayed through the purchase
    // API, which would charge for them all over again.
    for (const auto& t : s.towers) {
        // Fall back to arrow only if the saved tower no longer exists in
        // content, which is a content change rather than a normal load.
        const std::string wanted = defs_->hasTower(t.towerId) ? t.towerId : std::string{"arrow"};
        if (!defs_->hasTower(wanted)) break;
        const auto& def = defs_->tower(wanted);
        const auto e = ecs_.create();
        const core::Vec2 centre{static_cast<float>(t.x) + 0.5f, static_cast<float>(t.y) + 0.5f};
        ecs_.emplace<Position>(e, centre);
        ecs_.emplace<TileCoord>(e, t.x, t.y);
        ecs_.emplace<TowerTag>(e, def.id, t.level, t.goldSpent, t.elementId, t.towerSpec,
                               t.elementSpec, parsePriority(t.priority));
        ecs_.emplace<Cooldown>(e, 0.0f);
        ecs_.emplace<TargetRef>(e);
        ecs_.emplace<ShotCounter>(e);
        rebuildTower(e);
    }
}

void World::tick(float dt) {
    if (phase_ == Phase::Cleared || phase_ == Phase::Defeated) return;

    if (phase_ == Phase::Sandbox) {
        runStatusSystem(*this, dt);
        runMovementSystem(*this, dt);
        runTargetingSystem(*this, dt);
        runTowerBuffSystem(*this, dt);
        runFiringSystem(*this, dt);
        runTowerElementTick(*this, dt);
        runProjectileSystem(*this, dt);
        runDeathSystem(*this, dt);
        return;
    }

    if (phase_ == Phase::Build) {
        buildTimer_ -= dt;
        if (buildTimer_ <= 0.0f) {
            buildTimer_ = 0.0f;
            startNextWave();  // earlyStartBonus() is 0 here, so no free gold
        }
        return;
    }

    runWaveSystem(*this, dt);
    runStatusSystem(*this, dt);
    runMovementSystem(*this, dt);
    runLeakSystem(*this, dt);
    runTargetingSystem(*this, dt);
    runTowerBuffSystem(*this, dt);
    runFiringSystem(*this, dt);
    runTowerElementTick(*this, dt);
    runProjectileSystem(*this, dt);
    runDeathSystem(*this, dt);
}

}  // namespace td::sim
