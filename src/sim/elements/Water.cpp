// The three Water specialisations. Water is the control element: two of its
// three specs do no damage of their own, and pay off through what they let the
// rest of the board do.

#include <algorithm>

#include "sim/AreaEffects.h"
#include "sim/ElementBehavior.h"
#include "sim/World.h"
#include "sim/elements/Factories.h"

namespace td::sim {
namespace {

// --- Chill ----------------------------------------------------------------
// Stacking slow, so hit COUNT walks a target toward the cap. Note it never
// reaches a full stop: the cap is a slow, and stopping things is Freeze's job.
class ChillBehavior : public ElementBehavior {
public:
    explicit ChillBehavior(const core::StatBlock& s)
        : perHit_(s.get("water.chill.slowPerHit") * s.get("water.potency", 1.0f)),
          maxSlow_(std::min(0.9f, s.get("water.chill.maxSlow"))),
          duration_(s.get("water.chill.duration") * s.get("water.duration", 1.0f)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;
        if (auto* s = r.try_get<Slowed>(target)) {
            s->pct = std::min(maxSlow_, s->pct + perHit_);
            s->remaining = std::max(s->remaining, duration_);
        } else {
            r.emplace<Slowed>(target, std::min(maxSlow_, perHit_), duration_);
        }
    }

private:
    float perHit_, maxSlow_, duration_;
};

// --- Shatter --------------------------------------------------------------
// Pays out in proportion to how slowed the target already is. That makes it a
// genuine decision rather than a strictly-better pick: on its own it does
// nothing, and it is the reason a second slowing tower has value.
class ShatterBehavior : public ElementBehavior {
public:
    ShatterBehavior(const core::StatBlock& s, std::string dmgType)
        : bonusPerSlow_(s.get("water.shatter.bonusPerSlowPct") * s.get("water.potency", 1.0f)),
          minSlow_(s.get("water.shatter.minSlow")),
          type_(std::move(dmgType)) {}

    void onHit(World& w, entt::entity, entt::entity target, float dealt) override {
        if (dealt <= 0.0f) return;
        const auto* slow = w.reg().try_get<Slowed>(target);
        if (!slow || slow->pct < minSlow_) return;
        dealTyped(w, target, dealt * bonusPerSlow_ * slow->pct, type_);
    }

private:
    float bonusPerSlow_, minSlow_;
    std::string type_;
};

// --- Freeze ---------------------------------------------------------------
// A hard stop on a roll. Bosses take it as a heavy slow instead, handled in the
// movement system so no element has to know what a boss is.
class FreezeBehavior : public ElementBehavior {
public:
    explicit FreezeBehavior(const core::StatBlock& s)
        : chance_(s.get("water.freeze.chance") * s.get("water.potency", 1.0f)),
          duration_(s.get("water.freeze.duration") * s.get("water.duration", 1.0f)) {}

    void onHit(World& w, entt::entity, entt::entity target, float) override {
        auto& r = w.reg();
        if (!r.all_of<Health>(target)) return;
        if (!w.rng().chance(chance_)) return;
        if (auto* f = r.try_get<Frozen>(target)) {
            f->remaining = std::max(f->remaining, duration_);
        } else {
            r.emplace<Frozen>(target, duration_);
        }
    }

private:
    float chance_, duration_;
};

}  // namespace

std::unique_ptr<ElementBehavior> makeWater(const std::string& spec, const core::StatBlock& stats,
                                           const std::string& damageType) {
    if (spec == "chill") return std::make_unique<ChillBehavior>(stats);
    if (spec == "shatter") return std::make_unique<ShatterBehavior>(stats, damageType);
    if (spec == "freeze") return std::make_unique<FreezeBehavior>(stats);
    return std::make_unique<ElementBehavior>();  // imbued but not specialised
}

}  // namespace td::sim
