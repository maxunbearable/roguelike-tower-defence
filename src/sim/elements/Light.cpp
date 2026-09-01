// The three Light specialisations. Light is the answer to a map built to resist
// whatever you brought: Sear ignores resistance outright, Judgement reaches past
// the chaff a fast tower is chewing through, and Beacon turns the whole board's
// single-target fire into crowd fire.

#include <algorithm>

#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/World.h"
#include "sim/elements/Factories.h"

namespace td::sim {
namespace {

// --- Sear -----------------------------------------------------------------
// Flat damage per hit that bypasses resistance entirely. It does not scale with
// the tower's damage, so it is proportionally enormous on a fast weak tower and
// nearly irrelevant on a siege shell -- the opposite gradient to every other
// element, and the reason it is the answer to a resistant map.
class SearBehavior : public ElementBehavior {
public:
    explicit SearBehavior(const core::StatBlock& s)
        : flat_(s.get("light.sear.flatPerHit") * s.get("light.potency", 1.0f)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        if (flat_ <= 0.0f) return;
        dealFlat(w, target, flat_);
    }

private:
    float flat_;
};

// --- Judgement ------------------------------------------------------------
// Every Nth shot smites the HEALTHIEST enemy within reach instead of whatever
// the tower happened to be aiming at. Counted per tower, so a fast tower smites
// often -- and it is the only effect that deliberately ignores the target the
// firing system chose.
class JudgementBehavior : public ElementBehavior {
public:
    JudgementBehavior(const core::StatBlock& s, std::string dmgType)
        : damage_(s.get("light.judgement.damage") * s.get("light.potency", 1.0f)),
          radius_(s.get("light.judgement.radius")),
          everyN_(std::max(1, static_cast<int>(s.get("light.judgement.everyNShots", 5.0f)))),
          type_(std::move(dmgType)) {}

    void onShoot(World& w, entt::entity tower, entt::entity) override {
        auto& r = w.reg();
        auto* counter = r.try_get<ShotCounter>(tower);
        if (!counter) return;
        ++counter->n;
        if (counter->n % everyN_ != 0) return;

        const auto* tpos = r.try_get<Position>(tower);
        if (!tpos) return;

        // Healthiest in reach, by absolute health: the thing most likely to be
        // the problem.
        entt::entity best = entt::null;
        float bestHp = -1.0f;
        for (const auto e : enemiesWithin(w, tpos->v, radius_)) {
            const auto* hp = r.try_get<Health>(e);
            if (hp && hp->hp > bestHp) {
                bestHp = hp->hp;
                best = e;
            }
        }
        if (best == entt::null) return;

        dealTyped(w, best, damage_, type_);
        if (const auto* p = r.try_get<Position>(best)) {
            w.emit({VisualEvent::Kind::Hit, p->v, {}, damage_, true, "judgement"});
        }
    }

private:
    float damage_, radius_;
    int everyN_;
    std::string type_;
};

// --- Beacon ---------------------------------------------------------------
// Marks a target. While the mark holds, hits on it from ANY tower share a
// fraction of their damage with the enemies around it, which is applied in the
// combat system rather than here -- that is what lets one beacon tower upgrade
// the whole board's fire instead of only its own.
class BeaconBehavior : public ElementBehavior {
public:
    explicit BeaconBehavior(const core::StatBlock& s)
        : radius_(s.get("light.beacon.radius")),
          sharePct_(s.get("light.beacon.sharePct") * s.get("light.potency", 1.0f)),
          duration_(s.get("light.beacon.duration") * s.get("light.duration", 1.0f)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;
        r.emplace_or_replace<Beaconed>(target, radius_, sharePct_, duration_);
    }

private:
    float radius_, sharePct_, duration_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeLight(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType) {
    if (spec == "sear") return std::make_unique<SearBehavior>(stats);
    if (spec == "judgement") return std::make_unique<JudgementBehavior>(stats, damageType);
    if (spec == "beacon") return std::make_unique<BeaconBehavior>(stats);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
