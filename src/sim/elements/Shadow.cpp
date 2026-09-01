// The three Shadow specialisations. Shadow is the attrition element: none of its
// specs help against the enemy in front of you, and all three leave every enemy
// after it worse off.

#include <algorithm>

#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/World.h"
#include "sim/elements/Factories.h"

namespace td::sim {
namespace {

// --- Wither ---------------------------------------------------------------
// Accumulates with no timer, so hit COUNT walks a target toward its cap and it
// is never refreshed or lost -- only ended by the target dying. This is the one
// effect in the game that does not expire, which is what makes Shadow feel like
// grinding an enemy down rather than bursting it.
class WitherBehavior : public ElementBehavior {
public:
    explicit WitherBehavior(const core::StatBlock& s)
        : perHit_(s.get("shadow.wither.pctPerHit") * s.get("shadow.potency", 1.0f)),
          maxPct_(s.get("shadow.wither.maxPct")) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;
        if (auto* wi = r.try_get<Withered>(target)) {
            wi->pct = std::min(maxPct_, wi->pct + perHit_);
        } else {
            r.emplace<Withered>(target, std::min(maxPct_, perHit_));
        }
    }

private:
    float perHit_, maxPct_;
};

// --- Siphon ---------------------------------------------------------------
// Kills occasionally hand a life back. Its value rises exactly when the run is
// going badly, which no other effect does.
class SiphonBehavior : public ElementBehavior {
public:
    explicit SiphonBehavior(const core::StatBlock& s)
        : lifeChance_(s.get("shadow.siphon.lifeChance") * s.get("shadow.potency", 1.0f)),
          gold_(static_cast<int>(s.get("shadow.siphon.goldPerKill"))) {}

    void onKill(World& w, entt::entity) override {
        if (gold_ > 0) w.addGold(gold_);
        if (w.rng().chance(lifeChance_)) w.gainLife(1);
    }

private:
    float lifeChance_;
    int gold_;
};

// --- Rift -----------------------------------------------------------------
// A kill tears open a rift that keeps damaging whatever walks through it, so it
// rewards killing things EARLY on the path, where the rest of the wave follows.
class RiftBehavior : public ElementBehavior {
public:
    RiftBehavior(const core::StatBlock& s, std::string dmgType)
        : radius_(s.get("shadow.rift.radius") + s.get("shadow.potency", 1.0f) - 1.0f),
          dps_(s.get("shadow.rift.dps") * s.get("shadow.potency", 1.0f)),
          duration_(s.get("shadow.rift.duration") * s.get("shadow.duration", 1.0f)),
          type_(std::move(dmgType)) {}

    void onKill(World& w, entt::entity target) override {
        auto& r = w.reg();
        const auto* pos = r.try_get<Position>(target);
        if (!pos) return;
        // Its own entity: the enemy that opened it is about to be destroyed.
        const auto e = r.create();
        r.emplace<Rift>(e, pos->v, radius_, dps_, duration_, type_);
        w.emit({VisualEvent::Kind::Quake, pos->v, {}, radius_, false, "rift"});
    }

private:
    float radius_, dps_, duration_;
    std::string type_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeShadow(const std::string& spec, const core::StatBlock& stats,
                                            const std::string& damageType) {
    if (spec == "wither") return std::make_unique<WitherBehavior>(stats);
    if (spec == "siphon") return std::make_unique<SiphonBehavior>(stats);
    if (spec == "rift") return std::make_unique<RiftBehavior>(stats, damageType);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
