// The three Earth specialisations. They live in one file because they change
// together and share the same stat prefix; splitting them per-file would spread
// one cohesive tuning surface across three places.

#include <algorithm>
#include <vector>

#include "core/Damage.h"
#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/elements/Factories.h"
#include "sim/World.h"

namespace td::sim {
namespace {

// --- Poison ---------------------------------------------------------------
// Stacks on every hit, so hit FREQUENCY drives its power. That is what makes
// Elf+Poison the sustained-damage build without a line of code naming Elf.
class PoisonBehavior : public ElementBehavior {
public:
    PoisonBehavior(const core::StatBlock& s, std::string dmgType)
        : dps_(s.get("earth.poison.dpsPerStack") * s.get("earth.potency", 1.0f)),
          maxStacks_(std::max(1.0f, s.get("earth.poison.maxStacks"))),
          duration_(s.get("earth.poison.duration") * s.get("earth.duration", 1.0f)),
          spreadPct_(s.get("earth.poison.spreadPct")),
          spreadRadius_(s.get("earth.poison.spreadRadius")),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        applyStacks(w, target, 1.0f);
    }

    void onKill(World& w, entt::entity target) override {
        if (spreadPct_ <= 0.0f) return;
        const auto* dying = w.reg().try_get<Poisoned>(target);
        const auto* pos = w.reg().try_get<Position>(target);
        if (!dying || !pos) return;

        const float carried = dying->stacks * spreadPct_;
        if (carried <= 0.0f) return;

        const core::Vec2 origin = pos->v;
        std::vector<entt::entity> nearby;
        w.reg().view<const Position, const EnemyTag>().each(
            [&](entt::entity e, const Position& p, const EnemyTag&) {
                if (e != target && core::distance(origin, p.v) <= spreadRadius_) {
                    nearby.push_back(e);
                }
            });
        for (const auto e : nearby) applyStacks(w, e, carried);
    }

private:
    void applyStacks(World& w, entt::entity target, float amount) {
        if (!w.reg().all_of<Health>(target)) return;
        auto* existing = w.reg().try_get<Poisoned>(target);
        if (existing) {
            existing->stacks = std::min(maxStacks_, existing->stacks + amount);
            existing->remaining = duration_;  // refresh, never extend
        } else {
            w.reg().emplace<Poisoned>(target, std::min(maxStacks_, amount), maxStacks_, dps_,
                                      duration_, type_);
        }
    }

    float dps_, maxStacks_, duration_, spreadPct_, spreadRadius_;
    std::string type_;
};

// --- Rock -----------------------------------------------------------------
// Shreds armour per hit and adds flat damage, so it rewards heavy single hits
// AND high hit counts differently: Sniper gets burst through armour, Elf strips
// armour fast. Neither case is coded for by name.
class RockBehavior : public ElementBehavior {
public:
    RockBehavior(const core::StatBlock& s, std::string dmgType)
        : shredPerHit_(s.get("earth.rock.shredPerHit") * s.get("earth.potency", 1.0f)),
          shredDuration_(s.get("earth.rock.shredDuration") * s.get("earth.duration", 1.0f)),
          petrifyChance_(s.get("earth.rock.petrifyChance")),
          petrifyDuration_(s.get("earth.rock.petrifyDuration") * s.get("earth.duration", 1.0f)),
          flatBonus_(s.get("earth.rock.flatBonus") * s.get("earth.potency", 1.0f)),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;

        if (auto* shred = r.try_get<ArmorShred>(target)) {
            shred->amount += shredPerHit_;
            shred->remaining = shredDuration_;
        } else {
            r.emplace<ArmorShred>(target, shredPerHit_, shredDuration_);
        }

        if (flatBonus_ > 0.0f) dealTyped(w, target, flatBonus_, type_);

        if (w.rng().chance(petrifyChance_)) {
            if (auto* p = r.try_get<Petrified>(target)) {
                p->remaining = std::max(p->remaining, petrifyDuration_);
            } else {
                r.emplace<Petrified>(target, petrifyDuration_);
            }
        }
    }

private:
    float shredPerHit_, shredDuration_, petrifyChance_, petrifyDuration_, flatBonus_;
    std::string type_;
};

// --- Earthquake -----------------------------------------------------------
// Erupts every Nth shot. Because "every Nth shot" is counted per tower, a fast
// tower quakes constantly and a slow one quakes rarely but from further away --
// again, emergent from the firing profile.
class QuakeBehavior : public ElementBehavior {
public:
    QuakeBehavior(const core::StatBlock& s, std::string dmgType)
        : radius_(s.get("earth.quake.radius") + s.get("earth.potency", 1.0f) - 1.0f),
          damage_(s.get("earth.quake.damage") * s.get("earth.potency", 1.0f)),
          slowPct_(std::min(0.9f, s.get("earth.quake.slowPct"))),
          slowDuration_(s.get("earth.quake.slowDuration") * s.get("earth.duration", 1.0f)),
          everyN_(std::max(1, static_cast<int>(s.get("earth.quake.everyNShots", 3.0f)))),
          type_(std::move(dmgType)) {}

    void onShoot(World& w, entt::entity tower, entt::entity projectile) override {
        auto& r = w.reg();
        auto* counter = r.try_get<ShotCounter>(tower);
        if (!counter) return;
        ++counter->n;
        if (counter->n % everyN_ == 0) r.emplace_or_replace<QuakePayload>(projectile);
    }

    void onHit(World& w, entt::entity projectile, entt::entity target, float) override {
        auto& r = w.reg();
        if (projectile == entt::null || !r.valid(projectile) ||
            !r.all_of<QuakePayload>(projectile)) {
            return;
        }
        const auto* origin = r.try_get<Position>(target);
        if (!origin) return;
        const core::Vec2 centre = origin->v;

        w.emit({VisualEvent::Kind::Quake, centre, {}, radius_, false, "quake"});

        std::vector<entt::entity> caught;
        r.view<const Position, const EnemyTag>().each(
            [&](entt::entity e, const Position& p, const EnemyTag&) {
                if (core::distance(centre, p.v) <= radius_) caught.push_back(e);
            });

        for (const auto e : caught) {
            dealTyped(w, e, damage_, type_);
            if (auto* slow = r.try_get<Slowed>(e)) {
                slow->pct = std::max(slow->pct, slowPct_);  // maximum, never compounding
                slow->remaining = std::max(slow->remaining, slowDuration_);
            } else {
                r.emplace<Slowed>(e, slowPct_, slowDuration_);
            }
        }
        r.remove<QuakePayload>(projectile);  // one eruption per marked arrow
    }

private:
    float radius_, damage_, slowPct_, slowDuration_;
    int everyN_;
    std::string type_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeEarth(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType) {
    if (spec == "poison") return std::make_unique<PoisonBehavior>(stats, damageType);
    if (spec == "rock") return std::make_unique<RockBehavior>(stats, damageType);
    if (spec == "quake") return std::make_unique<QuakeBehavior>(stats, damageType);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
